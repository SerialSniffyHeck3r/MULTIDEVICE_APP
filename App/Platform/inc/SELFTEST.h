#ifndef SELFTEST_H
#define SELFTEST_H

#include "main.h"

#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

/* -------------------------------------------------------------------------- */
/* SELFTEST                                                                    */
/*                                                                            */
/* 역할                                                                       */
/* - 부팅 직후 수행되는 장치 self-test 결과를 한곳에 저장한다.                */
/* - GPS / IMU / SENSORS / HARDWARE 4개 카테고리를 동시에 검사한다.          */
/* - 검사 진행률과 PASS/FAIL 결과를 BOOT_SELFTEST_SCREEN이 읽을 수 있게      */
/*   공개 report 구조체로 제공한다.                                           */
/* - 검사 종료 후에도 결과를 static 저장소에 유지하므로,                      */
/*   런타임 다른 화면이나 로거가 나중에 실패 사실을 조회할 수 있다.          */
/*                                                                            */
/* 설계 원칙                                                                   */
/* - APP_STATE에 부팅 self-test 결과를 억지로 밀어 넣지 않는다.               */
/* - POWER_STATE와도 분리하여, 전원 상태머신과 부트 health state를             */
/*   서로 독립적으로 유지한다.                                                */
/* - UI는 이 모듈의 공개 report만 읽고,                                       */
/*   실제 저수준 판정은 이 모듈 내부 helper들이 맡는다.                       */
/* -------------------------------------------------------------------------- */

typedef enum
{
    SELFTEST_ITEM_STATE_IDLE = 0u,
    SELFTEST_ITEM_STATE_RUNNING,
    SELFTEST_ITEM_STATE_PASS,
    SELFTEST_ITEM_STATE_FAIL
} selftest_item_state_t;

/* -------------------------------------------------------------------------- */
/* fail mask                                                                   */
/*                                                                            */
/* 각 카테고리 실패 여부를 bitmask로 보관한다.                                 */
/* 나중에 런타임 코드가 "어느 축이 실패했는가"를 빠르게 판정할 수 있다.      */
/* -------------------------------------------------------------------------- */
enum
{
    SELFTEST_FAIL_GPS      = 0x01u,
    SELFTEST_FAIL_IMU      = 0x02u,
    SELFTEST_FAIL_SENSORS  = 0x04u,
    SELFTEST_FAIL_HARDWARE = 0x08u
};

/* -------------------------------------------------------------------------- */
/* item report                                                                  */
/*                                                                            */
/* state           : 현재 항목 상태                                            */
/* started_ms      : 이 항목 검사 시작 tick                                    */
/* finished_ms     : PASS/FAIL 확정 tick                                        */
/* deadline_ms     : timeout 기준 tick                                          */
/* progress_value  : 현재 진행 카운트                                           */
/* progress_target : 목표 카운트                                                */
/* short_text      : 짧은 디버그 설명                                           */
/* -------------------------------------------------------------------------- */
typedef struct
{
    selftest_item_state_t state;
    uint32_t started_ms;
    uint32_t finished_ms;
    uint32_t deadline_ms;
    uint32_t progress_value;
    uint32_t progress_target;
    char short_text[32];
} selftest_item_report_t;

/* -------------------------------------------------------------------------- */
/* 전체 self-test report                                                        */
/*                                                                            */
/* started    : SELFTEST_Begin() 호출 여부                                     */
/* finished   : 4개 항목이 모두 PASS/FAIL로 확정되었는가                        */
/* any_failed : 하나라도 FAIL이 있었는가                                       */
/* fail_mask  : 어떤 항목이 FAIL 했는가                                        */
/* start_ms   : 전체 self-test 시작 tick                                       */
/* finish_ms  : 전체 self-test 종료 tick                                       */
/* -------------------------------------------------------------------------- */
typedef struct
{
    bool started;
    bool finished;
    bool any_failed;
    uint32_t fail_mask;
    uint32_t start_ms;
    uint32_t finish_ms;
    selftest_item_report_t gps;
    selftest_item_report_t imu;
    selftest_item_report_t sensors;
    selftest_item_report_t hardware;
} selftest_report_t;

/* -------------------------------------------------------------------------- */
/* 공개 API                                                                     */
/* -------------------------------------------------------------------------- */

/* -------------------------------------------------------------------------- */
/* 내부 static 저장소를 초기 상태로 되돌린다.                                  */
/* 이 함수는 부팅 중 새 self-test 세션을 시작하기 전에 1회 호출한다.          */
/* -------------------------------------------------------------------------- */
void SELFTEST_Reset(void);

/* -------------------------------------------------------------------------- */
/* 4개 self-test 카테고리를 동시에 시작한다.                                   */
/*                                                                            */
/* 호출 전제                                                                    */
/* - U8G2 display init 완료                                                   */
/* - GPS / IMU / DS18 / Brightness / SPI Flash / SD runtime init 완료         */
/* - 아직 일반 런타임 UI 엔진으로 넘어가기 전                                 */
/* -------------------------------------------------------------------------- */
void SELFTEST_Begin(uint32_t now_ms);

/* -------------------------------------------------------------------------- */
/* self-test 상태머신 주기 처리                                                 */
/*                                                                            */
/* 이 함수는 blocking boot screen loop에서 반복 호출된다.                      */
/* 각 카테고리의 진행률과 PASS/FAIL 판정이 여기서 갱신된다.                    */
/* -------------------------------------------------------------------------- */
void SELFTEST_Task(uint32_t now_ms);

/* -------------------------------------------------------------------------- */
/* 현재 self-test가 active 상태인가?                                            */
/* -------------------------------------------------------------------------- */
bool SELFTEST_IsRunning(void);

/* -------------------------------------------------------------------------- */
/* 현재 self-test가 완전히 종료되었는가?                                        */
/* -------------------------------------------------------------------------- */
bool SELFTEST_IsFinished(void);

/* -------------------------------------------------------------------------- */
/* 하나 이상의 항목이 FAIL 했는가?                                             */
/* -------------------------------------------------------------------------- */
bool SELFTEST_AnyFailed(void);

/* -------------------------------------------------------------------------- */
/* fail bitmask getter                                                         */
/* -------------------------------------------------------------------------- */
uint32_t SELFTEST_GetFailMask(void);

/* -------------------------------------------------------------------------- */
/* 현재 저장된 전체 report를 out_report에 복사한다.                             */
/* out_report가 NULL이면 아무 일도 하지 않는다.                                */
/* -------------------------------------------------------------------------- */
void SELFTEST_CopyReport(selftest_report_t *out_report);

/* -------------------------------------------------------------------------- */
/* item state를 사람이 보기 쉬운 짧은 문자열로 변환한다.                       */
/* 반환 문자열은 static literal 이다.                                          */
/* -------------------------------------------------------------------------- */
const char *SELFTEST_GetItemStateText(selftest_item_state_t state);

#ifdef __cplusplus
}
#endif

#endif /* SELFTEST_H */
