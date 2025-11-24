#include "t4_headers.h"
#include "StdInc.h"
#include <algorithm>

#include <safetyhook.hpp>
#include <MemoryMgr.h>
static uintptr_t Path_NodesInCylinder_addr = 0x0055A1E0;


int Path_NodesInCylinder_ASM(const float* origin, pathsort_t* nodes, float maxDist, float maxHeight, int maxNodes, int typeFlags)
{
	int result;
	__asm
	{
		push typeFlags;
		push maxNodes;
		push maxHeight;
		push maxDist;
		mov eax, origin;
		mov edx, nodes;
		call Path_NodesInCylinder_addr;
		mov result, eax;
		add esp, 0x10;
	}
	return result;
}

inline int __cdecl Path_NodesInCylinder(
	float* origin,
	float maxDist,
	float maxHeight,
	pathsort_t* nodes,
	int maxNodes,
	int typeFlags) {
	auto result = Path_NodesInCylinder_ASM(origin, nodes, maxDist, maxHeight, maxNodes, typeFlags);
	return result;
}

int __cdecl Path_NodesInRadius(float* origin, float maxDist, pathsort_t* nodes, int maxNodes, int typeFlags)
{
	return Path_NodesInCylinder(origin, maxDist, 1000000000.0, nodes, maxNodes, typeFlags);
}

static uintptr_t SV_SightTraceCapsule_addr = 0x005AC430;

// this has a return in BO1
void SV_SightTraceCapsule_ASM(const float* start, float* end, int* hitNum, const float* mins, const float* maxs, int numHits, contents_e mask)
{
	__asm
	{
		push mask;
		push numHits;
		push maxs;
		push mins;
		push hitNum;
		mov edi, start;
		mov esi, end;
		call SV_SightTraceCapsule_addr;
		add esp, 0x14;
	}
}

inline BOOL SV_SightTraceCapsule(
	int* hitNum,
	const float* start,
	const float* mins,
	const float* maxs,
	float* end,
	contents_e mask) {

	SV_SightTraceCapsule_ASM(start, end, hitNum, mins, maxs, 1023, (contents_e)mask);

	return false;
}

inline BOOL SV_SightTraceCapsule(
	int* hitNum,
	const float* start,
	const float* mins,
	const float* maxs,
	float* end,
	int mask) {
	SV_SightTraceCapsule_ASM(start, end, hitNum, mins, maxs, 1023, (contents_e)mask);

	return false;
}

bool __cdecl Path_CompareNodesIncreasing(const pathsort_t& ps1, const pathsort_t& ps2)
{
	if (ps1.node->dynamic.wLinkCount)
	{
		if (!ps2.node->dynamic.wLinkCount)
			return 1;
	}
	else if (ps2.node->dynamic.wLinkCount)
	{
		return 0;
	}
	return ps2.metric > ps1.metric;
}

float actorMins[3] = { -15.f,-15.f,0.f };
float actorMaxs[3] = { 15.f,15.f,48.f };

const float zombie_fudge = 14.0f;
const float fudge_0 = 5.0f;

double __cdecl Vec3DistanceSq(const float* p1, const float* p2)
{
	float v_4; // [esp+4h] [ebp-8h]
	float v_8; // [esp+8h] [ebp-4h]

	v_4 = p2[1] - p1[1];
	v_8 = p2[2] - p1[2];
	return v_8 * v_8 + v_4 * v_4 + (float)(*p2 - *p1) * (float)(*p2 - *p1);
}



pathnode_t* __cdecl Path_NearestNodeNotCrossPlanes_lol(
	int typeFlags,
	int maxNodes,
	float* vOrigin,
	pathsort_t* nodes,
	float fMaxDist,
	float (*vNormal)[2],
	float* fDist,
	int iPlaneCount,
	int* returnCount,
	nearestNodeHeightCheck heightCheck)
{
	//int typeFlags;
	//int maxNodes;

	//__asm {
	//	mov typeFlags, edx
	//	mov maxNodes,ecx
	//}


	pathnode_t* closestNode; // [esp+74h] [ebp-470h]
	float distSq{}; // [esp+78h] [ebp-46Ch]
	float adjustedOrigin[3]{}; // [esp+7Ch] [ebp-468h] BYREF
	int j; // [esp+88h] [ebp-45Ch]
	pathnode_t* node; // [esp+8Ch] [ebp-458h]

	int iNodeCount{}; // [esp+B8h] [ebp-42Ch]
	float mins[3]{}; // [esp+BCh] [ebp-428h] BYREF
	float maxs[3]{}; // [esp+C8h] [ebp-41Ch] BYREF
	pathnode_t* failedNodes[256]{}; // [esp+D4h] [ebp-410h]
	int i{}; // [esp+4D8h] [ebp-Ch]
	int hitNum{}; // [esp+4DCh] [ebp-8h] BYREF
	int iFailedNodeCount{}; // [esp+4E0h] [ebp-4h]


	if (heightCheck)
	{
		iNodeCount = Path_NodesInRadius(vOrigin, fMaxDist, nodes, maxNodes, typeFlags);
	}
	else
	{
		adjustedOrigin[0] = *vOrigin;
		adjustedOrigin[1] = vOrigin[1];
		adjustedOrigin[2] = vOrigin[2];
		adjustedOrigin[2] = adjustedOrigin[2] - 120.0;
		iNodeCount = Path_NodesInCylinder(adjustedOrigin, fMaxDist, 184.0, nodes, maxNodes, typeFlags);
	}
	std::sort(
		nodes,
		&nodes[iNodeCount],
		Path_CompareNodesIncreasing
	);
	mins[0] = actorMins[0];
	mins[1] = -15.0;
	maxs[0] = actorMaxs[0];
	maxs[1] = 15.0;
	maxs[2] = 48.0;
	mins[2] = 0.0 + 17.0;
	if (isZombieMode())
	{
		mins[0] = mins[0] + zombie_fudge;
		mins[1] = mins[1] + zombie_fudge;
		maxs[0] = maxs[0] - zombie_fudge;
		maxs[1] = maxs[1] - zombie_fudge;
	}
	else
	{
		mins[0] = mins[0] + fudge_0;
		mins[1] = mins[1] + fudge_0;
		maxs[0] = maxs[0] - fudge_0;
		maxs[1] = maxs[1] - fudge_0;
	}
	iFailedNodeCount = 0;
	*returnCount = iNodeCount;
	for (i = 0; i < iNodeCount; ++i)
	{
		node = nodes[i].node;
		if (node->dynamic.wLinkCount)
		{
			if (*vOrigin > (float)((float)(node->constant.vOrigin[0] + -15.0) - 1.0)
				&& (float)((float)(node->constant.vOrigin[0] + 15.0) + 1.0) > *vOrigin
				&& vOrigin[1] > (float)((float)(node->constant.vOrigin[1] + -15.0) - 1.0)
				&& (float)((float)(node->constant.vOrigin[1] + 15.0) + 1.0) > vOrigin[1]
				&& vOrigin[2] > (float)((float)(node->constant.vOrigin[2] + 0.0) - 1.0)
				&& (float)((float)(node->constant.vOrigin[2] + 48.0) + 1.0) > vOrigin[2])
			{
				return node;
			}
			for (j = 0; j < iPlaneCount; ++j)
			{
				if ((float)((float)(node->constant.vOrigin[0] * (*vNormal)[2 * j])
					+ (float)(node->constant.vOrigin[1] * (*vNormal)[2 * j + 1])) > fDist[j])
					goto failed_node;
			}
			hitNum = 0;
			SV_SightTraceCapsule(&hitNum, vOrigin, mins, maxs, node->constant.vOrigin, (CONTENTS_SOLID | CONTENTS_GLASS | CONTENTS_MONSTERCLIP | CONTENTS_VEHICLE));
			if (!hitNum)
				return node;
		}
		else
		{
		failed_node:
			failedNodes[iFailedNodeCount++] = node;
		}
	}
	for (i = 0; i < iFailedNodeCount; ++i)
	{
		hitNum = 0;
		SV_SightTraceCapsule(&hitNum, vOrigin, mins, maxs, failedNodes[i]->constant.vOrigin, (CONTENTS_SOLID | CONTENTS_GLASS | CONTENTS_MONSTERCLIP | CONTENTS_VEHICLE));
		if (!hitNum)
			return failedNodes[i];
	}
	if (isZombieMode())
		return 0;
	closestNode = 0;
	for (i = 0; i < iNodeCount; ++i)
	{
		node = nodes[i].node;
		distSq = Vec3DistanceSq(node->constant.vOrigin, vOrigin);
		if (distSq < FLT_MAX)
			closestNode = node;
	}
	return closestNode;
}

pathnode_t* __cdecl Path_NearestNodeNotCrossPlanes_waw(
	float* vOrigin,
	pathsort_t* nodes,
	float fMaxDist,
	float (*vNormal)[2],
	float* fDist,
	int iPlaneCount,
	int* returnCount,
	nearestNodeHeightCheck heightCheck) {

	int typeFlags, maxNodes;

	__asm {
		mov typeFlags, edx
		mov maxNodes, ecx
	}

	Path_NearestNodeNotCrossPlanes_lol(typeFlags, maxNodes, vOrigin, nodes, fMaxDist, vNormal, fDist, iPlaneCount, returnCount, heightCheck);

}

dvar_t* g_t5_pathing;

//pathnode_t* Path_NearestNodeNotCrossPlanes_WAWcall(
//	int typeFlags,
//	int maxNodes,
//	float* vOrigin,
//	pathsort_t* nodes,
//	float fMaxDist,
//	float (*vNormal)[2],
//	float* fDist,
//	int iPlaneCount,
//	int* returnCount,
//	nearestNodeHeightCheck heightCheck) {
//	return Path_NearestNodeNotCrossPlanes(vOrigin, nodes, typeFlags, fMaxDist, vNormal, fDist, iPlaneCount, returnCount, maxNodes, heightCheck);
//}



SafetyHookInline Path_NearestNodeNotCrossPlanes_og_usercall;

void __declspec(naked) Path_NearestNodeNotCrossPlanes_stub()
{
	__asm
	{
		push ecx // maxNodes
		push edx // typeFlags
		call Path_NearestNodeNotCrossPlanes_lol
		add esp, 8
		retn
	}
}

void PatchT4E_Pathing() {
	g_t5_pathing = Dvar_RegisterBool(false, "g_t5_pathing", DVAR_FLAG_ARCHIVE);
	static auto testingthehack = safetyhook::create_mid(0x55C210, [](SafetyHookContext& ctx) {
		if (g_t5_pathing->isEnabled()) {
			ctx.eax = (uintptr_t)Path_NearestNodeNotCrossPlanes_lol(
				ctx.edx,                                          // typeFlags
				ctx.ecx,                                          // maxNodes
				*(float**)(ctx.esp + 0x4),                         // vOrigin
				*(pathsort_t**)(ctx.esp + 0x8),                  // nodes
				*(float*)(ctx.esp + 0xC),                        // fMaxDist
				*(float(**)[2])(ctx.esp + 0x10),                 // vNormal
				*(float**)(ctx.esp + 0x14),                      // fDist
				*(int*)(ctx.esp + 0x18),                         // iPlaneCount
				*(int**)(ctx.esp + 0x1C),                        // returnCount
				*(nearestNodeHeightCheck*)(ctx.esp + 0x20)       // heightCheck
			);
			ctx.eip = 0x0055C4C0;
		}
		});

	//Path_NearestNodeNotCrossPlanes_og_usercall = safetyhook::create_inline(0x55C210, Path_NearestNodeNotCrossPlanes_stub);
	//Memory::VP::InjectHook(0x55C210, Path_NearestNodeNotCrossPlanes_waw, Memory::VP::HookType::Jump);


}

