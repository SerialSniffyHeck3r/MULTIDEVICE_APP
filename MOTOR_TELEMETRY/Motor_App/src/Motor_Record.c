
#include "APP_MEMORY_SECTIONS.h"
#include "Motor_Record.h"

#include "Motor_State.h"

#include "APP_SD.h"
#include "fatfs.h"

#include <stdio.h>
#include <string.h>

/* -------------------------------------------------------------------------- */
/*  Recorder foreground I/O ?꾪솕??write queue                                 */
/*                                                                            */
/*  湲곗〈 臾몄젣                                                                   */
/*  - NAV 100ms / DYN 50ms / OBD 200ms 二쇨린留덈떎 f_write() 瑜??꾧꼍?먯꽌 吏곸젒     */
/*    ?몄텧?덈떎.                                                                */
/*  - SD card 吏?곗씠 ?앷린硫?UI / button / ?곸쐞 ?곹깭癒몄떊??媛숈? superloop ?덉뿉??*/
/*    媛숈씠 ?붾뱾?몃떎.                                                           */
/*                                                                            */
/*  ??援ъ“                                                                     */
/*  1) record payload ??癒쇱? RAM queue ???곸옱?쒕떎.                           */
/*  2) flush phase ?먯꽌 ?щ윭 record 瑜?burst buffer 濡?紐⑥븘 ??踰덉뿉 f_write()  */
/*     ?쒕떎.                                                                   */
/*  3) f_sync() ??start / stop ?먮뒗 ?二쇨린 sync ?쒖젏?먮쭔 ?섑뻾?쒕떎.            */
/*                                                                            */
/*  寃곌낵                                                                       */
/*  - foreground write ?몄텧 鍮덈룄 媛먯냼                                          */
/*  - header/payload ?댁쨷 f_write ?쒓굅                                         */
/*  - UI? logger ?ъ씠??媛꾩꽠 ?꾪솕                                              */
/* -------------------------------------------------------------------------- */
#define MOTOR_RECORD_QUEUE_DEPTH              32u
#define MOTOR_RECORD_QUEUE_PAYLOAD_MAX_BYTES  64u
#define MOTOR_RECORD_QUEUE_ITEM_MAX_BYTES     (sizeof(motor_log_record_header_t) + MOTOR_RECORD_QUEUE_PAYLOAD_MAX_BYTES)
#define MOTOR_RECORD_FLUSH_BURST_MAX_BYTES   512u
#define MOTOR_RECORD_FLUSH_WATERMARK_ITEMS      6u
#define MOTOR_RECORD_FLUSH_BATCH_ITEMS          8u
#define MOTOR_RECORD_FLUSH_PERIOD_MS          250u
#define MOTOR_RECORD_SYNC_PERIOD_MS          1000u
#define MOTOR_RECORD_PREROLL_DEPTH           256u
#define MOTOR_RECORD_ROOT_DIR_PRIMARY "0:/RECORD"
#define MOTOR_RECORD_ROOT_DIR_DRIVE_REL "0:RECORD"
#define MOTOR_RECORD_ROOT_DIR_FALLBACK "RECORD"
#define MOTOR_RECORD_ROOT_DIR_MAX_LEN         16u
#define MOTOR_RECORD_MAX_FILE_INDEX      9999999u

typedef struct
{
    uint16_t total_size;
    uint8_t  bytes[MOTOR_RECORD_QUEUE_ITEM_MAX_BYTES];
} motor_record_queue_item_t;

static FIL s_log_file;
static uint8_t s_log_file_open;
static uint32_t s_last_nav_write_ms;
static uint32_t s_last_dyn_write_ms;
static uint32_t s_last_obd_write_ms;
static uint32_t s_last_distance_integrate_ms;
static uint32_t s_distance_remainder_mm;
static uint32_t s_record_distance_remainder_mm;
static uint32_t s_session_counter;

static motor_record_queue_item_t s_record_queue[MOTOR_RECORD_QUEUE_DEPTH] APP_CCMRAM_BSS;
static uint8_t s_record_queue_head;
static uint8_t s_record_queue_tail;
static uint8_t s_record_queue_count;
static uint8_t s_record_flush_burst[MOTOR_RECORD_FLUSH_BURST_MAX_BYTES] APP_CCMRAM_BSS;
static motor_record_queue_item_t s_preroll_queue[MOTOR_RECORD_PREROLL_DEPTH] APP_CCMRAM_BSS;
static uint16_t s_preroll_head;
static uint16_t s_preroll_tail;
static uint16_t s_preroll_count;

static void motor_record_queue_reset(void);
static void motor_record_close_file_if_fs_safe(void);
static void motor_record_abort_io_session(motor_state_t *state,
                                          const char *toast_text,
                                          uint8_t request_storage_recovery);
static void motor_record_note_fs_failure(app_sd_fs_stage_t stage, FRESULT fr);

static void motor_record_reset_open_runtime(motor_state_t *state,
                                            motor_record_state_t next_state,
                                            uint8_t graceful_close_done)
{
    /* ---------------------------------------------------------------------- */
    /*  Recorder handle/runtime reset helper                                  */
    /*                                                                        */
    /*  The recorder can now terminate through multiple paths: normal stop,   */
    /*  start failure after f_open(), and SD teardown triggered by APP_SD.    */
    /*  This helper centralizes the cleanup rules so every exit path clears    */
    /*  the FIL handle, write queue, request flags, and stale filename        */
    /*  consistently before a later session starts again.                     */
    /* ---------------------------------------------------------------------- */
    memset(&s_log_file, 0, sizeof(s_log_file));
    s_log_file_open = 0u;
    motor_record_queue_reset();

    if (state != 0)
    {
        state->record.open_ok = false;
        state->record.graceful_close_done = (graceful_close_done != 0u) ? true : false;
        state->record.state = (uint8_t)next_state;
        state->record.file_name[0] = '\0';
        state->record.start_requested = false;
        state->record.stop_requested = false;
        state->record.marker_requested = false;
        state->record_session.active = false;
    }
}

static void motor_record_close_file_if_fs_safe(void)
{
    /* ---------------------------------------------------------------------- */
    /*  Recorder FIL best-effort close helper                                 */
    /*                                                                        */
    /*  왜 별도 helper 가 필요한가                                             */
    /*  - recorder write/sync/open error 는 "세션은 실패했지만 DET 핀 기준       */
    /*    물리 카드 제거는 아직 일어나지 않은" 중간 상태일 수 있다.              */
    /*  - 이런 순간에는 정상 close 가 항상 성공한다고 믿을 수는 없지만,          */
    /*    반대로 무조건 FIL 을 memset 으로 날려 버리면 FatFs 내부 입장에서는      */
    /*    "열려 있던 파일 객체를 상위가 몰래 잃어버린" 모양이 된다.              */
    /*                                                                        */
    /*  보수적 정책                                                             */
    /*  - APP_SD 가 아직 FatFs 접근을 허용하는 동안에만 f_close() 를 한 번       */
    /*    best-effort 로 시도한다.                                             */
    /*  - 여기서 실패하더라도 추가 recovery/unmount 를 강제하지는 않는다.        */
    /*    그 부분은 DET 기반 hot-remove 경로가 책임지고, recorder 는 세션만      */
    /*    국소적으로 폐기한다.                                                 */
    /* ---------------------------------------------------------------------- */
    if (s_log_file_open == 0u)
    {
        return;
    }

    if (APP_SD_IsFsAccessAllowedNow() == false)
    {
        return;
    }

    (void)f_close(&s_log_file);
}

static void motor_record_note_fs_failure(app_sd_fs_stage_t stage, FRESULT fr)
{
    /* ---------------------------------------------------------------------- */
    /*  Recorder 는 첫 FatFs 실패 stage 를 공용 SD 진단 저장소에 남긴다.         */
    /*                                                                        */
    /*  목적                                                                   */
    /*  - 사용자가 보는 토스트는 WRITE ERR / NAME ERR / DIR ERR 같이             */
    /*    사람이 읽기 좋은 분류다.                                              */
    /*  - 하지만 원인 분석에는 "정확히 어떤 FatFs API 가 어떤 FRESULT 로          */
    /*    실패했는가" 가 필요하다.                                               */
    /*                                                                        */
    /*  이 helper 는 그 raw 정보를 APP_STATE.sd 로 publish 해서                 */
    /*  디버그 화면/디버거/후속 분석이 동일한 값을 보게 만든다.                  */
    /* ---------------------------------------------------------------------- */
    APP_SD_RecordFsFailure(stage, (uint32_t)fr);
}

static void motor_record_abort_io_session(motor_state_t *state,
                                          const char *toast_text,
                                          uint8_t request_storage_recovery)
{
    uint8_t dropped_items;

    /* ---------------------------------------------------------------------- */
    /*  중요 회귀 분석 결과                                                    */
    /*                                                                        */
    /*  이전 수정에서는 recorder write error 직후                              */
    /*    1) FIL/runtime 을 즉시 memset 으로 지우고                            */
    /*    2) APP_SD_RequestRecovery() 로 다음 loop 에서 강제 unmount/remount    */
    /*  를 태웠다.                                                             */
    /*                                                                        */
    /*  그런데 첫 WRITE ERR 후 바로 BUSFAULT 가 재현되었고, 경로를 추적해 보니  */
    /*  "상위 recorder 가 자기 FIL 을 잃어버린 상태에서 shared storage 계층이   */
    /*   볼륨 teardown 을 강행"하는 조합이 지나치게 공격적이었다.               */
    /*                                                                        */
    /*  그래서 지금은 정책을 바꾼다.                                           */
    /*  - recorder-originated I/O error 는 우선 recorder 세션만 죽인다.        */
    /*  - 가능한 경우에만 f_close() 를 best-effort 로 시도한다.                */
    /*  - 그리고 recovery 가 필요할 때도 "정리 후 다음 loop 에서" 요청한다.    */
    /*    즉, teardown 은 여전히 APP_SD_Task() 가 수행한다.                    */
    /*                                                                        */
    /*  이렇게 하면 "WRITE ERR -> 즉시 BUSFAULT" 직행열차를 끊고,               */
    /*  오류가 나더라도 최소한 recorder local failure 로 격리된다.              */
    /* ---------------------------------------------------------------------- */
    dropped_items = s_record_queue_count;
    motor_record_close_file_if_fs_safe();
    motor_record_reset_open_runtime(state, MOTOR_RECORD_STATE_ERROR, 0u);

    if (state != 0)
    {
        state->record.drop_count += dropped_items;
        state->record.last_write_ms = state->now_ms;
        state->record.last_flush_ms = state->now_ms;
        state->record_session.active = false;
        state->record_session.stop_ms = state->now_ms;
    }

    if (request_storage_recovery != 0u)
    {
        /* ------------------------------------------------------------------ */
        /*  recorder I/O 실패 이후의 shared storage recovery                   */
        /*                                                                    */
        /*  이전 회귀에서는 FIL 을 거칠게 날려 버린 직후 곧바로 recovery 를      */
        /*  걸면서 BUSFAULT 로 이어졌다.                                       */
        /*                                                                    */
        /*  지금은 순서를 바꿨다.                                               */
        /*  1) 가능한 경우 먼저 f_close() best-effort                          */
        /*  2) recorder local runtime 정리                                     */
        /*  3) 그 다음에만 shared APP_SD remount recovery 요청                 */
        /*                                                                    */
        /*  그래서 genuine write/sync/open failure 가 한 번 발생했을 때        */
        /*  다음 시도에서 DIR/NAME ERR 루프에 빠지지 않고, 공통 storage 층이     */
        /*  깨끗한 mount session 으로 다시 시작할 기회를 준다.                 */
        /* ------------------------------------------------------------------ */
        APP_SD_RequestRecovery();
    }

    if (toast_text != 0)
    {
        Motor_State_ShowToast(toast_text, 1500u);
    }
}

static uint8_t motor_record_copy_text(char *dst, size_t dst_size, const char *src)
{
    if ((dst == 0) || (dst_size == 0u) || (src == 0))
    {
        return 0u;
    }

    (void)strncpy(dst, src, dst_size - 1u);
    dst[dst_size - 1u] = '\0';
    return 1u;
}

static uint8_t motor_record_dir_exists(const char *dir_path, FRESULT *out_fr)
{
    FILINFO info;
    FRESULT fr;

    if (dir_path == 0)
    {
        return 0u;
    }

    memset(&info, 0, sizeof(info));
    fr = f_stat(dir_path, &info);
    if (out_fr != 0)
    {
        *out_fr = fr;
    }
    if (fr != FR_OK)
    {
        motor_record_note_fs_failure(APP_SD_FS_STAGE_MOTOR_REC_DIR_STAT, fr);
        return 0u;
    }

    return ((info.fattrib & AM_DIR) != 0u) ? 1u : 0u;
}

static uint8_t motor_record_try_prepare_dir_candidate(const char *dir_path, FRESULT *out_fr)
{
    FRESULT fr;

    if (dir_path == 0)
    {
        return 0u;
    }

    if (motor_record_dir_exists(dir_path, &fr) != 0u)
    {
        return 1u;
    }

    /* ---------------------------------------------------------------------- */
    /*  RECORD directory auto-create policy                                   */
    /*                                                                        */
    /*  The recorder should own its top-level storage folder. If RECORD does  */
    /*  not exist yet, starting a session should create it instead of asking  */
    /*  the user to prepare the card manually.                                */
    /*                                                                        */
    /*  We re-check the path after f_mkdir() because FR_EXIST alone is not    */
    /*  enough to prove the path is a directory. A stale file named RECORD    */
    /*  must still be treated as a directory setup error.                     */
    /* ---------------------------------------------------------------------- */
    fr = f_mkdir(dir_path);
    if (out_fr != 0)
    {
        *out_fr = fr;
    }
    if ((fr != FR_OK) && (fr != FR_EXIST))
    {
        motor_record_note_fs_failure(APP_SD_FS_STAGE_MOTOR_REC_DIR_MKDIR, fr);
        return 0u;
    }

    return motor_record_dir_exists(dir_path, out_fr);
}

static uint8_t motor_record_resolve_root_dir(char *out_dir, size_t out_dir_size, FRESULT *out_fr)
{
    static const char *const s_dir_candidates[] =
    {
        MOTOR_RECORD_ROOT_DIR_PRIMARY,
        MOTOR_RECORD_ROOT_DIR_DRIVE_REL,
        MOTOR_RECORD_ROOT_DIR_FALLBACK
    };
    uint32_t i;

    if ((out_dir == 0) || (out_dir_size == 0u))
    {
        return 0u;
    }

    out_dir[0] = '\0';

    for (i = 0u; i < (sizeof(s_dir_candidates) / sizeof(s_dir_candidates[0])); ++i)
    {
        if (motor_record_try_prepare_dir_candidate(s_dir_candidates[i], out_fr) != 0u)
        {
            return motor_record_copy_text(out_dir, out_dir_size, s_dir_candidates[i]);
        }
    }

    return 0u;
}

static uint8_t motor_record_build_queue_item(motor_record_queue_item_t *slot,
                                             uint8_t type,
                                             uint32_t tick_ms,
                                             const void *payload,
                                             uint16_t payload_size)
{
    motor_log_record_header_t hdr;

    if ((slot == 0) || (payload_size > MOTOR_RECORD_QUEUE_PAYLOAD_MAX_BYTES))
    {
        return 0u;
    }

    memset(slot, 0, sizeof(*slot));

    hdr.type = type;
    hdr.reserved0 = 0u;
    hdr.payload_size = payload_size;
    hdr.tick_ms = tick_ms;

    memcpy(slot->bytes, &hdr, sizeof(hdr));
    if ((payload != 0) && (payload_size != 0u))
    {
        memcpy(&slot->bytes[sizeof(hdr)], payload, payload_size);
    }

    slot->total_size = (uint16_t)(sizeof(hdr) + payload_size);
    return 1u;
}

static void motor_record_trip_note_max(motor_trip_metrics_t *trip, uint16_t speed_kmh_x10)
{
    if ((trip != 0) && (speed_kmh_x10 > trip->max_speed_kmh_x10))
    {
        trip->max_speed_kmh_x10 = speed_kmh_x10;
    }
}

static void motor_record_trip_add_distance(motor_trip_metrics_t *trip, uint32_t delta_m)
{
    if ((trip != 0) && (delta_m != 0u))
    {
        trip->distance_m += delta_m;
    }
}

static void motor_record_note_live_speed_metrics(motor_state_t *state)
{
    if (state == 0)
    {
        return;
    }

    if (state->nav.speed_kmh_x10 > state->session.max_speed_kmh_x10)
    {
        state->session.max_speed_kmh_x10 = state->nav.speed_kmh_x10;
    }

    motor_record_trip_note_max(&state->session.trip_a_stats, state->nav.speed_kmh_x10);
    motor_record_trip_note_max(&state->session.trip_b_stats, state->nav.speed_kmh_x10);
    motor_record_trip_note_max(&state->session.trip_refuel, state->nav.speed_kmh_x10);
    motor_record_trip_note_max(&state->session.trip_today, state->nav.speed_kmh_x10);
    motor_record_trip_note_max(&state->session.trip_ignition, state->nav.speed_kmh_x10);
}

static void motor_record_note_record_speed_metrics(motor_state_t *state)
{
    if (state == 0)
    {
        return;
    }

    if (state->nav.speed_kmh_x10 > state->record_session.max_speed_kmh_x10)
    {
        state->record_session.max_speed_kmh_x10 = state->nav.speed_kmh_x10;
    }
}

static void motor_record_integrate_live_and_record_distance(motor_state_t *state)
{
    uint32_t dt_ms;
    uint64_t base_delta_mm;
    uint64_t live_delta_mm;
    uint32_t live_delta_m;

    if (state == 0)
    {
        return;
    }

    if (s_last_distance_integrate_ms == 0u)
    {
        s_last_distance_integrate_ms = state->now_ms;
        return;
    }

    if (state->now_ms <= s_last_distance_integrate_ms)
    {
        return;
    }

    dt_ms = state->now_ms - s_last_distance_integrate_ms;
    if (dt_ms > 1000u)
    {
        dt_ms = 1000u;
    }

    base_delta_mm = ((uint64_t)state->nav.speed_mmps * dt_ms) / 1000u;
    live_delta_mm = base_delta_mm + s_distance_remainder_mm;
    live_delta_m = (uint32_t)(live_delta_mm / 1000u);
    s_distance_remainder_mm = (uint32_t)(live_delta_mm % 1000u);

    state->session.distance_m += live_delta_m;
    state->session.trip_a_m += live_delta_m;
    state->session.trip_b_m += live_delta_m;
    motor_record_trip_add_distance(&state->session.trip_a_stats, live_delta_m);
    motor_record_trip_add_distance(&state->session.trip_b_stats, live_delta_m);
    motor_record_trip_add_distance(&state->session.trip_refuel, live_delta_m);
    motor_record_trip_add_distance(&state->session.trip_today, live_delta_m);
    motor_record_trip_add_distance(&state->session.trip_ignition, live_delta_m);

    if ((motor_record_state_t)state->record.state == MOTOR_RECORD_STATE_RECORDING)
    {
        uint64_t record_delta_mm;
        uint32_t record_delta_m;

        record_delta_mm = base_delta_mm + s_record_distance_remainder_mm;
        record_delta_m = (uint32_t)(record_delta_mm / 1000u);
        s_record_distance_remainder_mm = (uint32_t)(record_delta_mm % 1000u);
        state->record_session.distance_m += record_delta_m;
    }

    s_last_distance_integrate_ms = state->now_ms;
}

static void motor_record_queue_reset(void)
{
    s_record_queue_head = 0u;
    s_record_queue_tail = 0u;
    s_record_queue_count = 0u;
}

static void motor_record_preroll_reset(void)
{
    s_preroll_head = 0u;
    s_preroll_tail = 0u;
    s_preroll_count = 0u;
}

static uint8_t motor_record_queue_advance(uint8_t index)
{
    return (uint8_t)((index + 1u) % MOTOR_RECORD_QUEUE_DEPTH);
}

static uint16_t motor_record_preroll_advance(uint16_t index)
{
    return (uint16_t)((index + 1u) % MOTOR_RECORD_PREROLL_DEPTH);
}

static void motor_record_queue_pop_front_n(uint8_t count)
{
    while ((count != 0u) && (s_record_queue_count != 0u))
    {
        s_record_queue_head = motor_record_queue_advance(s_record_queue_head);
        s_record_queue_count--;
        count--;
    }
}

static uint8_t motor_record_enqueue_payload(uint8_t type,
                                            uint32_t tick_ms,
                                            const void *payload,
                                            uint16_t payload_size)
{
    motor_record_queue_item_t *slot;

    if (s_record_queue_count >= MOTOR_RECORD_QUEUE_DEPTH)
    {
        return 0u;
    }

    slot = &s_record_queue[s_record_queue_tail];
    if (motor_record_build_queue_item(slot, type, tick_ms, payload, payload_size) == 0u)
    {
        return 0u;
    }

    s_record_queue_tail = motor_record_queue_advance(s_record_queue_tail);
    s_record_queue_count++;
    return 1u;
}

static void motor_record_preroll_store_payload(uint8_t type,
                                               uint32_t tick_ms,
                                               const void *payload,
                                               uint16_t payload_size)
{
    motor_record_queue_item_t *slot;

    slot = &s_preroll_queue[s_preroll_tail];
    if (motor_record_build_queue_item(slot, type, tick_ms, payload, payload_size) == 0u)
    {
        return;
    }

    if (s_preroll_count >= MOTOR_RECORD_PREROLL_DEPTH)
    {
        s_preroll_head = motor_record_preroll_advance(s_preroll_head);
    }
    else
    {
        s_preroll_count++;
    }

    s_preroll_tail = motor_record_preroll_advance(s_preroll_tail);
}

static uint8_t motor_record_flush_queue(motor_state_t *state,
                                        uint8_t max_items,
                                        uint8_t sync_after_flush)
{
    uint8_t flushed_any;
    FRESULT fr;

    if ((state == 0) || (s_log_file_open == 0u) || (s_record_queue_count == 0u))
    {
        return 0u;
    }

    if (APP_SD_IsFsAccessAllowedNow() == false)
    {
        return 0u;
    }

    flushed_any = 0u;

    while ((max_items != 0u) && (s_record_queue_count != 0u))
    {
        UINT written;
        uint8_t burst_items;
        uint16_t burst_bytes;
        uint8_t i;

        burst_items = 0u;
        burst_bytes = 0u;

        /* ------------------------------------------------------------------ */
        /* queue head 遺??理쒕? max_items 媛쒕? burst buffer ???곗냽?쇰줈 紐⑥??? */
        /* ------------------------------------------------------------------ */
        for (i = 0u; (i < s_record_queue_count) && (i < max_items); i++)
        {
            const motor_record_queue_item_t *slot;
            uint8_t index;

            index = (uint8_t)((s_record_queue_head + i) % MOTOR_RECORD_QUEUE_DEPTH);
            slot = &s_record_queue[index];

            if ((slot->total_size == 0u) || (slot->total_size > MOTOR_RECORD_QUEUE_ITEM_MAX_BYTES))
            {
                state->record.drop_count++;
                motor_record_queue_pop_front_n(1u);
                break;
            }

            if ((burst_items != 0u) &&
                ((uint16_t)(burst_bytes + slot->total_size) > MOTOR_RECORD_FLUSH_BURST_MAX_BYTES))
            {
                break;
            }

            memcpy(&s_record_flush_burst[burst_bytes], slot->bytes, slot->total_size);
            burst_bytes = (uint16_t)(burst_bytes + slot->total_size);
            burst_items++;
        }

        if ((burst_items == 0u) || (burst_bytes == 0u))
        {
            break;
        }

        fr = f_write(&s_log_file, s_record_flush_burst, burst_bytes, &written);
        if ((fr != FR_OK) || (written != burst_bytes))
        {
            /* -------------------------------------------------------------- */
            /*  Runtime write failure is treated as a fatal recorder error.   */
            /*                                                                */
            /*  If we merely drop the burst and keep the FIL handle alive,    */
            /*  rapid REC stop/start or later hot-remove can pile new session */
            /*  logic on top of a half-broken FatFs state, which is exactly   */
            /*  the path that was producing WRITE ERR -> NAME ERR chains and  */
            /*  eventual hard crashes.                                        */
            /*                                                                */
            /*  We now abort the whole session immediately and ask APP_SD to   */
            /*  remount the shared volume before the next recording attempt.   */
            /* -------------------------------------------------------------- */
            motor_record_note_fs_failure(APP_SD_FS_STAGE_MOTOR_REC_WRITE, fr);
            state->record.drop_count += burst_items;
            motor_record_abort_io_session(state, "REC WRITE ERR", 1u);
            return 0u;
        }

        motor_record_queue_pop_front_n(burst_items);
        state->record.bytes_written += burst_bytes;
        state->record.last_write_ms = state->now_ms;
        flushed_any = 1u;

        if (max_items > burst_items)
        {
            max_items = (uint8_t)(max_items - burst_items);
        }
        else
        {
            max_items = 0u;
        }
    }

    if ((flushed_any != 0u) && (sync_after_flush != 0u) && (s_log_file_open != 0u))
    {
        fr = f_sync(&s_log_file);
        if (fr != FR_OK)
        {
            motor_record_note_fs_failure(APP_SD_FS_STAGE_MOTOR_REC_SYNC, fr);
            motor_record_abort_io_session(state, "REC SYNC ERR", 1u);
            return 0u;
        }
        state->record.last_flush_ms = state->now_ms;
    }

    return flushed_any;
}

static void motor_record_maybe_flush(motor_state_t *state)
{
    uint8_t queue_heavy;
    uint8_t periodic_flush_due;
    uint8_t sync_due;
    uint8_t flush_budget;

    if ((state == 0) || (s_log_file_open == 0u) || (s_record_queue_count == 0u))
    {
        return;
    }

    queue_heavy = (s_record_queue_count >= MOTOR_RECORD_FLUSH_WATERMARK_ITEMS) ? 1u : 0u;
    periodic_flush_due = ((uint32_t)(state->now_ms - state->record.last_write_ms) >= MOTOR_RECORD_FLUSH_PERIOD_MS) ? 1u : 0u;
    sync_due = ((uint32_t)(state->now_ms - state->record.last_flush_ms) >= MOTOR_RECORD_SYNC_PERIOD_MS) ? 1u : 0u;

    if ((queue_heavy == 0u) && (periodic_flush_due == 0u) && (sync_due == 0u))
    {
        return;
    }

    flush_budget = (sync_due != 0u) ? MOTOR_RECORD_QUEUE_DEPTH : MOTOR_RECORD_FLUSH_BATCH_ITEMS;
    (void)motor_record_flush_queue(state, flush_budget, sync_due);
}

static uint16_t motor_record_make_fat_date(uint16_t year, uint8_t month, uint8_t day)
{
    if (year < 1980u)
    {
        return 0u;
    }

    return (uint16_t)(((year - 1980u) << 9) |
                      (((uint16_t)month & 0x0Fu) << 5) |
                      ((uint16_t)day & 0x1Fu));
}

static uint16_t motor_record_make_fat_time(uint8_t hour, uint8_t minute, uint8_t second)
{
    return (uint16_t)((((uint16_t)hour & 0x1Fu) << 11) |
                      (((uint16_t)minute & 0x3Fu) << 5) |
                      (((uint16_t)(second / 2u)) & 0x1Fu));
}

static void motor_record_apply_stop_timestamp(const motor_state_t *state)
{
    FILINFO fno;

    if ((state == 0) || (state->record.file_name[0] == '\0'))
    {
        return;
    }

    if (state->snapshot.clock.local.year < 1980u)
    {
        return;
    }

    memset(&fno, 0, sizeof(fno));
    fno.fdate = motor_record_make_fat_date(state->snapshot.clock.local.year,
                                           state->snapshot.clock.local.month,
                                           state->snapshot.clock.local.day);
    fno.ftime = motor_record_make_fat_time(state->snapshot.clock.local.hour,
                                           state->snapshot.clock.local.min,
                                           state->snapshot.clock.local.sec);
#if _USE_CHMOD
    (void)f_utime(state->record.file_name, &fno);
#endif
}

static uint32_t motor_record_parse_file_index(const char *name)
{
    uint32_t value;
    uint8_t i;
    char ch;

    if (name == 0)
    {
        return 0u;
    }

    for (i = 0u; i < 7u; ++i)
    {
        ch = name[i];
        if ((ch < '0') || (ch > '9'))
        {
            return 0u;
        }
    }

    if ((name[7] != '.') ||
        !(((name[8] == 'M') || (name[8] == 'm')) &&
          ((name[9] == 'L') || (name[9] == 'l')) &&
          ((name[10] == 'G') || (name[10] == 'g'))) ||
        (name[11] != '\0'))
    {
        return 0u;
    }

    value = 0u;
    for (i = 0u; i < 7u; ++i)
    {
        value = (value * 10u) + (uint32_t)(name[i] - '0');
    }

    if ((value == 0u) || (value > MOTOR_RECORD_MAX_FILE_INDEX))
    {
        return 0u;
    }

    return value;
}

static uint32_t motor_record_find_next_file_index(const char *root_dir, FRESULT *out_fr)
{
    DIR dir;
    FILINFO fno;
    FRESULT fr;
    uint32_t max_index;

    max_index = 0u;
    memset(&dir, 0, sizeof(dir));
    memset(&fno, 0, sizeof(fno));

    if (root_dir == 0)
    {
        if (out_fr != 0)
        {
            *out_fr = FR_INVALID_OBJECT;
        }
        return 0u;
    }

    fr = f_opendir(&dir, root_dir);
    if (out_fr != 0)
    {
        *out_fr = fr;
    }
    if (fr != FR_OK)
    {
        motor_record_note_fs_failure(APP_SD_FS_STAGE_MOTOR_REC_NAME_OPENDIR, fr);
        return 0u;
    }

    for (;;)
    {
        uint32_t index;

        fr = f_readdir(&dir, &fno);
        if ((fr != FR_OK) || (fno.fname[0] == '\0'))
        {
            if (fr != FR_OK)
            {
                motor_record_note_fs_failure(APP_SD_FS_STAGE_MOTOR_REC_NAME_READDIR, fr);
            }
            break;
        }

        if ((fno.fattrib & AM_DIR) != 0u)
        {
            continue;
        }

        index = motor_record_parse_file_index(fno.fname);
        if (index > max_index)
        {
            max_index = index;
        }
    }

    (void)f_closedir(&dir);

    if (out_fr != 0)
    {
        *out_fr = fr;
    }

    if (max_index >= MOTOR_RECORD_MAX_FILE_INDEX)
    {
        return 0u;
    }

    return (max_index + 1u);
}

static uint8_t motor_record_flush_preroll_snapshot(motor_state_t *state)
{
    uint16_t remaining;
    uint16_t read_index;
    uint8_t flushed_any;
    FRESULT fr;

    if ((state == 0) || (s_log_file_open == 0u) || (s_preroll_count == 0u))
    {
        return 0u;
    }

    remaining = s_preroll_count;
    read_index = s_preroll_head;
    flushed_any = 0u;

    while (remaining != 0u)
    {
        UINT written;
        uint16_t burst_bytes;
        uint8_t burst_items;

        burst_bytes = 0u;
        burst_items = 0u;

        while (remaining != 0u)
        {
            const motor_record_queue_item_t *slot;

            slot = &s_preroll_queue[read_index];
            if ((slot->total_size == 0u) || (slot->total_size > MOTOR_RECORD_QUEUE_ITEM_MAX_BYTES))
            {
                read_index = motor_record_preroll_advance(read_index);
                remaining--;
                continue;
            }

            if ((burst_items != 0u) &&
                ((uint16_t)(burst_bytes + slot->total_size) > MOTOR_RECORD_FLUSH_BURST_MAX_BYTES))
            {
                break;
            }

            memcpy(&s_record_flush_burst[burst_bytes], slot->bytes, slot->total_size);
            burst_bytes = (uint16_t)(burst_bytes + slot->total_size);
            burst_items++;
            read_index = motor_record_preroll_advance(read_index);
            remaining--;
        }

        if ((burst_items == 0u) || (burst_bytes == 0u))
        {
            break;
        }

        fr = f_write(&s_log_file, s_record_flush_burst, burst_bytes, &written);
        if ((fr != FR_OK) || (written != burst_bytes))
        {
            motor_record_note_fs_failure(APP_SD_FS_STAGE_MOTOR_REC_WRITE, fr);
            state->record.drop_count += burst_items;
            motor_record_abort_io_session(state, "REC WRITE ERR", 1u);
            return 0u;
        }

        state->record.bytes_written += burst_bytes;
        state->record.last_write_ms = state->now_ms;
        flushed_any = 1u;
    }

    if (flushed_any != 0u)
    {
        fr = f_sync(&s_log_file);
        if (fr != FR_OK)
        {
            motor_record_note_fs_failure(APP_SD_FS_STAGE_MOTOR_REC_SYNC, fr);
            motor_record_abort_io_session(state, "REC SYNC ERR", 1u);
            return 0u;
        }
        state->record.last_flush_ms = state->now_ms;
    }

    return flushed_any;
}

static void motor_record_capture_nav_sample(motor_state_t *state, uint8_t to_record_queue)
{
    motor_log_nav_payload_t nav;

    if ((state == 0) || ((uint32_t)(state->now_ms - s_last_nav_write_ms) < 100u))
    {
        return;
    }

    memset(&nav, 0, sizeof(nav));
    nav.lat_e7 = state->nav.lat_e7;
    nav.lon_e7 = state->nav.lon_e7;
    nav.altitude_cm = state->nav.altitude_cm;
    nav.speed_mmps = state->nav.speed_mmps;
    nav.heading_deg_x10 = state->nav.heading_deg_x10;
    nav.hacc_mm = state->nav.hacc_mm;
    nav.vacc_mm = state->nav.vacc_mm;
    nav.fix_type = state->nav.fix_type;
    nav.sats_used = state->nav.sats_used;

    motor_record_preroll_store_payload(MOTOR_LOG_REC_NAV, state->now_ms, &nav, sizeof(nav));
    if ((to_record_queue != 0u) &&
        (motor_record_enqueue_payload(MOTOR_LOG_REC_NAV, state->now_ms, &nav, sizeof(nav)) == 0u))
    {
        state->record.drop_count++;
    }

    s_last_nav_write_ms = state->now_ms;
}

static void motor_record_capture_dyn_sample(motor_state_t *state, uint8_t to_record_queue)
{
    motor_log_dyn_payload_t dyn;

    if ((state == 0) || ((uint32_t)(state->now_ms - s_last_dyn_write_ms) < 50u))
    {
        return;
    }

    if (state->dyn.zero_valid != false)
    {
        memset(&dyn, 0, sizeof(dyn));
        dyn.bank_deg_x10 = state->dyn.est_bank_deg_x10;
        dyn.grade_deg_x10 = state->dyn.est_grade_deg_x10;
        dyn.bank_rate_dps_x10 = state->dyn.est_bank_rate_dps_x10;
        dyn.grade_rate_dps_x10 = state->dyn.est_grade_rate_dps_x10;
        dyn.lat_accel_mg = state->dyn.est_lat_accel_mg;
        dyn.lon_accel_mg = state->dyn.est_lon_accel_mg;
        dyn.confidence_permille = state->dyn.confidence_permille;
        dyn.speed_source = state->dyn.speed_source;
        dyn.heading_source = state->dyn.heading_source;

        motor_record_preroll_store_payload(MOTOR_LOG_REC_DYN, state->now_ms, &dyn, sizeof(dyn));
        if ((to_record_queue != 0u) &&
            (motor_record_enqueue_payload(MOTOR_LOG_REC_DYN, state->now_ms, &dyn, sizeof(dyn)) == 0u))
        {
            state->record.drop_count++;
        }
    }

    s_last_dyn_write_ms = state->now_ms;
}

static void motor_record_capture_obd_sample(motor_state_t *state, uint8_t to_record_queue)
{
    motor_log_obd_payload_t obd;

    if ((state == 0) ||
        (state->vehicle.connected == false) ||
        ((uint32_t)(state->now_ms - s_last_obd_write_ms) < 200u))
    {
        return;
    }

    memset(&obd, 0, sizeof(obd));
    obd.rpm = state->vehicle.rpm;
    obd.coolant_temp_c_x10 = state->vehicle.coolant_temp_c_x10;
    obd.gear = state->vehicle.gear;
    obd.battery_mv = state->vehicle.battery_mv;
    obd.throttle_percent = state->vehicle.throttle_percent;
    obd.dtc_count = state->vehicle.dtc_count;

    motor_record_preroll_store_payload(MOTOR_LOG_REC_OBD, state->now_ms, &obd, sizeof(obd));
    if ((to_record_queue != 0u) &&
        (motor_record_enqueue_payload(MOTOR_LOG_REC_OBD, state->now_ms, &obd, sizeof(obd)) == 0u))
    {
        state->record.drop_count++;
    }

    s_last_obd_write_ms = state->now_ms;
}

static void motor_record_close_with_summary(motor_state_t *state)
{
    motor_log_sum_payload_t sum;
    FRESULT fr;

    if ((state == 0) || (s_log_file_open == 0u))
    {
        return;
    }

    memset(&sum, 0, sizeof(sum));
    sum.ride_seconds = state->record_session.ride_seconds;
    sum.moving_seconds = state->record_session.moving_seconds;
    sum.distance_m = state->record_session.distance_m;
    sum.max_speed_kmh_x10 = state->record_session.max_speed_kmh_x10;
    sum.max_left_bank_deg_x10 = state->dyn.max_left_bank_deg_x10;
    sum.max_right_bank_deg_x10 = state->dyn.max_right_bank_deg_x10;
    sum.max_left_lat_mg = state->dyn.max_left_lat_mg;
    sum.max_right_lat_mg = state->dyn.max_right_lat_mg;
    sum.max_accel_mg = state->dyn.max_accel_mg;
    sum.max_brake_mg = state->dyn.max_brake_mg;
    sum.marker_count = state->record_session.marker_count;
    sum.drop_count = (uint16_t)state->record.drop_count;

    if (motor_record_enqueue_payload(MOTOR_LOG_REC_SUM, state->now_ms, &sum, sizeof(sum)) == 0u)
    {
        state->record.drop_count++;
    }

    while (s_record_queue_count != 0u)
    {
        if (motor_record_flush_queue(state, MOTOR_RECORD_QUEUE_DEPTH, 0u) == 0u)
        {
            if (s_record_queue_count != 0u)
            {
                motor_record_abort_io_session(state, "REC CLOSE ERR", 1u);
                return;
            }
            break;
        }
    }

    fr = f_sync(&s_log_file);
    if (fr != FR_OK)
    {
        motor_record_note_fs_failure(APP_SD_FS_STAGE_MOTOR_REC_SYNC, fr);
        motor_record_abort_io_session(state, "REC SYNC ERR", 1u);
        return;
    }

    fr = f_close(&s_log_file);
    if (fr != FR_OK)
    {
        motor_record_note_fs_failure(APP_SD_FS_STAGE_MOTOR_REC_CLOSE, fr);
        motor_record_abort_io_session(state, "REC CLOSE ERR", 1u);
        return;
    }

    motor_record_apply_stop_timestamp(state);
    motor_record_reset_open_runtime(state, MOTOR_RECORD_STATE_IDLE, 1u);
}

static void motor_record_start(motor_state_t *state)
{
    FRESULT fr;
    FRESULT dir_fr;
    FRESULT name_fr;
    char path[64];
    char root_dir[MOTOR_RECORD_ROOT_DIR_MAX_LEN];
    uint32_t file_index;
    uint32_t bytes_before_preroll;
    motor_log_header_payload_t hdr;

    if ((state == 0) || (s_log_file_open != 0u))
    {
        return;
    }

    if (APP_SD_IsFsAccessAllowedNow() == false)
    {
        Motor_State_ShowToast("NO SD / REC DENIED", 1400u);
        return;
    }

    dir_fr = FR_OK;
    if (motor_record_resolve_root_dir(root_dir, sizeof(root_dir), &dir_fr) == 0u)
    {
        state->record.state = (uint8_t)MOTOR_RECORD_STATE_ERROR;
        state->record.drop_count++;
        if ((dir_fr != FR_OK) && (dir_fr != FR_EXIST))
        {
            APP_SD_RequestRecovery();
        }
        Motor_State_ShowToast("REC DIR ERR", 1400u);
        return;
    }

    name_fr = FR_OK;
    file_index = motor_record_find_next_file_index(root_dir, &name_fr);
    if (file_index == 0u)
    {
        state->record.state = (uint8_t)MOTOR_RECORD_STATE_ERROR;
        state->record.drop_count++;
        if (name_fr != FR_OK)
        {
            APP_SD_RequestRecovery();
        }
        Motor_State_ShowToast("REC NAME ERR", 1400u);
        return;
    }

    s_session_counter = file_index;
    (void)snprintf(path, sizeof(path), "%s/%07lu.MLG", root_dir, (unsigned long)file_index);

    fr = f_open(&s_log_file, path, FA_WRITE | FA_CREATE_ALWAYS);
    if (fr != FR_OK)
    {
        motor_record_note_fs_failure(APP_SD_FS_STAGE_MOTOR_REC_OPEN, fr);
        motor_record_abort_io_session(state, "REC ERR", 1u);
        state->record.drop_count++;
        return;
    }

    s_log_file_open = 1u;
    motor_record_queue_reset();

    /* ---------------------------------------------------------------------- */
    /* ??session 硫뷀??곗씠?곕뒗 header enqueue / flush ?꾩뿉 癒쇱? 珥덇린?뷀븳??     */
    /*                                                                          */
    /* ?댁쑀                                                                     */
    /* - 利됱떆 flush ?섎뒗 file header bytes ??珥?bytes_written ???ы븿?쒗궓??    */
    /* - header enqueue ?ㅽ뙣??flush ?ㅽ뙣媛 ?앷린硫?drop_count 利앷?媛 ?좎??쒕떎.   */
    /* ---------------------------------------------------------------------- */
    state->record.bytes_written = 0u;
    state->record.drop_count = 0u;
    state->record.last_open_ms = state->now_ms;
    state->record.last_write_ms = state->now_ms;
    state->record.last_flush_ms = state->now_ms;

    memset(&hdr, 0, sizeof(hdr));
    hdr.magic = MOTOR_LOG_MAGIC;
    hdr.version = MOTOR_LOG_VERSION;
    hdr.session_id = ++state->record_session.session_id;
    hdr.start_tick_ms = state->now_ms;
    hdr.receiver_profile = state->settings.gps.receiver_profile;
    hdr.units_preset = state->settings.units.preset;
    hdr.yaw_trim_deg_x10 = state->settings.dynamics.mount_yaw_trim_deg_x10;
    hdr.forward_axis = state->settings.dynamics.mount_forward_axis;
    hdr.left_axis = state->settings.dynamics.mount_left_axis;

    if (motor_record_enqueue_payload(MOTOR_LOG_REC_HDR, state->now_ms, &hdr, sizeof(hdr)) == 0u)
    {
        state->record.drop_count++;
    }

    /* ---------------------------------------------------------------------- */
    /* ?쒖옉 吏곹썑 file header ????踰?利됱떆 ?대낫?몃떎.                            */
    /*                                                                          */
    /* ?댁쑀                                                                     */
    /* - session file ??留??앹꽦??吏곹썑?먮뒗 理쒖냼?쒖쓽 header 媛 ?붿뒪?ъ뿉 ?덉뼱??  */
    /*   異뷀썑 遺꾩꽍湲곕굹 蹂듦뎄 ?꾧뎄媛 ?뚯씪 ?섎?瑜??앸퀎?섍린 ?쎈떎.                    */
    /* - ?댄썑??怨좎＜湲?NAV / DYN / OBD ??queue + batch flush 濡??꾪솕?쒕떎.      */
    /* ---------------------------------------------------------------------- */
    if ((motor_record_flush_queue(state, MOTOR_RECORD_QUEUE_DEPTH, 1u) == 0u) ||
        (state->record.bytes_written == 0u))
    {
        state->record.drop_count++;
        motor_record_abort_io_session(state, "REC WRITE ERR", 1u);
        return;
    }

    bytes_before_preroll = state->record.bytes_written;
    if ((s_preroll_count != 0u) &&
        (motor_record_flush_preroll_snapshot(state) == 0u) &&
        (state->record.bytes_written == bytes_before_preroll))
    {
        state->record.drop_count++;
        motor_record_abort_io_session(state, "REC WRITE ERR", 1u);
        return;
    }

    (void)strncpy(state->record.file_name, path, sizeof(state->record.file_name) - 1u);
    state->record.file_name[sizeof(state->record.file_name) - 1u] = '\0';
    state->record.state = (uint8_t)MOTOR_RECORD_STATE_RECORDING;
    state->record.open_ok = true;
    state->record.graceful_close_done = false;
    state->record.record_sequence++;
    memset(&state->record_session, 0, sizeof(state->record_session));
    state->record_session.session_id = hdr.session_id;
    state->record_session.active = true;
    state->record_session.start_ms = state->now_ms;
    state->record_session.stop_ms = 0u;
    s_last_nav_write_ms = 0u;
    s_last_dyn_write_ms = 0u;
    s_last_obd_write_ms = 0u;
    s_record_distance_remainder_mm = 0u;
    Motor_State_ShowToast("REC START", 1200u);
}

void Motor_Record_Init(void)
{
    memset(&s_log_file, 0, sizeof(s_log_file));
    s_log_file_open = 0u;
    s_last_nav_write_ms = 0u;
    s_last_dyn_write_ms = 0u;
    s_last_obd_write_ms = 0u;
    s_last_distance_integrate_ms = 0u;
    s_distance_remainder_mm = 0u;
    s_record_distance_remainder_mm = 0u;
    s_session_counter = 0u;
    motor_record_queue_reset();
    motor_record_preroll_reset();
    memset(s_record_flush_burst, 0, sizeof(s_record_flush_burst));
}

void Motor_Record_RequestStart(void)
{
    motor_state_t *state;

    state = Motor_State_GetMutable();
    if (state != 0)
    {
        state->record.start_requested = true;
    }
}

void Motor_Record_RequestStop(void)
{
    motor_state_t *state;

    state = Motor_State_GetMutable();
    if (state != 0)
    {
        state->record.stop_requested = true;
    }
}

void Motor_Record_RequestMarker(void)
{
    motor_state_t *state;

    state = Motor_State_GetMutable();
    if (state != 0)
    {
        state->record.marker_requested = true;
    }
}

void Motor_Record_OnSdWillUnmount(void)
{
    motor_state_t *state;
    uint8_t had_active_file;
    uint8_t dropped_items;

    state = Motor_State_GetMutable();
    had_active_file = (s_log_file_open != 0u) ? 1u : 0u;
    dropped_items = s_record_queue_count;

    /* ---------------------------------------------------------------------- */
    /*  SD teardown abort path                                                */
    /*                                                                        */
    /*  APP_SD invokes this hook immediately before the shared volume is      */
    /*  unmounted/deinitialized. At that moment the physical card may already */
    /*  be gone, so the recorder must not run its normal graceful-close path  */
    /*  that assumes f_sync()/f_close() can still complete successfully.      */
    /*                                                                        */
    /*  We therefore drop the in-flight queue, forget the FIL handle, and     */
    /*  mark the session as aborted so a later reinsertion starts fresh with  */
    /*  no stale file context left behind.                                    */
    /* ---------------------------------------------------------------------- */
    motor_record_queue_reset();
    motor_record_reset_open_runtime(state, MOTOR_RECORD_STATE_ERROR, 0u);

    if (state != 0)
    {
        state->record.drop_count += dropped_items;
        state->record_session.active = false;
        state->record_session.stop_ms = state->now_ms;
    }

    if (had_active_file != 0u)
    {
        Motor_State_ShowToast("REC SD LOST", 1500u);
    }
}

void Motor_Record_Task(uint32_t now_ms)
{
    motor_state_t *state;

    (void)now_ms;
    state = Motor_State_GetMutable();
    if (state == 0)
    {
        return;
    }

    if ((state->settings.recording.auto_start_enabled != 0u) &&
        ((motor_record_state_t)state->record.state == MOTOR_RECORD_STATE_IDLE) &&
        (APP_SD_IsFsAccessAllowedNow() != false) &&
        (state->nav.speed_kmh_x10 >= state->settings.recording.auto_start_speed_kmh_x10))
    {
        state->record.start_requested = true;
    }

    if (((motor_record_state_t)state->record.state == MOTOR_RECORD_STATE_RECORDING) &&
        (state->settings.recording.auto_start_enabled != 0u) &&
        (state->nav.moving == false) &&
        (state->settings.recording.auto_stop_idle_seconds != 0u) &&
        (state->record_session.stopped_seconds >= state->settings.recording.auto_stop_idle_seconds))
    {
        state->record.stop_requested = true;
    }

    if ((state->record.start_requested != false) &&
        ((motor_record_state_t)state->record.state != MOTOR_RECORD_STATE_RECORDING))
    {
        state->record.start_requested = false;
        motor_record_start(state);
    }

    if ((state->record.stop_requested != false) && (s_log_file_open != 0u))
    {
        state->record.stop_requested = false;
        state->record.state = (uint8_t)MOTOR_RECORD_STATE_CLOSING;
        state->record_session.stop_ms = state->now_ms;
        state->record_session.active = false;
        motor_record_close_with_summary(state);
        Motor_State_ShowToast("REC STOP", 1200u);
        return;
    }

    motor_record_note_live_speed_metrics(state);
    motor_record_integrate_live_and_record_distance(state);
    motor_record_capture_nav_sample(state,
                                    ((motor_record_state_t)state->record.state == MOTOR_RECORD_STATE_RECORDING) ? 1u : 0u);
    motor_record_capture_dyn_sample(state,
                                    ((motor_record_state_t)state->record.state == MOTOR_RECORD_STATE_RECORDING) ? 1u : 0u);
    motor_record_capture_obd_sample(state,
                                    ((motor_record_state_t)state->record.state == MOTOR_RECORD_STATE_RECORDING) ? 1u : 0u);

    if ((motor_record_state_t)state->record.state == MOTOR_RECORD_STATE_PAUSED)
    {
        motor_record_maybe_flush(state);
        return;
    }

    if ((motor_record_state_t)state->record.state != MOTOR_RECORD_STATE_RECORDING)
    {
        return;
    }

    motor_record_note_record_speed_metrics(state);

    if (state->record.marker_requested != false)
    {
        motor_log_evt_payload_t evt;
        memset(&evt, 0, sizeof(evt));
        evt.event_code = 1u;
        evt.event_value = 0u;
        evt.aux_u32 = state->record_session.marker_count;
        if (motor_record_enqueue_payload(MOTOR_LOG_REC_EVT, state->now_ms, &evt, sizeof(evt)) != 0u)
        {
            state->record_session.marker_count++;
            Motor_State_ShowToast("MARK", 900u);
        }
        else
        {
            state->record.drop_count++;
        }
        state->record.marker_requested = false;
    }

    motor_record_maybe_flush(state);
    return;
#if 0
    if ((state->settings.recording.auto_start_enabled != 0u) &&
        ((motor_record_state_t)state->record.state == MOTOR_RECORD_STATE_IDLE) &&
        (state->nav.speed_kmh_x10 >= state->settings.recording.auto_start_speed_kmh_x10))
    {
        state->record.start_requested = true;
    }

    if (((motor_record_state_t)state->record.state == MOTOR_RECORD_STATE_RECORDING) &&
        (state->settings.recording.auto_start_enabled != 0u) &&
        (state->nav.moving == false) &&
        (state->settings.recording.auto_stop_idle_seconds != 0u) &&
        (state->record_session.stopped_seconds >= state->settings.recording.auto_stop_idle_seconds))
    {
        state->record.stop_requested = true;
    }

    if ((state->record.start_requested != false) &&
        ((motor_record_state_t)state->record.state != MOTOR_RECORD_STATE_RECORDING))
    {
        state->record.start_requested = false;
        motor_record_start(state);
    }

    if ((state->record.stop_requested != false) && (s_log_file_open != 0u))
    {
        state->record.stop_requested = false;
        state->record.state = (uint8_t)MOTOR_RECORD_STATE_CLOSING;
        state->record_session.stop_ms = state->now_ms;
        state->record_session.active = false;
        motor_record_close_with_summary(state);
        Motor_State_ShowToast("REC STOP", 1200u);
        return;
    }

    motor_record_note_live_speed_metrics(state);
    motor_record_integrate_live_and_record_distance(state);

    if ((motor_record_state_t)state->record.state == MOTOR_RECORD_STATE_PAUSED)
    {
        motor_record_maybe_flush(state);
        return;
    }

    if ((motor_record_state_t)state->record.state != MOTOR_RECORD_STATE_RECORDING)
    {
        return;
    }

    motor_record_note_record_speed_metrics(state);

    if ((uint32_t)(state->now_ms - s_last_nav_write_ms) >= 100u)
    {
        motor_log_nav_payload_t nav;
        memset(&nav, 0, sizeof(nav));
        nav.lat_e7 = state->nav.lat_e7;
        nav.lon_e7 = state->nav.lon_e7;
        nav.altitude_cm = state->nav.altitude_cm;
        nav.speed_mmps = state->nav.speed_mmps;
        nav.heading_deg_x10 = state->nav.heading_deg_x10;
        nav.hacc_mm = state->nav.hacc_mm;
        nav.vacc_mm = state->nav.vacc_mm;
        nav.fix_type = state->nav.fix_type;
        nav.sats_used = state->nav.sats_used;
        if (motor_record_enqueue_payload(MOTOR_LOG_REC_NAV, state->now_ms, &nav, sizeof(nav)) == 0u)
        {
            state->record.drop_count++;
        }
        s_last_nav_write_ms = state->now_ms;
    }

    if ((uint32_t)(state->now_ms - s_last_dyn_write_ms) >= 50u)
    {
        /* ------------------------------------------------------------------ */
        /*  DYN log ??zero_valid ?댄썑遺?곕쭔 湲곕줉?쒕떎.                         */
        /*                                                                    */
        /*  遺??吏곹썑 provisional display 媛믪? ?쇱씠?붿뿉寃뚮쭔 蹂댁뿬二쇨퀬,           */
        /*  ?뚯씪?먮뒗 ?뺤젙 zero 湲곗??????ㅼ쓽 canonical estimator 媛믩쭔 ?④릿?? */
        /* ------------------------------------------------------------------ */
        if (state->dyn.zero_valid != false)
        {
            motor_log_dyn_payload_t dyn;
            memset(&dyn, 0, sizeof(dyn));
            dyn.bank_deg_x10 = state->dyn.est_bank_deg_x10;
            dyn.grade_deg_x10 = state->dyn.est_grade_deg_x10;
            dyn.bank_rate_dps_x10 = state->dyn.est_bank_rate_dps_x10;
            dyn.grade_rate_dps_x10 = state->dyn.est_grade_rate_dps_x10;
            dyn.lat_accel_mg = state->dyn.est_lat_accel_mg;
            dyn.lon_accel_mg = state->dyn.est_lon_accel_mg;
            dyn.confidence_permille = state->dyn.confidence_permille;
            dyn.speed_source = state->dyn.speed_source;
            dyn.heading_source = state->dyn.heading_source;
            if (motor_record_enqueue_payload(MOTOR_LOG_REC_DYN, state->now_ms, &dyn, sizeof(dyn)) == 0u)
            {
                state->record.drop_count++;
            }
        }

        s_last_dyn_write_ms = state->now_ms;
    }

    if ((state->vehicle.connected != false) && ((uint32_t)(state->now_ms - s_last_obd_write_ms) >= 200u))
    {
        motor_log_obd_payload_t obd;
        memset(&obd, 0, sizeof(obd));
        obd.rpm = state->vehicle.rpm;
        obd.coolant_temp_c_x10 = state->vehicle.coolant_temp_c_x10;
        obd.gear = state->vehicle.gear;
        obd.battery_mv = state->vehicle.battery_mv;
        obd.throttle_percent = state->vehicle.throttle_percent;
        obd.dtc_count = state->vehicle.dtc_count;
        if (motor_record_enqueue_payload(MOTOR_LOG_REC_OBD, state->now_ms, &obd, sizeof(obd)) == 0u)
        {
            state->record.drop_count++;
        }
        s_last_obd_write_ms = state->now_ms;
    }

    if (state->record.marker_requested != false)
    {
        motor_log_evt_payload_t evt;
        memset(&evt, 0, sizeof(evt));
        evt.event_code = 1u;
        evt.event_value = 0u;
        evt.aux_u32 = state->record_session.marker_count;
        if (motor_record_enqueue_payload(MOTOR_LOG_REC_EVT, state->now_ms, &evt, sizeof(evt)) != 0u)
        {
            state->record_session.marker_count++;
            Motor_State_ShowToast("MARK", 900u);
        }
        else
        {
            state->record.drop_count++;
        }
        state->record.marker_requested = false;
    }

    /* ---------------------------------------------------------------------- */
    /*  session distance??GNSS speed ?곷텇 湲곕컲?쇰줈 ?꾩쟻?쒕떎.                  */
    /*  - dt???ㅼ젣 now_ms 李⑥씠濡?怨꾩궛?쒕떎.                                     */
    /*  - upper app layer?대?濡?raw position ?곷텇 ???                        */
    /*    APP_STATE snapshot???뺢퇋?붾맂 speed_mmps留??ъ슜?쒕떎.                  */
    /* ---------------------------------------------------------------------- */
    motor_record_maybe_flush(state);
#endif
}
