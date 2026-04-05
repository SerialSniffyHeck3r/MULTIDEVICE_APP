#ifndef VARIO_GLIDECOMPUTER_H
#define VARIO_GLIDECOMPUTER_H

#include "Vario_State.h"

#ifdef __cplusplus
extern "C" {
#endif

/* -------------------------------------------------------------------------- */
/*  Glide computer helper                                                     */
/*                                                                            */
/*  목적                                                                      */
/*  - Vario_State 가 이미 만든 runtime snapshot 만 받아서                     */
/*    추가 고수준 glide-computer 지표를 계산한다.                              */
/*  - lower-level sensor driver / APP_STATE raw owner를 직접 건드리지 않는다.  */
/*                                                                            */
/*  제공 기능                                                                  */
/*  - circling drift 기반 wind estimate                                       */
/*  - 3점 glider polar 기반 sink / speed-to-fly                              */
/*  - manual McCready / speed guidance                                        */
/*  - home-target 기준 final glide / arrival height                           */
/*  - pitot 없는 조건에서의 estimated TE                                      */
/*                                                                            */
/*  중요한 한계                                                                */
/*  - pitot/static 또는 TE probe가 없으므로 true TE / Netto / exact STF가      */
/*    아니라, GPS + baro + polar 기반의 "estimated" glide computer다.         */
/* -------------------------------------------------------------------------- */
void Vario_GlideComputer_Init(void);
void Vario_GlideComputer_ResetReference(void);
void Vario_GlideComputer_Update(vario_runtime_t *rt,
                                const vario_settings_t *settings,
                                uint32_t now_ms);

#ifdef __cplusplus
}
#endif

#endif /* VARIO_GLIDECOMPUTER_H */
