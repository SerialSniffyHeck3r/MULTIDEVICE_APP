#ifndef APP_PRODUCT_INIT_H
#define APP_PRODUCT_INIT_H

#include <stdbool.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

/* -------------------------------------------------------------------------- */
/*  APP product init / post-init facade                                       */
/*                                                                            */
/*  Design goal                                                               */
/*  - CubeMX-generated main.c should stay the owner of reset, clock setup,    */
/*    and MX_* peripheral initialization.                                     */
/*  - Product divergence must not be scattered across Cube-generated files.   */
/*  - Every product-specific decision is therefore routed through this facade. */
/*                                                                            */
/*  Lifecycle                                                                 */
/*  1) Cube-generated MX_* init finishes.                                     */
/*  2) APP_PRODUCT_PostCubePeripheralInit() applies product-level runtime     */
/*     ownership to already-generated GPIO / peripheral setup if needed.      */
/*  3) Shared platform services and POWER_STATE are initialized.              */
/*  4) APP_PRODUCT_PostPowerStateInit() may refine policy-specific runtime    */
/*     configuration after the common power overlay exists.                   */
/*  5) APP_PRODUCT_PostCommonBringup() is the hand-off point after shared     */
/*     sensors / comms / storage services are alive.                          */
/*  6) APP_PRODUCT_RunAppInit() and APP_PRODUCT_RunAppTask() enter the        */
/*     product app layer without main.c needing to know which app is active.  */
/* -------------------------------------------------------------------------- */

void APP_PRODUCT_OnFrameTickFromISR(void);
void APP_PRODUCT_PostCubePeripheralInit(void);
void APP_PRODUCT_ConfigureBootMode(bool boot_f2_held_for_selftest);
void APP_PRODUCT_PostPowerStateInit(void);
void APP_PRODUCT_InitUiSubsystem(void);
void APP_PRODUCT_DrawEarlyBoot(void);
void APP_PRODUCT_EnterPowerOnConfirmIfRequired(uint32_t now_ms);
void APP_PRODUCT_ServiceBootGate(uint32_t now_ms);
void APP_PRODUCT_PostCommonBringup(void);
void APP_PRODUCT_OnSdWillUnmount(void);
void APP_PRODUCT_RunAppInit(void);
void APP_PRODUCT_RunAppTask(void);
void APP_PRODUCT_OnBoardDebugButtonIrq(uint32_t now_ms);

#ifdef __cplusplus
}
#endif

#endif /* APP_PRODUCT_INIT_H */
