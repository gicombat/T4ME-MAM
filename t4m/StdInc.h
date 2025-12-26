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

// first two number => correspond to T4M Enhanced dll version (current r49 on github, in this code here only r48)
// 0 => separator
// XX => config file revision, currently 2
#define INTERNAL_VERSION_NUMBER 4802
#define STRINGIFY(x) #x
#define TO_STRING(x) STRINGIFY(x)

#define IS_BETA
#define BETA "b2"
#define VERSION "1.0"
#define FS_BASEGAME "data"
#define DATE __DATE__
#define TIME __TIME__
#define CONSOLEVERSION_BETA_STR "T4Me-MAM " VERSION "" BETA "> "
#define CONSOLEVERSION_STR "T4Me-MAM " VERSION "> "
#define CONSOLEVERSION_VULKAN_STR "T4Me-MAM " VERSION "> "
#define CONSOLEVERSION_BETA_VULKAN_STR "T4Me-MAM " VERSION "" BETA "> "
#define VERSION_BETA_STR "T4Me-MAM-SP " VERSION "" BETA " (built " DATE " " TIME " by gicombat)"
#define VERSION_STR "T4Me-MAM-SP " VERSION " (built " DATE " " TIME " by gicombat)"
#define VERSION_BETA_VULKAN_STR "T4Me-MAM-SP " VERSION "" BETA " with dxvk (built " DATE " " TIME " by gicombat)"
#define VERSION_VULKAN_STR "T4Me-MAM-SP " VERSION " with dxvk (built " DATE " " TIME " by gicombat)"
#define VERSIONMP_STR "T4Me-MAM-MP " VERSION " (built " DATE " " TIME " by gicombat)"
#define VERSIONMP_BETA_STR "T4Me-MAM-MP " VERSION "" BETA " (built " DATE " " TIME " by gicombat)"
#define BUILDLOG_STR VERSION_STR "\nlogfile created\n"
#define SHORTVERSION_BETA_STR "T4MAM\n" VERSION "" BETA ""
#define SHORTVERSION_STR "T4MAM\n" VERSION 
#define SHORTVERSION_VULKAN_STR "T4MAM\n" VERSION "\ndxvk"
#define SHORTVERSION_BETA_VULKAN_STR "T4MAM\n" VERSION "" BETA "\ndxvk"
#define LONGVERSION_STR SHORTVERSION_STR " CL " DATE " " TIME

#define CONFIG_FILE_LOCATION ".\\T4M.conf"

#ifdef IS_BETA
#define DEFAULT_CONFIG_FILE_HEADER "// " SHORTVERSION_BETA_STR "  Config File"
#else
#define DEFAULT_CONFIG_FILE_HEADER "// " SHORTVERSION_STR "  Config File"
#endif

#define DEFAULT_CONFIG_FILE "" DEFAULT_CONFIG_FILE_HEADER "\n \
// If you experience problem with the game like suttering, crash, vertex corruption etc\n \
// Its advise to enable Vulkan, however it's only recommand for Windows 10 and bove\n \
// You can enable it too on older version of Windows but be aware that your graphical card may be not compatible\n \
// since it need a Vulkan 1.3 capable driver and graphical card\n \
// Min Driver version need for the version of the vulkan driver bundled :\n \
// NVIDIA 510.47.03\n \
// AMD 22.0\n \
// Intel 22.0\n \
\n \
[Version] // Do not modify this, you can lost your custom parameter if modified \n \
Number = " TO_STRING(INTERNAL_VERSION_NUMBER) "\n \
[Options]\n \
EnableVulkan = 0\n"

#define ADC_Prescaler_1 1
#define ADC_Prescaler_2 2

static bool IsUsingVulkan;

static dvar_t* con_external;
static dvar_t* enable_scoreboard;
static dvar_t* disable_intro;
static dvar_t* vulkan;