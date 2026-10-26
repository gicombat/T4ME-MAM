// ==========================================================
// T4M project
//
// Component: clientdll
// Purpose: Client pose of a DObj (CG_DObjCalcPose / CG_DObjCalcBone), reconstructed so that
//          T4M passes run under the DObj lock around DObjCalcSkel: left-hand IK of actors
//          between two DObjCalcSkel calls, then the weapon sling.
//          Plans: plans/plan_ai_left_hand_ik.md, plans/plan_weapon_sling_physics.md
// ==========================================================

#include "StdInc.h"
#include "T4.h"
#include "Hooking.h"
#include <intrin.h>
#include <cstring>

using namespace T4::engine;

namespace
{
	// Vanilla spin of sub_4571F0 / sub_447250: wait for 0, then lock cmpxchg 0 -> 1.
	inline void DObjLock(DObj_s* obj)
	{
		volatile long* lock = reinterpret_cast<volatile long*>(&obj->locked);
		for (;;)
		{
			if (*lock)
				continue;
			if (_InterlockedCompareExchange(lock, 1, 0) == 0)
				return;
		}
	}

	inline void DObjUnlock(DObj_s* obj)
	{
		*reinterpret_cast<volatile long*>(&obj->locked) = 0;
	}

	// Tail of both reconstructions after the controllers. DObjCalcSkel skips the bones already
	// computed this frame, so the IK cuts it in two: wrist + tag chains, IK, then the rest (fingers,
	// twists, duplicates) from the corrected matrices.
	void CalcSkel(DObj_s* obj, const cpose_t* pose, int* partBits)
	{
		int before[4];
		memcpy(before, obj->skel.partBits.skel, sizeof(before));
		T4M::HandIKJob ik;
		if (T4M::HandIK_Prepare(pose, obj, partBits, before, &ik))
		{
			DObjCalcSkel(obj, ik.pass1);
			T4M::HandIK_Solve(obj, &ik);
		}
		DObjCalcSkel(obj, partBits);
		T4M::Sling_PostSkel(obj, before);
	}
}

namespace T4M
{
	// @modified — sub_4571F0 CG_DObjCalcPose ; vanilla + hand IK and sling around DObjCalcSkel.
	extern "C" DObjAnimMat* __cdecl CG_DObjCalcPose(const cpose_t* pose, DObj_s* obj, int* partBits)
	{
		DObjLock(obj);
		DObjAnimMat* mat;
		if (!CL_DObjCreateSkelForBones(obj, partBits, &mat))
		{
			DObjCompleteHierarchyBits(obj, partBits);
			T4::game::CG_DoControllers(pose, obj, partBits);
			CalcSkel(obj, pose, partBits);
		}
		DObjUnlock(obj);
		return mat;
	}

	// @modified — sub_447250 CG_DObjCalcBone ; vanilla + hand IK and sling around DObjCalcSkel.
	extern "C" void __cdecl CG_DObjCalcBone(const cpose_t* pose, DObj_s* obj, int boneIndex)
	{
		DObjLock(obj);
		if (!CL_DObjCreateSkelForBone(obj, boneIndex))
		{
			int partBits[4];
			DObjGetHierarchyBits(obj, boneIndex, partBits);
			T4::game::CG_DoControllers(pose, obj, partBits);
			CalcSkel(obj, pose, partBits);
		}
		DObjUnlock(obj);
	}
}

// @wrapper — usercall(obj@eax, pose, partBits) -> cdecl(pose, obj, partBits). Detoured over sub_4571F0.
__declspec(naked) void W_CG_DObjCalcPose()
{
	__asm
	{
		push dword ptr [esp + 8]   // partBits
		push eax                   // obj
		push dword ptr [esp + 0Ch] // pose
		call T4M::CG_DObjCalcPose
		add  esp, 0Ch
		ret
	}
}

// @wrapper — usercall(obj@eax, pose, boneIndex) -> cdecl(pose, obj, boneIndex). Detoured over sub_447250.
__declspec(naked) void W_CG_DObjCalcBone()
{
	__asm
	{
		push dword ptr [esp + 8]   // boneIndex
		push eax                   // obj
		push dword ptr [esp + 0Ch] // pose
		call T4M::CG_DObjCalcBone
		add  esp, 0Ch
		ret
	}
}

void PatchT4MAM_DObjPose()
{
	using Detours::X86::DetourFunction;
	const Detours::X86Option J = Detours::X86Option::USE_JUMP;

	DetourFunction(T4M::GetAddress("CG_DObjCalcPose"), (uintptr_t)&W_CG_DObjCalcPose, J);
	DetourFunction(T4M::GetAddress("CG_DObjCalcBone"), (uintptr_t)&W_CG_DObjCalcBone, J);
}
