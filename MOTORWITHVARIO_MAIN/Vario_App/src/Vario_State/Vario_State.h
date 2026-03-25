#ifndef VARIO_STATE_LOCAL_REDIRECT_H
#define VARIO_STATE_LOCAL_REDIRECT_H

/* -------------------------------------------------------------------------- */
/*  중요                                                                      */
/*                                                                            */
/*  Vario_State의 canonical public header는                                   */
/*      Vario_App/inc/Vario_State.h                                           */
/*  하나만 유지한다.                                                           */
/*                                                                            */
/*  이 파일은 legacy include 경로 호환용 redirect wrapper다.                  */
/*  src/Vario_State 경로에 남아 있는 구버전 선언이 inc 정본을 가리는          */
/*  include shadow 문제를 막기 위해, 실제 선언 원본은 모두 inc 헤더로         */
/*  강제 위임한다.                                                            */
/* -------------------------------------------------------------------------- */
#include "../../inc/Vario_State.h"

#endif /* VARIO_STATE_LOCAL_REDIRECT_H */
