#pragma once

#include <stdint.h>

#ifdef _WIN32
  #define time_c_API __declspec(dllexport)
#else
  #define time_c_API
#endif

#ifndef _MSC_VER
#define __cdecl __attribute__((ms_abi))
#endif

#ifdef __cplusplus
extern "C" {
#endif

time_c_API uint64_t __cdecl ignition_bridge_get_linux_ticks();

#ifdef __cplusplus
}
#endif
