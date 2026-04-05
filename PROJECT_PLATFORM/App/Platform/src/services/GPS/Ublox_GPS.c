#include "Ublox_GPS.h"
#include "APP_MEMORY_SECTIONS.h"
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

#ifndef UBLOX_GPS_SIGNAL_CONFIG_SETTLE_MS
#define UBLOX_GPS_SIGNAL_CONFIG_SETTLE_MS 600u
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
/*  ?꾨옒 key ID ?ㅼ? u-blox M10 Interface Description??CFG-* ?쒕?            */
/*  洹몃?濡???릿 寃껋씠??                                                       */
/*                                                                            */
/*  肄붾뱶?먯꽌 "臾댁뒯 媛믪쓣 ?대뼡 key???곌퀬 ?덈뒗吏" 瑜?紐낇솗?섍쾶 蹂댁씠寃??섎젮怨?     */
/*  ?꾨? ?곸닔濡??대쫫??遺숈???                                                */
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

static ubx_parser_t          s_parser;                    /* 硫붿씤 猷⑦봽?먯꽌留?留뚯???UBX parser ?곹깭 */
static gps_llh_speed_state_t s_llh_speed_state;           /* LLH delta 湲곕컲 ?뚯깮 ?띾룄 怨꾩궛 ?곹깭 */
static ublox_rx_ring_t       s_rx_ring APP_CCMRAM_BSS;                   /* ISR -> main ?⑤갑??SPSC ring */
static uint32_t              s_driver_start_ms;           /* ?쒕씪?대쾭 ?쒖옉 ?쒓컖 */

static volatile uint8_t      s_rx_parser_resync_request;  /* ISR媛 "parser reset ?대떖?? ?붿껌 */
static volatile uint8_t      s_rx_ring_flush_request;     /* ISR媛 "queued bytes 踰꾨젮?щ씪" ?붿껌 */

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
/*  gSpeed??移⑹씠 怨꾩궛??怨듭떇 ?띾룄怨? speed_llh_* ???곗냽 ?꾩튂 蹂?붾웾?쇰줈        */
/*  怨꾩궛??蹂댁“ ?띾룄??                                                        */
/*                                                                            */
/*  ???곕줈 ?먮깘?                                                              */
/*  - ?붾쾭源낇븷 ??"移⑹씠 留먰븳 ?띾룄" ? "?꾩튂 蹂?붾웾 湲곕컲 ?띾룄" 瑜??섎닠 蹂대㈃       */
/*    ?⑥뵮 ?댄빐媛 ?쎄린 ?뚮Ц?대떎.                                              */
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
        /* 二??⑥쐞 wrap-around 蹂댄샇.
         * ?꾩＜ ?뺢탳??week rollover 泥섎━???꾨땲吏留?
         * ?뺤긽 10 Hz / 20 Hz 釉뚮쭅???섍꼍?먯꽌??異⑸텇?섎떎. */
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

    /* 鍮꾩젙?곸쟻???먭컪? ?꾪꽣 ?댁쟾 媛믪쑝濡??뚮윭以?? */
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
/*  u-blox UBX ?꾨젅?꾩?                                                       */
/*  sync(2) + class(1) + id(1) + len(2) + payload + checksum(2) 援ъ“??      */
/*                                                                            */
/*  ?ш린?쒕뒗 payload留??ｌ쑝硫??섎㉧吏瑜?遺숈뿬???≪떊?쒕떎.                       */
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
    /*  GPS ?ㅼ젙 ?꾨젅???≪떊? 遺????bring-up ?④퀎?먯꽌留??뚮웾 諛쒖깮?쒕떎.       */
    /*  ?곕씪???ш린?쒕뒗 ?꾩쟾 鍮꾨룞湲?TX ?곹깭 癒몄떊源뚯? ?뚯뼱?ㅼ? ?딄퀬,             */
    /*  "?⑥씪 HAL_UART_Transmit + bounded timeout" ?쇰줈 臾띠뼱??                */
    /*  HAL_MAX_DELAY 臾댄븳 ?湲곕? ?쇳븯??履쎌쓣 ?앺븳??                          */
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
    /* MON-VER poll? payload媛 ?녿뒗 poll packet ?대떎. */
    ubx_send(UBX_CLASS_MON, UBX_ID_MON_VER, 0, 0u);
}

/* -------------------------------------------------------------------------- */
/*  Runtime config profile builder                                             */
/*                                                                            */
/*  ?ш린??"?ъ슜???ㅼ젙" ??"?ㅼ젣 u-blox key/value ?명듃" 濡?踰덉뿭?쒕떎.          */
/*                                                                            */
/*  ?ъ슜?먭? ?댄빐?섍린 ?ъ슫 ?ㅼ젙 ?대쫫? ?⑥닚?섍쾶 3媛쒕쭔 ?붾떎.                    */
/*  - GPS ONLY 20Hz                                                            */
/*  - GPS ONLY 10Hz                                                            */
/*  - MULTI CONSTELLATION 10Hz                                                 */
/*                                                                            */
/*  ?꾩썝 紐⑤뱶??                                                               */
/*  - HIGH POWER                                                               */
/*  - POWER SAVE                                                               */
/*  ??媛吏??                                                                 */
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
        /* ?ъ슜?먭? ?먰븯??湲곕낯媛?
         * MULTI CONST 10Hz + HIGH POWER
         *
         * ?ш린?쒕뒗 "GPS + SBAS + Galileo + BeiDou + QZSS + GLONASS"
         * 瑜?紐⑤몢 耳쒓퀬 100 ms 痢≪젙 二쇨린瑜??붿껌?쒕떎.
         *
         * ?ㅼ젣 紐⑤뱢/?덊뀒???꾨젰議곌굔/?뚯썾??議고빀???곕씪
         * 理쒖쥌 ?ㅽ슚 異쒕젰瑜좎? ?щ씪吏????덉?留?
         * 釉뚮쭅???쒕씪?대쾭 ?낆옣?먯꽌??'?붿껌???ㅼ젙' ??紐낇솗??蹂대궡??寃껋씠 以묒슂?섎떎.
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

    /* UART1 ?먯껜瑜?耳쒓퀬, 8N1 / 吏??baud / UBX only濡?留욎텣??
     * NMEA???낅젰/異쒕젰 紐⑤몢 ?덈떎. */
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

    /* ?꾩튂/?꾩꽦 ?뺣낫 異쒕젰 二쇨린 */
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
/*  ISR?먯꽌??parser state瑜?吏곸젒 留뚯?吏 ?딅뒗??                               */
/*                                                                            */
/*  ?댁쑀: parser??硫붿씤 猷⑦봽?먯꽌留??뚮퉬?섎룄濡??ㅺ퀎?섏뼱 ?덇퀬,                   */
/*  ISR?먯꽌 嫄대뱶由щ㈃ main parser? 寃쏀빀?????덈떎.                             */
/*                                                                            */
/*  ?곕씪??ISR??"蹂듦뎄 ?붿껌 ?뚮옒洹?留??몄슦怨?                                 */
/*  ?ㅼ젣 parser reset / ring flush??硫붿씤 猷⑦봽?먯꽌 ?덉쟾?섍쾶 ?섑뻾?쒕떎.          */
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
    /*  鍮좊Ⅸ ?덉텧                      */
    /* ------------------------------ */
    if ((s_rx_parser_resync_request == 0u) &&
        (s_rx_ring_flush_request    == 0u))
    {
        return;
    }

    /* ---------------------------------------------------------------------- */
    /*  recovery flag瑜??쎄퀬, parser/ring???꾩＜ 吏㏐쾶 reset ?쒕떎.              */
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
/*  HAL??1-byte Receive_IT ?촡rm 援ъ“瑜??ъ슜?섏? ?딅뒗??                      */
/*                                                                            */
/*  ???USART2 IRQ?먯꽌 RXNE/ERR瑜?吏곸젒 泥섎━?섍퀬,                              */
/*  諛붿씠?몃뒗 ring?먮쭔 ?ｋ뒗??                                                  */
/*                                                                            */
/*  ?μ젏:                                                                     */
/*  - 諛붿씠?몃쭏??HAL state machine ?촡rm ?ㅻ쾭?ㅻ뱶 ?쒓굅                         */
/*  - ISR ??踰덉뿉 ?щ윭 諛붿씠?몃? 鍮꾩슱 ???덉쓬                                   */
/*  - ORE(Overrun) 諛쒖깮 媛?μ꽦???ш쾶 ??땄                                     */
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
    /*  湲곗〈 direct IRQ 寃쎈줈 ?뺤?      */
    /* ------------------------------ */
    ublox_uart_fast_rx_stop_internal();

    /* ------------------------------ */
    /*  pending recovery request 珥덇린??*/
    /* ------------------------------ */
    s_rx_parser_resync_request = 0u;
    s_rx_ring_flush_request    = 0u;

    /* ------------------------------ */
    /*  HAL ?곹깭 蹂???뺣━             */
    /* ------------------------------ */
    UBLOX_GPS_UART_HANDLE.ErrorCode = HAL_UART_ERROR_NONE;
    UBLOX_GPS_UART_HANDLE.RxState   = HAL_UART_STATE_READY;

    /* ---------------------------------------------------------------------- */
    /*  ?좊났 以묒씤 SR/DR ?뚮옒洹몃? ??踰?鍮꾩썙??                                  */
    /*  ?댁쟾 ?몄뀡??李뚭볼湲곌? 泥??명꽣?쏀듃瑜??댁??쏀엳吏 ?딄쾶 ?쒕떎.                */
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
    /*  direct RX 寃쎈줈 ?꾩쟾???뺤?      */
    /* ------------------------------ */
    ublox_uart_fast_rx_stop_internal();

    /* ------------------------------ */
    /*  HAL UART ?ъ큹湲고솕              */
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

    /* ?먮낯 UBX-NAV-PVT??紐⑤뱺 ?듭떖 ?꾨뱶瑜?媛?ν븳 ??洹몃?濡???댁꽌 ?곷뒗??
     * ?대젃寃??대몢硫??섏쨷??UI瑜?媛덉븘?롮뼱??APP_STATE留?蹂대㈃ ?쒕떎. */
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

    /* "?꾩옱 fix媛 ?좏슚?쒓??" 瑜??щ엺???댄빐?섍린 ?ъ슫 bool ?섎굹濡?留뚮뱺?? */
    fix.valid =
        (fix.fixType != 0u) &&
        (fix.fixOk != false) &&
        (fix.valid_date != 0u) &&
        (fix.valid_time != 0u) &&
        (fix.invalid_llh == 0u);

    /* NAV-PVT??numSV???댁뿉 ?ъ슜???꾩꽦 ?섎떎. NAV-SAT媛 ?ㅼ뼱?ㅻ㈃ 嫄곌린??     * visible/used 移댁슫?몃? ???뺥솗?섍쾶 ?ㅼ떆 怨꾩궛?쒕떎. */
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
/*  parser媛 frame ?섎굹瑜??꾩꽦?섎㈃ ?ш린濡??ㅼ뼱?⑤떎.                             */
/*  ?ш린??class/id瑜?蹂닿퀬 ?뚮쭪? ?몃뱾?щ줈 蹂대궡怨?                             */
/*  紐⑤Ⅴ??硫붿떆吏??unknown bucket???ｋ뒗??                                   */
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
    /*  二쇱쓽                                                                    */
    /*                                                                        */
    /*  raw byte count / last_rx_ms ???댁젣 USART2 direct IRQ 寃쎈줈?먯꽌         */
    /*  媛깆떊?쒕떎.                                                              */
    /*                                                                        */
    /*  ???⑥닔??"ring?먯꽌 爰쇰궦 諛붿씠?몃? UBX parser state machine??癒뱀씠???? */
    /*  ?섎굹留??대떦?쒕떎.                                                       */
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
    /*  ?쒖옉 ?쒓컖怨?APP_STATE reset   */
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
    /*  ?덇굅???대쫫???④꺼 ?먮릺, ?ㅼ젣 援ы쁽? ??direct IRQ RX 寃쎈줈瑜??꾨떎.      */
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

    /* 1) ?대? ?뚯꽌 ?곹깭 + APP_STATE.gps ?곸뿭 珥덇린??*/
    ublox_reset_internal_state();

    /* 2) 遺??吏곹썑 紐⑤뱢???꾩쭅 ?뺤떊 紐?李⑤┛ ?쒓컙 議곌툑 以?? */
    HAL_Delay(100u);

    /* 3) UART ?숆린??     *
     *    ?ㅼ옣 蹂대뱶 / ?댁쟾 ?몄뀡 ?곹깭 / 紐⑤뱢 湲곕낯媛?李⑥씠瑜?媛먯븞??     *    紐?媛吏 ???baud?먯꽌 媛숈? "UART1 = 115200, UBX only" ?ㅼ젙??蹂대궦??
     *
     *    留덉?留됱뿉??host MCU UART??115200?쇰줈 留욎떠 ?볤퀬,
     *    洹??ㅼ쓬遺?곕뒗 怨꾩냽 115200 + UBX only 濡??댁슜?쒕떎.
     */
    ublox_uart_reinit(9600u);
    ublox_send_uart_profile(UBLOX_GPS_HOST_BAUD);
    HAL_Delay(80u);

    ublox_uart_reinit(38400u);
    ublox_send_uart_profile(UBLOX_GPS_HOST_BAUD);
    HAL_Delay(80u);

    ublox_uart_reinit(UBLOX_GPS_HOST_BAUD);

    /* ---------------------------------------------------------------------- */
    /*  ?댁젣遺?곕뒗 HAL 1-byte Receive_IT 媛 ?꾨땲??                             */
    /*  direct RXNE/ERR IRQ 寃쎈줈濡?諛쏅뒗??                                      */
    /* ---------------------------------------------------------------------- */
    ublox_uart_fast_rx_start_internal();

    /* ?뱀떆 ?대? 115200 ?곹깭???寃쎌슦瑜??꾪빐 留덉?留됱쑝濡???踰???蹂대궦?? */
    ublox_send_uart_profile(UBLOX_GPS_HOST_BAUD);
    HAL_Delay(20u);

    /* 4) ?ъ슜???ㅼ젙(APP_STATE.settings.gps)???쎌뼱??     *    GNSS / rate / power / output message 瑜???踰덉뿉 ?곸슜?쒕떎.
     *
     *    二쇱쓽: NAV-SAT ?ㅽ듃由쇱? ?ъ슜???붿껌?濡??좎??쒕떎.
     *
     *    留ㅼ슦 以묒슂
     *    - u-blox M10 臾몄꽌???곕Ⅴ硫?CFG-SIGNAL 怨꾩뿴 蹂寃쎌? GNSS subsystem
     *      restart瑜??좊컻?????덈떎.
     *    - ?곕씪??ACK/?ъ떆??留덉쭊 ?놁씠 諛붾줈 ?ㅼ쓬 poll???좊━硫?
     *      ?ㅼ젙???꾩쭅 ?덉젙?붾릺吏 ?딆? 吏㏃? 援ш컙怨?寃뱀튌 ???덈떎.
     *
     *    ??吏?먯뿉?쒕뒗 signal/power/rate瑜???踰덉뿉 諛붽씀誘濡?
     *    ?꾩냽 MON-VER / VALGET poll ?꾩뿉 異⑸텇??settle time???붾떎. */
    ublox_send_runtime_profile(gps_settings);
    HAL_Delay(UBLOX_GPS_SIGNAL_CONFIG_SETTLE_MS);

    /* 5) 紐⑤뱢 ?뺣낫? "?꾩옱 ?ㅼ젣 ?ㅼ젙" ???쎌뼱?⑤떎. */
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
    /*  ?덇굅??HAL RxCplt callback ?명솚??stub                                 */
    /*                                                                        */
    /*  ??援ъ“?먯꽌??USART2_IRQHandler -> Ublox_GPS_OnUartIrq() 寃쎈줈瑜??대떎. */
    /*  ?곕씪???ш린?쒕뒗 ?쇰????꾨Т 寃껊룄 ?섏? ?딅뒗??                         */
    /* ---------------------------------------------------------------------- */
    (void)huart;
}

void Ublox_GPS_OnUartError(UART_HandleTypeDef *huart)
{
    /* ---------------------------------------------------------------------- */
    /*  ?덇굅??HAL Error callback ?명솚???덉쟾留?                                */
    /*                                                                        */
    /*  ?뺤긽 寃쎈줈??direct USART2 IRQ ?댁?留?                                   */
    /*  ?뱀떆 ?대뵒?좉? HAL error callback 濡??ㅼ뼱?ㅻ뜑?쇰룄                         */
    /*  direct RX 寃쎈줈瑜??ㅼ떆 ?대젮 ?볥뒗??                                      */
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
    /*  ?낅젰 ?ъ씤??/ UART ?앸퀎 諛⑹뼱   */
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
    /*  1) ?곗꽑 UART ?섎뱶?⑥뼱 ?먮윭瑜?泥섎━?쒕떎.                                  */
    /*                                                                        */
    /*  FE/NE/ORE/PE 媛 ??諛붿씠?몃뒗 ?대? ?좊ː?????녿떎怨?蹂닿퀬,                */
    /*  parser/ring recovery瑜??붿껌?쒕떎.                                       */
    /* ---------------------------------------------------------------------- */
    if ((sr & (USART_SR_ORE | USART_SR_FE | USART_SR_NE | USART_SR_PE)) != 0u)
    {
        gps->uart_error_count++;

        if ((sr & USART_SR_ORE) != 0u) { gps->uart_error_ore_count++; }
        if ((sr & USART_SR_FE ) != 0u) { gps->uart_error_fe_count++;  }
        if ((sr & USART_SR_NE ) != 0u) { gps->uart_error_ne_count++;  }
        if ((sr & USART_SR_PE ) != 0u) { gps->uart_error_pe_count++;  }

        ublox_request_rx_recovery_from_isr(1u);

        /* SR -> DR ?쒖쑝濡??쎌뼱???먮윭/?섏떊 ?뚮옒洹몃? ?뺣━?쒕떎. */
        dummy = huart->Instance->SR;
        dummy = huart->Instance->DR;
        (void)dummy;

        sr = huart->Instance->SR;
    }

    /* ---------------------------------------------------------------------- */
    /*  2) RXNE 媛 ???덈뒗 ?숈븞 DR瑜?媛?ν븳 ??湲멸쾶 ?쎌뼱 鍮꾩슫??               */
    /*                                                                        */
    /*  ?닿쾶 HAL 1-byte rearm ?鍮?媛????李⑥씠??                              */
    /*  ?명꽣?쏀듃 ??踰덉뿉 ?щ윭 諛붿씠?몃? ring?쇰줈 ?몄뼱 ?대뒗??                    */
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
            /*  ring??苑?李쇰떎.                                               */
            /*                                                                */
            /*  ??諛붿씠?몃뒗 踰꾨━怨? ?ㅼ쓬 main task?먯꽌 parser/ring??源붾걫?섍쾶  */
            /*  reset ?섎룄濡??붿껌留??④릿??                                   */
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
        /*  burst瑜?鍮꾩슦???꾩쨷 ???섎뱶?⑥뼱 ?먮윭媛 ???섎룄 ?덉쑝誘濡?           */
        /*  猷⑦봽 ?덉뿉?쒕룄 ??踰???寃?ы븳??                                   */
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
    /*  3) raw byte 愿痢≪튂 publish                                             */
    /*                                                                        */
    /*  last_rx_ms / rx_bytes ??parser媛 ?꾨땲??IRQ ?섏떊 ?쒖젏?먯꽌 媛깆떊?쒕떎.   */
    /*  ?대젃寃??섎㈃ parser????媛踰쇱썙吏怨? raw RX ?쒓컖 ?섎??????뺥솗?댁쭊??  */
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
    /*  ISR媛 ?④꺼 ?볦? recovery ?붿껌??癒쇱? 泥섎━?쒕떎.                         */
    /*                                                                        */
    /*  ?대젃寃??섎㈃ ?먮윭/overflow ?댄썑??諛섏? 源⑥쭊 ?붿뿬 諛붿씠?몃?               */
    /*  parser媛 吏덉쭏 ?뚯? ?딄퀬, 鍮좊Ⅴ寃?源⑤걮??sync濡?蹂듦??쒕떎.                */
    /* ---------------------------------------------------------------------- */
    ublox_apply_pending_rx_recovery();

    /* IRQ?먯꽌??諛붿씠?몃? ring buffer?먮쭔 ?ｊ퀬,
     * ?ㅼ젣 UBX ?뚯떛? 硫붿씤 猷⑦봽(Task)?먯꽌 ?섑뻾?쒕떎. */
    ublox_rx_ring_drain(UBLOX_GPS_RX_PARSE_BUDGET);

    /* ?섏떊???딄린硫?留덉?留?媛믪쓣 吏?뚮쾭由ъ????딄퀬,
     * "???곗씠?곌? ?ㅻ옒 ???붾떎" ???섎?濡?fixOk留??대┛?? */
    if ((gps->fix.valid != false) && ((now_ms - gps->fix.last_update_ms) > 3000u))
    {
        gps_fix_basic_t fix = gps->fix;
        fix.fixOk = false;
        app_gps_publish_fix(&fix);
    }

    /* ?ㅼ젙 ?쎄린 ?묐떟?????ㅻ㈃ 3珥?議곗슜??援ш컙 ?ㅼ뿉留?1珥?二쇨린濡??ъ쭏?섑븳??
     * 釉뚮쭅??珥덈컲 UART媛 媛??諛붿걽 ???ъ쭏?섎? 紐곗븘移섏? ?딄쾶 ?댁꽌
     * RX ?ㅽ듃由??덉젙?깆쓣 ?곗꽑 ?뺣낫?쒕떎. */
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
    /*  ?뱀떆 ?몃? 肄붾뱶???덉쇅 寃쎈줈媛 UART RX瑜?爰쇰넧?ㅻ㈃                         */
    /*  硫붿씤 loop媛 ?먮룞?쇰줈 ?ㅼ떆 ?대┛??                                      */
    /* ---------------------------------------------------------------------- */
    if (gps->uart_rx_running == false)
    {
        ublox_uart_fast_rx_start_internal();
    }
}
