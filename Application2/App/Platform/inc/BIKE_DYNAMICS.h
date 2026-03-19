
#ifndef BIKE_DYNAMICS_H
#define BIKE_DYNAMICS_H

#include <stdint.h>
#include <stdbool.h>

#include "APP_STATE.h"

#ifdef __cplusplus
extern "C" {
#endif

/* -------------------------------------------------------------------------- */
/*  BIKE_DYNAMICS                                                              */
/*                                                                            */
/*  역할                                                                       */
/*  - MPU6050 / 추후 MPU9250 같은 6축 IMU의 raw 값을 사용해                    */
/*    모터사이클 프레임 기준의 banking angle(뱅킹각) / grade(오르막, 내리막) /  */
/*    lateral G / accel-decel 을 추정한다.                                     */
/*  - magnetometer는 사용하지 않는다.                                          */
/*  - GNSS speed / course, 그리고 추후 OBD speed는                            */
/*    "저주파 보정(anchor)" 으로만 사용한다.                                   */
/*  - ResetBankingAngleSensor()가 호출되면                                     */
/*    그 순간의 프레임 자세를 0도 / 0도 기준으로 다시 잡는다.                  */
/*                                                                            */
/*  출력 sign 규칙                                                             */
/*  - banking angle : + = 좌측으로 기울어짐(left lean), - = 우측 lean          */
/*  - grade         : + = 노즈 업 / 오르막,            - = 노즈 다운 / 내리막   */
/*  - lateral G     : + = 좌회전 방향 가속(left),      - = 우회전 방향 가속     */
/*  - accel-decel   : + = 가속,                        - = 제동                  */
/*                                                                            */
/*  사용 규칙                                                                  */
/*  - boot 시 BIKE_DYNAMICS_Init(now_ms) 한 번 호출                            */
/*  - main while(1) 에서 BIKE_DYNAMICS_Task(now_ms) 반복 호출                  */
/*  - UI / logger / dashboard는 APP_STATE_CopyBikeSnapshot() 만 사용           */
/* -------------------------------------------------------------------------- */
void BIKE_DYNAMICS_Init(uint32_t now_ms);
void BIKE_DYNAMICS_Task(uint32_t now_ms);

/* -------------------------------------------------------------------------- */
/*  수동 zero capture 요청                                                     */
/*                                                                            */
/*  이 함수는 "지금 자세를 bank=0 / grade=0 기준으로 새로 삼아라" 라는 요청만   */
/*  세팅한다. 실제 zero basis 캡처는 다음 유효 IMU 샘플에서 안전하게 수행된다.  */
/* -------------------------------------------------------------------------- */
void BIKE_DYNAMICS_RequestZeroCapture(void);

/* -------------------------------------------------------------------------- */
/*  강한 재정렬 요청                                                           */
/*                                                                            */
/*  - gravity estimator를 초기화하고                                           */
/*  - GNSS/OBD bias anchor도 다시 0부터 잡고                                   */
/*  - 다음 유효 IMU 샘플에서 zero capture를 다시 수행한다.                     */
/*                                                                            */
/*  필터 상수가 크게 바뀌었거나, 디버그 중 상태를 깨끗하게 다시 보고 싶을 때    */
/*  legacy bike debug page의 long press에서 사용하는 API다.                    */
/* -------------------------------------------------------------------------- */
void BIKE_DYNAMICS_RequestHardRezero(void);

/* -------------------------------------------------------------------------- */
/*  사용자 요구 함수명 wrapper                                                 */
/*                                                                            */
/*  질문에서 직접 지정한 이름을 그대로 제공한다.                               */
/* -------------------------------------------------------------------------- */
void ResetBankingAngleSensor(void);

#ifdef __cplusplus
}
#endif

#endif /* BIKE_DYNAMICS_H */
