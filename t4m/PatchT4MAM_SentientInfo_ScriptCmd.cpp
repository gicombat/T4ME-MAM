// ==========================================================
// T4M project
//
// Component: clientdll
// Purpose: AI limit phase 8.2 - reconstructions detoured so that their accesses to
//          actor_s::sentientInfo / vis_blockers go through T4M::Actor_SentientInfo /
//          T4M::Actor_VisBlocker (side table, see PatchT4MAM_ActorSentientInfo.cpp).
//          CoD4 actor_script_cmd.cpp / sentient_script_cmd.cpp: clearenemy, isknownenemyinradius,
//          getclosestenemysqdist
//
//          R&D analysis/actor_sentientinfo_sidetable.md
//
// Started: 2026-09-19
// ==========================================================

#include "StdInc.h"

using T4::engine::actor_s;
using T4::engine::sentient_info_t;

using T4::engine::sentient_s;
using T4::engine::gentity_s;

static_assert(offsetof(T4::engine::level_locals_s, time) == 0x1040, "level.time");
static_assert(offsetof(actor_s, faceLikelyEnemyPathNode) == 0x2054, "actor_s::faceLikelyEnemyPathNode");
static_assert(offsetof(actor_s, useEnemyGoal) == 0x19F0, "actor_s::useEnemyGoal");
static_assert(offsetof(actor_s, ignoreForFixedNodeSafeCheck) == 0x19F4, "actor_s::ignoreForFixedNodeSafeCheck");
static_assert(offsetof(actor_s, iPotentialCoverNodeCount) == 0x1A38, "actor_s::iPotentialCoverNodeCount");
static_assert(offsetof(actor_s, iPotentialReacquireNodeCount) == 0x1A64, "actor_s::iPotentialReacquireNodeCount");
static_assert(offsetof(actor_s, lastEnemySightPosValid) == 0x203C, "actor_s::lastEnemySightPosValid");
static_assert(offsetof(actor_s, eState) == 0xB8C && offsetof(actor_s, stateLevel) == 0xBB4, "actor_s::eState/stateLevel");
static_assert(offsetof(T4::engine::threat_bias_t, threatTable) == 0x20, "threat_bias_t::threatTable");

// @modified — WaW 0x4DE2E0 (method "clearenemy") / CoD4 game/actor_script_cmd.cpp
//   ActorCmd_ClearEnemy, with Sentient_SetEnemy(self->sentient, NULL, 1) and
//   EntHandle::setEnt(NULL) inlined exactly as vanilla. 1:1 with vanilla except
//   sentientInfo goes through the T4M accessor.
//   DETOURED — do not call directly.
void T4_Reconstructed::ActorCmd_ClearEnemy(scr_entref_t entref)
{
	actor_s* self;
	if (entref.classnum || !(self = T4::engine::g_entities[entref.entnum].actor))
	{
		T4::engine::gScrVarPub->error_index = -1;
		T4::engine::Scr_Error("not an actor", T4::engine::SCRIPTINSTANCE_SERVER, 0);
		self = nullptr;
	}

	// Actor_GetTargetSentient
	unsigned short targetNum = self->sentient->targetEnt.number;
	if (targetNum)
	{
		sentient_s* target = T4::engine::g_entities[targetNum - 1].sentient;
		if (target)
		{
			T4M::Actor_SentientInfo(self, static_cast<int>(target - *T4::engine::g_sentients))->lastKnownPosTime = 0;   // @modified
			self->faceLikelyEnemyPathNode = nullptr;
		}
	}

	sentient_s* sentient = self->sentient;
	if (sentient->scriptTargetEnt.number)
	{
		gentity_s* targetEnt = sentient->targetEnt.number ? &T4::engine::g_entities[sentient->targetEnt.number - 1] : nullptr;
		if (targetEnt == &T4::engine::g_entities[sentient->scriptTargetEnt.number - 1])
		{
			// scriptTargetEnt.setEnt(NULL)
			if (sentient->scriptTargetEnt.number)
			{
				gentity_s* oldEnt = &T4::engine::g_entities[sentient->scriptTargetEnt.number - 1];
				if (oldEnt)
				{
					const unsigned short infoIndex = sentient->scriptTargetEnt.infoIndex;
					T4::game::RemoveEntHandleInfo(&T4::engine::g_entitiesHandleList[oldEnt - T4::engine::g_entities], infoIndex);
					sentient->scriptTargetEnt.number = 0;
					sentient->scriptTargetEnt.infoIndex = 0;
				}
			}
		}
	}

	// Sentient_SetEnemy(self->sentient, NULL, 1)
	sentient = self->sentient;
	if (!sentient->targetEnt.number)
		return;

	sentient_s* enemySentient = T4::engine::g_entities[sentient->targetEnt.number - 1].sentient;
	if (enemySentient && sentient->iEnemyNotifyTime && T4::engine::level->time >= sentient->iEnemyNotifyTime)
	{
		T4::game::Actor_BroadcastTeamEvent(sentient, 4);
		sentient->iEnemyNotifyTime = 0;
	}

	if (&T4::engine::g_entities[sentient->targetEnt.number - 1] == nullptr)   // targetEnt.ent() == enemy (NULL)
		return;

	if (enemySentient)
	{
		if (sentient->iEnemyNotifyTime)
		{
			T4::game::Actor_BroadcastTeamEvent(sentient, 4);
			sentient->iEnemyNotifyTime = 0;
		}
		--enemySentient->attackerCount;
	}

	actor_s* actor = sentient->ent->actor;
	sentient->iEnemyNotifyTime = 0;
	T4::game::EntHandle_setEnt(&sentient->targetEnt, nullptr);

	// Scr_IsSystemActive
	if (T4::engine::gScrVarPub->timeArrayId && !T4::engine::gScrVarPub->error_message)
		T4::engine::Scr_NotifyNum_Internal(T4::engine::SCRIPTINSTANCE_SERVER, sentient->ent->s.number, 0,
		                                   *T4::engine::scr_const_enemy, 0);

	if (!actor)
		return;

	if (actor->useEnemyGoal)
	{
		actor->useEnemyGoal = 0;
		T4_Reconstructed::Actor_UpdateGoalPos(actor);
	}
	actor->iPotentialCoverNodeCount = 0;
	actor->iPotentialReacquireNodeCount = 0;
	actor->lastEnemySightPosValid = 0;
	T4_Reconstructed::Actor_UpdateLastEnemySightPos(actor);   // vanilla: tail jmp
}

// @modified — WaW 0x4DEC10 (method "isknownenemyinradius") / CoD4 game/actor_script_cmd.cpp
//   ActorCmd_IsKnownEnemyInRadius with Actor_IsKnownEnemyInRegion(self, NULL, pos, radius)
//   and Actor_IsDying inlined as vanilla. 1:1 with vanilla except:
//   - the sentientInfo row is walked from T4M::Actor_SentientInfo(self, 0);
//   - the loop bound is NEW_MAX_SENTIENTS instead of vanilla's baked 36 (cmp edx,24h @0x4DED17).
//   DETOURED — do not call directly.
void T4_Reconstructed::ActorCmd_IsKnownEnemyInRadius(scr_entref_t entref)
{
	actor_s* self;
	if (entref.classnum || !(self = T4::engine::g_entities[entref.entnum].actor))
	{
		T4::engine::gScrVarPub->error_index = -1;
		T4::engine::Scr_Error("not an actor", T4::engine::SCRIPTINSTANCE_SERVER, 0);
		self = nullptr;
	}

	float position[3];
	T4::engine::Scr_GetVector(T4::engine::SCRIPTINSTANCE_SERVER, position, 0);
	const float radius = T4::engine::Scr_GetFloat(T4::engine::SCRIPTINSTANCE_SERVER, 1);

	if (radius != 0.0f)   // ucomiss + jnp: NaN takes the loop
	{
		sentient_s* sentient = *T4::engine::g_sentients;
		const int levelTime = T4::engine::level->time;
		sentient_info_t* info = T4M::Actor_SentientInfo(self, 0);   // @modified

		for (unsigned int i = 0; i < NEW_MAX_SENTIENTS; ++i, ++info, ++sentient)   // @modified: vanilla 36
		{
			if (!sentient->inuse)
				continue;
			const int lastKnownPosTime = info->lastKnownPosTime;
			if (!lastKnownPosTime)
				continue;
			if (levelTime - lastKnownPosTime > 10000)
				continue;
			if (sentient->eTeam == self->sentient->eTeam)
				continue;
			gentity_s* ent = sentient->ent;
			if (static_cast<float>(ent->health) == 0.0f)   // cvtsi2ss + ucomiss, as vanilla
				continue;
			actor_s* actor = ent->actor;
			if (actor)
			{
				if (actor->ignoreForFixedNodeSafeCheck)
					continue;
				if (actor->eState[actor->stateLevel] == T4::engine::AIS_DEATH)
					continue;
			}
			if (T4::game::Actor_PointNearPoint(position, info->vLastKnownPos, radius))
			{
				T4::engine::Scr_AddInt(T4::engine::SCRIPTINSTANCE_SERVER, (*T4::engine::g_sentients)[i].ent == nullptr);
				return;
			}
		}
	}

	T4::engine::Scr_AddInt(T4::engine::SCRIPTINSTANCE_SERVER, 1);
}

// @modified — WaW 0x566E80 (method "getclosestenemysqdist") / CoD4 game/sentient_script_cmd.cpp
//   SentientCmd_GetClosestEnemySqDist, with Sentient_Get, Sentient_EnemyTeam,
//   Actor_CheckIgnore and Scr_AddFloat inlined as vanilla. 1:1 with vanilla except
//   sentientInfo goes through the T4M accessor.
//   DETOURED — do not call directly.
void T4_Reconstructed::SentientCmd_GetClosestEnemySqDist(scr_entref_t entref)
{
	sentient_s* self;
	if (entref.classnum || !(self = T4::engine::g_entities[entref.entnum].sentient))
	{
		T4::engine::gScrVarPub->error_index = -1;
		T4::engine::Scr_Error("not a sentient", T4::engine::SCRIPTINSTANCE_SERVER, 0);
		self = nullptr;
	}

	float closestDist = 100000000.0f;   // dword_8AFB78
	int enemyTeam[5];                   // Sentient_EnemyTeam, a stack table in vanilla
	enemyTeam[0] = 0;
	enemyTeam[1] = 2;
	enemyTeam[2] = 1;
	enemyTeam[3] = 0;
	enemyTeam[4] = 0;

	const int team = enemyTeam[self->eTeam];
	if (!team)
		return;

	gentity_s* selfEnt = self->ent;
	const float selfX = selfEnt->r.currentOrigin[0];
	const float selfY = selfEnt->r.currentOrigin[1];
	const float selfZ = selfEnt->r.currentOrigin[2];
	const int iTeamFlags = 1 << team;

	sentient_s* enemy = T4::game::Sentient_FirstSentient(iTeamFlags);
	if (enemy)
	{
		actor_s* selfActor = selfEnt->actor;   // NULL for a player: vanilla faults on the read below
		sentient_s* const sentients = *T4::engine::g_sentients;
		float best = closestDist;
		do
		{
			if (T4M::Actor_SentientInfo(selfActor, static_cast<int>(enemy - sentients))->lastKnownPosTime > 0   // @modified
			    && !(enemy->ent->flags & 4)
			    && !enemy->bIgnoreMe
			    && T4::engine::g_threatBias->threatTable[enemy->iThreatBiasGroupIndex][self->iThreatBiasGroupIndex] != static_cast<int>(0x80000000))
			{
				const float* org = enemy->ent->r.currentOrigin;
				const float dx = org[0] - selfX;
				const float dy = org[1] - selfY;
				const float dz = org[2] - selfZ;
				const float distSq = dz * dz + dy * dy + dx * dx;   // vanilla summation order
				if (best > distSq)   // comiss + jbe: NaN keeps best
					best = distSq;
			}
			enemy = T4::game::Sentient_NextSentient(enemy, iTeamFlags);
		} while (enemy);
		closestDist = best;
	}

	T4::engine::Scr_ClearOutParams(T4::engine::SCRIPTINSTANCE_SERVER);

	// Scr_AddFloat, inlined by vanilla
	T4::engine::scrVmPub_t* vm = T4::engine::gScrVmPub;
	T4::engine::VariableValue* top = vm->top;
	if (top == vm->maxstack)
		T4::engine::Com_Fatal("Internal script stack overflow");
	++top;
	++vm->inparamcount;
	vm->top = top;
	top->type = T4::engine::VAR_FLOAT;
	vm->top->u.floatValue = closestDist;
}

void PatchT4MAM_SentientInfo_ScriptCmd()
{
	Detours::X86::DetourFunction(T4M::GetAddress("ActorCmd_ClearEnemy"),
	                             reinterpret_cast<uintptr_t>(&T4_Reconstructed::ActorCmd_ClearEnemy),
	                             Detours::X86Option::USE_JUMP);
	Detours::X86::DetourFunction(T4M::GetAddress("ActorCmd_IsKnownEnemyInRadius"),
	                             reinterpret_cast<uintptr_t>(&T4_Reconstructed::ActorCmd_IsKnownEnemyInRadius),
	                             Detours::X86Option::USE_JUMP);
	Detours::X86::DetourFunction(T4M::GetAddress("SentientCmd_GetClosestEnemySqDist"),
	                             reinterpret_cast<uintptr_t>(&T4_Reconstructed::SentientCmd_GetClosestEnemySqDist),
	                             Detours::X86Option::USE_JUMP);
}
