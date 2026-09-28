// ==========================================================
// T4M project
//
// Component: clientdll
// Purpose: AI limit phase 8 - actor_s::sentientInfo[36] and actor_s::vis_blockers[36]
//          moved to side tables sized [NEW_MAX_ACTORS][NEW_MAX_SENTIENTS]. Both arrays
//          are indexed by sentient index: past 36, sentientInfo overwrites the rest of
//          its own actor and vis_blockers overwrites the next actor's ent pointer. Every
//          vanilla reader/writer is reconstructed and routed through the accessors below.
//
//          Inventory (65 sites / 35 entry points) and design:
//                            R&D analysis/actor_sentientinfo_sidetable.md
//
// Started: 2026-09-19
// ==========================================================

#include "StdInc.h"
#include <safetyhook.hpp>

// 0 while the readers/writers are being converted: the accessors return the in-struct
// arrays, so converted and not-yet-converted code agree (vanilla behavior, <= 32 AI).
// 1 once all 35 are converted: the accessors return the side tables, the in-struct
// arrays are poisoned, and t4m_aiSentinelCheck reports any site that still uses them.
#define AI_SIDETABLE_ACTIVE 1

#define SENTINEL_BYTE 0xA5

using T4::engine::actor_s;
using T4::engine::sentient_info_t;

static_assert(sizeof(actor_s) == 0x31B8, "actor_s layout");
static_assert(sizeof(sentient_info_t) == 0x28, "sentient_info_t layout");
static_assert(offsetof(actor_s, sentientInfo) == 0x1A88, "actor_s::sentientInfo");
static_assert(offsetof(actor_s, vis_blockers) == 0x3170, "actor_s::vis_blockers");

namespace
{
	sentient_info_t g_sentientInfoExt[NEW_MAX_ACTORS][NEW_MAX_SENTIENTS];
	unsigned short  g_visBlockersExt[NEW_MAX_ACTORS][NEW_MAX_SENTIENTS];

	// One report per actor slot and allocation: a missed site sits in a think loop.
	bool g_sentinelReported[NEW_MAX_ACTORS];

	int ActorIndex(const actor_s* actor)
	{
		return static_cast<int>(actor - *T4::engine::g_actors);
	}

#if AI_SIDETABLE_ACTIVE
	bool IsNodeByte(size_t fieldOfs)
	{
		return fieldOfs >= offsetof(sentient_info_t, pLastKnownNode)
		    && fieldOfs <  offsetof(sentient_info_t, pLastKnownNode) + sizeof(void*);
	}

	void PoisonSentientInfo(actor_s* actor)
	{
		memset(actor->sentientInfo, SENTINEL_BYTE, sizeof(actor->sentientInfo));
		// Stays null: actorFields converts sentientInfo[0..32].pLastKnownNode as SF_PATHNODE
		// on every save.
		for (sentient_info_t& info : actor->sentientInfo)
			info.pLastKnownNode = nullptr;
	}

	void PoisonVisBlockers(actor_s* actor)
	{
		memset(actor->vis_blockers, SENTINEL_BYTE, sizeof(actor->vis_blockers));
	}

	// @new - observation only. Runs at SV_EmitPacketActors entry (once per server frame
	// and client); never writes game memory.
	void SentinelCheckTick(SafetyHookContext&)
	{
		if (!ai_sentinel_check || !ai_sentinel_check->current.boolean)
			return;

		actor_s* const actors = *T4::engine::g_actors;
		for (int a = 0; a < NEW_MAX_ACTORS; ++a)
		{
			const actor_s* actor = &actors[a];
			if (!actor->inuse || g_sentinelReported[a])
				continue;

			const BYTE* info = reinterpret_cast<const BYTE*>(actor->sentientInfo);
			for (size_t i = 0; i < sizeof(actor->sentientInfo); ++i)
			{
				const size_t field = i % sizeof(sentient_info_t);
				if (info[i] != (IsNodeByte(field) ? 0 : SENTINEL_BYTE))
				{
					T4::engine::Com_Printf(14, "^1[T4M][ai] actor %d: sentientInfo[%d] +0x%X = 0x%02X - unconverted site\n",
					                       a, static_cast<int>(i / sizeof(sentient_info_t)), static_cast<int>(field), info[i]);
					g_sentinelReported[a] = true;
					break;
				}
			}
			if (g_sentinelReported[a])
				continue;

			const BYTE* vis = reinterpret_cast<const BYTE*>(actor->vis_blockers);
			for (size_t i = 0; i < sizeof(actor->vis_blockers); ++i)
			{
				if (vis[i] != SENTINEL_BYTE)
				{
					T4::engine::Com_Printf(14, "^1[T4M][ai] actor %d: vis_blockers[%d] = 0x%04X - unconverted site\n",
					                       a, static_cast<int>(i / 2), actor->vis_blockers[i / 2]);
					g_sentinelReported[a] = true;
					break;
				}
			}
		}
	}
#endif
}

// ==========================================================
// Accessors
// ==========================================================

T4::engine::sentient_info_t* T4M::Actor_SentientInfo(actor_s* actor, int sentientIndex)
{
#if AI_SIDETABLE_ACTIVE
	return &g_sentientInfoExt[ActorIndex(actor)][sentientIndex];
#else
	return &actor->sentientInfo[sentientIndex];
#endif
}

unsigned short* T4M::Actor_VisBlocker(actor_s* actor, int sentientIndex)
{
#if AI_SIDETABLE_ACTIVE
	return &g_visBlockersExt[ActorIndex(actor)][sentientIndex];
#else
	return &actor->vis_blockers[sentientIndex];
#endif
}

void T4M::Actor_SentientInfoReset(actor_s* actor)
{
	const int a = ActorIndex(actor);
	memset(g_sentientInfoExt[a], 0, sizeof(g_sentientInfoExt[a]));
	g_sentinelReported[a] = false;
#if AI_SIDETABLE_ACTIVE
	PoisonSentientInfo(actor);
#endif
}

void T4M::Actor_VisBlockersReset(actor_s* actor)
{
	const int a = ActorIndex(actor);
	memset(g_visBlockersExt[a], 0, sizeof(g_visBlockersExt[a]));
	g_sentinelReported[a] = false;
#if AI_SIDETABLE_ACTIVE
	PoisonVisBlockers(actor);
#endif
}

// ==========================================================
// Reconstructions
// ==========================================================

// @modified — sub_50F3F0 / CoD4 game/g_save.cpp ReadActor. 1:1 with vanilla, then the
//   side-table row is reset the way vanilla resets the in-struct sentientInfo.
//   vis_blockers is outside the 0x21CC archived bytes and not touched, as in vanilla.
//   DETOURED — do not call directly.
void T4_Reconstructed::ReadActor(actor_s* actor, T4::engine::SaveGame* save)
{
	T4::engine::SaveMemory_LoadRead(&actor->inuse, 4, save);
	if (!actor->inuse)
		return;

	T4::game::G_ReadStruct(T4::engine::actorFields, reinterpret_cast<unsigned char*>(actor), 0x21CC, save);
	T4::game::ReadActorPotentialCoverNodes(actor, save);
	actor->pszDebugInfo = T4::engine::emptyString;

	const float boundsMin = *T4::engine::ReadActor_boundsMinInit;
	const float boundsMax = *T4::engine::ReadActor_boundsMaxInit;

	// Function-local static of vanilla ReadActor: a prototype proximity visitor.
	DWORD* const guard = T4::engine::ReadActor_staticGuard;
	T4::engine::colgeom_visitor_inlined_t* const proto = T4::engine::ReadActor_staticProximity;
	if (!(*guard & 1))
	{
		*guard |= 1;

		T4::engine::colgeom_visitor_t& v = proto->baseclass_0;
		v.baseclass_0.__vftable = T4::engine::ReadActor_proximityVtbl.get();   // offset of the vtable itself
		v.m_mn.vec[0] = v.m_mn.vec[1] = v.m_mn.vec[2] = boundsMin;
		v.m_mn.vec[3] = 0.0f;
		v.m_mx.vec[0] = v.m_mx.vec[1] = v.m_mx.vec[2] = boundsMax;
		v.m_mx.vec[3] = 0.0f;
		v.m_p0.vec[0] = v.m_p0.vec[1] = v.m_p0.vec[2] = v.m_p0.vec[3] = 0.0f;
		v.m_p1.vec[0] = v.m_p1.vec[1] = v.m_p1.vec[2] = v.m_p1.vec[3] = 0.0f;
		v.m_delta.vec[1] = v.m_delta.vec[2] = v.m_delta.vec[3] = 0.0f;   // vec[0] not written by vanilla
		v.m_rvec.vec[0] = v.m_rvec.vec[1] = v.m_rvec.vec[2] = v.m_rvec.vec[3] = 0.0f;
		v.m_radius = 0.0f;
		proto->nprims = 0;

		T4::engine::crt_atexit(T4::engine::ReadActor_staticDtor.get());
	}

	T4::engine::colgeom_visitor_inlined_t& prox = actor->Physics.proximity_data;
	prox.baseclass_0.baseclass_0.__vftable = proto->baseclass_0.baseclass_0.__vftable;
	prox.nprims = 0;
	prox.baseclass_0.m_mn.vec[0] = prox.baseclass_0.m_mn.vec[1] = prox.baseclass_0.m_mn.vec[2] = boundsMin;
	prox.baseclass_0.m_mx.vec[0] = prox.baseclass_0.m_mx.vec[1] = prox.baseclass_0.m_mx.vec[2] = boundsMax;

	memset(actor->sentientInfo, 0, sizeof(actor->sentientInfo));
	T4M::Actor_SentientInfoReset(actor);   // @modified
}

// @wrapper — usercall(save@eax, actor@stack) ; vanilla ends in a plain retn, caller cleans.
__declspec(naked) void T4M::ReadActor_Wrapper()
{
	__asm
	{
		push    eax                         ; save
		push    dword ptr [esp+8]           ; actor (original arg_0)
		call    T4_Reconstructed::ReadActor
		add     esp, 8
		retn
	}
}

void PatchT4MAM_ActorSentientInfo()
{
	Detours::X86::DetourFunction(T4M::GetAddress("ReadActor"),
	                             reinterpret_cast<uintptr_t>(&T4M::ReadActor_Wrapper),
	                             Detours::X86Option::USE_JUMP);

	// Phase 8.2: every other reader/writer of the two arrays.
	PatchT4MAM_SentientInfo_Actor();
	PatchT4MAM_SentientInfo_EntInfo();
	PatchT4MAM_SentientInfo_Events();
	PatchT4MAM_SentientInfo_Senses();
	PatchT4MAM_SentientInfo_Threat();
	PatchT4MAM_SentientInfo_Turret();
	PatchT4MAM_SentientInfo_ScriptCmd();

#if AI_SIDETABLE_ACTIVE
	static auto sentinel_check_hook = safetyhook::create_mid(T4M::GetAddress("SV_EmitPacketActors"), &SentinelCheckTick);
#endif
}
