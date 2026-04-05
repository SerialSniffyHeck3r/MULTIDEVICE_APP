#ifndef APP_PRODUCT_H
#define APP_PRODUCT_H

/* -------------------------------------------------------------------------- */
/*  VARIO product configuration                                               */
/*                                                                            */
/*  This header is the authoritative product selector for the VARIO build.    */
/*  The shared APP code intentionally includes APP_PRODUCT.h instead of       */
/*  reaching into Vario_App directly, so product policy stays in one place.   */
/*                                                                            */
/*  Rule of thumb                                                             */
/*  - Pin mux / clock / peripheral instance selection still belongs to the    */
/*    shared APP Cube project and its IOC.                                    */
/*  - Product-level runtime ownership belongs in APP_PRODUCT_INIT.c.          */
/*  - VARIO app behavior belongs in Vario_App.                                */
/* -------------------------------------------------------------------------- */

#define APP_PRODUCT_NAME                      "VARIO_APP"
#define APP_PRODUCT_IS_MOTOR                 0
#define APP_PRODUCT_IS_VARIO                 1
#define APP_PRODUCT_USE_VARIO_UI_ENGINE      1
#define APP_PRODUCT_REQUIRE_POWER_ON_CONFIRM 1

#endif /* APP_PRODUCT_H */
