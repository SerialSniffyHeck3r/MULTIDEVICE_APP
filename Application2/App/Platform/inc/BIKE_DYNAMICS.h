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
/*  - MPU6050 같은 6축 IMU raw를 사용해                                        */
/*    모터사이클 프레임 기준의 banking angle / grade / lateral G /             */
/*    accel-decel 을 추정한다.                                                 */
/*  - magnetometer는 사용하지 않는다.                                          */
/*  - GNSS speed / GNSS heading / future OBD speed는                           */
/*    "저주파 anchor" 또는 "속도 anchor" 로만 사용한다.                        */
/*  - roll/pitch 추정 핵심은 IMU-only Mahony quaternion AHRS 이다.             */
/*    즉, gyro는 빠른 자세 변화에 반응하고 accel은 중력 방향으로만             */
/*    천천히 자세를 교정한다.                                                  */
/*                                                                            */
/*  출력 sign 규칙                                                             */
/*  - banking angle : + = left lean,  - = right lean                          */
/*  - grade         : + = nose up,    - = nose down                           */
/*  - lateral G     : + = left,       - = right                               */
/*  - accel-decel   : + = accel,      - = braking                             */
/* -------------------------------------------------------------------------- */
void BIKE_DYNAMICS_Init(uint32_t now_ms);
void BIKE_DYNAMICS_Task(uint32_t now_ms);

/* -------------------------------------------------------------------------- */
/*  수동 zero capture 요청                                                     */
/*                                                                            */
/*  이 함수는 "지금 자세를 bank=0 / grade=0 기준으로 새로 삼아라" 라는 요청만   */
/*  세팅한다. 실제 zero basis 캡처는 다음 유효 IMU 샘플에서                     */
/*  정지/안정 조건을 만족할 때 안전하게 수행된다.                              */
/* -------------------------------------------------------------------------- */
void BIKE_DYNAMICS_RequestZeroCapture(void);

/* -------------------------------------------------------------------------- */
/*  강한 재정렬 요청                                                           */
/*                                                                            */
/*  - attitude quaternion / gravity observer / bias anchor runtime 을          */
/*    다시 초기화한다.                                                         */
/*  - 단, 이미 측정한 gyro bias 자체는 유지한다.                              */
/*  - 다음 유효 IMU 샘플에서 자세를 다시 세우고, zero capture는                 */
/*    별도 요청 또는 auto-zero 정책으로 다시 수행한다.                         */
/* -------------------------------------------------------------------------- */
void BIKE_DYNAMICS_RequestHardRezero(void);

/* -------------------------------------------------------------------------- */
/*  자이로 바이어스 보정 요청                                                  */
/*                                                                            */
/*  이 함수는 "지금부터 일정 시간 동안 바이크를 최대한 정지 상태로 유지하고     */
/*  gyro raw 평균값을 bias로 저장하라" 는 요청을 세팅한다.                     */
/*  실제 보정 절차는 BIKE_DYNAMICS_Task() 안에서 상태기계로 진행된다.          */
/* -------------------------------------------------------------------------- */
void BIKE_DYNAMICS_RequestGyroBiasCalibration(void);

/* -------------------------------------------------------------------------- */
/*  사용자 요구 함수명 wrapper                                                 */
/* -------------------------------------------------------------------------- */
void ResetBankingAngleSensor(void);
void GyroBiasCorrection(void);

#ifdef __cplusplus
}
#endif

#endif /* BIKE_DYNAMICS_H */
