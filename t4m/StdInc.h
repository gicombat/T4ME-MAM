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

#include "T4.h"
#include "Utils.h"
#include "Utils\FileIO.h"
#include "io.h"
#define __thread __declspec(thread)
#define HardDebugBreak() MessageBoxA(0, __FUNCTION__, 0, 0);

// first two number => correspond to T4M Enhanced dll version (current r49 on github)
// 0 => separator
// XX => config file revision, currently b 3
#define INTERNAL_VERSION_NUMBER 4903
#define STRINGIFY(x) #x
#define TO_STRING(x) STRINGIFY(x)

#define IS_BETA
#define BETA "b3"
#define VERSION "1.0"
#define FS_BASEGAME "data"
#define DATE_T4 __DATE__
#define TIME __TIME__
#define CONSOLEVERSION_BETA_STR "T4Me-MAM " VERSION "" BETA "> "
#define CONSOLEVERSION_STR "T4Me-MAM " VERSION "> "
#define CONSOLEVERSION_VULKAN_STR "T4Me-MAM " VERSION "> "
#define CONSOLEVERSION_BETA_VULKAN_STR "T4Me-MAM " VERSION "" BETA "> "
#define VERSION_BETA_STR "T4Me-MAM-SP " VERSION "" BETA " (built " DATE_T4 " " TIME " by gicombat)"
#define VERSION_STR "T4Me-MAM-SP " VERSION " (built " DATE_T4 " " TIME " by gicombat)"
#define VERSION_BETA_VULKAN_STR "T4Me-MAM-SP " VERSION "" BETA " with dxvk (built " DATE_T4 " " TIME " by gicombat)"
#define VERSION_VULKAN_STR "T4Me-MAM-SP " VERSION " with dxvk (built " DATE_T4 " " TIME " by gicombat)"
#define VERSIONMP_STR "T4Me-MAM-MP " VERSION " (built " DATE_T4 " " TIME " by gicombat)"
#define VERSIONMP_BETA_STR "T4Me-MAM-MP " VERSION "" BETA " (built " DATE_T4 " " TIME " by gicombat)"
#define BUILDLOG_STR VERSION_STR "\nlogfile created\n"
#define SHORTVERSION_BETA_STR "T4MAM\n" VERSION "" BETA ""
#define SHORTVERSION_STR "T4MAM\n" VERSION 
#define SHORTVERSION_VULKAN_STR "T4MAM\n" VERSION "\ndxvk"
#define SHORTVERSION_BETA_VULKAN_STR "T4MAM\n" VERSION "" BETA "\ndxvk"
#define LONGVERSION_STR SHORTVERSION_STR " CL " DATE_T4 " " TIME
#define SHORTVERSION_CONFIG_BETA_STR "T4MAM " VERSION "" BETA ""
#define SHORTVERSION_CONFIG_STR "T4MAM " VERSION 

//#define VERSION COMMIT
#define VERSION_T4ME 49

#define force_gamma_update true

#include "hexrays_defs.h"
#include "game.hpp"
#include "enums.hpp"
#include "structs.hpp"
//#include "include\cod\clientscript\clientscript_public.hpp"
#include <cassert>


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

