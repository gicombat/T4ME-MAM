#ifndef T4M_VERSION_H
#define T4M_VERSION_H


#define PASTE4_(a, b, c, d) a##b##c##d
#define PASTE4(a, b, c, d)  PASTE4_(a, b, c, d)

#define STRINGIFY(x) #x
#define TO_STRING(x) STRINGIFY(x)

#define VERSION_T4ME 49 
#define BASE_VERSION 0
#define BETA_MINOR_VERSION 7

#define INTERNAL_VERSION_NUMBER PASTE4(VERSION_T4ME, 0, BASE_VERSION, BETA_MINOR_VERSION)

#define IS_BETA

#define BETA "b" TO_STRING(BETA_MINOR_VERSION)
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


#endif
