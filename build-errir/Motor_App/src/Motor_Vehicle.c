#include "Motor_Vehicle.h"

#include "Motor_State.h"
#include "APP_STATE.h"

#include <string.h>

static uint8_t s_connect_request;
static uint8_t s_disconnect_request;
static uint32_t s_link_transition_ms;

static uint32_t motor_vehicle_abs_u32(int32_t v)
{
    return (uint32_t)((v < 0) ? -v : v);
}

static void motor_vehicle_publish_obd_speed_input(uint32_t now_ms,
                                                  uint8_t valid,
                                                  uint32_t speed_mmps)
{
    /* ---------------------------------------------------------------------- */
    /*  BIKE_DYNAMICS가 읽는 OBD speed input 경로는 app_state.bike의           */
    /*  obd_input_* 3개 필드 하나뿐이다.                                       */
    /*                                                                        */
    /*  중요한 계층 규칙                                                       */
    /*  - Motor_App는 low-level IMU/GNSS driver를 직접 두드리지 않는다.       */
    /*  - future OBD service가 생겨도 같은 세 필드만 갱신하면 된다.           */
    /*  - 따라서 여기서는 shared API가 의도한 publish 지점만 연결한다.        */
    /* ---------------------------------------------------------------------- */
    g_app_state.bike.obd_input_speed_valid = (valid != 0u) ? true : false;
    g_app_state.bike.obd_input_speed_mmps = speed_mmps;
    g_app_state.bike.obd_input_last_update_ms = (valid != 0u) ? now_ms : 0u;
}

void Motor_Vehicle_Init(void)
{
    s_connect_request = 0u;
    s_disconnect_request = 0u;
    s_link_transition_ms = 0u;
    motor_vehicle_publish_obd_speed_input(0u, 0u, 0u);
}

void Motor_Vehicle_RequestConnect(void)
{
    s_connect_request = 1u;
    s_disconnect_request = 0u;
}

void Motor_Vehicle_RequestDisconnect(void)
{
    s_disconnect_request = 1u;
    s_connect_request = 0u;
}

void Motor_Vehicle_Task(uint32_t now_ms)
{
    motor_state_t *state;
    motor_vehicle_state_t *vehicle;
    uint8_t publish_speed_to_bike;

    state = Motor_State_GetMutable();
    if (state == 0)
    {
        return;
    }

    vehicle = &state->vehicle;
    publish_speed_to_bike = 0u;

    if (s_disconnect_request != 0u)
    {
        memset(vehicle, 0, sizeof(*vehicle));
        vehicle->link_state = (uint8_t)MOTOR_OBD_LINK_DISCONNECTED;
        s_disconnect_request = 0u;
        motor_vehicle_publish_obd_speed_input(now_ms, 0u, 0u);
        Motor_State_ShowToast("OBD OFF", 1000u);
    }

    if (s_connect_request != 0u)
    {
        vehicle->link_state = (uint8_t)MOTOR_OBD_LINK_CONNECTING;
        s_link_transition_ms = now_ms;
        s_connect_request = 0u;
        motor_vehicle_publish_obd_speed_input(now_ms, 0u, 0u);
        Motor_State_ShowToast("OBD CONNECT", 1000u);
    }

    if ((motor_obd_link_state_t)vehicle->link_state == MOTOR_OBD_LINK_CONNECTING)
    {
        if ((uint32_t)(now_ms - s_link_transition_ms) >= 800u)
        {
            vehicle->link_state = (uint8_t)MOTOR_OBD_LINK_CONNECTED;
            vehicle->connected = true;
            vehicle->data_valid = true;
            vehicle->rpm_valid = true;
            vehicle->coolant_valid = true;
            vehicle->gear_valid = true;
            vehicle->battery_valid = true;
            vehicle->last_update_ms = now_ms;
        }
    }

    if ((motor_obd_link_state_t)vehicle->link_state == MOTOR_OBD_LINK_CONNECTED)
    {
        /* ------------------------------------------------------------------ */
        /*  현재 저장소 단계에서는 실제 BT/UART OBD parser가 아직 없으므로      */
        /*  vehicle state는 주행 상태를 바탕으로 합성한다.                     */
        /*                                                                    */
        /*  그러나 BIKE_DYNAMICS와의 연결 방식은 placeholder가 아니라          */
        /*  실제 양산 구조와 동일하게 유지한다.                                */
        /*  - speed aid는 g_app_state.bike.obd_input_* 로 publish             */
        /*  - dynamics service는 그 필드만 읽어 OBD aided mode를 판단          */
        /* ------------------------------------------------------------------ */
        vehicle->last_update_ms = now_ms;
        vehicle->rpm = (uint16_t)(1200u + (uint16_t)((state->nav.speed_kmh_x10 * 55u) / 10u));
        vehicle->coolant_temp_c_x10 = (int16_t)(650 + (int16_t)((state->nav.speed_kmh_x10 > 300u) ? ((state->nav.speed_kmh_x10 - 300u) / 2u) : 0u));
        vehicle->gear = (int8_t)((state->nav.speed_kmh_x10 < 120u) ? 1 :
                                 (state->nav.speed_kmh_x10 < 250u) ? 2 :
                                 (state->nav.speed_kmh_x10 < 400u) ? 3 :
                                 (state->nav.speed_kmh_x10 < 550u) ? 4 :
                                 (state->nav.speed_kmh_x10 < 700u) ? 5 : 6);

        /* ------------------------------------------------------------------ */
        /*  throttle skeleton은 canonical longitudinal estimate를 기반으로      */
        /*  계산한다. display smoothing된 값이 아니라 est_* 를 쓰는 이유는     */
        /*  OBD stub UI도 내부 truth와 같은 반응속도를 갖게 하려는 것이다.     */
        /* ------------------------------------------------------------------ */
        vehicle->throttle_percent = (uint8_t)((state->dyn.est_lon_accel_mg > 0) ?
                                              (uint8_t)(motor_vehicle_abs_u32(state->dyn.est_lon_accel_mg) / 12u) :
                                              8u);
        if (vehicle->throttle_percent > 100u)
        {
            vehicle->throttle_percent = 100u;
        }

        vehicle->battery_mv = (uint16_t)(13100u + ((now_ms / 1000u) % 6u) * 50u);
        vehicle->dtc_count = 0u;
        vehicle->dtc_present = false;

        publish_speed_to_bike = ((state->settings.obd.preferred_speed_source == (uint8_t)APP_BIKE_SPEED_SOURCE_OBD) &&
                                 (state->settings.dynamics.obd_aid_enabled != 0u)) ? 1u : 0u;
    }

    if ((publish_speed_to_bike != 0u) && (vehicle->connected != false) && (vehicle->data_valid != false))
    {
        motor_vehicle_publish_obd_speed_input(now_ms, 1u, state->nav.speed_mmps);
    }
    else
    {
        motor_vehicle_publish_obd_speed_input(now_ms, 0u, 0u);
    }
}
