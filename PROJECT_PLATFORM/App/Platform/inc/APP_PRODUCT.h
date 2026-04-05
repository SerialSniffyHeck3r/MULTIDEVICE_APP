#ifndef APP_PRODUCT_H
#define APP_PRODUCT_H

/* -------------------------------------------------------------------------- */
/*  APP_PRODUCT fallback configuration                                        */
/*                                                                            */
/*  Purpose                                                                   */
/*  - The product-specific build projects provide their own APP_PRODUCT.h     */
/*    from Product/inc and that header always wins because Product/inc is     */
/*    searched before App/Platform/inc in MOTOR_APP and VARIO_APP builds.     */
/*  - This fallback copy exists only so the shared APP common project can     */
/*    still parse and regenerate CubeMX-owned code without depending on a     */
/*    concrete end-product application folder.                                */
/*                                                                            */
/*  Design rule                                                               */
/*  - The common APP Cube project is now treated as an IOC / peripheral       */
/*    generation authority first, not as a production image that should be    */
/*    flashed to hardware.                                                    */
/*  - Therefore the fallback intentionally selects neither MOTOR nor VARIO.   */
/*  - APP_PRODUCT_INIT.c sees this combination and routes all product entry   */
/*    points to harmless no-op stubs instead of linking a real application.   */
/*                                                                            */
/*  Why this is safer                                                         */
/*  - Building the common project will no longer silently behave like one     */
/*    product or the other.                                                   */
/*  - Real product policy remains owned only by each product project's        */
/*    Product/inc/APP_PRODUCT.h file.                                         */
/* -------------------------------------------------------------------------- */

#define APP_PRODUCT_NAME                      "APP_COMMON_IOC_ONLY"
#define APP_PRODUCT_IS_MOTOR                 0
#define APP_PRODUCT_IS_VARIO                 0
#define APP_PRODUCT_USE_VARIO_UI_ENGINE      0
#define APP_PRODUCT_REQUIRE_POWER_ON_CONFIRM 0

#endif /* APP_PRODUCT_H */
