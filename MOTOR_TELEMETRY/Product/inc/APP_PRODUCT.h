#ifndef APP_PRODUCT_H
#define APP_PRODUCT_H

/* -------------------------------------------------------------------------- */
/*  MOTOR product configuration                                               */
/*                                                                            */
/*  This header is the authoritative product selector for the MOTOR build.    */
/*  The shared APP code intentionally includes APP_PRODUCT.h instead of       */
/*  reaching into Motor_App directly, so product policy stays in one place.   */
/*                                                                            */
/*  Rule of thumb                                                             */
/*  - Pin mux / clock / peripheral instance selection still belongs to the    */
/*    shared APP Cube project and its IOC.                                    */
/*  - Product-level runtime ownership belongs in APP_PRODUCT_INIT.c.          */
/*  - MOTOR app behavior belongs in Motor_App.                                */
/* -------------------------------------------------------------------------- */

#define APP_PRODUCT_NAME                      "MOTOR_APP"
#define APP_PRODUCT_IS_MOTOR                 1
#define APP_PRODUCT_IS_VARIO                 0
#define APP_PRODUCT_USE_VARIO_UI_ENGINE      0
#define APP_PRODUCT_REQUIRE_POWER_ON_CONFIRM 1

#endif /* APP_PRODUCT_H */
