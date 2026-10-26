// ==========================================================
// T4M project
//
// Purpose: Sys_Error (sub_5FE8C0) reconstruction so a fatal error raised off the main
//          thread still reaches the user.
//
//   Vanilla suspends every other engine thread, then reports through the console window.
//   That window belongs to the thread that created it (the main thread). When Sys_Error
//   runs on another thread - the database thread's Com_Error handler (loc_59AC30) calls it -
//   ShowWindow / EM_REPLACESEL wait forever on the suspended main thread: the game freezes
//   and the error is never shown.
// ==========================================================

#include "StdInc.h"
#include "T4.h"

using namespace T4::engine;

// @modified - sub_5FE8C0 (Sys_Error). When the console window belongs to another thread,
// skip it and report through the message box only, then terminate: there is no window of
// ours to pump, and exit() could block on locks held by the suspended threads.
void __cdecl T4M::Sys_Error(const char* fmt, ...)
{
	char string[0x1000];

	EnterCriticalSection(com_errorCritSect);
	*com_errorEntered = 1;
	Sys_SuspendOtherThreads();

	va_list ap;
	va_start(ap, fmt);
	crt_vsnprintf(string, sizeof(string), fmt, ap);
	va_end(ap);

	FixWindowsDesktop();

	const HWND console = *s_wcd_hWnd;
	const bool consoleUsable = !console || GetWindowThreadProcessId(console, nullptr) == GetCurrentThreadId();
	if (consoleUsable)
	{
		Sys_ShowConsole();
		Conbuf_AppendText("\n\n");
		Conbuf_AppendText(string);
		Conbuf_AppendText("\n");
	}
	Sys_SetErrorText(string);

	if (!consoleUsable)
		TerminateProcess(GetCurrentProcess(), 0);

	MSG msg;
	while (GetMessageA(&msg, nullptr, 0, 0))
	{
		TranslateMessage(&msg);
		DispatchMessageA(&msg);
	}
	crt_exit(0);
}

void PatchT4MAM_SysError()
{
	Detours::X86::DetourFunction((uintptr_t)T4M::GetAddress("Com_Fatal"), (uintptr_t)&T4M::Sys_Error, Detours::X86Option::USE_JUMP);
}
