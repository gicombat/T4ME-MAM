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

#define BETA "b"
#define VERSION "2.0"
#define FS_BASEGAME "data"
#define DATE __DATE__
#define TIME __TIME__
#define CONSOLEVERSION_BETA_STR "T4Me-MAM " VERSION "" BETA "> "
#define CONSOLEVERSION_STR "T4Me-MAM " VERSION "> "
#define CONSOLEVERSION_VULKAN_STR "T4Me-MAM " VERSION "> "
#define CONSOLEVERSION_BETA_VULKAN_STR "T4Me-MAM " VERSION "" BETA "> "
#define VERSION_BETA_STR "T4Me-MAM-SP " VERSION "" BETA " (built " DATE " " TIME " by gicombat)"
#define VERSION_STR "T4Me-MAM-SP " VERSION " (built " DATE " " TIME " by gicombat)"
#define VERSION_BETA_VULKAN_STR "T4Me-MAM-SP " VERSION "" BETA " with dxvk 2.1 (built " DATE " " TIME " by gicombat)"
#define VERSION_VULKAN_STR "T4Me-MAM-SP " VERSION " with dxvk 2.1 (built " DATE " " TIME " by gicombat)"
#define VERSIONMP_STR "T4Me-MAM-MP " VERSION " (built " DATE " " TIME " by gicombat)"
#define VERSIONMP_BETA_STR "T4Me-MAM-MP " VERSION "" BETA " (built " DATE " " TIME " by gicombat)"
#define BUILDLOG_STR VERSION_STR "\nlogfile created\n"
#define SHORTVERSION_BETA_STR "T4Me-MAM " VERSION "" BETA ""
#define SHORTVERSION_STR "T4Me-MAM " VERSION 
#define SHORTVERSION_BETA_VULKAN_STR "TT4Me-MAM " VERSION "\ndxvk 2.1"
#define SHORTVERSION_VULKAN_STR "T4Me-MAM " VERSION "" BETA "\ndxvk 2.1"
#define LONGVERSION_STR SHORTVERSION_STR " CL " DATE " " TIME

#define ADC_Prescaler_1 1
#define ADC_Prescaler_2 2