
#include "Motor_Maintenance.h"

#include "Motor_State.h"

#include <string.h>

static uint32_t s_last_tick_ms;
static uint32_t s_last_distance_m;

const char *Motor_Maintenance_GetServiceLabel(uint8_t service_index)
{
    const motor_state_t *state;

    state = Motor_State_Get();
    if ((state == 0) || (service_index >= MOTOR_SERVICE_ITEM_COUNT))
    {
        return "SERVICE";
    }

    return state->settings.maintenance.items[service_index].label;
}

void Motor_Maintenance_Init(void)
{
    s_last_tick_ms = 0u;
    s_last_distance_m = 0u;
}

void Motor_Maintenance_ResetService(uint8_t service_index)
{
    motor_state_t *state;

    state = Motor_State_GetMutable();
    if ((state == 0) || (service_index >= MOTOR_SERVICE_ITEM_COUNT))
    {
        return;
    }

    state->settings.maintenance.items[service_index].last_service_odo_m = state->maintenance.odo_total_m;
    state->settings.maintenance.items[service_index].last_service_engine_seconds = state->maintenance.engine_on_seconds_total;
    state->settings.maintenance.items[service_index].last_service_year = state->snapshot.clock.local.year;
    state->settings.maintenance.items[service_index].last_service_month = state->snapshot.clock.local.month;
    state->settings.maintenance.items[service_index].last_service_day = state->snapshot.clock.local.day;
    Motor_State_ShowToast("SERVICE RESET", 1000u);
}

void Motor_Maintenance_Task(uint32_t now_ms)
{
    motor_state_t *state;
    uint8_t i;
    uint32_t ride_dt_ms;
    uint32_t odo_delta_m;
    uint32_t remain_m;
    uint32_t smallest_remain_m;

    state = Motor_State_GetMutable();
    if (state == 0)
    {
        return;
    }

    if (s_last_tick_ms == 0u)
    {
        s_last_tick_ms = now_ms;
        s_last_distance_m = state->session.distance_m;
    }

    ride_dt_ms = (uint32_t)(now_ms - s_last_tick_ms);
    if (ride_dt_ms >= 1000u)
    {
        uint32_t secs;

        secs = ride_dt_ms / 1000u;
        state->session.ride_seconds += secs;
        if (state->nav.moving != false)
        {
            state->session.moving_seconds += secs;
            state->maintenance.engine_on_seconds_total += secs;
        }
        else
        {
            state->session.stopped_seconds += secs;
        }
        s_last_tick_ms += secs * 1000u;
    }

    odo_delta_m = (state->session.distance_m >= s_last_distance_m) ? (state->session.distance_m - s_last_distance_m) : 0u;
    if (odo_delta_m != 0u)
    {
        state->maintenance.odo_total_m += odo_delta_m;
        s_last_distance_m = state->session.distance_m;
    }

    state->maintenance.due_count = 0u;
    state->maintenance.next_due_index = 0xFFu;
    smallest_remain_m = 0xFFFFFFFFu;

    for (i = 0u; i < MOTOR_SERVICE_ITEM_COUNT; i++)
    {
        const motor_service_item_config_t *cfg;
        uint8_t due;

        cfg = &state->settings.maintenance.items[i];
        due = 0u;

        if (cfg->enabled == 0u)
        {
            state->maintenance.due_flags[i] = 0u;
            continue;
        }

        if ((cfg->interval_km_x1000 != 0u) &&
            (state->maintenance.odo_total_m >= cfg->last_service_odo_m + (cfg->interval_km_x1000 * 1000u)))
        {
            due = 1u;
        }

        if ((cfg->interval_engine_seconds != 0u) &&
            (state->maintenance.engine_on_seconds_total >= cfg->last_service_engine_seconds + cfg->interval_engine_seconds))
        {
            due = 1u;
        }

        state->maintenance.due_flags[i] = due;
        if (due != 0u)
        {
            state->maintenance.due_count++;
        }

        if ((cfg->interval_km_x1000 != 0u) &&
            (state->maintenance.odo_total_m < cfg->last_service_odo_m + (cfg->interval_km_x1000 * 1000u)))
        {
            remain_m = (cfg->last_service_odo_m + (cfg->interval_km_x1000 * 1000u)) - state->maintenance.odo_total_m;
            if (remain_m < smallest_remain_m)
            {
                smallest_remain_m = remain_m;
                state->maintenance.next_due_index = i;
            }
        }
    }
}
