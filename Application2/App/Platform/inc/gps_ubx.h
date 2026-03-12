/* 레거시 호환용 wrapper.
 * 옛 코드가 gps_ubx.h를 include 하고 있어도,
 * 새 구조(APP_STATE + Ublox_GPS)로 바로 연결되도록 유지한다. */
#ifndef GPS_UBX_H
#define GPS_UBX_H

#include "APP_STATE.h"
#include "Ublox_GPS.h"

#define GPS_UBX_InitAndConfigure()   Init_Ublox_M10()
#define GPS_UBX_StartUartRx()        ((void)0)
#define GPS_UBX_OnByte(b)            Ublox_GPS_OnByte((b))

static inline bool GPS_UBX_GetLatestFix(gps_fix_basic_t *out)
{
    app_gps_state_t snapshot;

    APP_STATE_CopyGpsSnapshot(&snapshot);

    if (out != 0)
    {
        *out = snapshot.fix;
    }

    return snapshot.nav_pvt_valid;
}

#endif /* GPS_UBX_H */
