#include "APP_FAULT.h"

#include <stdio.h>
#include <string.h>

/* -------------------------------------------------------------------------- */
/*  compile-time sanity check                                                  */
/*                                                                            */
/*  persistent fault log는 BKPSRAM app-private 슬롯에 20 word 단위로 저장한다. */
/*  따라서 구조체 크기도 정확히 80바이트여야 한다.                            */
/* -------------------------------------------------------------------------- */

typedef char app_fault_log_size_must_be_80_bytes[
    (sizeof(app_fault_log_t) == (20u * sizeof(uint32_t))) ? 1 : -1];

/* -------------------------------------------------------------------------- */
/*  내부 상수                                                                  */
/* -------------------------------------------------------------------------- */

#define APP_FAULT_MAGIC                    0xFA17u
#define APP_FAULT_VERSION                  0x01u
#define APP_FAULT_WORD_COUNT               20u
#define APP_FAULT_BKPSRAM_OFFSET_BYTES     256u

#ifndef APP_FAULT_DEFAULT_SHOW_MS
#define APP_FAULT_DEFAULT_SHOW_MS          10000u
#endif

#ifndef APP_FAULT_REFRESH_MS
#define APP_FAULT_REFRESH_MS               100u
#endif

#ifndef APP_FAULT_TITLE_FONT
#define APP_FAULT_TITLE_FONT               u8g2_font_6x13B_tr
#endif

#ifndef APP_FAULT_BODY_FONT
#define APP_FAULT_BODY_FONT                u8g2_font_4x6_tr
#endif

#define APP_FAULT_TITLE_BASELINE_Y         14u
#define APP_FAULT_SEPARATOR_Y              18u

#define APP_FAULT_SUMMARY_Y0               26u
#define APP_FAULT_SUMMARY_LINE_H           7u
#define APP_FAULT_REG_Y0                   48u
#define APP_FAULT_REG_LINE_H               7u

#define APP_FAULT_PROGRESS_X               8u
#define APP_FAULT_PROGRESS_Y               121u
#define APP_FAULT_PROGRESS_W               224u
#define APP_FAULT_PROGRESS_H               5u

#define APP_FAULT_EXC_RETURN_EXTENDED_FRAME_MASK 0x10u
#define APP_FAULT_EXTENDED_FRAME_WORDS          18u

typedef char app_fault_bkpsram_offset_must_be_word_aligned[
    ((APP_FAULT_BKPSRAM_OFFSET_BYTES % sizeof(uint32_t)) == 0u) ? 1 : -1];

/* -------------------------------------------------------------------------- */
/*  내부 타입                                                                  */
/* -------------------------------------------------------------------------- */

typedef struct
{
    char lines[3][96];   /* 화면에 바로 뿌릴 요약 설명 3줄 */
    uint8_t count;       /* 실제 사용된 줄 수            */
} app_fault_summary_block_t;

typedef struct
{
    uint32_t mask;       /* CFSR 안에서 검사할 bit mask   */
    const char *text;    /* 해당 bit가 set일 때 보여줄 짧은 영어 설명 */
} app_fault_reason_map_t;

/* -------------------------------------------------------------------------- */
/*  CFSR reason table                                                          */
/*                                                                            */
/*  우선순위는 "개발 중 실제로 바로 도움이 되는 설명" 위주로 정했다.            */
/*  화면 높이가 제한되어 있으므로, 여기서 앞쪽 몇 개만 골라 보여준다.            */
/* -------------------------------------------------------------------------- */

static const app_fault_reason_map_t s_app_fault_reason_table[] =
{
    { SCB_CFSR_IACCVIOL_Msk,  "Instruction fetch from blocked/invalid memory." },
    { SCB_CFSR_DACCVIOL_Msk,  "Data access hit blocked or invalid memory." },
    { SCB_CFSR_MUNSTKERR_Msk, "Mem fault while unstacking exception frame." },
    { SCB_CFSR_MSTKERR_Msk,   "Mem fault while stacking exception frame." },
    { SCB_CFSR_MLSPERR_Msk,   "Mem fault during lazy FPU state save." },

    { SCB_CFSR_IBUSERR_Msk,      "Instruction bus error on code fetch." },
    { SCB_CFSR_PRECISERR_Msk,    "Precise bus error. BFAR is useful if valid." },
    { SCB_CFSR_IMPRECISERR_Msk,  "Imprecise bus error. PC may be approximate." },
    { SCB_CFSR_UNSTKERR_Msk,     "Bus fault while unstacking exception frame." },
    { SCB_CFSR_STKERR_Msk,       "Bus fault while stacking exception frame." },
    { SCB_CFSR_LSPERR_Msk,       "Bus fault during lazy FPU state save." },

    { SCB_CFSR_UNDEFINSTR_Msk, "Undefined instruction at stacked PC." },
    { SCB_CFSR_INVSTATE_Msk,   "Invalid CPU state. Check Thumb bit/state." },
    { SCB_CFSR_INVPC_Msk,      "Invalid PC or EXC_RETURN on exception exit." },
    { SCB_CFSR_NOCP_Msk,       "FPU or coprocessor access not enabled." },
    { SCB_CFSR_UNALIGNED_Msk,  "Unaligned access trap was enabled and hit." },
    { SCB_CFSR_DIVBYZERO_Msk,  "Divide by zero trap was enabled and hit." }
};

/* -------------------------------------------------------------------------- */
/*  backup domain access helpers                                               */
/*                                                                            */
/*  중요한 점                                                                 */
/*  - 이번 구현에서는 RTC backup register를 전혀 사용하지 않는다.              */
/*  - 대신 BKPSRAM의 app-private 영역에 fault log를 저장한다.                  */
/*  - fault context에서는 HAL init 상태를 믿지 않으므로,                        */
/*    HAL API 대신 최소한의 레지스터 접근으로 clock/regulator만 직접 올린다.   */
/*  - BKPSRAM retention은 VBAT와 backup regulator 상태에 의존하므로,            */
/*    write/read 전에 backup regulator enable과 ready를 한 번 확인한다.        */
/* -------------------------------------------------------------------------- */

static void app_fault_prepare_backup_domain(void)
{
    uint32_t wait_count;

    /* ---------------------------------------------------------------------- */
    /*  PWR peripheral clock는 DBP/BRE 제어에 필요하다.                        */
    /* ---------------------------------------------------------------------- */
    RCC->APB1ENR |= RCC_APB1ENR_PWREN;
    (void)RCC->APB1ENR;

    /* ---------------------------------------------------------------------- */
    /*  backup domain write unlock                                             */
    /* ---------------------------------------------------------------------- */
    PWR->CR |= PWR_CR_DBP;
    (void)PWR->CR;

#if defined(RCC_AHB1ENR_BKPSRAMEN)
    /* ---------------------------------------------------------------------- */
    /*  BKPSRAM AHB clock enable                                               */
    /* ---------------------------------------------------------------------- */
    RCC->AHB1ENR |= RCC_AHB1ENR_BKPSRAMEN;
    (void)RCC->AHB1ENR;
#endif

#if defined(PWR_CSR_BRE)
    /* ---------------------------------------------------------------------- */
    /*  backup regulator enable                                                */
    /*                                                                        */
    /*  VDD가 꺼지고 VBAT만 남아 있을 때도 BKPSRAM retention이 유지되려면        */
    /*  이 regulator가 켜져 있어야 한다.                                      */
    /* ---------------------------------------------------------------------- */
    PWR->CSR |= PWR_CSR_BRE;
    (void)PWR->CSR;
#endif

#if defined(PWR_CSR_BRR)
    /* ---------------------------------------------------------------------- */
    /*  ready bit는 무한 대기하지 않고 bounded loop로 본다.                     */
    /*  fault path에서 영원히 멈추는 것보다, 최악의 경우 retention만 포기하고    */
    /*  reset으로 넘어가는 편이 낫다.                                         */
    /* ---------------------------------------------------------------------- */
    wait_count = 100000u;
    while (((PWR->CSR & PWR_CSR_BRR) == 0u) && (wait_count > 0u))
    {
        wait_count--;
    }
#endif

    __DSB();
    __ISB();
}

static volatile uint32_t *app_fault_backup_word_array(void)
{
    /* ---------------------------------------------------------------------- */
    /*  BKPSRAM 첫 구간은 boot shared/control block이 쓸 수 있으므로             */
    /*  app fault log는 0x100 오프셋 이후의 app-private 영역에 둔다.            */
    /*                                                                        */
    /*  현재 로그 크기                                                         */
    /*    20 word x 4 byte = 80 byte                                           */
    /*                                                                        */
    /*  현재 로그 사용 범위                                                    */
    /*    BKPSRAM_BASE + 0x100 ~ +0x14F                                        */
    /* ---------------------------------------------------------------------- */
    return (volatile uint32_t *)(BKPSRAM_BASE + APP_FAULT_BKPSRAM_OFFSET_BYTES);
}

static void app_fault_backup_write_words(const uint32_t *src_words, uint32_t word_count)
{
    volatile uint32_t *dst_words;
    uint32_t i;

    if (src_words == 0)
    {
        return;
    }

    if (word_count > APP_FAULT_WORD_COUNT)
    {
        word_count = APP_FAULT_WORD_COUNT;
    }

    app_fault_prepare_backup_domain();
    dst_words = app_fault_backup_word_array();

    for (i = 0u; i < word_count; i++)
    {
        dst_words[i] = src_words[i];
    }

    __DSB();
    __ISB();
}

static void app_fault_backup_read_words(uint32_t *dst_words, uint32_t word_count)
{
    volatile uint32_t *src_words;
    uint32_t i;

    if (dst_words == 0)
    {
        return;
    }

    if (word_count > APP_FAULT_WORD_COUNT)
    {
        word_count = APP_FAULT_WORD_COUNT;
    }

    app_fault_prepare_backup_domain();
    src_words = app_fault_backup_word_array();

    for (i = 0u; i < word_count; i++)
    {
        dst_words[i] = src_words[i];
    }
}

/* -------------------------------------------------------------------------- */
/*  header / validity helpers                                                  */
/* -------------------------------------------------------------------------- */

static uint32_t app_fault_make_header(app_fault_type_t type)
{
    return (((uint32_t)APP_FAULT_MAGIC) << 16) |
           (((uint32_t)APP_FAULT_VERSION) << 8) |
           ((uint32_t)type & 0xFFu);
}

static uint16_t app_fault_header_get_magic(uint32_t header)
{
    return (uint16_t)(header >> 16);
}

static uint8_t app_fault_header_get_version(uint32_t header)
{
    return (uint8_t)((header >> 8) & 0xFFu);
}

static app_fault_type_t app_fault_header_get_type(uint32_t header)
{
    return (app_fault_type_t)(header & 0xFFu);
}

static bool app_fault_log_is_valid(const app_fault_log_t *log)
{
    app_fault_type_t type;

    if (log == 0)
    {
        return false;
    }

    if (app_fault_header_get_magic(log->header) != APP_FAULT_MAGIC)
    {
        return false;
    }

    if (app_fault_header_get_version(log->header) != APP_FAULT_VERSION)
    {
        return false;
    }

    type = app_fault_header_get_type(log->header);
    if (type == APP_FAULT_TYPE_NONE)
    {
        return false;
    }

    return true;
}

/* -------------------------------------------------------------------------- */
/*  public read / clear                                                        */
/* -------------------------------------------------------------------------- */

bool APP_FAULT_ReadPersistentLog(app_fault_log_t *out_log)
{
    if (out_log == 0)
    {
        return false;
    }

    memset(out_log, 0, sizeof(*out_log));
    app_fault_backup_read_words((uint32_t *)out_log, APP_FAULT_WORD_COUNT);

    if (app_fault_log_is_valid(out_log) == false)
    {
        memset(out_log, 0, sizeof(*out_log));
        return false;
    }

    return true;
}

void APP_FAULT_ClearPersistentLog(void)
{
    uint32_t clear_words[APP_FAULT_WORD_COUNT];

    memset(clear_words, 0, sizeof(clear_words));
    app_fault_backup_write_words(clear_words, APP_FAULT_WORD_COUNT);
}

/* -------------------------------------------------------------------------- */
/*  register snapshot helpers                                                  */
/* -------------------------------------------------------------------------- */

static const uint32_t *app_fault_get_basic_exception_frame(const uint32_t *raw_stack_frame,
                                                           uint32_t exc_return)
{
    /* Cortex-M4F에서는 FPU extended frame가 붙을 수 있다.
     * EXC_RETURN bit[4] == 0 이면 extended frame가 존재한다.
     * 이 경우 basic frame(r0~xPSR)은 그 뒤쪽 18워드 위치에 있다. */
    if (raw_stack_frame == 0)
    {
        return 0u;
    }

    if ((exc_return & APP_FAULT_EXC_RETURN_EXTENDED_FRAME_MASK) == 0u)
    {
        return raw_stack_frame + APP_FAULT_EXTENDED_FRAME_WORDS;
    }

    return raw_stack_frame;
}

static void app_fault_fill_common_registers(app_fault_log_t *log,
                                            app_fault_type_t type,
                                            uint32_t exc_return)
{
    if (log == 0)
    {
        return;
    }

    memset(log, 0, sizeof(*log));

    /* header는 '이 로그가 유효하다'는 마무리 표식 역할도 하기 때문에,
     * 나머지 값을 채우기 시작할 때 바로 넣어도 되고 마지막에 넣어도 된다.
     * 여기서는 코드 가독성을 위해 먼저 넣는다. */
    log->header = app_fault_make_header(type);

    /* fault status 레지스터들 */
    log->cfsr = SCB->CFSR;
    log->hfsr = SCB->HFSR;
    log->dfsr = SCB->DFSR;
    log->afsr = SCB->AFSR;
    log->mmfar = SCB->MMFAR;
    log->bfar = SCB->BFAR;
    log->shcsr = SCB->SHCSR;

    /* 예외 복귀 관련 값과 core control 상태 */
    log->exc_return = exc_return;
    log->msp = __get_MSP();
    log->psp = __get_PSP();
    log->control = __get_CONTROL();
}

static void app_fault_fill_stacked_frame(app_fault_log_t *log,
                                         const uint32_t *basic_frame)
{
    if ((log == 0) || (basic_frame == 0))
    {
        return;
    }

    /* Cortex-M basic exception frame layout
     *  [0] r0
     *  [1] r1
     *  [2] r2
     *  [3] r3
     *  [4] r12
     *  [5] lr
     *  [6] pc
     *  [7] xpsr
     */
    log->r0   = basic_frame[0];
    log->r1   = basic_frame[1];
    log->r2   = basic_frame[2];
    log->r3   = basic_frame[3];
    log->r12  = basic_frame[4];
    log->lr   = basic_frame[5];
    log->pc   = basic_frame[6];
    log->xpsr = basic_frame[7];
}

static void app_fault_record_exception(app_fault_type_t type,
                                       uint32_t *raw_stack_frame,
                                       uint32_t exc_return)
{
    app_fault_log_t log;
    const uint32_t *basic_frame;

    app_fault_fill_common_registers(&log, type, exc_return);

    /* stack frame는 가능한 경우에만 읽는다.
     * fault 원인이 stack 자체의 손상일 수도 있으므로, 이 단계는 이론상
     * 추가 fault를 유발할 가능성이 있다. 하지만 개발 중 가장 중요한 정보인
     * PC/LR/xPSR를 얻기 위해 포함한다. */
    basic_frame = app_fault_get_basic_exception_frame(raw_stack_frame, exc_return);
    app_fault_fill_stacked_frame(&log, basic_frame);

    app_fault_backup_write_words((const uint32_t *)&log, APP_FAULT_WORD_COUNT);
}

void APP_FAULT_RecordSoftware(app_fault_type_t type, uint32_t pc_hint)
{
    app_fault_log_t log;

    /* 소프트웨어 경로는 예외 스택 프레임이 없으므로,
     * '현재 읽을 수 있는 것들'만 채운다. */
    app_fault_fill_common_registers(&log, type, 0xFFFFFFFFu);
    log.pc = pc_hint;

    app_fault_backup_write_words((const uint32_t *)&log, APP_FAULT_WORD_COUNT);
}

/* -------------------------------------------------------------------------- */
/*  reset helper                                                               */
/* -------------------------------------------------------------------------- */

static void app_fault_request_system_reset(void)
{
    /* BKPSRAM write가 실제 메모리/버스 쪽으로 밀려나가도록 barrier를 준다. */
    __DSB();
    __ISB();

    NVIC_SystemReset();

    /* 정상이라면 여기로 돌아오지 않는다.
     * 만약 돌아왔다면 더 이상 진행하지 않고 정지한다. */
    for (;;)
    {
        __NOP();
    }
}

/* -------------------------------------------------------------------------- */
/*  fault handler C entry                                                      */
/* -------------------------------------------------------------------------- */

void APP_FAULT_NmiC(uint32_t *stack_frame, uint32_t exc_return)
{
    __disable_irq();
    app_fault_record_exception(APP_FAULT_TYPE_NMI, stack_frame, exc_return);
    app_fault_request_system_reset();
}

void APP_FAULT_HardFaultC(uint32_t *stack_frame, uint32_t exc_return)
{
    __disable_irq();
    app_fault_record_exception(APP_FAULT_TYPE_HARDFAULT, stack_frame, exc_return);
    app_fault_request_system_reset();
}

void APP_FAULT_MemManageC(uint32_t *stack_frame, uint32_t exc_return)
{
    __disable_irq();
    app_fault_record_exception(APP_FAULT_TYPE_MEMMANAGE, stack_frame, exc_return);
    app_fault_request_system_reset();
}

void APP_FAULT_BusFaultC(uint32_t *stack_frame, uint32_t exc_return)
{
    __disable_irq();
    app_fault_record_exception(APP_FAULT_TYPE_BUSFAULT, stack_frame, exc_return);
    app_fault_request_system_reset();
}

void APP_FAULT_UsageFaultC(uint32_t *stack_frame, uint32_t exc_return)
{
    __disable_irq();
    app_fault_record_exception(APP_FAULT_TYPE_USAGEFAULT, stack_frame, exc_return);
    app_fault_request_system_reset();
}

/* -------------------------------------------------------------------------- */
/*  text helpers                                                               */
/* -------------------------------------------------------------------------- */

static const char *app_fault_get_title(app_fault_type_t type)
{
    switch (type)
    {
    case APP_FAULT_TYPE_HARDFAULT:
        return "HARDFAULT";

    case APP_FAULT_TYPE_MEMMANAGE:
        return "MEMMANAGE";

    case APP_FAULT_TYPE_BUSFAULT:
        return "BUSFAULT";

    case APP_FAULT_TYPE_USAGEFAULT:
        return "USAGEFAULT";

    case APP_FAULT_TYPE_NMI:
        return "NMI";

    case APP_FAULT_TYPE_ERROR_HANDLER:
        return "ERROR_HANDLER";

    case APP_FAULT_TYPE_NONE:
    default:
        return "FAULT";
    }
}

static void app_fault_summary_clear(app_fault_summary_block_t *block)
{
    if (block == 0)
    {
        return;
    }

    memset(block, 0, sizeof(*block));
}

static void app_fault_summary_push(app_fault_summary_block_t *block, const char *text)
{
    size_t copy_len;

    if ((block == 0) || (text == 0))
    {
        return;
    }

    if (block->count >= 3u)
    {
        return;
    }

    copy_len = strlen(text);
    if (copy_len >= sizeof(block->lines[0]))
    {
        copy_len = sizeof(block->lines[0]) - 1u;
    }

    memcpy(block->lines[block->count], text, copy_len);
    block->lines[block->count][copy_len] = '\0';
    block->count++;
}

static void app_fault_build_summary(const app_fault_log_t *log,
                                    app_fault_summary_block_t *summary)
{
    app_fault_type_t type;
    uint32_t cfsr;
    uint32_t hfsr;
    uint32_t i;

    if ((log == 0) || (summary == 0))
    {
        return;
    }

    app_fault_summary_clear(summary);

    type = app_fault_header_get_type(log->header);
    cfsr = log->cfsr;
    hfsr = log->hfsr;

    /* --------------------------------------------------------------
     *  1) fault 종류별 기본 설명 1줄
     * ------------------------------------------------------------*/
    switch (type)
    {
    case APP_FAULT_TYPE_HARDFAULT:
        if ((hfsr & SCB_HFSR_FORCED_Msk) != 0u)
        {
            app_fault_summary_push(summary,
                "Escalated configurable fault. Check CFSR.");
        }
        else if ((hfsr & SCB_HFSR_VECTTBL_Msk) != 0u)
        {
            app_fault_summary_push(summary,
                "Vector table fetch bus fault on exception.");
        }
        else if ((hfsr & SCB_HFSR_DEBUGEVT_Msk) != 0u)
        {
            app_fault_summary_push(summary,
                "Debug event entered HardFault path.");
        }
        else
        {
            app_fault_summary_push(summary,
                "HardFault with no extra hard-fault bit set.");
        }
        break;

    case APP_FAULT_TYPE_MEMMANAGE:
        app_fault_summary_push(summary,
            "Memory protection fault or bad stack region.");
        break;

    case APP_FAULT_TYPE_BUSFAULT:
        app_fault_summary_push(summary,
            "Bus transfer failed on instruction or data.");
        break;

    case APP_FAULT_TYPE_USAGEFAULT:
        app_fault_summary_push(summary,
            "Illegal execution state or invalid instruction.");
        break;

    case APP_FAULT_TYPE_NMI:
        app_fault_summary_push(summary,
            "Platform NMI. Registers below are captured.");
        break;

    case APP_FAULT_TYPE_ERROR_HANDLER:
        app_fault_summary_push(summary,
            "Software fatal path via Error_Handler().");
        app_fault_summary_push(summary,
            "PC below is only a best-effort caller hint.");
        break;

    case APP_FAULT_TYPE_NONE:
    default:
        app_fault_summary_push(summary,
            "Unknown persistent fault record.");
        break;
    }

    /* --------------------------------------------------------------
     *  2) CFSR의 '가장 설명력이 큰 bit' 두 개 정도를 추가로 고른다.
     * ------------------------------------------------------------*/
    for (i = 0u; i < (sizeof(s_app_fault_reason_table) / sizeof(s_app_fault_reason_table[0])); i++)
    {
        if ((cfsr & s_app_fault_reason_table[i].mask) != 0u)
        {
            app_fault_summary_push(summary, s_app_fault_reason_table[i].text);

            if (summary->count >= 3u)
            {
                break;
            }
        }
    }

    /* --------------------------------------------------------------
     *  3) 주소 레지스터 valid bit가 있으면 마지막 보충 설명으로 사용
     * ------------------------------------------------------------*/
    if (summary->count < 3u)
    {
        if ((cfsr & (SCB_CFSR_MMARVALID_Msk | SCB_CFSR_BFARVALID_Msk)) ==
            (SCB_CFSR_MMARVALID_Msk | SCB_CFSR_BFARVALID_Msk))
        {
            app_fault_summary_push(summary,
                "MMFAR and BFAR below are valid.");
        }
        else if ((cfsr & SCB_CFSR_MMARVALID_Msk) != 0u)
        {
            app_fault_summary_push(summary,
                "MMFAR below is a valid fault address.");
        }
        else if ((cfsr & SCB_CFSR_BFARVALID_Msk) != 0u)
        {
            app_fault_summary_push(summary,
                "BFAR below is a valid fault address.");
        }
    }
}

/* -------------------------------------------------------------------------- */
/*  drawing helpers                                                            */
/* -------------------------------------------------------------------------- */

static void app_fault_draw_centered_title(u8g2_t *u8g2, const char *title)
{
    uint16_t title_width;
    uint16_t x;

    if ((u8g2 == 0) || (title == 0))
    {
        return;
    }

    u8g2_SetFont(u8g2, APP_FAULT_TITLE_FONT);
    title_width = u8g2_GetStrWidth(u8g2, title);

    if (title_width >= u8g2_GetDisplayWidth(u8g2))
    {
        x = 0u;
    }
    else
    {
        x = (uint16_t)((u8g2_GetDisplayWidth(u8g2) - title_width) / 2u);
    }

    u8g2_DrawStr(u8g2, x, APP_FAULT_TITLE_BASELINE_Y, title);
    u8g2_DrawHLine(u8g2, 0u, APP_FAULT_SEPARATOR_Y, u8g2_GetDisplayWidth(u8g2));
}

static void app_fault_draw_summary(u8g2_t *u8g2, const app_fault_summary_block_t *summary)
{
    uint8_t i;

    if ((u8g2 == 0) || (summary == 0))
    {
        return;
    }

    u8g2_SetFont(u8g2, APP_FAULT_BODY_FONT);

    for (i = 0u; i < summary->count; i++)
    {
        u8g2_DrawStr(u8g2,
                     2u,
                     (u8g2_uint_t)(APP_FAULT_SUMMARY_Y0 + (uint16_t)i * APP_FAULT_SUMMARY_LINE_H),
                     summary->lines[i]);
    }
}

static void app_fault_draw_reg_pair(u8g2_t *u8g2,
                                    uint8_t row_index,
                                    const char *left_name,
                                    uint32_t left_value,
                                    const char *right_name,
                                    uint32_t right_value)
{
    char line[48];
    uint8_t y;

    if ((u8g2 == 0) || (left_name == 0) || (right_name == 0))
    {
        return;
    }

    y = (uint8_t)(APP_FAULT_REG_Y0 + row_index * APP_FAULT_REG_LINE_H);

    snprintf(line,
             sizeof(line),
             "%s:%08lX %s:%08lX",
             left_name,
             (unsigned long)left_value,
             right_name,
             (unsigned long)right_value);

    u8g2_DrawStr(u8g2, 2u, y, line);
}

static void app_fault_draw_single_reg(u8g2_t *u8g2,
                                      uint8_t row_index,
                                      const char *name,
                                      uint32_t value)
{
    char line[24];
    uint8_t y;

    if ((u8g2 == 0) || (name == 0))
    {
        return;
    }

    y = (uint8_t)(APP_FAULT_REG_Y0 + row_index * APP_FAULT_REG_LINE_H);

    snprintf(line,
             sizeof(line),
             "%s:%08lX",
             name,
             (unsigned long)value);

    u8g2_DrawStr(u8g2, 2u, y, line);
}

static void app_fault_draw_register_block(u8g2_t *u8g2, const app_fault_log_t *log)
{
    if ((u8g2 == 0) || (log == 0))
    {
        return;
    }

    u8g2_SetFont(u8g2, APP_FAULT_BODY_FONT);

    /* row 0~9 : 총 10줄에 register dump를 최대한 빽빽하게 넣는다. */
    app_fault_draw_reg_pair(u8g2, 0u, "CFSR",  log->cfsr,       "HFSR",  log->hfsr);
    app_fault_draw_reg_pair(u8g2, 1u, "DFSR",  log->dfsr,       "AFSR",  log->afsr);
    app_fault_draw_reg_pair(u8g2, 2u, "MMFAR", log->mmfar,      "BFAR",  log->bfar);
    app_fault_draw_reg_pair(u8g2, 3u, "SHCSR", log->shcsr,      "EXRET", log->exc_return);
    app_fault_draw_reg_pair(u8g2, 4u, "MSP",   log->msp,        "PSP",   log->psp);
    app_fault_draw_reg_pair(u8g2, 5u, "R0",    log->r0,         "R1",    log->r1);
    app_fault_draw_reg_pair(u8g2, 6u, "R2",    log->r2,         "R3",    log->r3);
    app_fault_draw_reg_pair(u8g2, 7u, "R12",   log->r12,        "LR",    log->lr);
    app_fault_draw_reg_pair(u8g2, 8u, "PC",    log->pc,         "XPSR",  log->xpsr);
    app_fault_draw_single_reg(u8g2, 9u, "CTRL", log->control);
}

static void app_fault_draw_progress_bar(u8g2_t *u8g2,
                                        uint32_t elapsed_ms,
                                        uint32_t total_ms)
{
    uint32_t fill_w;

    if ((u8g2 == 0) || (total_ms == 0u))
    {
        return;
    }

    if (elapsed_ms >= total_ms)
    {
        fill_w = APP_FAULT_PROGRESS_W;
    }
    else
    {
        fill_w = (elapsed_ms * APP_FAULT_PROGRESS_W) / total_ms;
    }

    u8g2_DrawFrame(u8g2,
                   APP_FAULT_PROGRESS_X,
                   APP_FAULT_PROGRESS_Y,
                   APP_FAULT_PROGRESS_W,
                   APP_FAULT_PROGRESS_H);

    if (fill_w > 2u)
    {
        u8g2_DrawBox(u8g2,
                     (u8g2_uint_t)(APP_FAULT_PROGRESS_X + 1u),
                     (u8g2_uint_t)(APP_FAULT_PROGRESS_Y + 1u),
                     (u8g2_uint_t)(fill_w - 2u),
                     (u8g2_uint_t)(APP_FAULT_PROGRESS_H - 2u));
    }
}

static void app_fault_draw_screen(u8g2_t *u8g2,
                                  const app_fault_log_t *log,
                                  uint32_t elapsed_ms,
                                  uint32_t total_ms)
{
    app_fault_summary_block_t summary;
    app_fault_type_t type;

    if ((u8g2 == 0) || (log == 0))
    {
        return;
    }

    type = app_fault_header_get_type(log->header);

    app_fault_build_summary(log, &summary);

    u8g2_ClearBuffer(u8g2);

    /* 1) 큰 제목 */
    app_fault_draw_centered_title(u8g2, app_fault_get_title(type));

    /* 2) 친절한 요약 설명 */
    app_fault_draw_summary(u8g2, &summary);

    /* 3) fault 관련 register dump */
    app_fault_draw_register_block(u8g2, log);

    /* 4) 하단 progress bar */
    app_fault_draw_progress_bar(u8g2, elapsed_ms, total_ms);

    /* 부팅 단계에서는 frame-limit/smart-update보다 단순 full send가 더 안전하다. */
    u8g2_SendBuffer(u8g2);
}

/* -------------------------------------------------------------------------- */
/*  boot time show                                                             */
/* -------------------------------------------------------------------------- */

void APP_FAULT_BootCheckAndShow(u8g2_t *u8g2, uint32_t show_ms)
{
    app_fault_log_t log;
    uint32_t start_ms;
    uint32_t elapsed_ms;

    /* u8g2 핸들이 없으면 아무 것도 할 수 없다. */
    if (u8g2 == 0)
    {
        return;
    }

    /* 표시 시간 0이 들어오면 기본값 10초로 강제한다. */
    if (show_ms == 0u)
    {
        show_ms = APP_FAULT_DEFAULT_SHOW_MS;
    }

    /* persistent log가 없으면 곧바로 리턴 */
    if (APP_FAULT_ReadPersistentLog(&log) == false)
    {
        return;
    }

    /* fault 로그가 있을 때만 boot splash처럼 10초 정도 보여준다. */
    start_ms = HAL_GetTick();

    do
    {
        elapsed_ms = HAL_GetTick() - start_ms;
        app_fault_draw_screen(u8g2, &log, elapsed_ms, show_ms);

        /* HAL_Delay는 SysTick 기반이고, 이 시점에서는 main boot path 상이므로 안전하다.
         * 100ms 주기로 redraw하면 progress bar가 충분히 자연스럽게 움직인다. */
        HAL_Delay(APP_FAULT_REFRESH_MS);
    }
    while ((HAL_GetTick() - start_ms) < show_ms);

    /* 같은 로그가 다음 부팅에서도 다시 나오지 않게 clear */
    APP_FAULT_ClearPersistentLog();
}
