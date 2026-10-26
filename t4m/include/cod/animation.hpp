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

		// --- Client pose (plans/plan_weapon_sling_physics.md) ---------------
		// WaW sub_60B790 — usercall(obj@eax, partBits@stack) ; caller cleans 4.
		inline void DObjCalcSkel(const DObj_s* obj, int* partBits)
		{
			static void* fn = reinterpret_cast<void*>(T4M::GetAddress("DObjCalcSkel"));
			__asm
			{
				push partBits
				mov  eax, obj
				call fn
				add  esp, 4
			}
		}

		// WaW sub_478B80 — usercall(obj@esi, partBits@stack, pMatOut@stack) -> eax ; caller cleans 8.
		// Nonzero = nothing to compute (bones up to date, or no skel memory left).
		inline int CL_DObjCreateSkelForBones(const DObj_s* obj, int* partBits, DObjAnimMat** pMatOut)
		{
			static void* fn = reinterpret_cast<void*>(T4M::GetAddress("CL_DObjCreateSkelForBones"));
			int result;
			__asm
			{
				push pMatOut
				push partBits
				mov  esi, obj
				call fn
				add  esp, 8
				mov  result, eax
			}
			return result;
		}

		// WaW sub_63B380 — usercall(obj@esi, boneIndex@edx) -> eax.
		inline int CL_DObjCreateSkelForBone(const DObj_s* obj, int boneIndex)
		{
			static void* fn = reinterpret_cast<void*>(T4M::GetAddress("CL_DObjCreateSkelForBone"));
			int result;
			__asm
			{
				mov  esi, obj
				mov  edx, boneIndex
				call fn
				mov  result, eax
			}
			return result;
		}

		// WaW sub_60C290 — usercall(obj@edx, partBits@stack) ; caller cleans 4.
		inline void DObjCompleteHierarchyBits(const DObj_s* obj, int* partBits)
		{
			static void* fn = reinterpret_cast<void*>(T4M::GetAddress("DObjCompleteHierarchyBits"));
			__asm
			{
				push partBits
				mov  edx, obj
				call fn
				add  esp, 4
			}
		}

		// WaW sub_6090D0 — usercall(boneIndex@eax, obj@edx, partBits@stack) ; caller cleans 4.
		inline void DObjGetHierarchyBits(const DObj_s* obj, int boneIndex, int* partBits)
		{
			static void* fn = reinterpret_cast<void*>(T4M::GetAddress("DObjGetHierarchyBits"));
			__asm
			{
				push partBits
				mov  eax, boneIndex
				mov  edx, obj
				call fn
				add  esp, 4
			}
		}
	}
}