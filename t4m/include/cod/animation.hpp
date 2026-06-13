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
	}
}