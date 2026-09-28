#pragma once

namespace T4
{
	namespace engine
	{
		// Function
		WEAK symbol<void*(const char* name, void* alloc_cb)>Anim_RegisterByName{ "Anim_RegisterByName" };
		WEAK symbol<void(void* tree, int slot_idx)> Anim_AddTreeSlot{ "Anim_AddTreeSlot" };

		// Variable
		WEAK symbol<void> AnimAllocCb{ "AnimAllocCb" };		

		// --- AI limit phase 8.2 (DObj) ------------------------------
		// WaW sub_60FAE0 — usercall(obj@ecx, header@eax, buffer@stack) ; retn, caller cleans 4.
		// Buffer size 0x800 is hardcoded in the callee.
		inline void DObjDisplayAnimToBuffer(const void* obj, const char* header, char* buffer)
		{
			static void* fn = reinterpret_cast<void*>(T4M::GetAddress("DObjDisplayAnimToBuffer"));
			__asm
			{
				push buffer
				mov  ecx, obj
				mov  eax, header
				call fn
				add  esp, 4
			}
		}
	}
}