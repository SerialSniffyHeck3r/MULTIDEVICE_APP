
#include "Motor_Vehicle.h"

#include "Motor_State.h"

#include <string.h>

static uint8_t s_connect_request;
static uint8_t s_disconnect_request;
static uint32_t s_link_transition_ms;

static uint32_t motor_vehicle_abs_u32(int32_t v)
{
    return (uint32_t)((v < 0) ? -v : v);
}

void Motor_Vehicle_Init(void)
{
    s_connect_request = 0u;
    s_disconnect_request = 0u;
    s_link_transition_ms = 0u;
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

    state = Motor_State_GetMutable();
    if (state == 0)
    {
        return;
    }

    vehicle = &state->vehicle;

    if (s_disconnect_request != 0u)
    {
        memset(vehicle, 0, sizeof(*vehicle));
        vehicle->link_state = (uint8_t)MOTOR_OBD_LINK_DISCONNECTED;
        s_disconnect_request = 0u;
        Motor_State_ShowToast("OBD OFF", 1000u);
    }

    if (s_connect_request != 0u)
    {
        vehicle->link_state = (uint8_t)MOTOR_OBD_LINK_CONNECTING;
        s_link_transition_ms = now_ms;
        s_connect_request = 0u;
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
        /*  현재는 실제 BT UART OBD 모듈이 아직 없으므로                          */
        /*  skeleton 단계에서는 라이딩 속도와 시간 기반으로 값을 흉내만 낸다.     */
        /*  추후 여기서 Bluetooth RX line parser 또는 binary packet parser를      */
        /*  붙이면 된다.                                                         */
        /* ------------------------------------------------------------------ */
        vehicle->last_update_ms = now_ms;
        vehicle->rpm = (uint16_t)(1200u + (uint16_t)((state->nav.speed_kmh_x10 * 55u) / 10u));
        vehicle->coolant_temp_c_x10 = (int16_t)(650 + (int16_t)((state->nav.speed_kmh_x10 > 300u) ? ((state->nav.speed_kmh_x10 - 300u) / 2u) : 0u));
        vehicle->gear = (int8_t)((state->nav.speed_kmh_x10 < 120u) ? 1 :
                                 (state->nav.speed_kmh_x10 < 250u) ? 2 :
                                 (state->nav.speed_kmh_x10 < 400u) ? 3 :
                                 (state->nav.speed_kmh_x10 < 550u) ? 4 :
                                 (state->nav.speed_kmh_x10 < 700u) ? 5 : 6);
        vehicle->throttle_percent = (uint8_t)((state->dyn.lon_accel_mg > 0) ? (uint8_t)motor_vehicle_abs_u32(state->dyn.lon_accel_mg) / 12u : 8u);
        if (vehicle->throttle_percent > 100u)
        {
            vehicle->throttle_percent = 100u;
        }
        vehicle->battery_mv = (uint16_t)(13100u + ((now_ms / 1000u) % 6u) * 50u);
        vehicle->dtc_count = 0u;
        vehicle->dtc_present = false;
    }
}
