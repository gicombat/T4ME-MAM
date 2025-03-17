// --------------------------------------+
// System Dynamic Link Library Proxy
// by momo5502
// --------------------------------------+

#include "StdInc.h"
#include <vulkan/vulkan.h>

// Macro to declare an export
// --------------------------------------+
//#define EXPORT(_export) extern "C" __declspec(naked) __declspec(dllexport) void _export() { static FARPROC function = 0; if(!function) function = SDLLP::GetExport(__FUNCTION__, LIBRARY); __asm { jmp function } }  

// Static class
// --------------------------------------+
class SDLLP
{
private:
	static std::map<std::string, HINSTANCE> mLibraries;

	static void	Log(const char* message, ...);
public:
	static void	LoadLibrary(const char* library);
	static void	LoadLibraryLocal(const char* library);
	static bool	IsLibraryLoaded(const char* library);
	static bool UseVulkan();

public:
	static FARPROC GetExport(const char* function, const char* library);
};

//	Class variable declarations
// --------------------------------------+
std::map<std::string, HINSTANCE> SDLLP::mLibraries;

// Load necessary library
// --------------------------------------+
void SDLLP::LoadLibrary(const char* library)
{
	Log("[SDLLP] Loading library '%s'.", library);

	CHAR mPath[MAX_PATH];

	GetSystemDirectoryA(mPath, MAX_PATH);
	strcat_s(mPath, "\\");
	strcat_s(mPath, library);

	mLibraries[library] = ::LoadLibraryA(mPath);

	if (!IsLibraryLoaded(library)) Log("[SDLLP] Unable to load library '%s'.", library);
}

void SDLLP::LoadLibraryLocal(const char* library)
{
	Log("[SDLLP] Loading library '%s'.", library);

	mLibraries[library] = ::LoadLibraryA(library);

	if (!IsLibraryLoaded(library)) Log("[SDLLP] Unable to load library '%s'.", library);
}

// Check if export already loaded
// --------------------------------------+
bool SDLLP::IsLibraryLoaded(const char* library)
{
	return (mLibraries.find(library) != mLibraries.end() && mLibraries[library]);
}

// Get export address
// --------------------------------------+
FARPROC SDLLP::GetExport(const char* function, const char* library)
{
	Log("[SDLLP] Export '%s' requested from %s.", function, library);

	if (!IsLibraryLoaded(library)) LoadLibrary(library);

	FARPROC address = GetProcAddress(mLibraries[library], function);

	if (!address) Log("[SDLLP] Unable to load export '%s' from library '%s'.", function, library);
	return address;
}

// Write debug string
// --------------------------------------+
void SDLLP::Log(const char* message, ...)
{
	CHAR buffer[1024];
	va_list ap;

	va_start(ap, message);
	vsprintf(buffer, message, ap);
	va_end(ap);

	OutputDebugStringA(buffer);
}

// Test Vulkan APIy
// --------------------------------------+
bool SDLLP::UseVulkan()
{
	UINT isVulkan = GetPrivateProfileInt("Options", "EnableVulkan", 0, CONFIG_FILE_LOCATION);
	if (isVulkan == 1)
	{
		HMODULE vk = ::LoadLibraryA("vulkan-1.dll");
		if (!vk)
		{
			MessageBoxA(NULL,
		"Trying to launch the game with vulkan instead of DIrectX9 but it's missing vulkan dll. The game will launch in DirectX9 mode.",
		"Vulkan missing!",
		MB_OK | MB_ICONEXCLAMATION);
			return false;
		}
		else
		{
			return true;
		}
	}
	return false;
	//PFN_vkCreateInstance vkCreateInstance;
	//// ...
	//vkCreateInstance = GetProcAddress(vk, "vkCreateInstance");
	//vkGetInstanceProcAddr
	//vkCreateInstance
	//FARPROC vkGetInstanceProcAddr = GetProcAddress(vk, "vkGetInstanceProcAddr");
	//if (!vkGetInstanceProcAddr)
	//	return false;
	//int (*vkCreateInstance)(int*, void*, void**) = vkGetInstanceProcAddr(0, "vkCreateInstance");
	//if (!vkCreateInstance)
	//	return false;
	//void* instance = 0;
	//int result = vkCreateInstance((int[16]) { 1 }, 0, & instance);
	//if (!instance || result != 0)
	//	return false;
}


// --------------------------------------+
//	Adapt export functions and library
// --------------------------------------+

extern "C"
{
	__declspec(naked)
		__declspec(dllexport)
		void D3DPERF_BeginEvent()
	{
		if (SDLLP::UseVulkan())
		{
#ifdef VK_EXT_robustness2
			if (!SDLLP::IsLibraryLoaded("dxvk.dll"))  SDLLP::LoadLibraryLocal("dxvk.dll");
			static FARPROC function = 0;
			if (!function) function = SDLLP::GetExport(__FUNCTION__, "dxvk.dll");
#else
			if (!SDLLP::IsLibraryLoaded("dxvk_1_10_3.dll"))  SDLLP::LoadLibraryLocal("dxvk_1_10_3.dll");
			static FARPROC function = 0;
			if (!function) function = SDLLP::GetExport(__FUNCTION__, "dxvk_1_10_3.dll");
#endif
			__asm { jmp function }
		}
		else
		{
			static FARPROC function = 0;
			if (!function) function = SDLLP::GetExport(__FUNCTION__, "d3d9.dll");
			__asm { jmp function }
		}
	}
	
	__declspec(naked)
		__declspec(dllexport)
		void D3DPERF_EndEvent()
	{
		if (SDLLP::UseVulkan())
		{
#ifdef VK_EXT_robustness2
			if (!SDLLP::IsLibraryLoaded("dxvk.dll"))  SDLLP::LoadLibraryLocal("dxvk.dll");
			static FARPROC function = 0;
			if (!function) function = SDLLP::GetExport(__FUNCTION__, "dxvk.dll");
#else
			if (!SDLLP::IsLibraryLoaded("dxvk_1_10_3.dll"))  SDLLP::LoadLibraryLocal("dxvk_1_10_3.dll");
			static FARPROC function = 0;
			if (!function) function = SDLLP::GetExport(__FUNCTION__, "dxvk_1_10_3.dll");
#endif
			__asm { jmp function }
		}
		else
		{
			static FARPROC function = 0;
			if (!function) function = SDLLP::GetExport(__FUNCTION__, "d3d9.dll");
			__asm { jmp function }
		}
	}
	
	__declspec(naked)
		__declspec(dllexport)
		void Direct3DCreate9()
	{
		if (SDLLP::UseVulkan())
		{
#ifdef VK_EXT_robustness2
			if (!SDLLP::IsLibraryLoaded("dxvk.dll"))  SDLLP::LoadLibraryLocal("dxvk.dll");
			static FARPROC function = 0;
			if (!function) function = SDLLP::GetExport(__FUNCTION__, "dxvk.dll");
#else
			if (!SDLLP::IsLibraryLoaded("dxvk_1_10_3.dll"))  SDLLP::LoadLibraryLocal("dxvk_1_10_3.dll");
			static FARPROC function = 0;
			if (!function) function = SDLLP::GetExport(__FUNCTION__, "dxvk_1_10_3.dll");
#endif
			__asm { jmp function }
		}
		else
		{
			static FARPROC function = 0;
			if (!function) function = SDLLP::GetExport(__FUNCTION__, "d3d9.dll");
			__asm { jmp function }
		}
	}
}


//EXPORT(D3DPERF_BeginEvent)
//EXPORT(D3DPERF_EndEvent)
//EXPORT(Direct3DCreate9)