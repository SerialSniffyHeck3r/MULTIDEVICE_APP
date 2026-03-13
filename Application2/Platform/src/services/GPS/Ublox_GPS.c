#include "Ublox_GPS.h"
#include <string.h>
#include <math.h>

#ifndef M_PI
#define M_PI 3.14159265358979323846
#endif

#ifndef ARRAY_SIZE
#define ARRAY_SIZE(x) (sizeof(x) / sizeof((x)[0]))
#endif

#ifndef UBLOX_GPS_TX_TIMEOUT_MS
#define UBLOX_GPS_TX_TIMEOUT_MS 50u
#endif


/* -------------------------------------------------------------------------- */
/*  UART handle                                                                */
/* -------------------------------------------------------------------------- */

extern UART_HandleTypeDef UBLOX_GPS_UART_HANDLE;

/* -------------------------------------------------------------------------- */
/*  UBX protocol constants                                                     */
/* -------------------------------------------------------------------------- */

#define UBX_SYNC_1                          0xB5u
#define UBX_SYNC_2                          0x62u

#define UBX_CLASS_NAV                       0x01u
#define UBX_CLASS_ACK                       0x05u
#define UBX_CLASS_CFG                       0x06u
#define UBX_CLASS_MON                       0x0Au

#define UBX_ID_NAV_PVT                      0x07u
#define UBX_ID_NAV_SAT                      0x35u

#define UBX_ID_ACK_NAK                      0x00u
#define UBX_ID_ACK_ACK                      0x01u

#define UBX_ID_CFG_VALSET                   0x8Au
#define UBX_ID_CFG_VALGET                   0x8Bu

#define UBX_ID_MON_VER                      0x04u

/* -------------------------------------------------------------------------- */
/*  UBX-CFG-VALSET / VALGET layer constants                                    */
/* -------------------------------------------------------------------------- */

#define UBX_VALSET_LAYER_RAM                0x01u
#define UBX_VALGET_LAYER_RAM                0x00u

/* -------------------------------------------------------------------------- */
/*  M10 configuration item key IDs                                             */
/*                                                                            */
/*  아래 key ID 들은 u-blox M10 Interface Description의 CFG-* 표를            */
/*  그대로 옮긴 것이다.                                                       */
/*                                                                            */
/*  코드에서 "무슨 값을 어떤 key에 쓰고 있는지" 를 명확하게 보이게 하려고      */
/*  전부 상수로 이름을 붙였다.                                                */
/* -------------------------------------------------------------------------- */

#define UBX_CFG_KEY_MSGOUT_UBX_NAV_PVT_UART1   0x20910007UL
#define UBX_CFG_KEY_MSGOUT_UBX_NAV_SAT_UART1   0x20910016UL

#define UBX_CFG_KEY_RATE_MEAS                  0x30210001UL

#define UBX_CFG_KEY_PM_OPERATEMODE             0x20d00001UL

#define UBX_CFG_KEY_UART1_BAUDRATE             0x40520001UL
#define UBX_CFG_KEY_UART1_STOPBITS             0x20520002UL
#define UBX_CFG_KEY_UART1_DATABITS             0x20520003UL
#define UBX_CFG_KEY_UART1_PARITY               0x20520004UL
#define UBX_CFG_KEY_UART1_ENABLED              0x10520005UL

#define UBX_CFG_KEY_UART1INPROT_UBX            0x10730001UL
#define UBX_CFG_KEY_UART1INPROT_NMEA           0x10730002UL
#define UBX_CFG_KEY_UART1OUTPROT_UBX           0x10740001UL
#define UBX_CFG_KEY_UART1OUTPROT_NMEA          0x10740002UL

#define UBX_CFG_KEY_SIGNAL_GPS_ENA             0x1031001fUL
#define UBX_CFG_KEY_SIGNAL_GPS_L1CA_ENA        0x10310001UL

#define UBX_CFG_KEY_SIGNAL_SBAS_ENA            0x10310020UL
#define UBX_CFG_KEY_SIGNAL_SBAS_L1CA_ENA       0x10310005UL

#define UBX_CFG_KEY_SIGNAL_GAL_ENA             0x10310021UL
#define UBX_CFG_KEY_SIGNAL_GAL_E1_ENA          0x10310007UL

#define UBX_CFG_KEY_SIGNAL_BDS_ENA             0x10310022UL
#define UBX_CFG_KEY_SIGNAL_BDS_B1I_ENA         0x1031000dUL
#define UBX_CFG_KEY_SIGNAL_BDS_B1C_ENA         0x1031000fUL

#define UBX_CFG_KEY_SIGNAL_QZSS_ENA            0x10310024UL
#define UBX_CFG_KEY_SIGNAL_QZSS_L1CA_ENA       0x10310012UL
#define UBX_CFG_KEY_SIGNAL_QZSS_L1S_ENA        0x10310014UL

#define UBX_CFG_KEY_SIGNAL_GLO_ENA             0x10310025UL
#define UBX_CFG_KEY_SIGNAL_GLO_L1_ENA          0x10310018UL

/* -------------------------------------------------------------------------- */
/*  M10 configuration enum values                                              */
/* -------------------------------------------------------------------------- */

#define UBX_CFG_PM_OPERATEMODE_FULL            0u
#define UBX_CFG_PM_OPERATEMODE_PSMOO           1u
#define UBX_CFG_PM_OPERATEMODE_PSMCT           2u

#define UBX_CFG_UART_STOPBITS_ONE              1u
#define UBX_CFG_UART_DATABITS_EIGHT            0u
#define UBX_CFG_UART_PARITY_NONE               0u

/* -------------------------------------------------------------------------- */
/*  Internal parser state                                                      */
/* -------------------------------------------------------------------------- */

typedef struct
{
    uint8_t  cls;
    uint8_t  id;
    uint16_t len;
    uint16_t index;
    uint8_t  ck_a;
    uint8_t  ck_b;
    uint8_t  state;
    uint8_t  payload[UBLOX_GPS_MAX_PAYLOAD];
} ubx_parser_t;

typedef struct
{
    uint8_t has_prev;
    uint32_t prev_itow_ms;
    int32_t prev_lat;
    int32_t prev_lon;
    float   filt_speed_mps;
} gps_llh_speed_state_t;

typedef struct
{
    uint8_t  buf[256];
    uint16_t len;
} ubx_cfg_builder_t;

typedef struct
{
    volatile uint16_t head;
    volatile uint16_t tail;
    uint8_t data[UBLOX_GPS_RX_RING_SIZE];
} ublox_rx_ring_t;

typedef struct
{
    uint32_t key;
    uint8_t  value_size;
} ubx_cfg_query_key_info_t;

typedef struct
{
    uint16_t meas_rate_ms;
    uint8_t  pm_operate_mode;

    uint8_t gps_ena;
    uint8_t gps_l1ca_ena;

    uint8_t sbas_ena;
    uint8_t sbas_l1ca_ena;

    uint8_t gal_ena;
    uint8_t gal_e1_ena;

    uint8_t bds_ena;
    uint8_t bds_b1i_ena;
    uint8_t bds_b1c_ena;

    uint8_t qzss_ena;
    uint8_t qzss_l1ca_ena;
    uint8_t qzss_l1s_ena;

    uint8_t glo_ena;
    uint8_t glo_l1_ena;
} ublox_requested_profile_t;

enum
{
    UBX_WAIT_SYNC1 = 0,
    UBX_WAIT_SYNC2,
    UBX_WAIT_CLASS,
    UBX_WAIT_ID,
    UBX_WAIT_LEN1,
    UBX_WAIT_LEN2,
    UBX_WAIT_PAYLOAD,
    UBX_WAIT_CK_A,
    UBX_WAIT_CK_B
};

static ubx_parser_t          s_parser;                    /* 메인 루프에서만 만지는 UBX parser 상태 */
static gps_llh_speed_state_t s_llh_speed_state;           /* LLH delta 기반 파생 속도 계산 상태 */
static ublox_rx_ring_t       s_rx_ring;                   /* ISR -> main 단방향 SPSC ring */
static uint32_t              s_driver_start_ms;           /* 드라이버 시작 시각 */

static volatile uint8_t      s_rx_parser_resync_request;  /* ISR가 "parser reset 해달라" 요청 */
static volatile uint8_t      s_rx_ring_flush_request;     /* ISR가 "queued bytes 버려달라" 요청 */

static const ubx_cfg_query_key_info_t s_runtime_cfg_query_items[] =
{
    { UBX_CFG_KEY_UART1_ENABLED,            1u },
    { UBX_CFG_KEY_UART1_BAUDRATE,           4u },
    { UBX_CFG_KEY_UART1INPROT_UBX,          1u },
    { UBX_CFG_KEY_UART1INPROT_NMEA,         1u },
    { UBX_CFG_KEY_UART1OUTPROT_UBX,         1u },
    { UBX_CFG_KEY_UART1OUTPROT_NMEA,        1u },
    { UBX_CFG_KEY_RATE_MEAS,                2u },
    { UBX_CFG_KEY_PM_OPERATEMODE,           1u },
    { UBX_CFG_KEY_SIGNAL_GPS_ENA,           1u },
    { UBX_CFG_KEY_SIGNAL_GPS_L1CA_ENA,      1u },
    { UBX_CFG_KEY_SIGNAL_SBAS_ENA,          1u },
    { UBX_CFG_KEY_SIGNAL_SBAS_L1CA_ENA,     1u },
    { UBX_CFG_KEY_SIGNAL_GAL_ENA,           1u },
    { UBX_CFG_KEY_SIGNAL_GAL_E1_ENA,        1u },
    { UBX_CFG_KEY_SIGNAL_BDS_ENA,           1u },
    { UBX_CFG_KEY_SIGNAL_BDS_B1I_ENA,       1u },
    { UBX_CFG_KEY_SIGNAL_BDS_B1C_ENA,       1u },
    { UBX_CFG_KEY_SIGNAL_QZSS_ENA,          1u },
    { UBX_CFG_KEY_SIGNAL_QZSS_L1CA_ENA,     1u },
    { UBX_CFG_KEY_SIGNAL_QZSS_L1S_ENA,      1u },
    { UBX_CFG_KEY_SIGNAL_GLO_ENA,           1u },
    { UBX_CFG_KEY_SIGNAL_GLO_L1_ENA,        1u },
    { UBX_CFG_KEY_MSGOUT_UBX_NAV_PVT_UART1, 1u },
    { UBX_CFG_KEY_MSGOUT_UBX_NAV_SAT_UART1, 1u }
};

/* -------------------------------------------------------------------------- */
/*  Little-endian helpers                                                      */
/* -------------------------------------------------------------------------- */

static uint16_t ubx_u16_from_le(const uint8_t *p)
{
    return (uint16_t)((uint16_t)p[0] | ((uint16_t)p[1] << 8));
}

static uint32_t ubx_u32_from_le(const uint8_t *p)
{
    return ((uint32_t)p[0]) |
           ((uint32_t)p[1] << 8) |
           ((uint32_t)p[2] << 16) |
           ((uint32_t)p[3] << 24);
}

static void ubx_u16_to_le(uint8_t *p, uint16_t v)
{
    p[0] = (uint8_t)(v & 0xFFu);
    p[1] = (uint8_t)((v >> 8) & 0xFFu);
}

static void ubx_u32_to_le(uint8_t *p, uint32_t v)
{
    p[0] = (uint8_t)(v & 0xFFu);
    p[1] = (uint8_t)((v >> 8) & 0xFFu);
    p[2] = (uint8_t)((v >> 16) & 0xFFu);
    p[3] = (uint8_t)((v >> 24) & 0xFFu);
}

/* -------------------------------------------------------------------------- */
/*  RX ring buffer helpers                                                     */
/* -------------------------------------------------------------------------- */

static void ublox_rx_ring_reset(void)
{
    s_rx_ring.head = 0u;
    s_rx_ring.tail = 0u;
}

static uint16_t ublox_rx_ring_level_from(uint16_t head, uint16_t tail)
{
    if (head >= tail)
    {
        return (uint16_t)(head - tail);
    }

    return (uint16_t)(UBLOX_GPS_RX_RING_SIZE - tail + head);
}

static uint16_t ublox_rx_ring_level(void)
{
    return ublox_rx_ring_level_from(s_rx_ring.head, s_rx_ring.tail);
}

static uint16_t ublox_rx_ring_push_isr(uint8_t byte)
{
    uint16_t head = s_rx_ring.head;
    uint16_t tail = s_rx_ring.tail;
    uint16_t next = (uint16_t)(head + 1u);

    if (next >= UBLOX_GPS_RX_RING_SIZE)
    {
        next = 0u;
    }

    if (next == tail)
    {
        return 0xFFFFu;
    }

    s_rx_ring.data[head] = byte;
    s_rx_ring.head = next;

    return ublox_rx_ring_level_from(next, tail);
}

static uint8_t ublox_rx_ring_pop(uint8_t *byte)
{
    uint16_t tail = s_rx_ring.tail;

    if (byte == 0)
    {
        return 0u;
    }

    if (tail == s_rx_ring.head)
    {
        return 0u;
    }

    *byte = s_rx_ring.data[tail];
    tail = (uint16_t)(tail + 1u);

    if (tail >= UBLOX_GPS_RX_RING_SIZE)
    {
        tail = 0u;
    }

    s_rx_ring.tail = tail;
    return 1u;
}

static void ublox_rx_ring_drain(uint32_t budget)
{
    app_gps_state_t *gps = (app_gps_state_t *)&g_app_state.gps;
    uint32_t processed = 0u;
    uint8_t byte;
    uint16_t level;

    while (processed < budget)
    {
        if (ublox_rx_ring_pop(&byte) == 0u)
        {
            break;
        }

        Ublox_GPS_OnByte(byte);
        processed++;
    }

    level = ublox_rx_ring_level();
    gps->rx_ring_level = level;
    if (level > gps->rx_ring_high_watermark)
    {
        gps->rx_ring_high_watermark = level;
    }
}

/* -------------------------------------------------------------------------- */
/*  Parser helpers                                                             */
/* -------------------------------------------------------------------------- */

static void ubx_parser_reset(ubx_parser_t *p)
{
    memset(p, 0, sizeof(*p));
    p->state = UBX_WAIT_SYNC1;
}

static void ubx_checksum_reset(ubx_parser_t *p)
{
    p->ck_a = 0u;
    p->ck_b = 0u;
}

static void ubx_checksum_update(ubx_parser_t *p, uint8_t b)
{
    p->ck_a = (uint8_t)(p->ck_a + b);
    p->ck_b = (uint8_t)(p->ck_b + p->ck_a);
}

/* -------------------------------------------------------------------------- */
/*  APP_STATE write helpers                                                    */
/* -------------------------------------------------------------------------- */

static void app_gps_copy_frame(app_gps_last_frame_t *dst,
                               uint8_t cls,
                               uint8_t id,
                               const uint8_t *payload,
                               uint16_t len)
{
    uint16_t copy_len;

    if (dst == 0)
    {
        return;
    }

    memset(dst, 0, sizeof(*dst));
    dst->valid      = true;
    dst->msg_class  = cls;
    dst->msg_id     = id;
    dst->payload_len = len;
    dst->tick_ms    = HAL_GetTick();

    copy_len = len;
    if (copy_len > APP_GPS_LAST_RAW_MAX)
    {
        copy_len = APP_GPS_LAST_RAW_MAX;
    }

    if ((payload != 0) && (copy_len > 0u))
    {
        memcpy(dst->payload, payload, copy_len);
    }
}

static void app_gps_publish_fix(const gps_fix_basic_t *fix)
{
    app_gps_state_t *gps = (app_gps_state_t *)&g_app_state.gps;

    if (fix == 0)
    {
        return;
    }

    gps->fix = *fix;
    gps->sat_count_visible = fix->numSV_visible;
    gps->sat_count_used    = fix->numSV_used;
}

/* -------------------------------------------------------------------------- */
/*  LLH based derived speed                                                    */
/*                                                                            */
/*  gSpeed는 칩이 계산한 공식 속도고, speed_llh_* 는 연속 위치 변화량으로        */
/*  계산한 보조 속도다.                                                        */
/*                                                                            */
/*  왜 따로 두냐?                                                              */
/*  - 디버깅할 때 "칩이 말한 속도" 와 "위치 변화량 기반 속도" 를 나눠 보면       */
/*    훨씬 이해가 쉽기 때문이다.                                              */
/* -------------------------------------------------------------------------- */

static float gps_llh_distance_m(int32_t lat1_1e7,
                                int32_t lon1_1e7,
                                int32_t lat2_1e7,
                                int32_t lon2_1e7)
{
    const float earth_r = 6371000.0f;
    const float deg_to_rad = (float)M_PI / 180.0f;
    const float scale = 1.0e-7f * deg_to_rad;

    const float lat1 = (float)lat1_1e7 * scale;
    const float lon1 = (float)lon1_1e7 * scale;
    const float lat2 = (float)lat2_1e7 * scale;
    const float lon2 = (float)lon2_1e7 * scale;

    const float dlat = lat2 - lat1;
    const float dlon = lon2 - lon1;
    const float cos_lat = cosf(0.5f * (lat1 + lat2));
    const float dx = earth_r * dlon * cos_lat;
    const float dy = earth_r * dlat;

    return sqrtf((dx * dx) + (dy * dy));
}

static void gps_update_derived_speed_from_nav_pvt(gps_fix_basic_t *fix,
                                                  const ubx_nav_pvt_t *nav_pvt)
{
    gps_llh_speed_state_t *s = &s_llh_speed_state;
    float dt_s;
    float dist_m;
    float v_mps;
    float tau;
    float alpha;
    uint32_t dt_ms;

    if ((fix == 0) || (nav_pvt == 0))
    {
        return;
    }

    if ((fix->valid == false) || (fix->invalid_llh != 0u))
    {
        return;
    }

    if (s->has_prev == 0u)
    {
        s->has_prev      = 1u;
        s->prev_itow_ms  = nav_pvt->iTOW;
        s->prev_lat      = nav_pvt->lat;
        s->prev_lon      = nav_pvt->lon;
        s->filt_speed_mps = 0.0f;
        fix->speed_llh_mps = 0.0f;
        fix->speed_llh_kmh = 0.0f;
        return;
    }

    if (nav_pvt->iTOW >= s->prev_itow_ms)
    {
        dt_ms = nav_pvt->iTOW - s->prev_itow_ms;
    }
    else
    {
        /* 주 단위 wrap-around 보호.
         * 아주 정교한 week rollover 처리는 아니지만,
         * 정상 10 Hz / 20 Hz 브링업 환경에서는 충분하다. */
        dt_ms = 0u;
    }

    if ((dt_ms == 0u) || (dt_ms > 5000u))
    {
        s->prev_itow_ms   = nav_pvt->iTOW;
        s->prev_lat       = nav_pvt->lat;
        s->prev_lon       = nav_pvt->lon;
        fix->speed_llh_mps = s->filt_speed_mps;
        fix->speed_llh_kmh = s->filt_speed_mps * 3.6f;
        return;
    }

    dt_s = (float)dt_ms * 0.001f;
    dist_m = gps_llh_distance_m(s->prev_lat, s->prev_lon, nav_pvt->lat, nav_pvt->lon);
    v_mps = dist_m / dt_s;

    /* 비정상적인 튐값은 필터 이전 값으로 눌러준다. */
    if (v_mps > 150.0f)
    {
        v_mps = s->filt_speed_mps;
    }

    tau = 0.30f;
    alpha = dt_s / (tau + dt_s);
    s->filt_speed_mps += alpha * (v_mps - s->filt_speed_mps);

    fix->speed_llh_mps = s->filt_speed_mps;
    fix->speed_llh_kmh = s->filt_speed_mps * 3.6f;

    s->prev_itow_ms = nav_pvt->iTOW;
    s->prev_lat     = nav_pvt->lat;
    s->prev_lon     = nav_pvt->lon;
}

/* -------------------------------------------------------------------------- */
/*  Generic UBX TX                                                             */
/*                                                                            */
/*  u-blox UBX 프레임은                                                       */
/*  sync(2) + class(1) + id(1) + len(2) + payload + checksum(2) 구조다.      */
/*                                                                            */
/*  여기서는 payload만 넣으면 나머지를 붙여서 송신한다.                       */
/* -------------------------------------------------------------------------- */

static void ubx_send(uint8_t cls, uint8_t id, const void *payload, uint16_t len)
{
    uint8_t frame[2u + 4u + UBLOX_GPS_MAX_PAYLOAD + 2u];
    uint8_t ck_a = 0u;
    uint8_t ck_b = 0u;
    uint16_t i;
    uint16_t frame_length;
    const uint8_t *ptr;

    if (len > UBLOX_GPS_MAX_PAYLOAD)
    {
        return;
    }

    frame[0] = UBX_SYNC_1;
    frame[1] = UBX_SYNC_2;
    frame[2] = cls;
    frame[3] = id;
    frame[4] = (uint8_t)(len & 0xFFu);
    frame[5] = (uint8_t)((len >> 8) & 0xFFu);

    ck_a = 0u;
    ck_b = 0u;

    for (i = 2u; i < 6u; i++)
    {
        ck_a = (uint8_t)(ck_a + frame[i]);
        ck_b = (uint8_t)(ck_b + ck_a);
    }

    ptr = (const uint8_t *)payload;
    for (i = 0u; i < len; i++)
    {
        frame[6u + i] = ptr[i];
        ck_a = (uint8_t)(ck_a + ptr[i]);
        ck_b = (uint8_t)(ck_b + ck_a);
    }

    frame[6u + len] = ck_a;
    frame[7u + len] = ck_b;
    frame_length = (uint16_t)(8u + len);

    /* ---------------------------------------------------------------------- */
    /*  GPS 설정 프레임 송신은 부팅 시 bring-up 단계에서만 소량 발생한다.       */
    /*  따라서 여기서는 완전 비동기 TX 상태 머신까지 끌어오지 않고,             */
    /*  "단일 HAL_UART_Transmit + bounded timeout" 으로 묶어서                 */
    /*  HAL_MAX_DELAY 무한 대기를 피하는 쪽을 택한다.                          */
    /* ---------------------------------------------------------------------- */
    (void)HAL_UART_Transmit(&UBLOX_GPS_UART_HANDLE,
                            frame,
                            frame_length,
                            UBLOX_GPS_TX_TIMEOUT_MS);
}


/* -------------------------------------------------------------------------- */
/*  VALSET / VALGET payload builders                                           */
/* -------------------------------------------------------------------------- */

static void ubx_cfg_builder_reset(ubx_cfg_builder_t *b)
{
    if (b == 0)
    {
        return;
    }

    memset(b, 0, sizeof(*b));
}

static uint8_t ubx_cfg_builder_push_u1(ubx_cfg_builder_t *b, uint32_t key, uint8_t value)
{
    if ((b == 0) || ((uint16_t)(b->len + 5u) > (uint16_t)sizeof(b->buf)))
    {
        return 0u;
    }

    ubx_u32_to_le(&b->buf[b->len], key);
    b->len += 4u;
    b->buf[b->len++] = value;
    return 1u;
}

static uint8_t ubx_cfg_builder_push_u2(ubx_cfg_builder_t *b, uint32_t key, uint16_t value)
{
    if ((b == 0) || ((uint16_t)(b->len + 6u) > (uint16_t)sizeof(b->buf)))
    {
        return 0u;
    }

    ubx_u32_to_le(&b->buf[b->len], key);
    b->len += 4u;
    ubx_u16_to_le(&b->buf[b->len], value);
    b->len += 2u;
    return 1u;
}

static uint8_t ubx_cfg_builder_push_u4(ubx_cfg_builder_t *b, uint32_t key, uint32_t value)
{
    if ((b == 0) || ((uint16_t)(b->len + 8u) > (uint16_t)sizeof(b->buf)))
    {
        return 0u;
    }

    ubx_u32_to_le(&b->buf[b->len], key);
    b->len += 4u;
    ubx_u32_to_le(&b->buf[b->len], value);
    b->len += 4u;
    return 1u;
}

static void ubx_cfg_send_valset_ram(const ubx_cfg_builder_t *builder)
{
    uint8_t payload[4 + sizeof(builder->buf)];

    if ((builder == 0) || (builder->len == 0u))
    {
        return;
    }

    payload[0] = 0u;                    /* version */
    payload[1] = UBX_VALSET_LAYER_RAM;  /* RAM layer only */
    payload[2] = 0u;
    payload[3] = 0u;

    memcpy(&payload[4], builder->buf, builder->len);
    ubx_send(UBX_CLASS_CFG, UBX_ID_CFG_VALSET, payload, (uint16_t)(4u + builder->len));
}

static void ubx_cfg_send_valget_poll(const ubx_cfg_query_key_info_t *items, uint16_t item_count)
{
    uint8_t payload[4 + (ARRAY_SIZE(s_runtime_cfg_query_items) * 4u)];
    uint16_t i;
    uint16_t offset;
    app_gps_runtime_config_t *cfg = (app_gps_runtime_config_t *)&g_app_state.gps.runtime_cfg;

    if ((items == 0) || (item_count == 0u))
    {
        return;
    }

    payload[0] = 0u;                    /* request version */
    payload[1] = UBX_VALGET_LAYER_RAM;  /* query RAM layer */
    payload[2] = 0u;                    /* position LSB */
    payload[3] = 0u;                    /* position MSB */

    offset = 4u;
    for (i = 0u; i < item_count; i++)
    {
        ubx_u32_to_le(&payload[offset], items[i].key);
        offset += 4u;
    }

    cfg->query_started   = true;
    cfg->query_complete  = false;
    cfg->query_failed    = false;
    cfg->query_attempts++;
    cfg->last_query_tx_ms = HAL_GetTick();

    ubx_send(UBX_CLASS_CFG, UBX_ID_CFG_VALGET, payload, offset);
}

static void ubx_send_mon_ver_poll(void)
{
    /* MON-VER poll은 payload가 없는 poll packet 이다. */
    ubx_send(UBX_CLASS_MON, UBX_ID_MON_VER, 0, 0u);
}

/* -------------------------------------------------------------------------- */
/*  Runtime config profile builder                                             */
/*                                                                            */
/*  여기서 "사용자 설정" 을 "실제 u-blox key/value 세트" 로 번역한다.          */
/*                                                                            */
/*  사용자가 이해하기 쉬운 설정 이름은 단순하게 3개만 둔다.                    */
/*  - GPS ONLY 20Hz                                                            */
/*  - GPS ONLY 10Hz                                                            */
/*  - MULTI CONSTELLATION 10Hz                                                 */
/*                                                                            */
/*  전원 모드는                                                                */
/*  - HIGH POWER                                                               */
/*  - POWER SAVE                                                               */
/*  두 가지다.                                                                 */
/* -------------------------------------------------------------------------- */

static void ublox_build_requested_profile(const app_gps_settings_t *settings,
                                          ublox_requested_profile_t *profile)
{
    if ((settings == 0) || (profile == 0))
    {
        return;
    }

    memset(profile, 0, sizeof(*profile));

    switch (settings->boot_profile)
    {
    case APP_GPS_BOOT_PROFILE_GPS_ONLY_20HZ:
        profile->meas_rate_ms = 50u;
        profile->gps_ena = 1u;
        profile->gps_l1ca_ena = 1u;
        break;

    case APP_GPS_BOOT_PROFILE_GPS_ONLY_10HZ:
        profile->meas_rate_ms = 100u;
        profile->gps_ena = 1u;
        profile->gps_l1ca_ena = 1u;
        break;

    case APP_GPS_BOOT_PROFILE_MULTI_CONST_10HZ:
    default:
        /* 사용자가 원하는 기본값:
         * MULTI CONST 10Hz + HIGH POWER
         *
         * 여기서는 "GPS + SBAS + Galileo + BeiDou + QZSS + GLONASS"
         * 를 모두 켜고 100 ms 측정 주기를 요청한다.
         *
         * 실제 모듈/안테나/전력조건/펌웨어 조합에 따라
         * 최종 실효 출력률은 달라질 수 있지만,
         * 브링업 드라이버 입장에서는 '요청한 설정' 을 명확히 보내는 것이 중요하다.
         */
        profile->meas_rate_ms = 100u;

        profile->gps_ena = 1u;
        profile->gps_l1ca_ena = 1u;

        profile->sbas_ena = 1u;
        profile->sbas_l1ca_ena = 1u;

        profile->gal_ena = 1u;
        profile->gal_e1_ena = 1u;

        profile->bds_ena = 1u;
        profile->bds_b1c_ena = 1u;

        profile->qzss_ena = 1u;
        profile->qzss_l1ca_ena = 1u;
        profile->qzss_l1s_ena = 1u;

        profile->glo_ena = 1u;
        profile->glo_l1_ena = 1u;
        break;
    }

    switch (settings->power_profile)
    {
    case APP_GPS_POWER_PROFILE_POWER_SAVE:
        profile->pm_operate_mode = UBX_CFG_PM_OPERATEMODE_PSMCT;
        break;

    case APP_GPS_POWER_PROFILE_HIGH_POWER:
    default:
        profile->pm_operate_mode = UBX_CFG_PM_OPERATEMODE_FULL;
        break;
    }
}

static void ublox_send_uart_profile(uint32_t baudrate)
{
    ubx_cfg_builder_t b;

    ubx_cfg_builder_reset(&b);

    /* UART1 자체를 켜고, 8N1 / 지정 baud / UBX only로 맞춘다.
     * NMEA는 입력/출력 모두 끈다. */
    (void)ubx_cfg_builder_push_u4(&b, UBX_CFG_KEY_UART1_BAUDRATE, baudrate);
    (void)ubx_cfg_builder_push_u1(&b, UBX_CFG_KEY_UART1_STOPBITS, UBX_CFG_UART_STOPBITS_ONE);
    (void)ubx_cfg_builder_push_u1(&b, UBX_CFG_KEY_UART1_DATABITS, UBX_CFG_UART_DATABITS_EIGHT);
    (void)ubx_cfg_builder_push_u1(&b, UBX_CFG_KEY_UART1_PARITY,   UBX_CFG_UART_PARITY_NONE);
    (void)ubx_cfg_builder_push_u1(&b, UBX_CFG_KEY_UART1_ENABLED,  1u);
    (void)ubx_cfg_builder_push_u1(&b, UBX_CFG_KEY_UART1INPROT_UBX,  1u);
    (void)ubx_cfg_builder_push_u1(&b, UBX_CFG_KEY_UART1INPROT_NMEA, 0u);
    (void)ubx_cfg_builder_push_u1(&b, UBX_CFG_KEY_UART1OUTPROT_UBX, 1u);
    (void)ubx_cfg_builder_push_u1(&b, UBX_CFG_KEY_UART1OUTPROT_NMEA, 0u);

    ubx_cfg_send_valset_ram(&b);
}

static void ublox_send_runtime_profile(const app_gps_settings_t *settings)
{
    ublox_requested_profile_t profile;
    ubx_cfg_builder_t b;

    ublox_build_requested_profile(settings, &profile);
    ubx_cfg_builder_reset(&b);

    /* 위치/위성 정보 출력 주기 */
    (void)ubx_cfg_builder_push_u1(&b, UBX_CFG_KEY_MSGOUT_UBX_NAV_PVT_UART1, 1u);
    (void)ubx_cfg_builder_push_u1(&b, UBX_CFG_KEY_MSGOUT_UBX_NAV_SAT_UART1, 1u);

    /* navigation measurement period */
    (void)ubx_cfg_builder_push_u2(&b, UBX_CFG_KEY_RATE_MEAS, profile.meas_rate_ms);

    /* power mode */
    (void)ubx_cfg_builder_push_u1(&b, UBX_CFG_KEY_PM_OPERATEMODE, profile.pm_operate_mode);

    /* constellation / signal enable */
    (void)ubx_cfg_builder_push_u1(&b, UBX_CFG_KEY_SIGNAL_GPS_ENA,        profile.gps_ena);
    (void)ubx_cfg_builder_push_u1(&b, UBX_CFG_KEY_SIGNAL_GPS_L1CA_ENA,   profile.gps_l1ca_ena);

    (void)ubx_cfg_builder_push_u1(&b, UBX_CFG_KEY_SIGNAL_SBAS_ENA,       profile.sbas_ena);
    (void)ubx_cfg_builder_push_u1(&b, UBX_CFG_KEY_SIGNAL_SBAS_L1CA_ENA,  profile.sbas_l1ca_ena);

    (void)ubx_cfg_builder_push_u1(&b, UBX_CFG_KEY_SIGNAL_GAL_ENA,        profile.gal_ena);
    (void)ubx_cfg_builder_push_u1(&b, UBX_CFG_KEY_SIGNAL_GAL_E1_ENA,     profile.gal_e1_ena);

    (void)ubx_cfg_builder_push_u1(&b, UBX_CFG_KEY_SIGNAL_BDS_ENA,        profile.bds_ena);
    (void)ubx_cfg_builder_push_u1(&b, UBX_CFG_KEY_SIGNAL_BDS_B1I_ENA,    profile.bds_b1i_ena);
    (void)ubx_cfg_builder_push_u1(&b, UBX_CFG_KEY_SIGNAL_BDS_B1C_ENA,    profile.bds_b1c_ena);

    (void)ubx_cfg_builder_push_u1(&b, UBX_CFG_KEY_SIGNAL_QZSS_ENA,       profile.qzss_ena);
    (void)ubx_cfg_builder_push_u1(&b, UBX_CFG_KEY_SIGNAL_QZSS_L1CA_ENA,  profile.qzss_l1ca_ena);
    (void)ubx_cfg_builder_push_u1(&b, UBX_CFG_KEY_SIGNAL_QZSS_L1S_ENA,   profile.qzss_l1s_ena);

    (void)ubx_cfg_builder_push_u1(&b, UBX_CFG_KEY_SIGNAL_GLO_ENA,        profile.glo_ena);
    (void)ubx_cfg_builder_push_u1(&b, UBX_CFG_KEY_SIGNAL_GLO_L1_ENA,     profile.glo_l1_ena);

    ubx_cfg_send_valset_ram(&b);
}

/* -------------------------------------------------------------------------- */
/*  RX recovery request helpers                                                */
/* -------------------------------------------------------------------------- */

/* -------------------------------------------------------------------------- */
/*  ISR에서는 parser state를 직접 만지지 않는다.                               */
/*                                                                            */
/*  이유: parser는 메인 루프에서만 소비하도록 설계되어 있고,                   */
/*  ISR에서 건드리면 main parser와 경합할 수 있다.                             */
/*                                                                            */
/*  따라서 ISR는 "복구 요청 플래그"만 세우고,                                 */
/*  실제 parser reset / ring flush는 메인 루프에서 안전하게 수행한다.          */
/* -------------------------------------------------------------------------- */
static void ublox_request_rx_recovery_from_isr(uint8_t flush_ring)
{
    s_rx_parser_resync_request = 1u;

    if (flush_ring != 0u)
    {
        s_rx_ring_flush_request = 1u;
    }
}

static void ublox_apply_pending_rx_recovery(void)
{
    app_gps_state_t *gps = (app_gps_state_t *)&g_app_state.gps;
    uint8_t need_parser_resync;
    uint8_t need_ring_flush;

    /* ------------------------------ */
    /*  빠른 탈출                      */
    /* ------------------------------ */
    if ((s_rx_parser_resync_request == 0u) &&
        (s_rx_ring_flush_request    == 0u))
    {
        return;
    }

    /* ---------------------------------------------------------------------- */
    /*  recovery flag를 읽고, parser/ring을 아주 짧게 reset 한다.              */
    /* ---------------------------------------------------------------------- */
    __disable_irq();

    need_parser_resync       = s_rx_parser_resync_request;
    need_ring_flush          = s_rx_ring_flush_request;
    s_rx_parser_resync_request = 0u;
    s_rx_ring_flush_request    = 0u;

    if (need_ring_flush != 0u)
    {
        ublox_rx_ring_reset();
        gps->rx_ring_level = 0u;
    }

    if (need_parser_resync != 0u)
    {
        gps->parser_resync_count++;
        ubx_parser_reset(&s_parser);
    }

    __enable_irq();
}

/* -------------------------------------------------------------------------- */
/*  UART control                                                               */
/* -------------------------------------------------------------------------- */

/* -------------------------------------------------------------------------- */
/*  HAL의 1-byte Receive_IT 재arm 구조를 사용하지 않는다.                      */
/*                                                                            */
/*  대신 USART2 IRQ에서 RXNE/ERR를 직접 처리하고,                              */
/*  바이트는 ring에만 넣는다.                                                  */
/*                                                                            */
/*  장점:                                                                     */
/*  - 바이트마다 HAL state machine 재arm 오버헤드 제거                         */
/*  - ISR 한 번에 여러 바이트를 비울 수 있음                                   */
/*  - ORE(Overrun) 발생 가능성을 크게 낮춤                                     */
/* -------------------------------------------------------------------------- */
static void ublox_uart_fast_rx_stop_internal(void)
{
    app_gps_state_t *gps = (app_gps_state_t *)&g_app_state.gps;

    __HAL_UART_DISABLE_IT(&UBLOX_GPS_UART_HANDLE, UART_IT_RXNE);
    __HAL_UART_DISABLE_IT(&UBLOX_GPS_UART_HANDLE, UART_IT_PE);
    __HAL_UART_DISABLE_IT(&UBLOX_GPS_UART_HANDLE, UART_IT_ERR);

    gps->uart_rx_running = false;
}

static void ublox_uart_fast_rx_start_internal(void)
{
    app_gps_state_t *gps = (app_gps_state_t *)&g_app_state.gps;
    volatile uint32_t dummy;

    /* ------------------------------ */
    /*  기존 direct IRQ 경로 정지      */
    /* ------------------------------ */
    ublox_uart_fast_rx_stop_internal();

    /* ------------------------------ */
    /*  pending recovery request 초기화 */
    /* ------------------------------ */
    s_rx_parser_resync_request = 0u;
    s_rx_ring_flush_request    = 0u;

    /* ------------------------------ */
    /*  HAL 상태 변수 정리             */
    /* ------------------------------ */
    UBLOX_GPS_UART_HANDLE.ErrorCode = HAL_UART_ERROR_NONE;
    UBLOX_GPS_UART_HANDLE.RxState   = HAL_UART_STATE_READY;

    /* ---------------------------------------------------------------------- */
    /*  잠복 중인 SR/DR 플래그를 한 번 비워서                                   */
    /*  이전 세션의 찌꺼기가 첫 인터럽트를 어지럽히지 않게 한다.                */
    /* ---------------------------------------------------------------------- */
    dummy = UBLOX_GPS_UART_HANDLE.Instance->SR;
    dummy = UBLOX_GPS_UART_HANDLE.Instance->DR;
    (void)dummy;

    /* ------------------------------ */
    /*  direct RXNE / ERR interrupt ON */
    /* ------------------------------ */
    __HAL_UART_ENABLE_IT(&UBLOX_GPS_UART_HANDLE, UART_IT_PE);
    __HAL_UART_ENABLE_IT(&UBLOX_GPS_UART_HANDLE, UART_IT_ERR);
    __HAL_UART_ENABLE_IT(&UBLOX_GPS_UART_HANDLE, UART_IT_RXNE);

    gps->uart_rx_running = true;
}

static void ublox_uart_reinit(uint32_t baudrate)
{
    app_gps_state_t *gps = (app_gps_state_t *)&g_app_state.gps;

    /* ------------------------------ */
    /*  direct RX 경로 완전히 정지      */
    /* ------------------------------ */
    ublox_uart_fast_rx_stop_internal();

    /* ------------------------------ */
    /*  HAL UART 재초기화              */
    /* ------------------------------ */
    (void)HAL_UART_DeInit(&UBLOX_GPS_UART_HANDLE);
    UBLOX_GPS_UART_HANDLE.Init.BaudRate = baudrate;
    (void)HAL_UART_Init(&UBLOX_GPS_UART_HANDLE);

    gps->uart_rx_running = false;
}
/* -------------------------------------------------------------------------- */
/*  Runtime config VALGET response decode                                      */
/* -------------------------------------------------------------------------- */

static uint8_t ubx_cfg_query_key_size(uint32_t key)
{
    uint16_t i;

    for (i = 0u; i < (uint16_t)ARRAY_SIZE(s_runtime_cfg_query_items); i++)
    {
        if (s_runtime_cfg_query_items[i].key == key)
        {
            return s_runtime_cfg_query_items[i].value_size;
        }
    }

    return 0u;
}

static void ublox_runtime_cfg_set_u1(app_gps_runtime_config_t *cfg, uint32_t key, uint8_t value)
{
    if (cfg == 0)
    {
        return;
    }

    switch (key)
    {
    case UBX_CFG_KEY_UART1_ENABLED:            cfg->uart1_enabled = (value != 0u); break;
    case UBX_CFG_KEY_UART1INPROT_UBX:          cfg->uart1_in_ubx  = (value != 0u); break;
    case UBX_CFG_KEY_UART1INPROT_NMEA:         cfg->uart1_in_nmea = (value != 0u); break;
    case UBX_CFG_KEY_UART1OUTPROT_UBX:         cfg->uart1_out_ubx = (value != 0u); break;
    case UBX_CFG_KEY_UART1OUTPROT_NMEA:        cfg->uart1_out_nmea = (value != 0u); break;
    case UBX_CFG_KEY_PM_OPERATEMODE:           cfg->pm_operate_mode = value; break;

    case UBX_CFG_KEY_SIGNAL_GPS_ENA:           cfg->gps_ena = (value != 0u); break;
    case UBX_CFG_KEY_SIGNAL_GPS_L1CA_ENA:      cfg->gps_l1ca_ena = (value != 0u); break;

    case UBX_CFG_KEY_SIGNAL_SBAS_ENA:          cfg->sbas_ena = (value != 0u); break;
    case UBX_CFG_KEY_SIGNAL_SBAS_L1CA_ENA:     cfg->sbas_l1ca_ena = (value != 0u); break;

    case UBX_CFG_KEY_SIGNAL_GAL_ENA:           cfg->gal_ena = (value != 0u); break;
    case UBX_CFG_KEY_SIGNAL_GAL_E1_ENA:        cfg->gal_e1_ena = (value != 0u); break;

    case UBX_CFG_KEY_SIGNAL_BDS_ENA:           cfg->bds_ena = (value != 0u); break;
    case UBX_CFG_KEY_SIGNAL_BDS_B1I_ENA:       cfg->bds_b1i_ena = (value != 0u); break;
    case UBX_CFG_KEY_SIGNAL_BDS_B1C_ENA:       cfg->bds_b1c_ena = (value != 0u); break;

    case UBX_CFG_KEY_SIGNAL_QZSS_ENA:          cfg->qzss_ena = (value != 0u); break;
    case UBX_CFG_KEY_SIGNAL_QZSS_L1CA_ENA:     cfg->qzss_l1ca_ena = (value != 0u); break;
    case UBX_CFG_KEY_SIGNAL_QZSS_L1S_ENA:      cfg->qzss_l1s_ena = (value != 0u); break;

    case UBX_CFG_KEY_SIGNAL_GLO_ENA:           cfg->glo_ena = (value != 0u); break;
    case UBX_CFG_KEY_SIGNAL_GLO_L1_ENA:        cfg->glo_l1_ena = (value != 0u); break;

    case UBX_CFG_KEY_MSGOUT_UBX_NAV_PVT_UART1: cfg->msgout_nav_pvt_uart1 = value; break;
    case UBX_CFG_KEY_MSGOUT_UBX_NAV_SAT_UART1: cfg->msgout_nav_sat_uart1 = value; break;

    default:
        break;
    }
}

static void ublox_runtime_cfg_set_u2(app_gps_runtime_config_t *cfg, uint32_t key, uint16_t value)
{
    if (cfg == 0)
    {
        return;
    }

    switch (key)
    {
    case UBX_CFG_KEY_RATE_MEAS:
        cfg->meas_rate_ms = value;
        break;

    default:
        break;
    }
}

static void ublox_runtime_cfg_set_u4(app_gps_runtime_config_t *cfg, uint32_t key, uint32_t value)
{
    if (cfg == 0)
    {
        return;
    }

    switch (key)
    {
    case UBX_CFG_KEY_UART1_BAUDRATE:
        cfg->uart1_baudrate = value;
        break;

    default:
        break;
    }
}

static void handle_cfg_valget(const uint8_t *payload, uint16_t len)
{
    app_gps_state_t *gps = (app_gps_state_t *)&g_app_state.gps;
    app_gps_runtime_config_t *cfg = &gps->runtime_cfg;
    uint16_t offset;
    uint32_t key;
    uint8_t value_size;

    if ((payload == 0) || (len < 4u))
    {
        return;
    }

    gps->cfg_valget_valid = true;
    gps->cfg_valget_payload_len = (len > APP_GPS_LAST_RAW_MAX) ? APP_GPS_LAST_RAW_MAX : len;
    memcpy(gps->cfg_valget_payload, payload, gps->cfg_valget_payload_len);

    cfg->query_started   = true;
    cfg->query_complete  = true;
    cfg->query_failed    = false;
    cfg->response_layer  = payload[1];
    cfg->last_query_rx_ms = HAL_GetTick();

    /* payload[0] = version(1 expected)
     * payload[1] = layer
     * payload[2:3] = position
     * payload[4...] = repeated (key + value)
     */
    offset = 4u;
    while ((uint16_t)(offset + 4u) <= len)
    {
        key = ubx_u32_from_le(&payload[offset]);
        offset += 4u;

        value_size = ubx_cfg_query_key_size(key);
        if ((value_size == 0u) || ((uint16_t)(offset + value_size) > len))
        {
            break;
        }

        switch (value_size)
        {
        case 1u:
            ublox_runtime_cfg_set_u1(cfg, key, payload[offset]);
            break;

        case 2u:
            ublox_runtime_cfg_set_u2(cfg, key, ubx_u16_from_le(&payload[offset]));
            break;

        case 4u:
            ublox_runtime_cfg_set_u4(cfg, key, ubx_u32_from_le(&payload[offset]));
            break;

        default:
            break;
        }

        offset = (uint16_t)(offset + value_size);
    }
}

/* -------------------------------------------------------------------------- */
/*  ACK / MON-VER decode                                                       */
/* -------------------------------------------------------------------------- */

static void trim_ascii_field(char *dst, uint16_t dst_size)
{
    int32_t i;

    if ((dst == 0) || (dst_size == 0u))
    {
        return;
    }

    dst[dst_size - 1u] = '\0';

    for (i = (int32_t)dst_size - 2; i >= 0; i--)
    {
        if ((dst[i] == '\0') || (dst[i] == ' '))
        {
            dst[i] = '\0';
        }
        else
        {
            break;
        }
    }
}

static void copy_ascii_field(char *dst, uint16_t dst_size, const uint8_t *src, uint16_t src_size)
{
    uint16_t copy_size;

    if ((dst == 0) || (dst_size == 0u))
    {
        return;
    }

    memset(dst, 0, dst_size);

    if ((src == 0) || (src_size == 0u))
    {
        return;
    }

    copy_size = src_size;
    if (copy_size >= dst_size)
    {
        copy_size = (uint16_t)(dst_size - 1u);
    }

    memcpy(dst, src, copy_size);
    dst[dst_size - 1u] = '\0';
    trim_ascii_field(dst, dst_size);
}

static void handle_ack_ack(const uint8_t *payload, uint16_t len)
{
    app_gps_state_t *gps = (app_gps_state_t *)&g_app_state.gps;
    ubx_ack_payload_t ack;

    if ((payload == 0) || (len < sizeof(ack)))
    {
        return;
    }

    memcpy(&ack, payload, sizeof(ack));

    gps->ack_ack_valid = true;
    gps->ack_ack_count++;
    gps->last_ack_ack_raw = ack;

    gps->ack_ack.valid   = true;
    gps->ack_ack.cls_id  = ack.clsID;
    gps->ack_ack.msg_id  = ack.msgID;
    gps->ack_ack.tick_ms = HAL_GetTick();
}

static void handle_ack_nak(const uint8_t *payload, uint16_t len)
{
    app_gps_state_t *gps = (app_gps_state_t *)&g_app_state.gps;
    ubx_ack_payload_t nak;

    if ((payload == 0) || (len < sizeof(nak)))
    {
        return;
    }

    memcpy(&nak, payload, sizeof(nak));

    gps->ack_nak_valid = true;
    gps->ack_nak_count++;
    gps->last_ack_nak_raw = nak;

    gps->ack_nak.valid   = true;
    gps->ack_nak.cls_id  = nak.clsID;
    gps->ack_nak.msg_id  = nak.msgID;
    gps->ack_nak.tick_ms = HAL_GetTick();
}

static void handle_mon_ver(const uint8_t *payload, uint16_t len)
{
    app_gps_state_t *gps = (app_gps_state_t *)&g_app_state.gps;
    app_gps_mon_ver_t *ver = &gps->mon_ver;
    uint16_t ext_count;
    uint16_t i;

    if ((payload == 0) || (len < 40u))
    {
        return;
    }

    memset(ver, 0, sizeof(*ver));

    copy_ascii_field(ver->sw_version, (uint16_t)sizeof(ver->sw_version), &payload[0], 30u);
    copy_ascii_field(ver->hw_version, (uint16_t)sizeof(ver->hw_version), &payload[30], 10u);

    ext_count = (uint16_t)((len - 40u) / 30u);
    if (ext_count > APP_GPS_MON_VER_EXT_MAX)
    {
        ext_count = APP_GPS_MON_VER_EXT_MAX;
    }

    for (i = 0u; i < ext_count; i++)
    {
        copy_ascii_field(ver->extensions[i],
                         (uint16_t)sizeof(ver->extensions[i]),
                         &payload[40u + (i * 30u)],
                         30u);
    }

    ver->extension_count = (uint8_t)ext_count;
    ver->received_ms     = HAL_GetTick();
    ver->valid           = true;

    gps->mon_ver_valid   = true;
}

/* -------------------------------------------------------------------------- */
/*  NAV-PVT / NAV-SAT decode                                                   */
/* -------------------------------------------------------------------------- */

static void handle_nav_pvt(const uint8_t *payload, uint16_t len)
{
    app_gps_state_t *gps = (app_gps_state_t *)&g_app_state.gps;
    gps_fix_basic_t fix;
    ubx_nav_pvt_t local;

    if ((payload == 0) || (len < sizeof(ubx_nav_pvt_t)))
    {
        return;
    }

    memcpy(&local, payload, sizeof(local));

    gps->nav_pvt = local;
    gps->nav_pvt_valid = true;

    fix = gps->fix;

    /* 원본 UBX-NAV-PVT의 모든 핵심 필드를 가능한 한 그대로 풀어서 적는다.
     * 이렇게 해두면 나중에 UI를 갈아엎어도 APP_STATE만 보면 된다. */
    fix.iTOW_ms   = local.iTOW;
    fix.year      = local.year;
    fix.month     = local.month;
    fix.day       = local.day;
    fix.hour      = local.hour;
    fix.min       = local.min;
    fix.sec       = local.sec;

    fix.raw_valid  = local.valid;
    fix.raw_flags  = local.flags;
    fix.raw_flags2 = local.flags2;
    fix.raw_flags3 = local.flags3;

    fix.tAcc_ns    = local.tAcc;
    fix.nano_ns    = local.nano;

    fix.valid_date      = (uint8_t)((local.valid >> 0) & 0x01u);
    fix.valid_time      = (uint8_t)((local.valid >> 1) & 0x01u);
    fix.fully_resolved  = (uint8_t)((local.valid >> 2) & 0x01u);
    fix.valid_mag       = (uint8_t)((local.valid >> 3) & 0x01u);

    fix.fixType         = local.fixType;
    fix.fixOk           = ((local.flags & 0x01u) != 0u);
    fix.diff_soln       = (uint8_t)((local.flags >> 1) & 0x01u);
    fix.psm_state       = (uint8_t)((local.flags >> 2) & 0x07u);
    fix.head_veh_valid  = (uint8_t)((local.flags >> 5) & 0x01u);
    fix.carr_soln       = (uint8_t)((local.flags >> 6) & 0x03u);

    fix.confirmed_avail = (uint8_t)((local.flags2 >> 5) & 0x01u);
    fix.confirmed_date  = (uint8_t)((local.flags2 >> 6) & 0x01u);
    fix.confirmed_time  = (uint8_t)((local.flags2 >> 7) & 0x01u);

    fix.invalid_llh         = (uint8_t)((local.flags3 >> 0) & 0x01u);
    fix.last_correction_age = (uint8_t)((local.flags3 >> 1) & 0x0Fu);
    fix.auth_time           = (uint8_t)((local.flags3 >> 13) & 0x01u);

    fix.numSV_nav_pvt = local.numSV;

    fix.lon       = local.lon;
    fix.lat       = local.lat;
    fix.height    = local.height;
    fix.hMSL      = local.hMSL;
    fix.hAcc      = local.hAcc;
    fix.vAcc      = local.vAcc;

    fix.velN      = local.velN;
    fix.velE      = local.velE;
    fix.velD      = local.velD;
    fix.gSpeed    = local.gSpeed;
    fix.headMot   = local.headMot;
    fix.headVeh   = local.headVeh;
    fix.sAcc      = local.sAcc;
    fix.headAcc   = local.headAcc;
    fix.pDOP      = local.pDOP;
    fix.magDec    = local.magDec;
    fix.magAcc    = local.magAcc;

    /* "현재 fix가 유효한가?" 를 사람이 이해하기 쉬운 bool 하나로 만든다. */
    fix.valid =
        (fix.fixType != 0u) &&
        (fix.fixOk != false) &&
        (fix.valid_date != 0u) &&
        (fix.valid_time != 0u) &&
        (fix.invalid_llh == 0u);

    /* NAV-PVT의 numSV는 해에 사용된 위성 수다. NAV-SAT가 들어오면 거기서
     * visible/used 카운트를 더 정확하게 다시 계산한다. */
    fix.numSV_used = local.numSV;

    fix.last_update_ms = HAL_GetTick();
    if (fix.valid)
    {
        fix.last_fix_ms = fix.last_update_ms;
    }

    gps_update_derived_speed_from_nav_pvt(&fix, &local);
    app_gps_publish_fix(&fix);
}

static void handle_nav_sat(const uint8_t *payload, uint16_t len)
{
    app_gps_state_t *gps = (app_gps_state_t *)&g_app_state.gps;
    gps_fix_basic_t fix;
    ubx_nav_sat_header_t header;
    uint8_t i;
    uint8_t count;
    uint16_t expected_len;

    if ((payload == 0) || (len < sizeof(ubx_nav_sat_header_t)))
    {
        return;
    }

    memcpy(&header, payload, sizeof(header));
    count = header.numSvs;

    expected_len = (uint16_t)sizeof(ubx_nav_sat_header_t) +
                   (uint16_t)((uint16_t)count * (uint16_t)sizeof(ubx_nav_sat_sv_t));

    if (len < expected_len)
    {
        return;
    }

    if (count > APP_GPS_MAX_SATS)
    {
        count = APP_GPS_MAX_SATS;
    }

    gps->nav_sat_header = header;
    gps->nav_sat_valid  = true;
    gps->nav_sat_count  = count;

    memset(gps->sats, 0, sizeof(gps->sats));
    memset(gps->nav_sat_sv, 0, sizeof(gps->nav_sat_sv));

    for (i = 0u; i < count; i++)
    {
        const uint8_t *sv_ptr = payload +
                                sizeof(ubx_nav_sat_header_t) +
                                ((uint16_t)i * (uint16_t)sizeof(ubx_nav_sat_sv_t));
        ubx_nav_sat_sv_t sv;
        app_gps_sat_t sat;

        memcpy(&sv, sv_ptr, sizeof(sv));
        gps->nav_sat_sv[i] = sv;

        memset(&sat, 0, sizeof(sat));
        sat.gnss_id            = sv.gnssId;
        sat.sv_id              = sv.svId;
        sat.cno_dbhz           = sv.cno;
        sat.elevation_deg      = sv.elev;
        sat.azimuth_deg        = sv.azim;
        sat.pseudorange_res_dm = sv.prRes;

        sat.quality_ind        = (uint8_t)((sv.flags >> 0) & 0x07u);
        sat.used_in_solution   = (uint8_t)((sv.flags >> 3) & 0x01u);
        sat.health             = (uint8_t)((sv.flags >> 4) & 0x03u);
        sat.diff_corr          = (uint8_t)((sv.flags >> 6) & 0x01u);
        sat.smoothed           = (uint8_t)((sv.flags >> 7) & 0x01u);
        sat.orbit_source       = (uint8_t)((sv.flags >> 8) & 0x07u);
        sat.eph_avail          = (uint8_t)((sv.flags >> 11) & 0x01u);
        sat.alm_avail          = (uint8_t)((sv.flags >> 12) & 0x01u);
        sat.ano_avail          = (uint8_t)((sv.flags >> 13) & 0x01u);
        sat.aop_avail          = (uint8_t)((sv.flags >> 14) & 0x01u);
        sat.sbas_corr_used     = (uint8_t)((sv.flags >> 16) & 0x01u);
        sat.rtcm_corr_used     = (uint8_t)((sv.flags >> 17) & 0x01u);
        sat.slas_corr_used     = (uint8_t)((sv.flags >> 18) & 0x01u);
        sat.spartn_corr_used   = (uint8_t)((sv.flags >> 19) & 0x01u);
        sat.pr_corr_used       = (uint8_t)((sv.flags >> 20) & 0x01u);
        sat.cr_corr_used       = (uint8_t)((sv.flags >> 21) & 0x01u);
        sat.do_corr_used       = (uint8_t)((sv.flags >> 22) & 0x01u);
        sat.clas_corr_used     = (uint8_t)((sv.flags >> 23) & 0x01u);
        sat.visible            = 1u;
        sat.flags_raw          = sv.flags;

        gps->sats[i] = sat;
    }

    fix = gps->fix;
    fix.numSV_visible = count;
    fix.numSV_used    = 0u;

    for (i = 0u; i < count; i++)
    {
        if (gps->sats[i].used_in_solution != 0u)
        {
            fix.numSV_used++;
        }
    }

    fix.last_update_ms = HAL_GetTick();
    app_gps_publish_fix(&fix);
}

/* -------------------------------------------------------------------------- */
/*  UBX message dispatcher                                                     */
/*                                                                            */
/*  parser가 frame 하나를 완성하면 여기로 들어온다.                             */
/*  여기서 class/id를 보고 알맞은 핸들러로 보내고,                             */
/*  모르는 메시지는 unknown bucket에 넣는다.                                   */
/* -------------------------------------------------------------------------- */

static void ubx_dispatch(uint8_t cls, uint8_t id, uint16_t len, const uint8_t *payload)
{
    app_gps_state_t *gps = (app_gps_state_t *)&g_app_state.gps;

    gps->frames_ok++;
    gps->last_rx_ms      = HAL_GetTick();
    gps->last_message_ms = gps->last_rx_ms;

    app_gps_copy_frame(&gps->last_frame, cls, id, payload, len);

    if ((cls == UBX_CLASS_NAV) && (id == UBX_ID_NAV_PVT))
    {
        handle_nav_pvt(payload, len);
        return;
    }

    if ((cls == UBX_CLASS_NAV) && (id == UBX_ID_NAV_SAT))
    {
        handle_nav_sat(payload, len);
        return;
    }

    if ((cls == UBX_CLASS_ACK) && (id == UBX_ID_ACK_ACK))
    {
        handle_ack_ack(payload, len);
        return;
    }

    if ((cls == UBX_CLASS_ACK) && (id == UBX_ID_ACK_NAK))
    {
        handle_ack_nak(payload, len);
        return;
    }

    if ((cls == UBX_CLASS_MON) && (id == UBX_ID_MON_VER))
    {
        handle_mon_ver(payload, len);
        return;
    }

    if ((cls == UBX_CLASS_CFG) && (id == UBX_ID_CFG_VALGET))
    {
        handle_cfg_valget(payload, len);
        return;
    }

    gps->unknown_msg_count++;
    app_gps_copy_frame(&gps->last_unknown_frame, cls, id, payload, len);
}

/* -------------------------------------------------------------------------- */
/*  Public byte-by-byte parser                                                 */
/* -------------------------------------------------------------------------- */

void Ublox_GPS_OnByte(uint8_t byte)
{
    ubx_parser_t *p = &s_parser;
    app_gps_state_t *gps = (app_gps_state_t *)&g_app_state.gps;

    /* ---------------------------------------------------------------------- */
    /*  주의                                                                    */
    /*                                                                        */
    /*  raw byte count / last_rx_ms 는 이제 USART2 direct IRQ 경로에서         */
    /*  갱신한다.                                                              */
    /*                                                                        */
    /*  이 함수는 "ring에서 꺼낸 바이트를 UBX parser state machine에 먹이는 일" */
    /*  하나만 담당한다.                                                       */
    /* ---------------------------------------------------------------------- */

    switch (p->state)
    {
    case UBX_WAIT_SYNC1:
        if (byte == UBX_SYNC_1)
        {
            p->state = UBX_WAIT_SYNC2;
        }
        break;

    case UBX_WAIT_SYNC2:
        if (byte == UBX_SYNC_2)
        {
            p->state = UBX_WAIT_CLASS;
            ubx_checksum_reset(p);
        }
        else
        {
            gps->parser_resync_count++;
            p->state = UBX_WAIT_SYNC1;
        }
        break;

    case UBX_WAIT_CLASS:
        p->cls = byte;
        ubx_checksum_update(p, byte);
        p->state = UBX_WAIT_ID;
        break;

    case UBX_WAIT_ID:
        p->id = byte;
        ubx_checksum_update(p, byte);
        p->state = UBX_WAIT_LEN1;
        break;

    case UBX_WAIT_LEN1:
        p->len = byte;
        ubx_checksum_update(p, byte);
        p->state = UBX_WAIT_LEN2;
        break;

    case UBX_WAIT_LEN2:
        p->len |= (uint16_t)((uint16_t)byte << 8);
        ubx_checksum_update(p, byte);

        if (p->len > UBLOX_GPS_MAX_PAYLOAD)
        {
            gps->frames_dropped_oversize++;
            ubx_parser_reset(p);
        }
        else if (p->len == 0u)
        {
            p->state = UBX_WAIT_CK_A;
        }
        else
        {
            p->index = 0u;
            p->state = UBX_WAIT_PAYLOAD;
        }
        break;

    case UBX_WAIT_PAYLOAD:
        p->payload[p->index++] = byte;
        ubx_checksum_update(p, byte);

        if (p->index >= p->len)
        {
            p->state = UBX_WAIT_CK_A;
        }
        break;

    case UBX_WAIT_CK_A:
        if (byte == p->ck_a)
        {
            p->state = UBX_WAIT_CK_B;
        }
        else
        {
            gps->frames_bad_checksum++;
            ubx_parser_reset(p);
        }
        break;

    case UBX_WAIT_CK_B:
        if (byte == p->ck_b)
        {
            ubx_dispatch(p->cls, p->id, p->len, p->payload);
        }
        else
        {
            gps->frames_bad_checksum++;
        }

        ubx_parser_reset(p);
        break;

    default:
        gps->parser_resync_count++;
        ubx_parser_reset(p);
        break;
    }
}

/* -------------------------------------------------------------------------- */
/*  Internal reset / start helpers                                             */
/* -------------------------------------------------------------------------- */

static void ublox_reset_internal_state(void)
{
    /* ------------------------------ */
    /*  parser / filter state reset   */
    /* ------------------------------ */
    ubx_parser_reset(&s_parser);
    memset(&s_llh_speed_state, 0, sizeof(s_llh_speed_state));

    /* ------------------------------ */
    /*  RX ring / recovery flag reset */
    /* ------------------------------ */
    ublox_rx_ring_reset();
    s_rx_parser_resync_request = 0u;
    s_rx_ring_flush_request    = 0u;

    /* ------------------------------ */
    /*  시작 시각과 APP_STATE reset   */
    /* ------------------------------ */
    s_driver_start_ms = HAL_GetTick();
    APP_STATE_ResetGps();
}


/* -------------------------------------------------------------------------- */
/*  Public API                                                                 */
/* -------------------------------------------------------------------------- */
void Ublox_GPS_Init(void)
{
    ublox_reset_internal_state();
}

void Ublox_GPS_StartRxIT(void)
{
    /* ---------------------------------------------------------------------- */
    /*  레거시 이름을 남겨 두되, 실제 구현은 새 direct IRQ RX 경로를 탄다.      */
    /* ---------------------------------------------------------------------- */
    ublox_uart_fast_rx_start_internal();
}

void Ublox_GPS_StartRxIrqDriven(void)
{
    ublox_uart_fast_rx_start_internal();
}

void Init_Ublox_M10(void)
{
    const app_gps_settings_t *gps_settings =
        (const app_gps_settings_t *)&g_app_state.settings.gps;

    /* 1) 내부 파서 상태 + APP_STATE.gps 영역 초기화 */
    ublox_reset_internal_state();

    /* 2) 부팅 직후 모듈이 아직 정신 못 차린 시간 조금 준다. */
    HAL_Delay(100u);

    /* 3) UART 동기화
     *
     *    실장 보드 / 이전 세션 상태 / 모듈 기본값 차이를 감안해
     *    몇 가지 대표 baud에서 같은 "UART1 = 115200, UBX only" 설정을 보낸다.
     *
     *    마지막에는 host MCU UART도 115200으로 맞춰 놓고,
     *    그 다음부터는 계속 115200 + UBX only 로 운용한다.
     */
    ublox_uart_reinit(9600u);
    ublox_send_uart_profile(UBLOX_GPS_HOST_BAUD);
    HAL_Delay(80u);

    ublox_uart_reinit(38400u);
    ublox_send_uart_profile(UBLOX_GPS_HOST_BAUD);
    HAL_Delay(80u);

    ublox_uart_reinit(UBLOX_GPS_HOST_BAUD);

    /* ---------------------------------------------------------------------- */
    /*  이제부터는 HAL 1-byte Receive_IT 가 아니라                              */
    /*  direct RXNE/ERR IRQ 경로로 받는다.                                      */
    /* ---------------------------------------------------------------------- */
    ublox_uart_fast_rx_start_internal();

    /* 혹시 이미 115200 상태였던 경우를 위해 마지막으로 한 번 더 보낸다. */
    ublox_send_uart_profile(UBLOX_GPS_HOST_BAUD);
    HAL_Delay(20u);

    /* 4) 사용자 설정(APP_STATE.settings.gps)을 읽어서
     *    GNSS / rate / power / output message 를 한 번에 적용한다.
     *
     *    주의: NAV-SAT 스트림은 사용자 요청대로 유지한다. */
    ublox_send_runtime_profile(gps_settings);
    HAL_Delay(20u);

    /* 5) 모듈 정보와 "현재 실제 설정" 을 읽어온다. */
    ubx_send_mon_ver_poll();
    ubx_cfg_send_valget_poll(s_runtime_cfg_query_items,
                             (uint16_t)ARRAY_SIZE(s_runtime_cfg_query_items));

    ((app_gps_state_t *)&g_app_state.gps)->configured = true;
}

void Ublox_GPS_ConfigDefault(void)
{
    Init_Ublox_M10();
}

void Ublox_GPS_OnUartRxCplt(UART_HandleTypeDef *huart)
{
    /* ---------------------------------------------------------------------- */
    /*  레거시 HAL RxCplt callback 호환용 stub                                 */
    /*                                                                        */
    /*  새 구조에서는 USART2_IRQHandler -> Ublox_GPS_OnUartIrq() 경로를 쓴다. */
    /*  따라서 여기서는 일부러 아무 것도 하지 않는다.                         */
    /* ---------------------------------------------------------------------- */
    (void)huart;
}

void Ublox_GPS_OnUartError(UART_HandleTypeDef *huart)
{
    /* ---------------------------------------------------------------------- */
    /*  레거시 HAL Error callback 호환용 안전망                                 */
    /*                                                                        */
    /*  정상 경로는 direct USART2 IRQ 이지만,                                   */
    /*  혹시 어디선가 HAL error callback 로 들어오더라도                         */
    /*  direct RX 경로를 다시 살려 놓는다.                                      */
    /* ---------------------------------------------------------------------- */
    if (huart == 0)
    {
        return;
    }

    if (huart->Instance != UBLOX_GPS_UART_HANDLE.Instance)
    {
        return;
    }

    Ublox_GPS_OnUartIrq(huart);
    ublox_uart_fast_rx_start_internal();
}

void Ublox_GPS_OnUartIrq(UART_HandleTypeDef *huart)
{
    app_gps_state_t *gps = (app_gps_state_t *)&g_app_state.gps;
    uint32_t sr;
    uint32_t bytes_this_irq = 0u;
    uint16_t ring_level_snapshot;
    uint32_t now_ms;
    volatile uint32_t dummy;

    /* ------------------------------ */
    /*  입력 포인터 / UART 식별 방어   */
    /* ------------------------------ */
    if (huart == 0)
    {
        return;
    }

    if (huart->Instance != UBLOX_GPS_UART_HANDLE.Instance)
    {
        return;
    }

    ring_level_snapshot = gps->rx_ring_level;
    sr = huart->Instance->SR;

    /* ---------------------------------------------------------------------- */
    /*  1) 우선 UART 하드웨어 에러를 처리한다.                                  */
    /*                                                                        */
    /*  FE/NE/ORE/PE 가 뜬 바이트는 이미 신뢰할 수 없다고 보고,                */
    /*  parser/ring recovery를 요청한다.                                       */
    /* ---------------------------------------------------------------------- */
    if ((sr & (USART_SR_ORE | USART_SR_FE | USART_SR_NE | USART_SR_PE)) != 0u)
    {
        gps->uart_error_count++;

        if ((sr & USART_SR_ORE) != 0u) { gps->uart_error_ore_count++; }
        if ((sr & USART_SR_FE ) != 0u) { gps->uart_error_fe_count++;  }
        if ((sr & USART_SR_NE ) != 0u) { gps->uart_error_ne_count++;  }
        if ((sr & USART_SR_PE ) != 0u) { gps->uart_error_pe_count++;  }

        ublox_request_rx_recovery_from_isr(1u);

        /* SR -> DR 순으로 읽어서 에러/수신 플래그를 정리한다. */
        dummy = huart->Instance->SR;
        dummy = huart->Instance->DR;
        (void)dummy;

        sr = huart->Instance->SR;
    }

    /* ---------------------------------------------------------------------- */
    /*  2) RXNE 가 서 있는 동안 DR를 가능한 한 길게 읽어 비운다.               */
    /*                                                                        */
    /*  이게 HAL 1-byte rearm 대비 가장 큰 차이다.                              */
    /*  인터럽트 한 번에 여러 바이트를 ring으로 쓸어 담는다.                    */
    /* ---------------------------------------------------------------------- */
    while ((sr & USART_SR_RXNE) != 0u)
    {
        uint8_t byte;
        uint16_t new_ring_level;

        byte = (uint8_t)(huart->Instance->DR & 0xFFu);
        bytes_this_irq++;

        new_ring_level = ublox_rx_ring_push_isr(byte);

        if (new_ring_level == 0xFFFFu)
        {
            /* -------------------------------------------------------------- */
            /*  ring이 꽉 찼다.                                               */
            /*                                                                */
            /*  새 바이트는 버리고, 다음 main task에서 parser/ring을 깔끔하게  */
            /*  reset 하도록 요청만 남긴다.                                   */
            /* -------------------------------------------------------------- */
            gps->uart_ring_overflow_count++;
            ublox_request_rx_recovery_from_isr(1u);
        }
        else
        {
            ring_level_snapshot = new_ring_level;
        }

        sr = huart->Instance->SR;

        /* ------------------------------------------------------------------ */
        /*  burst를 비우는 도중 새 하드웨어 에러가 뜰 수도 있으므로            */
        /*  루프 안에서도 한 번 더 검사한다.                                   */
        /* ------------------------------------------------------------------ */
        if ((sr & (USART_SR_ORE | USART_SR_FE | USART_SR_NE | USART_SR_PE)) != 0u)
        {
            gps->uart_error_count++;

            if ((sr & USART_SR_ORE) != 0u) { gps->uart_error_ore_count++; }
            if ((sr & USART_SR_FE ) != 0u) { gps->uart_error_fe_count++;  }
            if ((sr & USART_SR_NE ) != 0u) { gps->uart_error_ne_count++;  }
            if ((sr & USART_SR_PE ) != 0u) { gps->uart_error_pe_count++;  }

            ublox_request_rx_recovery_from_isr(1u);

            dummy = huart->Instance->SR;
            dummy = huart->Instance->DR;
            (void)dummy;

            sr = huart->Instance->SR;
        }
    }

    /* ---------------------------------------------------------------------- */
    /*  3) raw byte 관측치 publish                                             */
    /*                                                                        */
    /*  last_rx_ms / rx_bytes 는 parser가 아니라 IRQ 수신 시점에서 갱신한다.   */
    /*  이렇게 하면 parser는 더 가벼워지고, raw RX 시각 의미도 더 정확해진다.  */
    /* ---------------------------------------------------------------------- */
    if (bytes_this_irq != 0u)
    {
        now_ms = HAL_GetTick();

        gps->uart_irq_bytes += bytes_this_irq;
        gps->rx_bytes       += bytes_this_irq;
        gps->last_rx_ms      = now_ms;
        gps->rx_ring_level   = ring_level_snapshot;

        if (ring_level_snapshot > gps->rx_ring_high_watermark)
        {
            gps->rx_ring_high_watermark = ring_level_snapshot;
        }
    }

    gps->uart_rx_running = true;
}

void Ublox_GPS_Task(uint32_t now_ms)
{
    app_gps_state_t *gps = (app_gps_state_t *)&g_app_state.gps;
    app_gps_runtime_config_t *cfg = &gps->runtime_cfg;

    /* ---------------------------------------------------------------------- */
    /*  ISR가 남겨 놓은 recovery 요청을 먼저 처리한다.                         */
    /*                                                                        */
    /*  이렇게 하면 에러/overflow 이후의 반쯤 깨진 잔여 바이트를               */
    /*  parser가 질질 끌지 않고, 빠르게 깨끗한 sync로 복귀한다.                */
    /* ---------------------------------------------------------------------- */
    ublox_apply_pending_rx_recovery();

    /* IRQ에서는 바이트를 ring buffer에만 넣고,
     * 실제 UBX 파싱은 메인 루프(Task)에서 수행한다. */
    ublox_rx_ring_drain(UBLOX_GPS_RX_PARSE_BUDGET);

    /* 수신이 끊기면 마지막 값을 지워버리지는 않고,
     * "새 데이터가 오래 안 왔다" 는 의미로 fixOk만 내린다. */
    if ((gps->fix.valid != false) && ((now_ms - gps->fix.last_update_ms) > 3000u))
    {
        gps_fix_basic_t fix = gps->fix;
        fix.fixOk = false;
        app_gps_publish_fix(&fix);
    }

    /* 설정 읽기 응답이 안 오면 3초 조용한 구간 뒤에만 1초 주기로 재질의한다.
     * 브링업 초반 UART가 가장 바쁠 때 재질의를 몰아치지 않게 해서
     * RX 스트림 안정성을 우선 확보한다. */
    if ((cfg->query_complete == false) && (cfg->query_failed == false))
    {
        if ((now_ms - s_driver_start_ms) >= UBLOX_GPS_QUERY_START_DELAY_MS)
        {
            if ((now_ms - cfg->last_query_tx_ms) >= UBLOX_GPS_QUERY_RETRY_MS)
            {
                if (cfg->query_attempts < (uint8_t)(1u + UBLOX_GPS_QUERY_MAX_RETRY))
                {
                    ubx_cfg_send_valget_poll(s_runtime_cfg_query_items,
                                             (uint16_t)ARRAY_SIZE(s_runtime_cfg_query_items));
                }
                else
                {
                    cfg->query_failed = true;
                }
            }
        }
    }

    /* ---------------------------------------------------------------------- */
    /*  혹시 외부 코드나 예외 경로가 UART RX를 꺼놨다면                         */
    /*  메인 loop가 자동으로 다시 살린다.                                      */
    /* ---------------------------------------------------------------------- */
    if (gps->uart_rx_running == false)
    {
        ublox_uart_fast_rx_start_internal();
    }
}
