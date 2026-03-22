#ifndef VARIO_UI_VARIO_FILTER_H
#define VARIO_UI_VARIO_FILTER_H

#include <stdbool.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

/* -------------------------------------------------------------------------- */
/* UI vario 전용 robust filter                                                 */
/*                                                                            */
/* 이 필터는 저수준 센서 raw 값을 직접 읽지 않는다.                           */
/* 반드시 APP_STATE 에서 memcpy 해 온 고수준 snapshot 중                      */
/* "already sane 한 slow vario" 값만 입력으로 받는다.                         */
/*                                                                            */
/* 출력 용도                                                                  */
/* - Vario main UI 의 큰 vario 숫자                                            */
/* - Vario main UI 의 작은 vario 숫자                                          */
/* - Vario main UI 오른쪽 side bar 의 세로 vario 막대                          */
/*                                                                            */
/* 즉, 최종적으로는 Vario_State.runtime.baro_vario_mps 로 publish 되어        */
/* 화면 렌더러가 그대로 소비하는 "표시 전용 바리오 값" 을 만드는 필터다.      */
/* -------------------------------------------------------------------------- */

#ifndef VARIO_UI_VARIO_FILTER_INPUT_WINDOW
#define VARIO_UI_VARIO_FILTER_INPUT_WINDOW 7u
#endif

#ifndef VARIO_UI_VARIO_FILTER_OUTPUT_WINDOW
#define VARIO_UI_VARIO_FILTER_OUTPUT_WINDOW 3u
#endif

typedef struct
{
    /* ---------------------------------------------------------------------- */
    /* 필터 초기화 / 래치 상태                                                 */
    /* ---------------------------------------------------------------------- */
    bool initialized;
    bool zero_latched;

    /* ---------------------------------------------------------------------- */
    /* ring buffer 상태                                                        */
    /* - input_window_mps  : Hampel/MAD outlier detector 입력 창              */
    /* - output_window_mps : 마지막 표시 후보 3개에 대한 median 창            */
    /* ---------------------------------------------------------------------- */
    uint8_t input_count;
    uint8_t input_head;
    uint8_t output_count;
    uint8_t output_head;

    /* ---------------------------------------------------------------------- */
    /* 시간축                                                                  */
    /* - timestamp_ms 는 APP_STATE altitude snapshot 의 last_update_ms        */
    /*   또는 그와 동일한 host tick 계열 값을 넣는다.                          */
    /* ---------------------------------------------------------------------- */
    uint32_t last_update_ms;

    /* ---------------------------------------------------------------------- */
    /* 내부 이력 버퍼                                                          */
    /* ---------------------------------------------------------------------- */
    float input_window_mps[VARIO_UI_VARIO_FILTER_INPUT_WINDOW];
    float output_window_mps[VARIO_UI_VARIO_FILTER_OUTPUT_WINDOW];

    /* ---------------------------------------------------------------------- */
    /* 디버그/튜닝용 내부 상태                                                 */
    /* ---------------------------------------------------------------------- */
    float robust_input_mps;
    float smoothed_mps;
    float display_mps;
} vario_ui_vario_filter_t;

/* -------------------------------------------------------------------------- */
/* 구조체 전체를 0으로 지우고, 아직 입력을 받지 않은 상태로 만든다.           */
/* -------------------------------------------------------------------------- */
void Vario_UiVarioFilter_Init(vario_ui_vario_filter_t *filter);

/* -------------------------------------------------------------------------- */
/* 첫 유효 샘플이 들어왔을 때 창 전체를 같은 값으로 채워                      */
/* 시작 프레임에서 불필요한 튐이 나오지 않도록 초기상태를 맞춘다.            */
/* -------------------------------------------------------------------------- */
void Vario_UiVarioFilter_Reset(vario_ui_vario_filter_t *filter,
                               float input_mps,
                               uint32_t timestamp_ms);

/* -------------------------------------------------------------------------- */
/* 새 slow vario 샘플 1개를 입력받아 display 용 바리오 값을 갱신한다.        */
/*                                                                            */
/* 처리 단계                                                                  */
/* 1) 7-sample Hampel/MAD 기반 outlier clamp                                  */
/* 2) innovation 크기에 따라 빨라졌다 느려지는 adaptive EMA                   */
/* 3) 마지막 3개 출력 후보의 median                                           */
/* 4) 0 근처 chatter 제거용 hysteresis zero latch                             */
/*                                                                            */
/* damping_level / average_seconds 는 기존 QuickSet UI 와 의미를 맞춘다.      */
/* - damping_level 이 클수록 정지 시 더 차분해진다.                           */
/* - average_seconds 가 클수록 출력이 더 묵직해진다.                          */
/* -------------------------------------------------------------------------- */
float Vario_UiVarioFilter_Update(vario_ui_vario_filter_t *filter,
                                 float input_mps,
                                 uint32_t timestamp_ms,
                                 uint8_t damping_level,
                                 uint8_t average_seconds);

#ifdef __cplusplus
}
#endif

#endif /* VARIO_UI_VARIO_FILTER_H */
