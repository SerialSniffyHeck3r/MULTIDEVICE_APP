
#include "Motor_Navigation.h"

#include "Motor_State.h"

#include <string.h>

static uint32_t s_last_breadcrumb_ms;
static int32_t  s_last_breadcrumb_lat_e7;
static int32_t  s_last_breadcrumb_lon_e7;

static uint32_t motor_nav_abs_u32(int32_t v)
{
    return (uint32_t)((v < 0) ? -v : v);
}

void Motor_Navigation_Init(void)
{
    s_last_breadcrumb_ms = 0u;
    s_last_breadcrumb_lat_e7 = 0;
    s_last_breadcrumb_lon_e7 = 0;
}

void Motor_Navigation_Task(uint32_t now_ms)
{
    motor_state_t *state;
    uint32_t dist_e7;
    uint16_t idx;

    state = Motor_State_GetMutable();
    if (state == 0)
    {
        return;
    }

    /* ---------------------------------------------------------------------- */
    /*  breadcrumb는 최소 거리 또는 최소 시간 조건이 만족될 때만 한 점 찍는다.   */
    /*  위경도 delta를 정확한 meter로 투영하지는 않고, skeleton 단계에서는       */
    /*  간단한 e7 delta gate를 사용한다.                                       */
    /* ---------------------------------------------------------------------- */
    dist_e7 = motor_nav_abs_u32(state->nav.lat_e7 - s_last_breadcrumb_lat_e7) +
              motor_nav_abs_u32(state->nav.lon_e7 - s_last_breadcrumb_lon_e7);

    if ((state->nav.valid != false) &&
        ((uint32_t)(now_ms - s_last_breadcrumb_ms) >= 500u) &&
        ((dist_e7 >= 30u) || ((uint32_t)(now_ms - s_last_breadcrumb_ms) >= 3000u)))
    {
        idx = (uint16_t)(state->breadcrumb.head % MOTOR_BREADCRUMB_POINT_COUNT);
        state->breadcrumb.points[idx].lat_e7 = state->nav.lat_e7;
        state->breadcrumb.points[idx].lon_e7 = state->nav.lon_e7;
        state->breadcrumb.points[idx].alt_cm = state->nav.altitude_cm;
        state->breadcrumb.points[idx].tick_ms = now_ms;
        state->breadcrumb.points[idx].valid = 1u;
        state->breadcrumb.head = (uint16_t)((idx + 1u) % MOTOR_BREADCRUMB_POINT_COUNT);
        if (state->breadcrumb.count < MOTOR_BREADCRUMB_POINT_COUNT)
        {
            state->breadcrumb.count++;
        }

        if (state->breadcrumb.home_valid == 0u)
        {
            state->breadcrumb.home_lat_e7 = state->nav.lat_e7;
            state->breadcrumb.home_lon_e7 = state->nav.lon_e7;
            state->breadcrumb.home_valid = 1u;
        }

        s_last_breadcrumb_ms = now_ms;
        s_last_breadcrumb_lat_e7 = state->nav.lat_e7;
        s_last_breadcrumb_lon_e7 = state->nav.lon_e7;
    }
}
