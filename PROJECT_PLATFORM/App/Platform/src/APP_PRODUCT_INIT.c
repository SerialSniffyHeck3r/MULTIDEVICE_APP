#include "APP_PRODUCT_INIT.h"

#include "APP_PRODUCT.h"

#include "POWER_STATE.h"
#include "button.h"

#if APP_PRODUCT_USE_VARIO_UI_ENGINE
#include "ui_engine.h"
#endif

#if APP_PRODUCT_IS_MOTOR
#include "Motor_Task.h"
#include "Motor_Record.h"
#elif APP_PRODUCT_IS_VARIO
#include "Vario_Task.h"
#endif

/* -------------------------------------------------------------------------- */
/*  Shared product divergence point                                           */
/*                                                                            */
/*  Why this file exists                                                      */
/*  - main.c is intentionally kept as a Cube-friendly shell.                  */
/*  - Product-specific behavior must therefore live behind stable hooks that  */
/*    survive IOC regeneration without forcing the developer to diff large    */
/*    main.c changes every time CubeMX touches generated code.                */
/*                                                                            */
/*  What belongs here                                                         */
/*  - Product-dependent boot UI choices                                       */
/*  - Product-dependent post-init ownership decisions                         */
/*  - Product-dependent app entry / task dispatch                             */
/*  - Product-only runtime hooks such as board debug button routing           */
/*                                                                            */
/*  What must NOT belong here                                                 */
/*  - Raw CubeMX-generated pin assignments                                    */
/*  - Product code that should instead live inside Motor_App or Vario_App     */
/*    once the product app has already started                                */
/* -------------------------------------------------------------------------- */

#if !APP_PRODUCT_USE_VARIO_UI_ENGINE
/* -------------------------------------------------------------------------- */
/*  Motor build frame blink ownership                                         */
/*                                                                            */
/*  The shared UI engine owns these blink phases in VARIO builds.             */
/*  MOTOR builds intentionally exclude ui_engine.c, but some shared display   */
/*  helpers still consume the exported blink phase symbols.                   */
/*                                                                            */
/*  Therefore the symbols remain globally visible here for MOTOR builds, and  */
/*  the TIM7 frame interrupt advances them through APP_PRODUCT_OnFrameTick... */
/* -------------------------------------------------------------------------- */
volatile bool SlowToggle2Hz = false;
volatile bool FastToggle5Hz = false;
static uint8_t s_ui_slow_divider = 0u;
static uint8_t s_ui_fast_divider = 0u;
#endif

void APP_PRODUCT_OnFrameTickFromISR(void)
{
#if APP_PRODUCT_USE_VARIO_UI_ENGINE
    /* ---------------------------------------------------------------------- */
    /*  VARIO keeps the shared UI engine as the frame owner.                  */
    /* ---------------------------------------------------------------------- */
    UI_Engine_OnFrameTickFromISR();
#else
    /* ---------------------------------------------------------------------- */
    /*  MOTOR does not link the shared ui_engine.c.                           */
    /*                                                                        */
    /*  We still preserve the historical 5 Hz / 2 Hz blink phases here so    */
    /*  any shared status / test rendering code continues to see stable       */
    /*  visual timing semantics even when the full VARIO UI engine is absent. */
    /* ---------------------------------------------------------------------- */
    s_ui_fast_divider++;
    if (s_ui_fast_divider >= 4u)
    {
        s_ui_fast_divider = 0u;
        FastToggle5Hz = (FastToggle5Hz == false);
    }

    s_ui_slow_divider++;
    if (s_ui_slow_divider >= 10u)
    {
        s_ui_slow_divider = 0u;
        SlowToggle2Hz = (SlowToggle2Hz == false);
    }
#endif
}

void APP_PRODUCT_PostCubePeripheralInit(void)
{
#if APP_PRODUCT_IS_MOTOR
    /* ---------------------------------------------------------------------- */
    /*  MOTOR post-Cube hook                                                  */
    /*                                                                        */
    /*  This is the correct place for future MOTOR-only runtime ownership     */
    /*  changes that must happen after Cube has generated GPIO / peripheral   */
    /*  configuration but before higher services start.                       */
    /*                                                                        */
    /*  Typical future examples                                               */
    /*  - IGN input pin arming                                                */
    /*  - AUX rail FET default-safe output state                              */
    /*  - wake source / low-power policy GPIO preparation                     */
    /*  - disabling or parking hardware blocks that MOTOR will never use      */
    /*                                                                        */
    /*  Intentionally left as a no-op today so the refactor does not change   */
    /*  current runtime behavior.                                             */
    /* ---------------------------------------------------------------------- */
    return;
#else
    /* ---------------------------------------------------------------------- */
    /*  VARIO post-Cube hook                                                  */
    /*                                                                        */
    /*  This is the symmetrical place for future battery-powered VARIO-only   */
    /*  runtime adjustments after Cube init has completed.                    */
    /*                                                                        */
    /*  Typical future examples                                               */
    /*  - soft-power switch pin ownership                                     */
    /*  - battery-gated accessory rail defaults                               */
    /*  - transport lock / power-on countdown related GPIO policy             */
    /*                                                                        */
    /*  Left as a no-op for now to preserve current behavior bit-for-bit.     */
    /* ---------------------------------------------------------------------- */
    return;
#endif
}

void APP_PRODUCT_ConfigureBootMode(bool boot_f2_held_for_selftest)
{
#if APP_PRODUCT_USE_VARIO_UI_ENGINE
    const bool boot_f3_held_for_legacy_ui = (Button_IsPressed(BUTTON_ID_3) != false);

    /* ---------------------------------------------------------------------- */
    /*  Boot mode arbitration for VARIO UI                                    */
    /*                                                                        */
    /*  Priority order                                                        */
    /*  1) F2 hold: maintenance self-test wins first                          */
    /*  2) F3 hold: legacy UI boot profile                                    */
    /*  3) default: normal VARIO UI profile                                   */
    /*                                                                        */
    /*  Keeping this decision in one function avoids spreading raw button     */
    /*  ownership logic through main.c.                                       */
    /* ---------------------------------------------------------------------- */
    if ((boot_f2_held_for_selftest == false) &&
        (boot_f3_held_for_legacy_ui != false))
    {
        UI_Engine_SetBootMode(UI_ENGINE_BOOT_MODE_LEGACY);
    }
    else
    {
        UI_Engine_SetBootMode(UI_ENGINE_BOOT_MODE_VARIO);
    }
#else
    /* ---------------------------------------------------------------------- */
    /*  MOTOR does not use the shared VARIO UI engine boot-mode switch.       */
    /*  The parameter is intentionally accepted so main.c can stay product-   */
    /*  agnostic and call the same hook for both products.                    */
    /* ---------------------------------------------------------------------- */
    (void)boot_f2_held_for_selftest;
#endif
}

void APP_PRODUCT_PostPowerStateInit(void)
{
#if APP_PRODUCT_IS_MOTOR
    /* ---------------------------------------------------------------------- */
    /*  MOTOR power-policy hand-off point                                     */
    /*                                                                        */
    /*  POWER_STATE_Init() has already restored the common runtime overlay.   */
    /*  If MOTOR later needs IGN-driven shutdown timing, STOP/STANDBY entry,  */
    /*  or AUX rail policy binding, extend this hook instead of patching      */
    /*  Cube-generated gpio.c or scattering policy into unrelated modules.    */
    /* ---------------------------------------------------------------------- */
    return;
#else
    /* ---------------------------------------------------------------------- */
    /*  VARIO power-policy hand-off point                                     */
    /*                                                                        */
    /*  This is the matching extension slot for battery / transport-lock /    */
    /*  low-battery auto-off policy decisions once the common overlay exists. */
    /* ---------------------------------------------------------------------- */
    return;
#endif
}

void APP_PRODUCT_InitUiSubsystem(void)
{
#if APP_PRODUCT_USE_VARIO_UI_ENGINE
    /* ---------------------------------------------------------------------- */
    /*  VARIO keeps the shared UI engine as the top-level UI root.            */
    /* ---------------------------------------------------------------------- */
    UI_Engine_Init();
#else
    /* ---------------------------------------------------------------------- */
    /*  MOTOR owns its own UI stack and therefore has nothing to do here.     */
    /* ---------------------------------------------------------------------- */
    return;
#endif
}

void APP_PRODUCT_DrawEarlyBoot(void)
{
#if APP_PRODUCT_IS_MOTOR
    Motor_App_EarlyBootDraw();
#elif APP_PRODUCT_IS_VARIO
    UI_Engine_EarlyBootDraw();
#else
    /* ---------------------------------------------------------------------- */
    /*  Common IOC-only fallback                                              */
    /*                                                                        */
    /*  The shared APP project is intentionally no longer a real product      */
    /*  image. When it is built only for syntax / Cube regeneration safety,   */
    /*  there is no product boot screen to draw here.                         */
    /* ---------------------------------------------------------------------- */
    return;
#endif
}

void APP_PRODUCT_EnterPowerOnConfirmIfRequired(uint32_t now_ms)
{
#if APP_PRODUCT_REQUIRE_POWER_ON_CONFIRM
    POWER_STATE_EnterPowerOnConfirm(now_ms);
#else
    /* ---------------------------------------------------------------------- */
    /*  Some products may not use a manual power-confirm overlay at all.      */
    /*  Accept the common API call anyway so main.c does not need product     */
    /*  branches to skip the gate.                                            */
    /* ---------------------------------------------------------------------- */
    (void)now_ms;
#endif
}

void APP_PRODUCT_ServiceBootGate(uint32_t now_ms)
{
    /* ---------------------------------------------------------------------- */
    /*  The current gate UI is implemented by POWER_STATE.                    */
    /*                                                                        */
    /*  This wrapper deliberately exists so future products can extend the    */
    /*  gate behavior without main.c needing product branches.                */
    /* ---------------------------------------------------------------------- */
    POWER_STATE_TaskBootGate(now_ms);
}

void APP_PRODUCT_PostCommonBringup(void)
{
#if APP_PRODUCT_IS_MOTOR
    /* ---------------------------------------------------------------------- */
    /*  MOTOR post-common-bringup hook                                        */
    /*                                                                        */
    /*  This hook runs after shared GPS / Bluetooth / sensor / audio / SD     */
    /*  services are alive but before the MOTOR app state machine starts.     */
    /*                                                                        */
    /*  Use this location for future MOTOR-only decisions such as             */
    /*  - late disabling of unused services                                   */
    /*  - preparing STOP/STANDBY transition prerequisites                     */
    /*  - arming IGN-dependent delayed shutdown logic                         */
    /*                                                                        */
    /*  Left intentionally empty for now to preserve current behavior.        */
    /* ---------------------------------------------------------------------- */
    return;
#else
    /* ---------------------------------------------------------------------- */
    /*  VARIO post-common-bringup hook                                        */
    /*                                                                        */
    /*  Use this location for future VARIO-only decisions after shared        */
    /*  drivers are running, for example battery-centric service ownership    */
    /*  or transport-lock related behavior that depends on active runtime     */
    /*  services already being present.                                       */
    /* ---------------------------------------------------------------------- */
    return;
#endif
}

void APP_PRODUCT_OnSdWillUnmount(void)
{
#if APP_PRODUCT_IS_MOTOR
    /* ---------------------------------------------------------------------- */
    /*  MOTOR storage teardown bridge                                         */
    /*                                                                        */
    /*  APP_SD owns the physical card lifecycle and therefore decides when    */
    /*  the active volume is about to be torn down. The MOTOR recorder keeps  */
    /*  its own FIL handle and queue state, so it must be told explicitly     */
    /*  before APP_SD unmounts or de-initializes the shared SD peripheral.    */
    /*                                                                        */
    /*  The recorder hook is deliberately routed through this product facade   */
    /*  instead of including MOTOR headers directly inside shared storage      */
    /*  code. That keeps the shared APP layer product-agnostic and preserves   */
    /*  the "common lower layer / separate app layer" boundary we just        */
    /*  established for MOTOR and VARIO.                                      */
    /* ---------------------------------------------------------------------- */
    Motor_Record_OnSdWillUnmount();
#else
    /* ---------------------------------------------------------------------- */
    /*  VARIO currently has no product-specific SD client that keeps an       */
    /*  independent FIL handle beyond the shared platform services.           */
    /* ---------------------------------------------------------------------- */
    return;
#endif
}

void APP_PRODUCT_RunAppInit(void)
{
#if APP_PRODUCT_IS_MOTOR
    Motor_App_Init();
#elif APP_PRODUCT_IS_VARIO
    Vario_App_Init();
#else
    /* ---------------------------------------------------------------------- */
    /*  Common IOC-only fallback                                              */
    /*                                                                        */
    /*  No concrete product application is linked in the shared Cube project. */
    /*  This keeps the common project buildable without dragging MOTOR_APP    */
    /*  or VARIO_APP source trees into the IOC authority project.             */
    /* ---------------------------------------------------------------------- */
    return;
#endif
}

void APP_PRODUCT_RunAppTask(void)
{
#if APP_PRODUCT_IS_MOTOR
    Motor_App_Task();
#elif APP_PRODUCT_IS_VARIO
    Vario_App_Task();
#else
    /* ---------------------------------------------------------------------- */
    /*  Common IOC-only fallback                                              */
    /*                                                                        */
    /*  Intentionally idle. The shared project exists so CubeMX can own the   */
    /*  board topology, not to run a deployable application image.            */
    /* ---------------------------------------------------------------------- */
    return;
#endif
}

void APP_PRODUCT_OnBoardDebugButtonIrq(uint32_t now_ms)
{
#if APP_PRODUCT_USE_VARIO_UI_ENGINE
    UI_Engine_OnBoardDebugButtonIrq(now_ms);
#elif APP_PRODUCT_IS_MOTOR
    Motor_App_OnBoardDebugButtonIrq(now_ms);
#else
    (void)now_ms;
#endif
}
