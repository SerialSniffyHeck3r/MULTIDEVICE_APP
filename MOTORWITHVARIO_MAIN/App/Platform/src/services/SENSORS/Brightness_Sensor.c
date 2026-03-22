#include "Brightness_Sensor.h"

#include <string.h>

/* -------------------------------------------------------------------------- */
/*  외부 ADC handle                                                            */
/* -------------------------------------------------------------------------- */
extern ADC_HandleTypeDef BRIGHTNESS_SENSOR_ADC_HANDLE;

/* -------------------------------------------------------------------------- */
/*  드라이버 내부 runtime 상태                                                 */
/*                                                                            */
/*  기존에는 "한 번 due 되면 평균 샘플 16개를 그 자리에서 전부 읽는" 구조였다.  */
/*  이제는 burst accumulator를 runtime에 두고,                                 */
/*  샘플을 main loop 여러 바퀴에 나눠 1개씩 수집한다.                           */
/* -------------------------------------------------------------------------- */
typedef struct
{
    uint32_t next_burst_start_ms;       /* 다음 평균 burst 시작 시각              */
    uint32_t next_sample_due_ms;        /* burst 내부 다음 샘플 due 시각          */

    uint32_t raw_sum;                   /* 이번 burst의 raw 누적합                */
    uint16_t raw_min;                   /* 이번 burst의 최소 raw                  */
    uint16_t raw_max;                   /* 이번 burst의 최대 raw                  */
    uint16_t samples_collected;         /* 이번 burst에서 모은 샘플 수            */

    uint8_t  burst_active;              /* 현재 평균 burst가 진행 중인가          */
} brightness_sensor_runtime_t;

static brightness_sensor_runtime_t s_brightness_rt;

/* -------------------------------------------------------------------------- */
/*  시간 helper                                                                */
/* -------------------------------------------------------------------------- */
static uint8_t Brightness_Sensor_TimeDue(uint32_t now_ms, uint32_t due_ms)
{
    return ((int32_t)(now_ms - due_ms) >= 0) ? 1u : 0u;
}

/* -------------------------------------------------------------------------- */
/*  APP_STATE slice 초기화 helper                                               */
/* -------------------------------------------------------------------------- */
static void Brightness_Sensor_ResetAppStateSlice(app_brightness_state_t *brightness)
{
    if (brightness == 0)
    {
        return;
    }

    memset(brightness, 0, sizeof(*brightness));

    brightness->initialized = false;
    brightness->valid       = false;
    brightness->last_update_ms = 0u;
    brightness->sample_count   = 0u;

    brightness->raw_last     = 0u;
    brightness->raw_average  = 0u;
    brightness->raw_min      = 0u;
    brightness->raw_max      = 0u;

    brightness->calibrated_counts   = 0u;
    brightness->normalized_permille = 0u;
    brightness->brightness_percent  = 0u;
    brightness->voltage_mv          = 0u;

    /* ---------------------------------------------------------------------- */
    /*  calibration 관련 현재 설정값도 공개 저장소에 함께 넣는다.             */
    /*  UI 페이지에서 사용자가 숫자를 바로 보고 튜닝할 수 있게 하기 위함이다. */
    /* ---------------------------------------------------------------------- */
    brightness->debug.last_hal_status             = (uint8_t)HAL_OK;
    brightness->debug.read_fail_count             = 0u;
    brightness->debug.hal_error_count             = 0u;
    brightness->debug.last_read_start_ms          = 0u;
    brightness->debug.last_read_complete_ms       = 0u;

    brightness->debug.average_count               = (uint16_t)BRIGHTNESS_SENSOR_AVERAGE_COUNT;
    brightness->debug.adc_timeout_ms              = (uint16_t)BRIGHTNESS_SENSOR_ADC_TIMEOUT_MS;
    brightness->debug.period_ms                   = (uint32_t)BRIGHTNESS_SENSOR_PERIOD_MS;
    brightness->debug.sampling_time               = (uint32_t)BRIGHTNESS_SENSOR_ADC_SAMPLING_TIME;
    brightness->debug.calibration_offset_counts   = (int32_t)BRIGHTNESS_SENSOR_CAL_OFFSET_COUNTS;
    brightness->debug.calibration_gain_num        = (uint32_t)BRIGHTNESS_SENSOR_CAL_GAIN_NUM;
    brightness->debug.calibration_gain_den        = (uint32_t)BRIGHTNESS_SENSOR_CAL_GAIN_DEN;
    brightness->debug.calibration_raw_0_percent   = (uint16_t)BRIGHTNESS_SENSOR_CAL_RAW_AT_0_PERCENT;
    brightness->debug.calibration_raw_100_percent = (uint16_t)BRIGHTNESS_SENSOR_CAL_RAW_AT_100_PERCENT;
}

/* -------------------------------------------------------------------------- */
/*  ADC channel runtime 재설정 helper                                          */
/*                                                                            */
/*  CDS divider는 source impedance가 큰 경우가 많다.                           */
/*  CubeMX 기본값인 짧은 sampling time 은 지나치게 공격적일 수 있으므로,        */
/*  burst 시작 시점마다 원하는 sampling time을 다시 적용한다.                 */
/* -------------------------------------------------------------------------- */
static HAL_StatusTypeDef Brightness_Sensor_ConfigureRegularChannel(void)
{
    ADC_ChannelConfTypeDef channel_config;

    memset(&channel_config, 0, sizeof(channel_config));

    channel_config.Channel      = BRIGHTNESS_SENSOR_ADC_CHANNEL;
    channel_config.Rank         = BRIGHTNESS_SENSOR_ADC_RANK;
    channel_config.SamplingTime = BRIGHTNESS_SENSOR_ADC_SAMPLING_TIME;

    return HAL_ADC_ConfigChannel(&BRIGHTNESS_SENSOR_ADC_HANDLE, &channel_config);
}

/* -------------------------------------------------------------------------- */
/*  단일 raw 샘플 읽기 helper                                                  */
/*                                                                            */
/*  이 함수는 channel이 이미 원하는 값으로 맞춰져 있다고 가정한다.             */
/*  즉, burst 시작 시 ConfigureRegularChannel()를 한 번 해 두면                */
/*  개별 샘플 수집에서는 Start/Poll/GetValue/Stop만 수행해서 오버헤드를 줄인다. */
/* -------------------------------------------------------------------------- */
static HAL_StatusTypeDef Brightness_Sensor_ReadSingleRawPrepared(uint16_t *out_raw)
{
    HAL_StatusTypeDef status;
    uint32_t raw_value;

    if (out_raw == 0)
    {
        return HAL_ERROR;
    }

    status = HAL_ADC_Start(&BRIGHTNESS_SENSOR_ADC_HANDLE);
    if (status != HAL_OK)
    {
        return status;
    }

    status = HAL_ADC_PollForConversion(&BRIGHTNESS_SENSOR_ADC_HANDLE,
                                       BRIGHTNESS_SENSOR_ADC_TIMEOUT_MS);
    if (status != HAL_OK)
    {
        (void)HAL_ADC_Stop(&BRIGHTNESS_SENSOR_ADC_HANDLE);
        return status;
    }

    raw_value = HAL_ADC_GetValue(&BRIGHTNESS_SENSOR_ADC_HANDLE);

    status = HAL_ADC_Stop(&BRIGHTNESS_SENSOR_ADC_HANDLE);
    if (status != HAL_OK)
    {
        return status;
    }

    if (raw_value > 4095u)
    {
        raw_value = 4095u;
    }

    *out_raw = (uint16_t)raw_value;
    return HAL_OK;
}

/* -------------------------------------------------------------------------- */
/*  calibration helper                                                         */
/* -------------------------------------------------------------------------- */
static uint16_t Brightness_Sensor_ClampToU12(int32_t value)
{
    if (value < 0)
    {
        return 0u;
    }

    if (value > 4095)
    {
        return 4095u;
    }

    return (uint16_t)value;
}

static uint16_t Brightness_Sensor_ApplyCountCalibration(uint16_t raw_average)
{
    int32_t calibrated;
    int64_t scaled;

    calibrated = (int32_t)raw_average + (int32_t)BRIGHTNESS_SENSOR_CAL_OFFSET_COUNTS;

    /* ---------------------------------------------------------------------- */
    /*  gain 분모가 0이면 divide-by-zero 를 피하기 위해                         */
    /*  raw + offset 결과만 사용한다.                                          */
    /* ---------------------------------------------------------------------- */
    if (BRIGHTNESS_SENSOR_CAL_GAIN_DEN == 0u)
    {
        return Brightness_Sensor_ClampToU12(calibrated);
    }

    scaled = ((int64_t)calibrated * (int64_t)BRIGHTNESS_SENSOR_CAL_GAIN_NUM);
    scaled = scaled / (int64_t)BRIGHTNESS_SENSOR_CAL_GAIN_DEN;

    return Brightness_Sensor_ClampToU12((int32_t)scaled);
}

static uint16_t Brightness_Sensor_NormalizePermille(uint16_t calibrated_counts)
{
    int32_t range;
    int32_t delta;
    int32_t permille;

    if (BRIGHTNESS_SENSOR_CAL_RAW_AT_100_PERCENT > BRIGHTNESS_SENSOR_CAL_RAW_AT_0_PERCENT)
    {
        range = (int32_t)BRIGHTNESS_SENSOR_CAL_RAW_AT_100_PERCENT -
                (int32_t)BRIGHTNESS_SENSOR_CAL_RAW_AT_0_PERCENT;
        delta = (int32_t)calibrated_counts -
                (int32_t)BRIGHTNESS_SENSOR_CAL_RAW_AT_0_PERCENT;
    }
    else if (BRIGHTNESS_SENSOR_CAL_RAW_AT_100_PERCENT < BRIGHTNESS_SENSOR_CAL_RAW_AT_0_PERCENT)
    {
        range = (int32_t)BRIGHTNESS_SENSOR_CAL_RAW_AT_0_PERCENT -
                (int32_t)BRIGHTNESS_SENSOR_CAL_RAW_AT_100_PERCENT;
        delta = (int32_t)BRIGHTNESS_SENSOR_CAL_RAW_AT_0_PERCENT -
                (int32_t)calibrated_counts;
    }
    else
    {
        return 0u;
    }

    permille = (delta * 1000) / range;

    if (permille < 0)
    {
        permille = 0;
    }

    if (permille > 1000)
    {
        permille = 1000;
    }

    return (uint16_t)permille;
}

/* -------------------------------------------------------------------------- */
/*  burst accumulator helper                                                   */
/* -------------------------------------------------------------------------- */
static void Brightness_Sensor_ResetBurstAccumulator(void)
{
    s_brightness_rt.raw_sum = 0u;
    s_brightness_rt.raw_min = 4095u;
    s_brightness_rt.raw_max = 0u;
    s_brightness_rt.samples_collected = 0u;
}

static void Brightness_Sensor_StartBurst(app_brightness_state_t *brightness,
                                         uint32_t now_ms)
{
    HAL_StatusTypeDef status;

    if (brightness == 0)
    {
        return;
    }

    status = Brightness_Sensor_ConfigureRegularChannel();
    brightness->debug.last_hal_status = (uint8_t)status;

    if (status != HAL_OK)
    {
        brightness->debug.read_fail_count++;
        brightness->debug.hal_error_count++;
        s_brightness_rt.burst_active = 0u;
        s_brightness_rt.next_burst_start_ms = now_ms + BRIGHTNESS_SENSOR_PERIOD_MS;
        return;
    }

    Brightness_Sensor_ResetBurstAccumulator();

    s_brightness_rt.burst_active = 1u;
    s_brightness_rt.next_sample_due_ms = now_ms;
    brightness->debug.last_read_start_ms = now_ms;
}

static void Brightness_Sensor_FinishBurst(app_brightness_state_t *brightness,
                                          uint32_t now_ms)
{
    uint16_t raw_average;
    uint16_t calibrated_counts;
    uint16_t normalized_permille;

    if ((brightness == 0) || (s_brightness_rt.samples_collected == 0u))
    {
        return;
    }

    raw_average = (uint16_t)(s_brightness_rt.raw_sum /
                             (uint32_t)s_brightness_rt.samples_collected);

    calibrated_counts = Brightness_Sensor_ApplyCountCalibration(raw_average);
    normalized_permille = Brightness_Sensor_NormalizePermille(calibrated_counts);

    brightness->valid               = true;
    brightness->last_update_ms      = now_ms;
    brightness->sample_count++;

    brightness->raw_average         = raw_average;
    brightness->raw_min             = s_brightness_rt.raw_min;
    brightness->raw_max             = s_brightness_rt.raw_max;
    brightness->calibrated_counts   = calibrated_counts;
    brightness->normalized_permille = normalized_permille;
    brightness->brightness_percent  = (uint8_t)((normalized_permille + 5u) / 10u);
    brightness->voltage_mv          = ((uint32_t)calibrated_counts * BRIGHTNESS_SENSOR_VREF_MV) / 4095u;

    brightness->debug.last_read_complete_ms = now_ms;

    s_brightness_rt.burst_active = 0u;
    s_brightness_rt.next_burst_start_ms = now_ms + BRIGHTNESS_SENSOR_PERIOD_MS;
}

/* -------------------------------------------------------------------------- */
/*  공개 API: init                                                              */
/* -------------------------------------------------------------------------- */
void Brightness_Sensor_Init(void)
{
    app_brightness_state_t *brightness;

    brightness = (app_brightness_state_t *)&g_app_state.brightness;

    memset(&s_brightness_rt, 0, sizeof(s_brightness_rt));
    Brightness_Sensor_ResetAppStateSlice(brightness);

    /* ---------------------------------------------------------------------- */
    /*  첫 burst는 부팅 직후 곧바로 한 번 시도한다.                            */
    /* ---------------------------------------------------------------------- */
    s_brightness_rt.next_burst_start_ms = HAL_GetTick();
    s_brightness_rt.next_sample_due_ms  = HAL_GetTick();
    s_brightness_rt.burst_active        = 0u;
    Brightness_Sensor_ResetBurstAccumulator();

    brightness->initialized = true;
}

/* -------------------------------------------------------------------------- */
/*  공개 API: periodic task                                                    */
/*                                                                            */
/*  이 함수는 main loop 한 번에 "샘플 1개"만 처리하려는 방향으로 동작한다.      */
/*  즉, 16샘플 평균을 유지하면서도 worst-case blocking 시간을 크게 줄인다.      */
/* -------------------------------------------------------------------------- */
void Brightness_Sensor_Task(uint32_t now_ms)
{
    app_brightness_state_t *brightness;
    HAL_StatusTypeDef status;
    uint16_t raw_value;

    brightness = (app_brightness_state_t *)&g_app_state.brightness;

    if (brightness->initialized == false)
    {
        return;
    }

    /* ---------------------------------------------------------------------- */
    /*  burst가 아직 시작되지 않았으면, 주기 due 시점에 새 burst를 연다.        */
    /* ---------------------------------------------------------------------- */
    if (s_brightness_rt.burst_active == 0u)
    {
        if (Brightness_Sensor_TimeDue(now_ms, s_brightness_rt.next_burst_start_ms) == 0u)
        {
            return;
        }

        Brightness_Sensor_StartBurst(brightness, now_ms);

        if (s_brightness_rt.burst_active == 0u)
        {
            return;
        }
    }

    /* ---------------------------------------------------------------------- */
    /*  burst 내부에서도 모든 샘플을 한 번에 읽지 않고                         */
    /*  next_sample_due_ms에 맞춰 1개씩만 수집한다.                             */
    /* ---------------------------------------------------------------------- */
    if (Brightness_Sensor_TimeDue(now_ms, s_brightness_rt.next_sample_due_ms) == 0u)
    {
        return;
    }

    status = Brightness_Sensor_ReadSingleRawPrepared(&raw_value);
    brightness->debug.last_hal_status = (uint8_t)status;

    if (status != HAL_OK)
    {
        brightness->debug.read_fail_count++;
        brightness->debug.hal_error_count++;

        s_brightness_rt.burst_active = 0u;
        s_brightness_rt.next_burst_start_ms = now_ms + BRIGHTNESS_SENSOR_PERIOD_MS;
        return;
    }

    /* ---------------------------------------------------------------------- */
    /*  이번 샘플을 burst accumulator에 반영한다.                               */
    /* ---------------------------------------------------------------------- */
    brightness->raw_last = raw_value;
    s_brightness_rt.raw_sum += raw_value;

    if (raw_value < s_brightness_rt.raw_min)
    {
        s_brightness_rt.raw_min = raw_value;
    }

    if (raw_value > s_brightness_rt.raw_max)
    {
        s_brightness_rt.raw_max = raw_value;
    }

    s_brightness_rt.samples_collected++;

    if (s_brightness_rt.samples_collected >= (uint16_t)BRIGHTNESS_SENSOR_AVERAGE_COUNT)
    {
        Brightness_Sensor_FinishBurst(brightness, now_ms);
        return;
    }

    s_brightness_rt.next_sample_due_ms = now_ms + BRIGHTNESS_SENSOR_INTER_SAMPLE_MS;
}
