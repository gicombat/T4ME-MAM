#pragma once

#define _CRT_SECURE_NO_WARNINGS
#define WIN32_LEAN_AND_MEAN

#include <windows.h>
#include <direct.h>
#include <fstream>
#include <string>
#include <vector>
#include <map>

#include <zlib.h>
#pragma comment(lib, "zlib.lib")

#include "resource.h"

#include "Hooking.h"
#include "AddrMap.h"

#include "T4.h"
#include "Utils.h"
#include "AddrMap.h"
#include "Utils\FileIO.h"
#include "io.h"
#define __thread __declspec(thread)
#define HardDebugBreak() MessageBoxA(0, __FUNCTION__, 0, 0);

#include "t4m_version.h"

#define force_gamma_update true

#include "hexrays_defs.h"
#include "game.hpp"
#include "enums.hpp"
#include "structs.hpp"
//#include "include\cod\clientscript\clientscript_public.hpp"
#include <cassert>


using namespace T4::dvar;

// cdecl
template<typename Ret, typename... Args>
inline Ret cdecl_call(uintptr_t addr, Args... args) {
    return reinterpret_cast<Ret(__cdecl*)(Args...)>(addr)(args...);
}

// stdcall
template<typename Ret, typename... Args>
inline Ret stdcall_call(uintptr_t addr, Args... args) {
    return reinterpret_cast<Ret(__stdcall*)(Args...)>(addr)(args...);
}

// fastcall
template<typename Ret, typename... Args>
inline Ret fastcall_call(uintptr_t addr, Args... args) {
    return reinterpret_cast<Ret(__fastcall*)(Args...)>(addr)(args...);
}

// thiscall
template<typename Ret, typename... Args>
inline Ret thiscall_call(uintptr_t addr, Args... args) {
    return reinterpret_cast<Ret(__thiscall*)(Args...)>(addr)(args...);
}

//#include "symbols.hpp"


#define ADC_Prescaler_1 1
#define ADC_Prescaler_2 2

