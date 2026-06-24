#pragma once
//
// sszfunc_defs.hpp
//
// Global SSZ function pointer definitions.
// Include this header file in any translation unit that needs to define
// or reference sszrefnewfunc and sszrefdeletefunc.
//

#include <stdint.h>

// Forward declare the calling convention macro in case it's not defined
#ifndef SSZ_STDCALL
#define SSZ_STDCALL __stdcall
#endif

// Define the memory management function pointers
// These must be defined in exactly one translation unit (usually main.cpp)
extern void* (SSZ_STDCALL *sszrefnewfunc)(intptr_t);
extern void (SSZ_STDCALL *sszrefdeletefunc)(void*);
