// ==========================================================
// T4M project
//
// Component: clientdll
// Purpose: AI limit phase 8.2 - reconstructions detoured so that their accesses to
//          actor_s::sentientInfo / vis_blockers go through T4M::Actor_SentientInfo /
//          T4M::Actor_VisBlocker (side table, see PatchT4MAM_ActorSentientInfo.cpp).
//          CoD4 actor_threat.cpp / actor_turret.cpp: Actor_IsFullyAware, Actor_UpdateSingleThreat, Actor_UpdateThreat,
//          Actor_CanAttackAll, Actor_FlagEnemyUnattackable, Actor_Turret_Think
//
//          R&D analysis/actor_sentientinfo_sidetable.md
//
// Started: 2026-09-19
// ==========================================================

#include "StdInc.h"

#include <cstring>

using T4::engine::actor_s;
using T4::engine::sentient_s;
using T4::engine::sentient_info_t;
using T4::engine::gentity_s;
using T4::engine::pathnode_t;

// T4::engine::actor_s / sentient_s / gentity_s field offsets this file relies on.
// cod/ ASSERT_STRUCT_* are inert in C++ (CLAUDE.md), so they are checked here.
static_assert(sizeof(gentity_s) == 0x378, "gentity_s");
static_assert(sizeof(sentient_s) == 0x88, "sentient_s");
static_assert(offsetof(gentity_s, r) + offsetof(T4::engine::entityShared_s, currentOrigin) == 0x160, "gentity_s::r.currentOrigin");
static_assert(offsetof(gentity_s, r) + offsetof(T4::engine::entityShared_s, ownerNum) == 0x178, "gentity_s::r.ownerNum");
static_assert(offsetof(gentity_s, client) == 0x180 && offsetof(gentity_s, actor) == 0x184, "gentity_s client/actor");
static_assert(offsetof(gentity_s, sentient) == 0x188 && offsetof(gentity_s, pTurretInfo) == 0x190, "gentity_s sentient/pTurretInfo");
static_assert(offsetof(gentity_s, flags) == 0x1B4 && offsetof(gentity_s, health) == 0x1C8 && offsetof(gentity_s, maxhealth) == 0x1CC, "gentity_s flags/health");
static_assert(offsetof(gentity_s, tagInfo) == 0x2D0, "gentity_s::tagInfo");
static_assert(offsetof(gentity_s, s) + offsetof(T4::engine::entityState_s, weapon) == 0xE0, "gentity_s::s.weapon");
static_assert(offsetof(T4::engine::gclient_s, sess) == 0x20AC, "gclient_s::sess");
static_assert(offsetof(T4::engine::TurretInfo, state) == 0x4 && offsetof(T4::engine::TurretInfo, flags) == 0x8, "TurretInfo");
static_assert(offsetof(T4::engine::tagInfo_s, parent) == 0x0, "tagInfo_s::parent");
static_assert(offsetof(sentient_s, eTeam) == 0x4 && offsetof(sentient_s, iThreatBias) == 0x8, "sentient_s eTeam/iThreatBias");
static_assert(offsetof(sentient_s, iThreatBiasGroupIndex) == 0xC && offsetof(sentient_s, bIgnoreMe) == 0x10, "sentient_s grp/ignoreMe");
static_assert(offsetof(sentient_s, bIgnoreAll) == 0x11 && offsetof(sentient_s, iEnemyNotifyTime) == 0x24, "sentient_s ignoreAll/notify");
static_assert(offsetof(sentient_s, attackerCount) == 0x28 && offsetof(sentient_s, targetEnt) == 0x34, "sentient_s attackers/targetEnt");
static_assert(offsetof(sentient_s, scriptTargetEnt) == 0x38 && offsetof(sentient_s, entityTargetThreat) == 0x3C, "sentient_s scriptTarget");
static_assert(offsetof(sentient_s, turretInvulnerability) == 0x55 && offsetof(sentient_s, pClaimedNode) == 0x58, "sentient_s turret/claimed");
static_assert(offsetof(actor_s, ent) == 0x0 && offsetof(actor_s, sentient) == 0x4, "actor_s ent/sentient");
static_assert(offsetof(actor_s, eState) == 0xB8C && offsetof(actor_s, eSubState) == 0xBA0 && offsetof(actor_s, stateLevel) == 0xBB4, "actor_s state");
static_assert(offsetof(actor_s, bUseGoalWeight) == 0xD8A, "actor_s::bUseGoalWeight");
static_assert(offsetof(actor_s, Path) == 0x1554 && offsetof(T4::engine::path_t, wPathLen) == 0x380, "actor_s::Path");
static_assert(offsetof(actor_s, iTeamMoveDodgeTime) == 0x1958 && offsetof(actor_s, pPileUpActor) == 0x195C && offsetof(actor_s, pPileUpEnt) == 0x1960, "actor_s pileup");
static_assert(offsetof(actor_s, codeGoal) == 0x198C && offsetof(T4::engine::actor_goal_s, radius) == 0x18 && offsetof(T4::engine::actor_goal_s, height) == 0x1C, "actor_s::codeGoal");
static_assert(offsetof(actor_s, useEnemyGoal) == 0x19F0, "actor_s::useEnemyGoal");
static_assert(offsetof(actor_s, bPacifist) == 0x1A2C && offsetof(actor_s, iPacifistWait) == 0x1A30, "actor_s pacifist");
static_assert(offsetof(actor_s, iPotentialCoverNodeCount) == 0x1A38 && offsetof(actor_s, iPotentialReacquireNodeCount) == 0x1A64, "actor_s potential nodes");
static_assert(offsetof(actor_s, pFavoriteEnemy) == 0x2028 && offsetof(actor_s, lastEnemySightPosValid) == 0x203C, "actor_s favorite/sightpos");
static_assert(offsetof(actor_s, threatUpdateTime) == 0x20D8 && offsetof(actor_s, hasThreateningEnemy) == 0x20DC, "actor_s threat");
static_assert(offsetof(actor_s, pGrenade) == 0x20E8 && offsetof(actor_s, pTurret) == 0x2134, "actor_s grenade/turret");
static_assert(offsetof(actor_s, flashBanged) == 0x21C0 && offsetof(actor_s, pszDebugInfo) == 0x21CC, "actor_s flash/debug");
static_assert(offsetof(T4::engine::scrVarPub_t, error_message) == 0xC && offsetof(T4::engine::scrVarPub_t, timeArrayId) == 0x18, "scrVarPub_t");
static_assert(offsetof(T4::engine::scr_const_t, enemy) == 0x244, "scr_const_t::enemy");
static_assert(offsetof(T4::engine::level_locals_s, time) == 0x1040, "level_locals_s::time");

namespace
{
	float F32(unsigned int bits) { float f; memcpy(&f, &bits, 4); return f; }

	int LevelTime() { return T4::engine::level->time; }

	int SentientIndex(const sentient_s* s)
	{
		return static_cast<int>(s - *T4::engine::g_sentients);
	}

	// EntHandle deref as inlined by vanilla: g_entities[number - 1].
	gentity_s* EntFromNumber(unsigned short number)
	{
		gentity_s* const ents = T4::engine::g_entities;
		return &ents[number - 1];
	}

	// SentientHandle deref as inlined by vanilla: immediate 0x18E73C0 + (number-1)*0x88.
	// That immediate is relocated by PatchT4MAM_ActorLimit; level.sentients follows the pool.
	sentient_s* SentientFromNumber(unsigned short number)
	{
		return &(*T4::engine::g_sentients)[number - 1];
	}

	int ThreatBias(int groupEnemy, int groupSelf)
	{
		return T4::engine::g_threatBias->threatTable[groupEnemy][groupSelf];
	}

	// Inlined Sentient_EnemyTeam table (stack array { 0, 2, 1, 0, 0 } in vanilla).
	int EnemyTeamOf(int team)
	{
		int table[5];
		table[0] = 0;
		table[1] = 2;
		table[2] = 1;
		table[3] = 0;
		table[4] = 0;
		return table[team];
	}
}

// ==========================================================
// Actor_FlagEnemyUnattackable — loc_4E31E0 (not an IDA proc). Only entry: the tail jmp at
// 0x4D958F from ActorCmd_FlagEnemyUnattackable (0x4D9550, "flagenemyunattackable",
// actor method table 0x83C9D4). No other code or data reference reaches its body.
// ==========================================================

// @modified — loc_4E31E0 / CoD4 game/actor_threat.cpp Actor_FlagEnemyUnattackable. 1:1 with vanilla
//   except sentientInfo/vis_blockers go through the T4M accessors.
//   DETOURED — do not call directly.
void T4_Reconstructed::Actor_FlagEnemyUnattackable(actor_s* self)
{
	const unsigned short number = self->sentient->targetEnt.number;
	if (!number)
		return;

	sentient_s* const enemy = EntFromNumber(number)->sentient;
	if (!enemy)
		return;

	if (self->eState[self->stateLevel] != 1)   // AIS_EXPOSED
		return;

	T4M::Actor_SentientInfo(self, SentientIndex(enemy))->attackTime = 0x7FFFFFFF;
	self->eSubState[self->stateLevel] = static_cast<T4::engine::ai_substate_t>(0x65);   // STATE_EXPOSED_NONCOMBAT
}

// @wrapper — usercall(self@ecx), retn. Vanilla leaves ecx untouched: kept.
__declspec(naked) void T4M::Actor_FlagEnemyUnattackable_Wrapper()
{
	__asm
	{
		push    ecx
		push    ecx
		call    T4_Reconstructed::Actor_FlagEnemyUnattackable
		add     esp, 4
		pop     ecx
		retn
	}
}

// ==========================================================
// Actor_IsFullyAware — sub_4E3270
// ==========================================================

// @modified — sub_4E3270 / CoD4 game/actor_threat.cpp Actor_IsFullyAware. 1:1 with vanilla except
//   sentientInfo/vis_blockers go through the T4M accessors.
//   DETOURED — do not call directly.
int T4_Reconstructed::Actor_IsFullyAware(actor_s* self, sentient_s* enemy, int isCurrentEnemy)
{
	sentient_info_t* const info = T4M::Actor_SentientInfo(self, SentientIndex(enemy));

	if (!isCurrentEnemy)
		return (LevelTime() - info->lastKnownPosTime) < 10000;

	const float* const origin = enemy->ent->r.currentOrigin;
	const float dx = origin[0] - info->vLastKnownPos[0];
	const float dz = origin[2] - info->vLastKnownPos[2];
	const float dy = origin[1] - info->vLastKnownPos[1];
	float distSq = dx * dx + dz * dz;
	distSq = distSq + dy * dy;
	if (!(4096.0f > distSq))
		return 0;

	sentient_s* const selfSentient = self->sentient;
	pathnode_t* claimed = selfSentient->pClaimedNode;
	if (!claimed)
		claimed = T4::game::Sentient_NearestNode(selfSentient);

	pathnode_t* const enemyNode = T4::game::Sentient_NearestNode(enemy);
	if (!claimed || !enemyNode)
		return 0;
	if (!T4::game::Path_NodesVisible(enemyNode, claimed))
		return 0;
	return 1;
}

// @wrapper — usercall(self@eax, enemy@edi, isCurrentEnemy@stack) -> eax ; retn 4.
__declspec(naked) void T4M::Actor_IsFullyAware_Wrapper()
{
	__asm
	{
		push    dword ptr [esp+4]           ; isCurrentEnemy
		push    edi                         ; enemy
		push    eax                         ; self
		call    T4_Reconstructed::Actor_IsFullyAware
		add     esp, 0Ch
		retn    4
	}
}

// ==========================================================
// Actor_UpdateSingleThreat — sub_4E3540 (stdcall, retn 8)
// ==========================================================

// @modified — sub_4E3540 / CoD4 game/actor_threat.cpp Actor_UpdateSingleThreat. 1:1 with vanilla
//   except sentientInfo/vis_blockers go through the T4M accessors. WaW retail: no debug strings.
//   DETOURED — do not call directly.
int __stdcall T4_Reconstructed::Actor_UpdateSingleThreat(actor_s* self, sentient_s* enemy)
{
	sentient_info_t* const info = T4M::Actor_SentientInfo(self, SentientIndex(enemy));
	int friendlyTimingOut = 0;

	if (self->bPacifist)
	{
		const int lastAttackMeTime = info->iLastAttackMeTime;
		if (!lastAttackMeTime || LevelTime() - lastAttackMeTime >= self->iPacifistWait)
			return static_cast<int>(0x80000000);
	}

	sentient_s* const selfSentient = self->sentient;
	const int bias = ThreatBias(enemy->iThreatBiasGroupIndex, selfSentient->iThreatBiasGroupIndex);
	if (bias == static_cast<int>(0x80000000))
		return static_cast<int>(0x80000000);

	const float* const selfOrigin = self->ent->r.currentOrigin;
	const float dz = info->vLastKnownPos[2] - selfOrigin[2];
	const float dy = info->vLastKnownPos[1] - selfOrigin[1];
	const float dx = info->vLastKnownPos[0] - selfOrigin[0];
	float distSq = dz * dz + dy * dy;
	distSq = distSq + dx * dx;
	const float dist = T4::engine::I_sqrt(distSq);

	const unsigned short targetNum = selfSentient->targetEnt.number;
	sentient_s* const currentTarget = targetNum ? EntFromNumber(targetNum)->sentient : nullptr;
	const int isCurrentEnemy = (enemy == currentTarget) ? 1 : 0;
	const int isVisible = info->VisCache.bVisible;

	int isFullyAware;
	if (isVisible || T4_Reconstructed::Actor_IsFullyAware(self, enemy, isCurrentEnemy))
		isFullyAware = 1;
	else
		isFullyAware = 0;

	gentity_s* const enemyEnt = enemy->ent;
	const int isDamaged = (static_cast<double>(enemyEnt->maxhealth) * 0.8 > static_cast<double>(enemyEnt->health)) ? 1 : 0;
	const int isPlayer = enemyEnt->client != nullptr ? 1 : 0;

	float scarinessDiff;
	bool scary = isFullyAware != 0;
	if (!scary && isCurrentEnemy)
	{
		friendlyTimingOut = (LevelTime() - info->lastKnownPosTime) < 10000;
		scary = friendlyTimingOut != 0;
	}
	if (scary)
	{
		const float scarinessMe    = T4::game::Sentient_GetScarinessForDistance(self->sentient, dist);
		const float scarinessOther = T4::game::Sentient_GetScarinessForDistance(enemy, dist);
		scarinessDiff = scarinessMe - scarinessOther;
	}
	else
	{
		scarinessDiff = 0.0f;
	}

	int threat = enemy->iThreatBias + bias;

	int awareness = 0;
	if (isVisible)
		awareness = 1000;
	else if (isFullyAware)
		awareness = 500;
	else if (friendlyTimingOut)
		awareness = 250;
	threat += awareness;

	int scariness = static_cast<int>(scarinessDiff * -100.0f);
	if (scariness >= 1000)
		scariness = 1000;
	else if (scariness <= -500)
		scariness = -500;
	threat += scariness;

	// Vanilla WaW constants: 10000 cut-off, 3e-5 rate; rounding via x87 fistp (current mode).
	static const double kRoundBias = 9.313225746154785e-10;   // qword_8AF568 (2^-30)
	int distThreat;
	if (!(dist >= 10000.0f))
	{
		const float diff = 10000.0f - dist;
		float scaled = F32(0x37FBA882) * diff;
		scaled = scaled * diff;
		__asm
		{
			fld     scaled
			fadd    kRoundBias
			fistp   distThreat
		}
	}
	else
	{
		distThreat = 0;
	}

	threat += T4::game::Actor_ThreatFromAttackerCount(self, enemy, isCurrentEnemy) + distThreat;

	int bonus = 0;
	if (isCurrentEnemy)
	{
		if (isFullyAware)
			bonus = (isPlayer && isDamaged) ? 1000 : 500;
		else
			bonus = friendlyTimingOut ? 200 : 100;
	}
	threat += bonus;

	if (dist > 256.0f)
		threat += T4::game::Actor_ThreatCoveringFire(enemy, self);

	return T4::game::Actor_ThreatFlashed(enemy) + threat;
}

// ==========================================================
// Actor_UpdateThreat — sub_4E3900 (stdcall, retn 4)
// ==========================================================

// @modified — sub_4E3900 / CoD4 game/actor_threat.cpp Actor_UpdateThreat. 1:1 with vanilla except
//   sentientInfo/vis_blockers go through the T4M accessors. The bIgnoreAll branch keeps vanilla's
//   inlined Sentient_SetEnemy(self->sentient, NULL, 1).
//   DETOURED — do not call directly.
void __stdcall T4_Reconstructed::Actor_UpdateThreat(actor_s* self)
{
	if ((*T4::engine::ai_threatUpdateInterval)->current.integer)
	{
		if (LevelTime() < self->threatUpdateTime)
			return;
		T4::game::Actor_IncrementThreatTime(self);
	}

	if ((*T4::engine::ai_showPotentialThreatDir)->current.enabled)
		T4::game::Actor_PotentialThreat_Debug(self);

	sentient_s* const sentient = self->sentient;
	if (sentient->bIgnoreAll)
	{
		if (!sentient->targetEnt.number)
			return;
		if (!EntFromNumber(sentient->targetEnt.number))
			return;
		if (!sentient->targetEnt.number)
			return;

		sentient_s* const oldEnemy = EntFromNumber(sentient->targetEnt.number)->sentient;
		if (oldEnemy && sentient->iEnemyNotifyTime && LevelTime() >= sentient->iEnemyNotifyTime)
		{
			T4::game::Actor_BroadcastTeamEvent(sentient, 4);
			sentient->iEnemyNotifyTime = 0;
		}

		if (!EntFromNumber(sentient->targetEnt.number))
			return;

		if (oldEnemy)
		{
			if (sentient->iEnemyNotifyTime)
			{
				T4::game::Actor_BroadcastTeamEvent(sentient, 4);
				sentient->iEnemyNotifyTime = 0;
			}
			--oldEnemy->attackerCount;
		}

		actor_s* const actor = sentient->ent->actor;
		sentient->iEnemyNotifyTime = 0;
		T4::game::EntHandle_setEnt(&sentient->targetEnt, nullptr);

		T4::engine::scrVarPub_t* const varPub = T4::engine::gScrVarPub;
		if (varPub->timeArrayId && !varPub->error_message)
			T4::engine::Scr_NotifyNum_Internal(T4::engine::SCRIPTINSTANCE_SERVER, sentient->ent->s.number, 0,
			                                   T4::engine::scr_const->enemy, 0);

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
		T4_Reconstructed::Actor_UpdateLastEnemySightPos(actor);
		return;
	}

	if (sentient->scriptTargetEnt.number && sentient->entityTargetThreat == 1.0f)
	{
		T4::game::Sentient_SetEnemy(sentient, EntFromNumber(sentient->scriptTargetEnt.number), 1);
		return;
	}

	const int enemyTeam = EnemyTeamOf(sentient->eTeam);
	if (!enemyTeam)
		return;

	const int teamMask = 1 << enemyTeam;
	int bestThreat = static_cast<int>(0x80000001);
	gentity_s* bestEnt = nullptr;
	const unsigned char isUsingTurret = static_cast<unsigned char>(T4::game::Actor_IsUsingTurret(self));
	unsigned char bestIsThreatening = isUsingTurret;
	self->hasThreateningEnemy = 0;

	for (sentient_s* other = T4::game::Sentient_FirstSentient(teamMask); other; other = T4::game::Sentient_NextSentient(other, teamMask))
	{
		sentient_info_t* const info = T4M::Actor_SentientInfo(self, SentientIndex(other));
		if (info->lastKnownPosTime <= 0)
			continue;
		if (other->ent->flags & 4)
			continue;
		if (other->bIgnoreMe)
			continue;
		if (ThreatBias(other->iThreatBiasGroupIndex, self->sentient->iThreatBiasGroupIndex) == static_cast<int>(0x80000000))
			continue;

		T4::engine::gclient_s* const client = other->ent->client;
		if (client && client->sess.sessionState)
			continue;

		const int time = LevelTime();
		bool recentlyVisible;
		if (isUsingTurret)
		{
			recentlyVisible = true;
		}
		else
		{
			const int lastVisTime = info->VisCache.iLastVisTime;
			recentlyVisible = lastVisTime && time - lastVisTime < 10000;
		}

		const int lastAttackMeTime = info->iLastAttackMeTime;
		const bool recentlyAttacked = lastAttackMeTime && time - lastAttackMeTime < 10000;

		const unsigned char isThreatening = ((recentlyVisible || recentlyAttacked) && !(info->attackTime > time)) ? 1 : 0;
		self->hasThreateningEnemy = isThreatening;

		if (!isThreatening && bestIsThreatening)
			continue;
		if (self->pTurret && other->turretInvulnerability)
			continue;

		const int threat = T4_Reconstructed::Actor_UpdateSingleThreat(self, other);
		if (threat == static_cast<int>(0x80000000))
			continue;

		if (!(bestThreat < threat) && (bestIsThreatening || !isThreatening))
		{
			const unsigned short fav = self->pFavoriteEnemy.number;
			if (!fav)
				continue;
			if (other != SentientFromNumber(fav))
				continue;
		}

		const unsigned short fav = self->pFavoriteEnemy.number;
		if (fav && other == SentientFromNumber(fav))
			bestThreat = 0x7FFFFFFE;
		else
			bestThreat = threat;
		bestEnt = other->ent;
		bestIsThreatening = isThreatening;
	}

	sentient_s* const selfSentient = self->sentient;
	const unsigned short scriptTarget = selfSentient->scriptTargetEnt.number;
	if (scriptTarget)
	{
		const float scaled = static_cast<float>(bestThreat) * F32(0x3951B717);   // 1/5000
		if (selfSentient->entityTargetThreat > scaled)
			bestEnt = EntFromNumber(scriptTarget);
	}
	T4::game::Sentient_SetEnemy(selfSentient, bestEnt, 1);
}

// ==========================================================
// Actor_CanAttackAll — sub_4E3CF0 (stdcall, retn 4)
// ==========================================================

// @modified — sub_4E3CF0 / CoD4 game/actor_threat.cpp Actor_CanAttackAll. 1:1 with vanilla except
//   sentientInfo/vis_blockers go through the T4M accessors.
//   DETOURED — do not call directly.
void __stdcall T4_Reconstructed::Actor_CanAttackAll(actor_s* self)
{
	const int enemyTeam = EnemyTeamOf(self->sentient->eTeam);
	if (!enemyTeam)
		return;

	const int teamMask = 1 << enemyTeam;
	for (sentient_s* other = T4::game::Sentient_FirstSentient(teamMask); other; other = T4::game::Sentient_NextSentient(other, teamMask))
		T4M::Actor_SentientInfo(self, SentientIndex(other))->attackTime = 0;
}

// ==========================================================
// Actor_Turret_Think — 0x4E5060 (not an IDA proc, ends at 0x4E5359). Only reference: AI state
// function table of species 0 (off_8DC3C8[0] = 0x83CE38, 7 dwords per state), row AIS_TURRET
// (0x83CE70), slot pfnThink (+0x10) = 0x83CE80. Called as `mov ecx, actor ; call [tbl+state*0x1C+0x10]`
// from sub_4B6450 (0x4B650A).
// ==========================================================

// @modified — 0x4E5060 / CoD4 game/actor_turret.cpp Actor_Turret_Think. 1:1 with vanilla except
//   sentientInfo/vis_blockers go through the T4M accessors.
//   DETOURED — do not call directly.
int T4_Reconstructed::Actor_Turret_Think(actor_s* self)
{
	if (self->Path.wPathLen)
	{
		T4::game::Path_AddTrimmedAmount(&self->Path, self->ent->r.currentOrigin);
		T4::game::Path_Clear(&self->Path);
		self->iTeamMoveDodgeTime = 0;
	}

	gentity_s* const turret = self->pTurret;
	self->pPileUpActor = 0;
	self->pPileUpEnt = 0;

	if (!turret || !turret->r.ownerNum.number || EntFromNumber(turret->r.ownerNum.number) != self->ent)
		goto stop_use_turret;

	{
		T4::engine::TurretInfo* const turretInfo = turret->pTurretInfo;

		const unsigned short targetNum = self->sentient->targetEnt.number;
		sentient_s* const target = targetNum ? EntFromNumber(targetNum)->sentient : nullptr;
		bool knowAboutEnemy = false;
		if (target)
		{
			if (T4::game::Actor_CanSeeEnemyViaClaimedNode(self))
			{
				knowAboutEnemy = true;
			}
			else
			{
				const int lastKnownPosTime = T4M::Actor_SentientInfo(self, SentientIndex(target))->lastKnownPosTime;
				if (lastKnownPosTime && LevelTime() - lastKnownPosTime < 10000)
					knowAboutEnemy = true;
			}
		}
		if (!knowAboutEnemy)
			self->useEnemyGoal = 0;

		if (!T4::game::Actor_KeepClaimedNode(self))
		{
			T4::game::Actor_UpdateDesiredChainPos(self);
			T4_Reconstructed::Actor_UpdateGoalPos(self);
		}

		const int flags = turretInfo->flags;
		if (!(flags & 0x2000))
		{
			// Inlined Actor_PointNearGoal(turret origin, &self->codeGoal, 92).
			const float dz = turret->r.currentOrigin[2] - self->codeGoal.pos[2];
			const float height = self->codeGoal.height;
			if (dz * dz > height * height)
				goto stop_use_turret;

			const float dy = self->codeGoal.pos[1] - turret->r.currentOrigin[1];
			const float dx = self->codeGoal.pos[0] - turret->r.currentOrigin[0];
			const float radius = self->codeGoal.radius + 92.0f;
			float distSq = dy * dy;
			distSq = distSq + dx * dx;
			if (distSq > radius * radius)
				goto stop_use_turret;
		}

		gentity_s* const selfEnt = self->ent;
		T4::engine::tagInfo_s* const tagInfo = selfEnt->tagInfo;
		if (!tagInfo || tagInfo->parent != turret)
			goto stop_use_turret;
		if (static_cast<signed char>(flags) < 0)   // flags & 0x80
			goto stop_use_turret;

		if (self->pGrenade.number && !turret->tagInfo)
		{
			if (!T4::game::Actor_Grenade_IsPointSafe(self, selfEnt->r.currentOrigin))
			{
				T4::game::Actor_StopUseTurret(self);
				T4::game::Actor_SetState(self, 3);   // AIS_GRENADE_RESPONSE
				return 1;                           // ACTOR_THINK_REPEAT
			}
			T4::game::EntHandle_setEnt(&self->pGrenade, nullptr);
		}

		if (self->flashBanged)
			goto stop_use_turret;

		T4::game::Actor_PreThink(self);

		const char* debugInfo = nullptr;
		switch (turretInfo->state)
		{
		case 0:
			debugInfo = (turretInfo->flags & 2) ? "auto_turret_idle" : "manual_turret_idle";
			break;
		case 1:
			debugInfo = (turretInfo->flags & 2) ? "auto_turret_firing_head" : "manual_turret_firing_head";
			break;
		case 2:
			debugInfo = (turretInfo->flags & 2) ? "auto_turret_firing_feet" : "manual_turret_firing_feet";
			break;
		default:
			break;
		}
		if (debugInfo)
			self->pszDebugInfo = const_cast<char*>(debugInfo);

		const int weapon = self->pTurret->s.weapon;
		T4::engine::scr_animscript_t* const animScript = &T4::engine::g_scr_animWeapons[weapon];
		if (!animScript->func)
			T4::engine::Com_Error(1, "\x15no script specified for weapon info '%s' being used by AI",
			                      T4::engine::bg_weaponDefs.get()[weapon]->szInternalName);

		pathnode_t* const node = T4::game::Sentient_NearestNode(self->sentient);
		if (node && T4::game::Path_CanClaimNode(node, self->sentient))
			T4::game::Path_ForceClaimNode(node, self->sentient);

		T4::game::Actor_SetAnimScript(self, animScript, 0, 1);   // AI_MOVE_STOP, AI_ANIM_MOVE_CODE

		if (!static_cast<unsigned char>(T4::game::Actor_IsUsingTurret(self)))
			goto stop_use_turret;

		self->bUseGoalWeight = 0;
		return T4::game::Actor_Turret_PostThink(self);
	}

stop_use_turret:
	T4::game::Actor_StopUseTurret(self);
	T4::game::Actor_SetState(self, 1);   // AIS_EXPOSED
	return 1;                           // ACTOR_THINK_REPEAT
}

// @wrapper — usercall(self@ecx) -> eax ; retn. Caller (0x4B650A) reloads ecx/edx after the call.
__declspec(naked) void T4M::Actor_Turret_Think_Wrapper()
{
	__asm
	{
		push    ecx
		call    T4_Reconstructed::Actor_Turret_Think
		add     esp, 4
		retn
	}
}

void PatchT4MAM_SentientInfo_Threat()
{
	Detours::X86::DetourFunction(T4M::GetAddress("Actor_FlagEnemyUnattackable"),
	                             reinterpret_cast<uintptr_t>(&T4M::Actor_FlagEnemyUnattackable_Wrapper),
	                             Detours::X86Option::USE_JUMP);
	Detours::X86::DetourFunction(T4M::GetAddress("Actor_IsFullyAware"),
	                             reinterpret_cast<uintptr_t>(&T4M::Actor_IsFullyAware_Wrapper),
	                             Detours::X86Option::USE_JUMP);
	Detours::X86::DetourFunction(T4M::GetAddress("Actor_UpdateSingleThreat"),
	                             reinterpret_cast<uintptr_t>(&T4_Reconstructed::Actor_UpdateSingleThreat),
	                             Detours::X86Option::USE_JUMP);
	Detours::X86::DetourFunction(T4M::GetAddress("Actor_UpdateThreat"),
	                             reinterpret_cast<uintptr_t>(&T4_Reconstructed::Actor_UpdateThreat),
	                             Detours::X86Option::USE_JUMP);
	Detours::X86::DetourFunction(T4M::GetAddress("Actor_CanAttackAll"),
	                             reinterpret_cast<uintptr_t>(&T4_Reconstructed::Actor_CanAttackAll),
	                             Detours::X86Option::USE_JUMP);
	Detours::X86::DetourFunction(T4M::GetAddress("Actor_Turret_Think"),
	                             reinterpret_cast<uintptr_t>(&T4M::Actor_Turret_Think_Wrapper),
	                             Detours::X86Option::USE_JUMP);
}
