#ifndef BRIGHTNESS_SENSOR_H
#define BRIGHTNESS_SENSOR_H

#include "main.h"
#include "adc.h"
#include "APP_STATE.h"

#ifdef __cplusplus
extern "C" {
#endif

/* -------------------------------------------------------------------------- */
/*  Brightness_Sensor                                                          */
/*                                                                            */
/*  목적                                                                      */
/*  - CDS(LDR) 밝기 센서를 ADC1 채널 3(PA3)로 주기적으로 읽는다.              */
/*  - STM32F407 계열은 다른 일부 STM32처럼 HAL self calibration API가          */
/*    제공되지 않으므로, offset / gain / 0% / 100% 기준점을                   */
/*    소프트웨어 후처리로 적용한다.                                           */
/*  - 공개 결과는 APP_STATE.brightness slice에만 저장한다.                    */
/*                                                                            */
/*  이번 최적화의 핵심                                                         */
/*  - 기존 구현은 100ms마다 16샘플을 한 번에 몰아서 읽었다.                   */
/*  - 이 방식은 ADC PollForConversion blocking 구간이 짧더라도                 */
/*    한 task call이 덩어리로 길어질 수 있다.                                  */
/*  - 따라서 이제는 "burst 평균" 개념은 유지하되,                              */
/*    샘플을 loop 여러 번에 나눠 1개씩 수집하는 incremental state machine으로   */
/*    바꿔서 worst-case main loop 체류 시간을 줄인다.                          */
/*                                                                            */
/*  CubeMX 재생성 내성 포인트                                                  */
/*  - adc.c 가 다시 생성되어 샘플링 타임이 바뀌더라도,                         */
/*    이 드라이버가 runtime 에서 원하는 채널/샘플링 타임을 다시 설정한다.     */
/*  - 따라서 CDS처럼 source impedance가 큰 센서에 맞춰                         */
/*    긴 sampling time 을 안전하게 강제할 수 있다.                            */
/* -------------------------------------------------------------------------- */

#ifndef BRIGHTNESS_SENSOR_ADC_HANDLE
#define BRIGHTNESS_SENSOR_ADC_HANDLE           hadc1
#endif

#ifndef BRIGHTNESS_SENSOR_ADC_CHANNEL
#define BRIGHTNESS_SENSOR_ADC_CHANNEL          ADC_CHANNEL_3
#endif

#ifndef BRIGHTNESS_SENSOR_ADC_RANK
#define BRIGHTNESS_SENSOR_ADC_RANK             1u
#endif

#ifndef BRIGHTNESS_SENSOR_ADC_SAMPLING_TIME
#define BRIGHTNESS_SENSOR_ADC_SAMPLING_TIME    ADC_SAMPLETIME_480CYCLES
#endif

#ifndef BRIGHTNESS_SENSOR_PERIOD_MS
#define BRIGHTNESS_SENSOR_PERIOD_MS            100u
#endif

#ifndef BRIGHTNESS_SENSOR_AVERAGE_COUNT
#define BRIGHTNESS_SENSOR_AVERAGE_COUNT        16u
#endif

#ifndef BRIGHTNESS_SENSOR_ADC_TIMEOUT_MS
#define BRIGHTNESS_SENSOR_ADC_TIMEOUT_MS       5u
#endif

#ifndef BRIGHTNESS_SENSOR_VREF_MV
#define BRIGHTNESS_SENSOR_VREF_MV              3300u
#endif

/* -------------------------------------------------------------------------- */
/*  burst 내부 inter-sample 간격                                               */
/*                                                                            */
/*  0이면 main loop가 도는 즉시 다음 샘플을 이어서 수집한다.                   */
/*  1ms 정도를 주면 "16개 연속 blocking" 대신                                  */
/*  "여러 loop에 나눠 1개씩" 읽게 되어 체감 블로킹이 크게 줄어든다.            */
/* -------------------------------------------------------------------------- */
#ifndef BRIGHTNESS_SENSOR_INTER_SAMPLE_MS
#define BRIGHTNESS_SENSOR_INTER_SAMPLE_MS      1u
#endif

/* -------------------------------------------------------------------------- */
/*  소프트웨어 calibration knobs                                               */
/*                                                                            */
/*  1) offset counts                                                           */
/*     - ADC raw 에 먼저 더하는 값                                             */
/*  2) gain num/den                                                            */
/*     - offset 적용 후 gain 을 곱한다.                                        */
/*  3) raw at 0% / 100%                                                        */
/*     - 캘리브레이션 후 counts 를 0~100% 로 정규화하는 기준점                 */
/*                                                                            */
/*  중요                                                                      */
/*  - CDS divider 방향에 따라 밝을수록 raw 가 커질 수도, 작아질 수도 있다.    */
/*  - 따라서 0% 기준과 100% 기준 중 어느 쪽이 더 큰지는                        */
/*    드라이버가 자동으로 해석한다.                                            */
/* -------------------------------------------------------------------------- */
#ifndef BRIGHTNESS_SENSOR_CAL_OFFSET_COUNTS
#define BRIGHTNESS_SENSOR_CAL_OFFSET_COUNTS    0
#endif

#ifndef BRIGHTNESS_SENSOR_CAL_GAIN_NUM
#define BRIGHTNESS_SENSOR_CAL_GAIN_NUM         1000u
#endif

#ifndef BRIGHTNESS_SENSOR_CAL_GAIN_DEN
#define BRIGHTNESS_SENSOR_CAL_GAIN_DEN         1000u
#endif

#ifndef BRIGHTNESS_SENSOR_CAL_RAW_AT_0_PERCENT
#define BRIGHTNESS_SENSOR_CAL_RAW_AT_0_PERCENT 4095u
#endif

#ifndef BRIGHTNESS_SENSOR_CAL_RAW_AT_100_PERCENT
#define BRIGHTNESS_SENSOR_CAL_RAW_AT_100_PERCENT 0u
#endif

void Brightness_Sensor_Init(void);
void Brightness_Sensor_Task(uint32_t now_ms);

#ifdef __cplusplus
}
#endif

#endif /* BRIGHTNESS_SENSOR_H */
