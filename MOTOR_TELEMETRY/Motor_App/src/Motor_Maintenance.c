
#include "Motor_Maintenance.h"

#include "Motor_Settings.h"
#include "Motor_State.h"

#include <string.h>

static uint32_t s_last_tick_ms;
static uint32_t s_last_distance_m;
static uint8_t s_last_moving;

static void motor_maintenance_trip_reset(motor_trip_metrics_t *trip)
{
    if (trip != 0)
    {
        memset(trip, 0, sizeof(*trip));
    }
}

static uint8_t motor_maintenance_same_day(const motor_state_t *state)
{
    if (state == 0)
    {
        return 1u;
    }

    return (uint8_t)((state->session.today_anchor_year == state->snapshot.clock.local.year) &&
                     (state->session.today_anchor_month == state->snapshot.clock.local.month) &&
                     (state->session.today_anchor_day == state->snapshot.clock.local.day));
}

static void motor_maintenance_update_today_anchor(motor_state_t *state)
{
    if (state == 0)
    {
        return;
    }

    state->session.today_anchor_year = state->snapshot.clock.local.year;
    state->session.today_anchor_month = state->snapshot.clock.local.month;
    state->session.today_anchor_day = state->snapshot.clock.local.day;
}

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
    s_last_moving = 0u;
}

void Motor_Maintenance_ResetService(uint8_t service_index)
{
    const motor_state_t        *state;
    motor_settings_t           *settings;
    motor_service_item_config_t *cfg;

    state = Motor_State_Get();
    settings = Motor_Settings_GetMutable();
    if ((state == 0) || (settings == 0) || (service_index >= MOTOR_SERVICE_ITEM_COUNT))
    {
        return;
    }

    /* ---------------------------------------------------------------------- */
    /*  persistent maintenance 기준점은 runtime copy(state.settings)가 아니라  */
    /*  Motor_Settings canonical store를 수정해야 한다.                        */
    /* ---------------------------------------------------------------------- */
    cfg = &settings->maintenance.items[service_index];

    cfg->last_service_odo_m = state->maintenance.odo_total_m;
    cfg->last_service_engine_seconds = state->maintenance.engine_on_seconds_total;
    cfg->last_service_year = state->snapshot.clock.local.year;
    cfg->last_service_month = state->snapshot.clock.local.month;
    cfg->last_service_day = state->snapshot.clock.local.day;

    Motor_Settings_Commit();

    /* ---------------------------------------------------------------------- */
    /*  이번 프레임의 runtime settings copy도 즉시 다시 맞춰 두면             */
    /*  다음 Motor_State_Task()까지 한 프레임 stale 상태로 남지 않는다.        */
    /* ---------------------------------------------------------------------- */
    Motor_State_RefreshSettingsSnapshot();
    Motor_State_RequestRedraw();
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
        s_last_moving = (state->nav.moving != false) ? 1u : 0u;
    }

    if (state->snapshot.clock.local.year >= 2000u)
    {
        if (state->session.today_anchor_year == 0u)
        {
            motor_maintenance_update_today_anchor(state);
        }
        else if ((motor_maintenance_same_day(state) == 0u) &&
                 (state->snapshot.clock.local.hour >= 5u) &&
                 (state->nav.moving == false))
        {
            state->session.today_reset_pending = 1u;
        }

        if ((state->session.today_reset_pending != 0u) &&
            (s_last_moving == 0u) &&
            (state->nav.moving != false))
        {
            motor_maintenance_trip_reset(&state->session.trip_today);
            motor_maintenance_update_today_anchor(state);
            state->session.today_reset_pending = 0u;
        }
    }

    ride_dt_ms = (uint32_t)(now_ms - s_last_tick_ms);
    if (ride_dt_ms >= 1000u)
    {
        uint32_t secs;

        secs = ride_dt_ms / 1000u;
        state->session.ride_seconds += secs;
        state->session.trip_a_stats.ride_seconds += secs;
        state->session.trip_b_stats.ride_seconds += secs;
        state->session.trip_refuel.ride_seconds += secs;
        state->session.trip_today.ride_seconds += secs;
        state->session.trip_ignition.ride_seconds += secs;
        if (state->nav.moving != false)
        {
            state->session.moving_seconds += secs;
            state->maintenance.engine_on_seconds_total += secs;
            state->session.trip_a_stats.moving_seconds += secs;
            state->session.trip_b_stats.moving_seconds += secs;
            state->session.trip_refuel.moving_seconds += secs;
            state->session.trip_today.moving_seconds += secs;
            state->session.trip_ignition.moving_seconds += secs;

            if ((motor_record_state_t)state->record.state == MOTOR_RECORD_STATE_RECORDING)
            {
                state->record_session.moving_seconds += secs;
            }
        }
        else
        {
            state->session.stopped_seconds += secs;

            if ((motor_record_state_t)state->record.state == MOTOR_RECORD_STATE_RECORDING)
            {
                state->record_session.stopped_seconds += secs;
            }
        }

        if ((motor_record_state_t)state->record.state == MOTOR_RECORD_STATE_RECORDING)
        {
            state->record_session.ride_seconds += secs;
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

    s_last_moving = (state->nav.moving != false) ? 1u : 0u;
}
