#pragma once

// Path_* (pathnode build / init). T4::game per the namespace bucketing rule (gameplay code).
// Builder chain used by Mod Tools "Connect Paths" (g_connectpaths), see Path_InitPaths sub_55A4B0.

namespace T4
{
	namespace game
	{
		// (Path_InitLinkCounts sub_55A2D0 is DETOURED to T4M::Path_InitLinkCounts.)

		// sub_560B10 - links + vis + node tree for every node in gameWorldSp.path.
		// Requires g_pathLinkPool == 0, totalLinkCount == 0, pathVis == NULL.
		WEAK engine::symbol<void()> Path_ConnectPaths{ "Path_ConnectPaths" };
		// sub_55F2A0 - friendly chains; allocates the chain maps only when NULL.
		WEAK engine::symbol<void()> Path_BuildChains{ "Path_BuildChains" };
		// sub_55F590 - resets wOverlapNode to -1 then flags duplicate / overlapping nodes.
		WEAK engine::symbol<bool()> Path_CheckNodeOverlaps{ "Path_CheckNodeOverlaps" };

		// WaW sub_55EF30 - usercall(type@edi, spawnVar@stack) ; retn, caller cleans 4.
		// Builds nodes[nodeCount] from spawnVar key/values, then nodeCount++.
		inline void Path_SpawnNodeStatic(int nodeType, const engine::SpawnVar* spawnVar)
		{
			static void* fn = reinterpret_cast<void*>(T4M::GetAddress("Path_SpawnNodeStatic"));
			__asm
			{
				push spawnVar
				mov  edi, nodeType
				call fn
				add  esp, 4
			}
		}

		// WaW sub_55A420 - usercall(node@edx) ; retn. Clears dynamic/transient, forward from fAngle.
		inline void Path_InitNodeDynamic(engine::pathnode_t* node)
		{
			static void* fn = reinterpret_cast<void*>(T4M::GetAddress("Path_InitNodeDynamic"));
			__asm
			{
				mov  edx, node
				call fn
			}
		}
	}
}
