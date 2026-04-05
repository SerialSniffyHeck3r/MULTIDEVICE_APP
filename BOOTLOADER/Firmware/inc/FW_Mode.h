#ifndef FW_MODE_H
#define FW_MODE_H

#include "main.h"

#ifdef __cplusplus
extern "C" {
#endif

/* -------------------------------------------------------------------------- */
/*  FW_Mode                                                                    */
/*                                                                            */
/*  Reset 직후                                                                 */
/*    - 진입 조건을 판단하고                                                   */
/*    - 필요 없으면 Main App으로 점프하고                                      */
/*    - 필요하면 EnterFWFlashTool()로 들어간다.                               */
/* -------------------------------------------------------------------------- */

void FW_MODE_RunFromReset(void);
void EnterFWFlashTool(void);

#ifdef __cplusplus
}
#endif

#endif /* FW_MODE_H */
