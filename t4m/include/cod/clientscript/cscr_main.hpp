#pragma once

namespace T4 { namespace engine {
	WEAK symbol<unsigned int(scriptInstance_t inst, const char* file, PrecacheEntry* entries, int entriesCount)> Scr_LoadScriptInternal { "Scr_LoadScriptInternal" };
	WEAK symbol<void(scriptInstance_t inst)>Scr_EndLoadScripts { "Scr_EndLoadScripts" };
	WEAK symbol<void(scriptInstance_t inst, void *(__cdecl *Alloc)(int), int user, int modChecksum)> Scr_PrecacheAnimTrees { "Scr_PrecacheAnimTrees" };
	WEAK symbol<void(scriptInstance_t inst)>Scr_EndLoadAnimTrees { "Scr_EndLoadAnimTrees" };

	// WaW sub_689470 — usercall(token@ecx) -> bool@al ; no stack args
	inline bool Scr_IsIdentifier(char* token)
	{
		static void* fn = reinterpret_cast<void*>(T4M::GetAddress("Scr_IsIdentifier"));
		bool result;
		__asm
		{
			mov   ecx, token
			call  fn
			mov   result, al
		}
		return result;
	}
	
	// WaW sub_6894B0 — usercall(file@eax, inst@ecx, handle@stack0) -> uint@eax ; caller-cleans
	inline unsigned int Scr_GetFunctionHandle(const char* file, scriptInstance_t inst, const char* handle)
	{
		static void* fn = reinterpret_cast<void*>(T4M::GetAddress("Scr_GetFunctionHandle"));
		unsigned int result;
		__asm
		{
			push  handle
			mov   eax, file
			mov   ecx, inst
			call  fn
			add   esp, 4
			mov   result, eax
		}
		return result;
	}
	
	// WaW sub_6895A0 — usercall(inst@eax, stringValue@edi) -> uint@eax ; no stack args
	inline unsigned int SL_TransferToCanonicalString(scriptInstance_t inst, unsigned int stringValue)
	{
		static void* fn = reinterpret_cast<void*>(T4M::GetAddress("SL_TransferToCanonicalString"));
		unsigned int result;
		__asm
		{
			mov   eax, inst
			mov   edi, stringValue
			call  fn
			mov   result, eax
		}
		return result;
	}
	
	// WaW sub_6895F0 — usercall(token@eax, inst@esi) -> uint@eax ; no stack args
	inline unsigned int SL_GetCanonicalString(const char* token, scriptInstance_t inst)
	{
		static void* fn = reinterpret_cast<void*>(T4M::GetAddress("SL_GetCanonicalString"));
		unsigned int result;
		__asm
		{
			mov   eax, token
			mov   esi, inst
			call  fn
			mov   result, eax
		}
		return result;
	}
	
	// WaW sub_689660 — usercall(inst@edi, user@stack0) -> void ; caller-cleans
	inline void Scr_BeginLoadScripts(scriptInstance_t inst, int user)
	{
		static void* fn = reinterpret_cast<void*>(T4M::GetAddress("Scr_BeginLoadScripts"));
		__asm
		{
			push  user
			mov   edi, inst
			call  fn
			add   esp, 4
		}
	}
	
	// WaW sub_689880 — usercall(inst@ecx, user@eax) -> void ; no stack args
	inline void Scr_BeginLoadAnimTrees(scriptInstance_t inst, int user)
	{
		static void* fn = reinterpret_cast<void*>(T4M::GetAddress("Scr_BeginLoadAnimTrees"));
		__asm
		{
			mov   ecx, inst
			mov   eax, user
			call  fn
		}
	}
	
	// WaW sub_689900 — usercall(max_size@edi, buf@stack0) -> int@eax ; caller-cleans
	inline int Scr_ScanFile(int max_size, char* buf)
	{
		static void* fn = reinterpret_cast<void*>(T4M::GetAddress("Scr_ScanFile"));
		int result;
		__asm
		{
			push  buf
			mov   edi, max_size
			call  fn
			add   esp, 4
			mov   result, eax
		}
		return result;
	}
	
	// WaW sub_689C60 — usercall(file@ecx, inst@edx) -> uint@eax ; no stack args
	inline unsigned int Scr_LoadScript(const char* file, scriptInstance_t inst)
	{
		static void* fn = reinterpret_cast<void*>(T4M::GetAddress("Scr_LoadScript"));
		unsigned int result;
		__asm
		{
			mov   ecx, file
			mov   edx, inst
			call  fn
			mov   result, eax
		}
		return result;
	}
	
	// WaW sub_689E50 — usercall(inst@eax) -> void ; no stack args
	inline void Scr_FreeScripts(scriptInstance_t inst)
	{
		static void* fn = reinterpret_cast<void*>(T4M::GetAddress("Scr_FreeScripts"));
		__asm
		{
			mov   eax, inst
			call  fn
		}
	}

	// Warning Adress unknow for now
	WEAK symbol<int(scriptInstance_t inst, const char* pos)>Scr_IsInOpcodeMemory{ "Scr_IsInOpcodeMemory" };
	WEAK symbol<void(scriptInstance_t inst)>SL_BeginLoadScripts{ "SL_BeginLoadScripts" };
	WEAK symbol<void(bool loadedImpureScript)>Scr_SetLoadedImpureScript{ "Scr_SetLoadedImpureScript" };
} } // namespace T4::engine