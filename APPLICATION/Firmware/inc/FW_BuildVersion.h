#ifndef FW_BUILD_VERSION_H
#define FW_BUILD_VERSION_H

/* -------------------------------------------------------------------------- */
/*  단일 버전 소스                                                             */
/*                                                                            */
/*  - app build                                                               */
/*  - SYSUPDATE.bin package header                                             */
/*  - F/W Update Mode UI                                                      */
/*  이 세 군데가 같은 값을 보도록 문자열/정수 버전을 함께 둔다.                */
/* -------------------------------------------------------------------------- */

#define FW_BUILD_VERSION_STRING   "0.0.1.0"
#define FW_BUILD_VERSION_U32      (0x00000100u)

#endif /* FW_BUILD_VERSION_H */
