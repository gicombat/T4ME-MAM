#pragma once
#include <Windows.h>
#include <filesystem>
#include <direct.h>
#include <fstream>
#include <string>
#include <vector>
#include <map>

class SDLLP
{
private:
	static std::map<std::string, HINSTANCE> mLibraries;

	static void	Log(const char* message, ...);
public:
	static bool	LoadLibrary(const char* library);
	static bool	LoadLibraryLocal(const char* library);
	static bool	IsLibraryLoaded(const char* library);
	static bool UseVulkan();

public:
	static FARPROC GetExport(const char* function, const char* library);
};