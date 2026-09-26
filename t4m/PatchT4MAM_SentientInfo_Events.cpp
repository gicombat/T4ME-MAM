// ==========================================================
// T4M project
//
// Component: clientdll
// Purpose: AI limit phase 8.2 - reconstructions detoured so that their accesses to
//          actor_s::sentientInfo / vis_blockers go through T4M::Actor_SentientInfo /
//          T4M::Actor_VisBlocker (side table, see PatchT4MAM_ActorSentientInfo.cpp).
//          CoD4 actor_events.cpp / actor_exposed.cpp / actor_orientation.cpp / actor_senses.cpp:
//          Actor_EventPain, Actor_EventBullet, Actor_ReceivePointEvent, Actor_ReceiveLineEvent,
//          Actor_Exposed_Combat, Actor_FaceEnemy, Actor_SightTrace (vis_blockers)
//
//          R&D analysis/actor_sentientinfo_sidetable.md
//
// Started: 2026-09-19
// ==========================================================

#include "StdInc.h"


using T4::engine::actor_s;
using T4::engine::sentient_s;
using T4::engine::sentient_info_t;
using T4::engine::gentity_s;
using T4::engine::ai_orient_t;
using T4::engine::pathnode_t;

namespace
{
	// (sentient - level.sentients) / 0x88 — same signed truncating division as vanilla's
	// imul 78787879h / sar edx,6 / shr+add, including for a null sentient.
	int SentientIndex(const sentient_s* sentient)
	{
		const int diff = static_cast<int>(reinterpret_cast<uintptr_t>(sentient)
		                                - reinterpret_cast<uintptr_t>(*T4::engine::g_sentients));
		return diff / static_cast<int>(sizeof(sentient_s));
	}

	int LevelTime()
	{
		return *T4::engine::level_time;
	}
}

// ==========================================================
// sub_4C7890 — Actor_EventPain
// ==========================================================

// @modified — sub_4C7890 / CoD4 game/actor_events.cpp Actor_EventPain (Actor_WasAttackedBy
//   inlined, pCasualty dropped by WaW). 1:1 with vanilla except sentientInfo/vis_blockers go
//   through the T4M accessors.
//   DETOURED — do not call directly.
void T4_Reconstructed::Actor_EventPain(actor_s* self, sentient_s* pAttacker)
{
	const int idx = SentientIndex(pAttacker);
	T4M::Actor_SentientInfo(self, idx)->iLastAttackMeTime = LevelTime();   // @modified
	T4_Reconstructed::Actor_UpdateLastKnownPos(self, pAttacker);
}

// @wrapper — usercall(pAttacker@eax, self@edi) ; vanilla ends in a plain retn.
__declspec(naked) void T4M::Actor_EventPain_Wrapper()
{
	__asm
	{
		push    eax                         ; pAttacker
		push    edi                         ; self
		call    T4_Reconstructed::Actor_EventPain
		add     esp, 8
		retn
	}
}

// ==========================================================
// sub_4C7930 — Actor_EventBullet
// ==========================================================

// @modified — sub_4C7930 / CoD4 game/actor_events.cpp Actor_EventBullet (Actor_WasAttackedBy
//   inlined; WaW drops the null-sentient test, its only caller does it). 1:1 with vanilla
//   except sentientInfo/vis_blockers go through the T4M accessors.
//   DETOURED — do not call directly.
void T4_Reconstructed::Actor_EventBullet(actor_s* self, gentity_s* originator, const float* vStart,
                                         const float* vEnd, int suppression)
{
	const int idx = SentientIndex(originator->sentient);
	T4M::Actor_SentientInfo(self, idx)->iLastAttackMeTime = LevelTime();   // @modified
	T4_Reconstructed::Actor_UpdateLastKnownPos(self, originator->sentient);
	if (suppression == 0)   // DO_SUPPRESSION
		T4::game::Actor_AddSuppressionLine(self, vStart, originator->sentient, vEnd);
}

// @wrapper — usercall(originator@edi, self, vStart, vEnd, suppression) ; retn 10h.
//   Vanilla preserves ecx (push ecx / pop ecx frame), so the wrapper does too.
__declspec(naked) void T4M::Actor_EventBullet_Wrapper()
{
	__asm
	{
		push    ecx
		push    dword ptr [esp+14h]         ; suppression
		push    dword ptr [esp+14h]         ; vEnd
		push    dword ptr [esp+14h]         ; vStart
		push    edi                         ; originator
		push    dword ptr [esp+18h]         ; self
		call    T4_Reconstructed::Actor_EventBullet
		add     esp, 14h
		pop     ecx
		retn    10h
	}
}

// ==========================================================
// sub_4C79A0 — Actor_ReceivePointEvent
// ==========================================================

// @modified — sub_4C79A0 / CoD4 game/actor_events.cpp Actor_ReceivePointEvent (WaW: no
//   Actor_DumpEvents, no fDistSqrd/fRadiusSqrd, Actor_CaresAboutInfo partly inlined).
//   1:1 with vanilla except sentientInfo/vis_blockers go through the T4M accessors.
//   DETOURED — do not call directly.
void T4_Reconstructed::Actor_ReceivePointEvent(actor_s* self, gentity_s* originator, int eType,
                                               const float* vOrigin)
{
	// Vanilla: jmp jpt_4C79A9[eType*4], no bounds check (10 cases).
	switch (eType)
	{
	case 0:   // AI_EV_FOOTSTEP
	case 1:   // AI_EV_FOOTSTEP_LITE
	{
		sentient_s* const sentient = originator->sentient;
		const int time = LevelTime();
		const int lastKnownPosTime = T4M::Actor_SentientInfo(self, SentientIndex(sentient))->lastKnownPosTime;   // @modified
		if (lastKnownPosTime > 0 && time - lastKnownPosTime < 2000)
			return;
		if (T4::game::Actor_IsUsingTurret(self)
		    && self->pTurret->pTurretInfo->state != 0
		    && sentient
		    && time - lastKnownPosTime >= 3000)
			return;
		T4_Reconstructed::Actor_UpdateLastKnownPos(self, sentient);
		return;
	}

	case 2:   // AI_EV_NEW_ENEMY
		T4::game::Actor_EventNewEnemy(self, originator->sentient);
		return;

	case 3:   // AI_EV_PAIN
	{
		sentient_s* const attacker = originator->sentient->lastAttacker->sentient;
		if (!attacker)
			return;
		const int lastKnownPosTime = T4M::Actor_SentientInfo(self, SentientIndex(attacker))->lastKnownPosTime;   // @modified
		if (lastKnownPosTime > 0 && !T4::game::Actor_CaresAboutInfoTime(lastKnownPosTime))
			return;
		T4_Reconstructed::Actor_EventPain(self, originator->sentient->lastAttacker->sentient);   // sub_4C7890
		return;
	}

	case 4:   // AI_EV_DEATH
	{
		sentient_s* const attacker = originator->sentient->lastAttacker->sentient;
		if (!attacker)
			return;
		const int lastKnownPosTime = T4M::Actor_SentientInfo(self, SentientIndex(attacker))->lastKnownPosTime;   // @modified
		if (lastKnownPosTime > 0 && !T4::game::Actor_CaresAboutInfoTime(lastKnownPosTime))
			return;
		T4_Reconstructed::Actor_UpdateLastKnownPos(self, originator->sentient->lastAttacker->sentient);
		return;
	}

	case 5:   // AI_EV_EXPLOSION
		T4::game::Actor_EventExplosion(originator, self, vOrigin);
		return;

	case 7:   // AI_EV_PROJECTILE_PING
	{
		const unsigned short parent = originator->parent.number;
		if (parent && &T4::engine::g_entities.get()[parent - 1] == self->ent)
			return;
	}
	// fall through
	case 6:   // AI_EV_GRENADE_PING
		T4::game::Actor_GrenadePing(originator, self);
		return;

	case 8:   // AI_EV_GUNSHOT
	case 9:   // AI_EV_SILENCED_SHOT
	{
		if (originator == self->ent)
			return;
		sentient_s* const sentient = originator->sentient;
		const int lastKnownPosTime = T4M::Actor_SentientInfo(self, SentientIndex(sentient))->lastKnownPosTime;   // @modified
		if (lastKnownPosTime > 0 && !T4::game::Actor_CaresAboutInfoTime(lastKnownPosTime))
			return;
		T4_Reconstructed::Actor_UpdateLastKnownPos(self, sentient);
		return;
	}
	}
}

// @wrapper — usercall(eType@eax, originator@ecx, self, vOrigin) ; retn 8.
__declspec(naked) void T4M::Actor_ReceivePointEvent_Wrapper()
{
	__asm
	{
		push    dword ptr [esp+8]           ; vOrigin
		push    eax                         ; eType
		push    ecx                         ; originator
		push    dword ptr [esp+10h]         ; self
		call    T4_Reconstructed::Actor_ReceivePointEvent
		add     esp, 10h
		retn    8
	}
}

// ==========================================================
// sub_4C7BD0 — Actor_ReceiveLineEvent
// ==========================================================

// @modified — sub_4C7BD0 / CoD4 game/actor_events.cpp Actor_ReceiveLineEvent (WaW: two
//   stack args after self, Actor_WasAttackedBy + Actor_UpdateLastKnownPos inlined for 0xF).
//   1:1 with vanilla except sentientInfo/vis_blockers go through the T4M accessors.
//   DETOURED — do not call directly.
void T4_Reconstructed::Actor_ReceiveLineEvent(actor_s* self, gentity_s* originator, int eType,
                                              const float* vStart, const float* vEnd)
{
	if (eType == 0xE)   // AI_EV_BULLET
	{
		if (originator == self->ent || !originator->sentient)
			return;
		T4_Reconstructed::Actor_EventBullet(self, originator, vStart, vEnd, 0);   // sub_4C7930
		return;
	}

	if (eType != 0xF || !originator->sentient)
		return;

	const int idx = SentientIndex(originator->sentient);
	T4M::Actor_SentientInfo(self, idx)->iLastAttackMeTime = LevelTime();   // @modified
	T4_Reconstructed::Actor_UpdateLastKnownPos(self, originator->sentient);
}

// @wrapper — usercall(eType@eax, originator@ecx, self, arg1, arg2) ; retn 0Ch.
__declspec(naked) void T4M::Actor_ReceiveLineEvent_Wrapper()
{
	__asm
	{
		push    dword ptr [esp+0Ch]         ; vEnd
		push    dword ptr [esp+0Ch]         ; vStart
		push    eax                         ; eType
		push    ecx                         ; originator
		push    dword ptr [esp+14h]         ; self
		call    T4_Reconstructed::Actor_ReceiveLineEvent
		add     esp, 14h
		retn    0Ch
	}
}

// ==========================================================
// sub_4C7DB0 — Actor_Exposed_Combat
// ==========================================================

// @modified — sub_4C7DB0 / CoD4 game/actor_exposed.cpp Actor_Exposed_Combat
//   (Actor_GetTargetSentient, Path_IsValidClaimNode and the ORIENT_TO_ENEMY
//   Actor_SetOrientMode inlined as vanilla does). 1:1 with vanilla except
//   sentientInfo/vis_blockers go through the T4M accessors.
//   DETOURED — do not call directly.
void T4_Reconstructed::Actor_Exposed_Combat(actor_s* self)
{
	T4::game::Actor_AnimCombat(self);

	const unsigned short target = self->sentient->targetEnt.number;
	sentient_s* const enemy = target ? T4::engine::g_entities.get()[target - 1].sentient : nullptr;

	if (T4::game::Actor_IsAtGoal(self) && enemy)
	{
		T4::game::Actor_Exposed_CheckLockGoal(self);

		const int lastVisTime = T4M::Actor_SentientInfo(self, SentientIndex(enemy))->VisCache.iLastVisTime;   // @modified
		if (!lastVisTime || LevelTime() - lastVisTime >= 10000)
		{
			pathnode_t* const node = self->sentient->pClaimedNode;
			if (node && (node->constant.spawnflags & 0x8000))
			{
				// Vanilla var_4 is the frame slot made by "push ecx": not initialized.
				void* script;
				if (!T4::game::Actor_Cover_PickAttackScript(self, node, 1, &script) || script)
				{
					T4::game::Actor_SetOrientMode(self, T4::engine::AI_ORIENT_TO_GOAL);   // tail jmp in vanilla
					return;
				}
			}
		}
	}

	// Actor_SetOrientMode(self, AI_ORIENT_TO_ENEMY), inlined.
	// volatile: vanilla really stores 1 then 3; keep the compiler from dropping the first store.
	volatile int* const eMode = reinterpret_cast<volatile int*>(&self->CodeOrient.eMode);
	const sentient_s* const sentient = self->sentient;
	if (sentient && sentient->syncedMeleeEnt.number)
		*eMode = T4::engine::AI_ORIENT_DONT_CHANGE;
	*eMode = T4::engine::AI_ORIENT_TO_ENEMY;
}

// @wrapper — usercall(self@esi) ; vanilla ends in a plain retn (or tail-jumps to one).
__declspec(naked) void T4M::Actor_Exposed_Combat_Wrapper()
{
	__asm
	{
		push    esi                         ; self
		call    T4_Reconstructed::Actor_Exposed_Combat
		add     esp, 4
		retn
	}
}

// ==========================================================
// sub_4D5D60 — Actor_FaceEnemy
// ==========================================================

// @modified — sub_4D5D60 / CoD4 game/actor_orientation.cpp Actor_FaceEnemy
//   (Actor_GetTargetPosition / Actor_HasPath / Actor_FaceVector partly inlined as vanilla
//   does). 1:1 with vanilla except sentientInfo/vis_blockers go through the T4M accessors.
//   DETOURED — do not call directly.
void T4_Reconstructed::Actor_FaceEnemy(actor_s* self, ai_orient_t* pOrient)
{
	sentient_s* const selfSentient = self->sentient;
	const unsigned short target = selfSentient->targetEnt.number;
	if (!target)
	{
		T4::game::Actor_FaceLikelyEnemyPath(self, pOrient);
		return;
	}

	// lea ecx, g_entities[target - 1] ; test ecx, ecx — kept although never null.
	gentity_s* const targetEnt = &T4::engine::g_entities.get()[target - 1];
	if (!targetEnt)
	{
		T4::game::Actor_FaceLikelyEnemyPath(self, pOrient);
		return;
	}

	sentient_s* const enemy = targetEnt->sentient;
	float v[3];

	if (enemy)
	{
		sentient_info_t* const info = T4M::Actor_SentientInfo(self, SentientIndex(enemy));   // @modified
		if (!info->VisCache.bVisible)
		{
			if (self->species)
			{
				const float* const origin = self->ent->r.currentOrigin;
				v[2] = info->vLastKnownPos[2] - origin[2];
				v[0] = info->vLastKnownPos[0] - origin[0];
				v[1] = info->vLastKnownPos[1] - origin[1];
				if (!(v[0] * v[0] + v[1] * v[1] >= 1.0f))   // comiss / jb
					return;
				T4::game::Actor_FaceVector(v, pOrient);
				return;
			}

			const int lastVisTime = info->VisCache.iLastVisTime;
			if (!lastVisTime)
			{
				T4::game::Actor_FaceLikelyEnemyPath(self, pOrient);
				return;
			}

			if (self->Path.wPathLen <= 0)
			{
				if (LevelTime() - lastVisTime < 10000)
					return;
				T4::game::Actor_FaceLikelyEnemyPath(self, pOrient);
				return;
			}

			const int time = LevelTime();
			if (static_cast<float>(time) > static_cast<float>(self->Path.iPathTime) + 300.0f   // comiss / jbe
			    && time - lastVisTime > 500)
			{
				const pathnode_t* const node = T4::game::Path_FindFacingNode(selfSentient, enemy, info);
				if (node)
				{
					const float* const origin = self->ent->r.currentOrigin;
					v[2] = node->constant.vOrigin[2] - origin[2];
					v[0] = node->constant.vOrigin[0] - origin[0];
					v[1] = node->constant.vOrigin[1] - origin[1];
					if (!(v[0] * v[0] + v[1] * v[1] >= 1.0f))
						return;
					T4::game::Actor_FaceVector(v, pOrient);
					return;
				}
			}
		}
	}

	// loc_4D5F39 — Actor_GetTargetPosition + Actor_FaceVector inlined; re-reads targetEnt.
	{
		const float* const targetOrigin = T4::engine::g_entities.get()[self->sentient->targetEnt.number - 1].r.currentOrigin;
		const float* const origin = self->ent->r.currentOrigin;
		v[2] = targetOrigin[2] - origin[2];
		v[0] = targetOrigin[0] - origin[0];
		v[1] = targetOrigin[1] - origin[1];
		if (!(v[0] * v[0] + v[1] * v[1] >= 1.0f))
			return;

		float angles[3];
		T4::engine::vectoangles_vanilla(v, angles);
		T4::game::Actor_SetDesiredAngles(pOrient, angles[0], angles[1]);
	}
}

// @wrapper — usercall(self@eax, pOrient) ; retn 4.
__declspec(naked) void T4M::Actor_FaceEnemy_Wrapper()
{
	__asm
	{
		push    dword ptr [esp+4]           ; pOrient
		push    eax                         ; self
		call    T4_Reconstructed::Actor_FaceEnemy
		add     esp, 8
		retn    4
	}
}

// ==========================================================
// sub_4DEF20 — Actor_SightTrace
// ==========================================================

// @modified — sub_4DEF20 / CoD4 game/actor_senses.cpp Actor_SightTrace (WaW adds the
//   per-sentient vis_blockers hint fed into SV_SightTrace, and a proximity prim list).
//   1:1 with vanilla except sentientInfo/vis_blockers go through the T4M accessors.
//   Plain __stdcall (4 stack args, retn 10h, result in al): detoured without a wrapper.
//   DETOURED — do not call directly.
bool __stdcall T4_Reconstructed::Actor_SightTrace(actor_s* self, const float* start, const float* end,
                                                  int passEntNum)
{
	// Vanilla reuses the 'start' arg slot as hitNum: it keeps the pointer value unless the
	// pass entity is a sentient, and that value is what SV_SightTrace reads as its hint.
	int hitNum = reinterpret_cast<int>(start);

	++self->iTraceCount;

	gentity_s* const passEnt = &T4::engine::g_entities.get()[passEntNum];
	if (passEnt->sentient)
		hitNum = *T4M::Actor_VisBlocker(self, SentientIndex(passEnt->sentient));   // @modified

	const T4::engine::colgeom_visitor_inlined_t* const proximity = *T4::engine::actorSightProximity;
	const void* prims = nullptr;
	int nprims = 0;
	if (proximity)
	{
		nprims = proximity->nprims;
		prims = proximity->prims;
	}

	if (self->ignoreCloseFoliage)
	{
		float adjusted[3];
		adjusted[0] = end[0] - start[0];
		adjusted[1] = end[1] - start[1];
		adjusted[2] = end[2] - start[2];
		T4::engine::Vec3Normalize(adjusted);

		const float dist = (*T4::dvar::ai_foliageSeeThroughDist)->current.value;
		adjusted[0] = adjusted[0] * dist + start[0];
		adjusted[1] = dist * adjusted[1] + start[1];
		adjusted[2] = dist * adjusted[2] + start[2];

		T4::engine::SV_SightTrace(&hitNum, adjusted, end, self->ent->s.number, passEntNum, 0x280D803, nprims, prims);
		if (!hitNum)
			T4::engine::SV_SightTrace(&hitNum, start, adjusted, self->ent->s.number, passEntNum, 0x280D801, nprims, prims);
	}
	else
	{
		const float dx = end[0] - start[0];
		const float dz = end[2] - start[2];
		const float dy = end[1] - start[1];
		const float dist = (*T4::dvar::ai_foliageSeeThroughDist)->current.value;
		const float distSq = dx * dx + dz * dz + dy * dy;   // vanilla summation order: x, z, y
		const int contentmask = (dist * dist > distSq) ? 0x280D801 : 0x280D803;   // comiss / jbe
		T4::engine::SV_SightTrace(&hitNum, start, end, self->ent->s.number, passEntNum, contentmask, nprims, prims);
	}

	sentient_s* const passSentient = passEnt->sentient;   // re-read, as vanilla
	if (passSentient)
		*T4M::Actor_VisBlocker(self, SentientIndex(passSentient)) = static_cast<unsigned short>(hitNum);   // @modified

	if (hitNum)
		return false;
	if (0.2f > T4::engine::SV_FX_GetVisibility(start, end))   // comiss / ja
		return false;
	return true;
}

void PatchT4MAM_SentientInfo_Events()
{
	Detours::X86::DetourFunction(T4M::GetAddress("Actor_EventPain"),
	                             reinterpret_cast<uintptr_t>(&T4M::Actor_EventPain_Wrapper),
	                             Detours::X86Option::USE_JUMP);
	Detours::X86::DetourFunction(T4M::GetAddress("Actor_EventBullet"),
	                             reinterpret_cast<uintptr_t>(&T4M::Actor_EventBullet_Wrapper),
	                             Detours::X86Option::USE_JUMP);
	Detours::X86::DetourFunction(T4M::GetAddress("Actor_ReceivePointEvent"),
	                             reinterpret_cast<uintptr_t>(&T4M::Actor_ReceivePointEvent_Wrapper),
	                             Detours::X86Option::USE_JUMP);
	Detours::X86::DetourFunction(T4M::GetAddress("Actor_ReceiveLineEvent"),
	                             reinterpret_cast<uintptr_t>(&T4M::Actor_ReceiveLineEvent_Wrapper),
	                             Detours::X86Option::USE_JUMP);
	Detours::X86::DetourFunction(T4M::GetAddress("Actor_Exposed_Combat"),
	                             reinterpret_cast<uintptr_t>(&T4M::Actor_Exposed_Combat_Wrapper),
	                             Detours::X86Option::USE_JUMP);
	Detours::X86::DetourFunction(T4M::GetAddress("Actor_FaceEnemy"),
	                             reinterpret_cast<uintptr_t>(&T4M::Actor_FaceEnemy_Wrapper),
	                             Detours::X86Option::USE_JUMP);
	Detours::X86::DetourFunction(T4M::GetAddress("Actor_SightTrace"),
	                             reinterpret_cast<uintptr_t>(&T4_Reconstructed::Actor_SightTrace),
	                             Detours::X86Option::USE_JUMP);
}
