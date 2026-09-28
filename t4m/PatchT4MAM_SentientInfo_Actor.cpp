// ==========================================================
// T4M project
//
// Component: clientdll
// Purpose: AI limit phase 8.2 - reconstructions detoured so that their accesses to
//          actor_s::sentientInfo / vis_blockers go through T4M::Actor_SentientInfo /
//          T4M::Actor_VisBlocker (side table, see PatchT4MAM_ActorSentientInfo.cpp).
//          CoD4 actor.cpp / actor_cover.cpp: SentientInfo_Copy, Actor_DissociateSentient, Actor_Pain,
//          Actor_InFixedNodeExposedCombat, Actor_UpdateGoalPos, Actor_Cover_CheckWithEnemy
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
using T4::engine::pathnode_t;
using T4::engine::pathnodeRange_t;

// cod/ ASSERT_* macros are inert in C++: every offset this file relies on is checked here.
static_assert(sizeof(sentient_s) == 0x88, "sentient_s");
static_assert(offsetof(sentient_s, eTeam) == 0x4, "sentient_s::eTeam");
static_assert(offsetof(sentient_s, iThreatBiasGroupIndex) == 0xC, "sentient_s::iThreatBiasGroupIndex");
static_assert(offsetof(sentient_s, bIgnoreMe) == 0x10, "sentient_s::bIgnoreMe");
static_assert(offsetof(sentient_s, iEnemyNotifyTime) == 0x24, "sentient_s::iEnemyNotifyTime");
static_assert(offsetof(sentient_s, attackerCount) == 0x28, "sentient_s::attackerCount");
static_assert(offsetof(sentient_s, lastAttacker) == 0x2C, "sentient_s::lastAttacker");
static_assert(offsetof(sentient_s, syncedMeleeEnt) == 0x30, "sentient_s::syncedMeleeEnt");
static_assert(offsetof(sentient_s, targetEnt) == 0x34, "sentient_s::targetEnt");
static_assert(offsetof(sentient_s, bInMeleeCharge) == 0x84, "sentient_s::bInMeleeCharge");
static_assert(sizeof(gentity_s) == 0x378, "gentity_s");
static_assert(offsetof(gentity_s, r) + offsetof(T4::engine::entityShared_s, currentOrigin) == 0x160, "gentity_s::r.currentOrigin");
static_assert(offsetof(gentity_s, r) + offsetof(T4::engine::entityShared_s, currentAngles) == 0x16C, "gentity_s::r.currentAngles");
static_assert(offsetof(gentity_s, actor) == 0x184, "gentity_s::actor");
static_assert(offsetof(gentity_s, sentient) == 0x188, "gentity_s::sentient");
static_assert(offsetof(gentity_s, flags) == 0x1B4, "gentity_s::flags");
static_assert(offsetof(gentity_s, u) == 0x258, "gentity_s::u");
static_assert(offsetof(actor_s, species) == 0x8, "actor_s::species");
static_assert(offsetof(actor_s, eState) == 0xB8C, "actor_s::eState");
static_assert(offsetof(actor_s, stateLevel) == 0xBB4, "actor_s::stateLevel");
static_assert(offsetof(actor_s, allowPain) == 0xC38, "actor_s::allowPain");
static_assert(offsetof(actor_s, iDamageTaken) == 0xD54, "actor_s::iDamageTaken");
static_assert(offsetof(actor_s, iDamageYaw) == 0xD58, "actor_s::iDamageYaw");
static_assert(offsetof(actor_s, damageDir) == 0xD5C, "actor_s::damageDir");
static_assert(offsetof(actor_s, damageHitLoc) == 0xD68, "actor_s::damageHitLoc");
static_assert(offsetof(actor_s, damageWeapon) == 0xD6A, "actor_s::damageWeapon");
static_assert(offsetof(actor_s, damageMod) == 0xD6C, "actor_s::damageMod");
static_assert(offsetof(actor_s, pAnimScriptFunc) == 0xD78, "actor_s::pAnimScriptFunc");
static_assert(offsetof(actor_s, iFollowMin) == 0x1944, "actor_s::iFollowMin");
static_assert(offsetof(actor_s, iFollowMax) == 0x1948, "actor_s::iFollowMax");
static_assert(offsetof(actor_s, codeGoal) == 0x198C, "actor_s::codeGoal");
static_assert(offsetof(actor_s, codeGoalSrc) == 0x19B4, "actor_s::codeGoalSrc");
static_assert(offsetof(actor_s, scriptGoal) == 0x19B8, "actor_s::scriptGoal");
static_assert(offsetof(actor_s, scriptGoalEnt) == 0x19E0, "actor_s::scriptGoalEnt");
static_assert(offsetof(actor_s, pathEnemyFightDist) == 0x19E8, "actor_s::pathEnemyFightDist");
static_assert(offsetof(actor_s, useEnemyGoal) == 0x19F0, "actor_s::useEnemyGoal");
static_assert(offsetof(actor_s, pDesiredChainPos) == 0x1A08, "actor_s::pDesiredChainPos");
static_assert(offsetof(actor_s, arrivalInfo) + offsetof(T4::engine::ActorCoverArrivalInfo, animscriptOverrideRunTo) == 0x1A10, "actor_s::arrivalInfo.animscriptOverrideRunTo");
static_assert(offsetof(actor_s, iPotentialCoverNodeCount) == 0x1A38, "actor_s::iPotentialCoverNodeCount");
static_assert(offsetof(actor_s, iPotentialReacquireNodeCount) == 0x1A64, "actor_s::iPotentialReacquireNodeCount");
static_assert(offsetof(actor_s, fovDot) == 0x1A7C, "actor_s::fovDot");
static_assert(offsetof(actor_s, fMaxSightDistSqrd) == 0x1A80, "actor_s::fMaxSightDistSqrd");
static_assert(offsetof(actor_s, talkToSpecies) == 0x202C, "actor_s::talkToSpecies");
static_assert(offsetof(actor_s, lastEnemySightPosValid) == 0x203C, "actor_s::lastEnemySightPosValid");
static_assert(offsetof(actor_s, pGrenade) == 0x20E8, "actor_s::pGrenade");
static_assert(offsetof(T4::engine::actor_goal_s, ang) == 0xC, "actor_goal_s::ang");
static_assert(offsetof(T4::engine::actor_goal_s, radius) == 0x18, "actor_goal_s::radius");
static_assert(offsetof(T4::engine::actor_goal_s, height) == 0x1C, "actor_goal_s::height");
static_assert(offsetof(T4::engine::actor_goal_s, node) == 0x20, "actor_goal_s::node");
static_assert(offsetof(T4::engine::actor_goal_s, volume) == 0x24, "actor_goal_s::volume");
static_assert(offsetof(T4::engine::pathnode_constant_t, vOrigin) == 0x14, "pathnode_constant_t::vOrigin");
static_assert(offsetof(T4::engine::pathnode_constant_t, fRadius) == 0x2C, "pathnode_constant_t::fRadius");
static_assert(sizeof(pathnodeRange_t) == 0xC, "pathnodeRange_t");
static_assert(offsetof(T4::engine::level_locals_s, time) == 0x1040, "level_locals_s::time");
static_assert(offsetof(T4::engine::scrVarPub_t, error_message) == 0xC, "scrVarPub_t::error_message");
static_assert(offsetof(T4::engine::scrVarPub_t, timeArrayId) == 0x18, "scrVarPub_t::timeArrayId");
static_assert(offsetof(T4::engine::scr_const_t, enemy) == 0x244, "scr_const_t::enemy");
static_assert(offsetof(T4::engine::threat_bias_t, threatTable) == 0x20, "threat_bias_t::threatTable");
static_assert(sizeof(T4::engine::ai_funcs_t) == 0x1C, "ai_funcs_t");
static_assert(offsetof(T4::engine::ai_funcs_t, pfnPain) == 0x18, "ai_funcs_t::pfnPain");

// Naked-wrapper helpers (used between __asm blocks, never inside one): the vanilla entries below leave every XMM register they do not
// use intact and LTCG callers rely on it (sub_4BB8F0 / sub_4BBB50 keep xmm5-7 live across
// Actor_UpdateGoalPos). The C++ side may use any XMM register, so wrappers keep all eight.
#define T4M_SAVE_XMM                                          \
	__asm sub    esp, 80h                                     \
	__asm movdqu [esp+00h], xmm0                              \
	__asm movdqu [esp+10h], xmm1                              \
	__asm movdqu [esp+20h], xmm2                              \
	__asm movdqu [esp+30h], xmm3                              \
	__asm movdqu [esp+40h], xmm4                              \
	__asm movdqu [esp+50h], xmm5                              \
	__asm movdqu [esp+60h], xmm6                              \
	__asm movdqu [esp+70h], xmm7

#define T4M_RESTORE_XMM                                       \
	__asm movdqu xmm0, [esp+00h]                              \
	__asm movdqu xmm1, [esp+10h]                              \
	__asm movdqu xmm2, [esp+20h]                              \
	__asm movdqu xmm3, [esp+30h]                              \
	__asm movdqu xmm4, [esp+40h]                              \
	__asm movdqu xmm5, [esp+50h]                              \
	__asm movdqu xmm6, [esp+60h]                              \
	__asm movdqu xmm7, [esp+70h]                              \
	__asm add    esp, 80h

namespace
{
	// Vanilla: sub reg, level.sentients ; imul 78787879h ; sar edx, 6 ; add sign bit.
	int SentientIndex(const sentient_s* sentient)
	{
		const int diff = static_cast<int>(reinterpret_cast<uintptr_t>(sentient)
		                                - reinterpret_cast<uintptr_t>(*T4::engine::g_sentients));
		return diff / static_cast<int>(sizeof(sentient_s));
	}

	// Vanilla copies these floats with fld/fstp.
	void CopyFloatX87(float* dst, const float* src)
	{
		__asm
		{
			mov  eax, src
			mov  ecx, dst
			fld  dword ptr [eax]
			fstp dword ptr [ecx]
		}
	}

	// SL_AddRefToString, inlined by vanilla: lock xadd on the 12-byte string node refcount.
	void SL_AddRefToString_Inlined(unsigned int stringValue)
	{
		char* const buf = reinterpret_cast<char*>(T4::engine::gScrMemTreePub->mt_buffer);
		InterlockedExchangeAdd(reinterpret_cast<volatile LONG*>(buf + stringValue * 12), 1);
	}
}

// ==========================================================
// sub_4B4650 — SentientInfo_Copy
// ==========================================================

// @modified — sub_4B4650 / CoD4 game/actor.cpp SentientInfo_Copy. 1:1 with vanilla except sentientInfo/vis_blockers go through the T4M accessors.
//   DETOURED — do not call directly.
void T4_Reconstructed::SentientInfo_Copy(actor_s* pTo, const actor_s* pFrom, int index)
{
	if (!(pFrom->talkToSpecies & (1 << pTo->species)))
		return;

	sentient_info_t* const to   = T4M::Actor_SentientInfo(pTo, index);
	sentient_info_t* const from = T4M::Actor_SentientInfo(const_cast<actor_s*>(pFrom), index);

	if (to->lastKnownPosTime >= from->lastKnownPosTime)   // jge: signed
		return;

	to->iLastAttackMeTime = 0;
	to->attackTime        = 0;
	to->lastKnownPosTime  = from->lastKnownPosTime;
	CopyFloatX87(&to->vLastKnownPos[0], &from->vLastKnownPos[0]);
	CopyFloatX87(&to->vLastKnownPos[1], &from->vLastKnownPos[1]);
	CopyFloatX87(&to->vLastKnownPos[2], &from->vLastKnownPos[2]);
	to->pLastKnownNode = from->pLastKnownNode;
}

// @wrapper — usercall(index@eax, pTo@edx, pFrom@esi) ; retn. Vanilla clobbers only eax/ecx;
//   eax is left as index*0x28 once the talkToSpecies gate passed, index otherwise.
__declspec(naked) void T4M::SentientInfo_Copy_Wrapper()
{
	__asm
	{
		push    ecx
		push    edx
	}
	T4M_SAVE_XMM
	__asm
	{
		push    eax                         ; kept for the eax residue
		push    eax                         ; index
		push    esi                         ; pFrom
		push    edx                         ; pTo
		call    T4_Reconstructed::SentientInfo_Copy
		add     esp, 0Ch
		pop     eax
	}
	T4M_RESTORE_XMM
	__asm
	{
		pop     edx
		pop     ecx

		push    edi
		push    ecx
		mov     ecx, [edx+8]
		mov     edi, 1
		shl     edi, cl
		test    [esi+202Ch], edi
		pop     ecx
		pop     edi
		jz      done
		lea     eax, [eax+eax*4]
		shl     eax, 3
	done:
		retn
	}
}

// ==========================================================
// sub_4B56F0 — Actor_DissociateSentient
// ==========================================================

// @modified — sub_4B56F0 / CoD4 game/actor.cpp Actor_DissociateSentient. 1:1 with vanilla except sentientInfo/vis_blockers go through the T4M accessors.
//   Sentient_SetEnemy(self->sentient, NULL, 1) is inlined, as in vanilla. The CoD4 eOtherTeam
//   argument does not exist in WaW.
//   DETOURED — do not call directly.
void T4_Reconstructed::Actor_DissociateSentient(actor_s* self, sentient_s* other)
{
	memset(T4M::Actor_SentientInfo(self, SentientIndex(other)), 0, sizeof(sentient_info_t));
	T4::game::Actor_DissociateSuppressor(self, other);

	sentient_s* const sentient = self->sentient;
	gentity_s*  const ents     = T4::engine::g_entities.get();

	const sentient_s* const target = sentient->targetEnt.number
	                               ? ents[sentient->targetEnt.number - 1].sentient
	                               : nullptr;
	if (other != target)
		return;
	if (!sentient->targetEnt.number)
		return;

	sentient_s* const enemy = ents[sentient->targetEnt.number - 1].sentient;
	if (enemy && sentient->iEnemyNotifyTime && T4::engine::level->time >= sentient->iEnemyNotifyTime)
	{
		T4::game::Actor_BroadcastTeamEvent(sentient, 4);
		sentient->iEnemyNotifyTime = 0;
	}

	// targetEnt.ent() != NULL, computed without a null short-cut as vanilla does.
	const uintptr_t enemyEnt = reinterpret_cast<uintptr_t>(ents)
	                         + static_cast<uintptr_t>((sentient->targetEnt.number - 1) * 0x378);
	if (!enemyEnt)
		return;

	if (enemy)
	{
		if (sentient->iEnemyNotifyTime)
		{
			T4::game::Actor_BroadcastTeamEvent(sentient, 4);
			sentient->iEnemyNotifyTime = 0;
		}
		--enemy->attackerCount;
	}

	actor_s* const actor = sentient->ent->actor;
	sentient->iEnemyNotifyTime = 0;
	T4::game::EntHandle_setEnt(&sentient->targetEnt, nullptr);

	// Scr_IsSystemActive()
	if (T4::engine::gScrVarPub->timeArrayId && !T4::engine::gScrVarPub->error_message)
		T4::engine::Scr_NotifyNum_Internal(T4::engine::SCRIPTINSTANCE_SERVER, sentient->ent->s.number, 0,
		                                   T4::engine::scr_const->enemy, 0);

	if (!actor)
		return;

	if (actor->useEnemyGoal)
	{
		actor->useEnemyGoal = 0;
		T4_Reconstructed::Actor_UpdateGoalPos(actor);
	}
	actor->iPotentialCoverNodeCount     = 0;
	actor->iPotentialReacquireNodeCount = 0;
	actor->lastEnemySightPosValid       = 0;
	T4_Reconstructed::Actor_UpdateLastEnemySightPos(actor);
}

// @wrapper — usercall(self@ecx, other@eax) ; retn. Vanilla keeps ecx (push/pop ecx), ebx, ebp, esi, edi.
__declspec(naked) void T4M::Actor_DissociateSentient_Wrapper()
{
	__asm
	{
		push    ecx
		push    edx
	}
	T4M_SAVE_XMM
	__asm
	{
		push    eax                         ; other
		push    ecx                         ; self
		call    T4_Reconstructed::Actor_DissociateSentient
		add     esp, 8
	}
	T4M_RESTORE_XMM
	__asm
	{
		pop     edx
		pop     ecx
		retn
	}
}

// ==========================================================
// sub_4B6870 — Actor_Pain
// ==========================================================

// @modified — sub_4B6870 / CoD4 game/actor.cpp Actor_Pain. 1:1 with vanilla except sentientInfo/vis_blockers go through the T4M accessors.
//   Plain cdecl, reached through the entity handler table (.data 0x8DBD18) — no wrapper.
//   vectoyaw, G_GetHitLocationString, Scr_SetString and Actor_WasAttackedBy are inlined, as in vanilla.
//   DETOURED — do not call directly.
void T4_Reconstructed::Actor_Pain(gentity_s* self, gentity_s* pAttacker, int iDamage, const float* vPoint,
                                  int iMod, const float* vDir, int hitLoc, int weaponIdx)
{
	actor_s* const actor = self->actor;
	actor->iDamageTaken = iDamage;

	// vectoyaw(vDir)
	float yaw = 0.0f;
	if (vDir[1] != 0.0f || vDir[0] != 0.0f)
	{
		const float a = static_cast<float>(T4::engine::libm_sse2_atan2(static_cast<double>(vDir[1]),
		                                                               static_cast<double>(vDir[0])))
		              * 57.2957763671875f;          // 0x42652EE0, not 180/pi rounded
		float base = 0.0f;
		if (!(a >= 0.0f))                           // comiss ; jnb: NaN takes the +360 path
			base = 360.0f;
		yaw = base + a;
	}

	const float turns = (yaw - self->r.currentAngles[1]) * 0.0027777778f;   // 0x3B360B61
	const float whole = T4::engine::floorf_sse(turns + 0.5f);
	actor->iDamageYaw = static_cast<int>((turns - whole) * 360.0f);

	CopyFloatX87(&actor->damageDir[0], &vDir[0]);
	CopyFloatX87(&actor->damageDir[1], &vDir[1]);
	CopyFloatX87(&actor->damageDir[2], &vDir[2]);

	const unsigned int hitLocStr = T4::engine::g_HitLocConstNames.get()[hitLoc];
	if (hitLocStr)
		SL_AddRefToString_Inlined(hitLocStr);
	if (static_cast<unsigned short>(actor->damageHitLoc))
		T4::engine::SL_RemoveRefToString(static_cast<unsigned short>(actor->damageHitLoc), T4::engine::SCRIPTINSTANCE_SERVER);
	actor->damageHitLoc = static_cast<__int16>(hitLocStr);

	const unsigned int modStr = *T4::engine::modNames.get()[iMod];
	if (modStr)
		SL_AddRefToString_Inlined(modStr);
	if (static_cast<unsigned short>(actor->damageMod))
		T4::engine::SL_RemoveRefToString(static_cast<unsigned short>(actor->damageMod), T4::engine::SCRIPTINSTANCE_SERVER);
	actor->damageMod = static_cast<__int16>(modStr);

	if (pAttacker)
		T4::engine::Scr_SetStringFromCharString(T4::engine::bg_weaponDefs.get()[weaponIdx]->szInternalName,
		                                        reinterpret_cast<unsigned short*>(&actor->damageWeapon));

	const T4::engine::ai_funcs_t* const funcs = T4::engine::AIFuncTable.get()[actor->species];
	funcs[actor->eState[actor->stateLevel]].pfnPain(actor, pAttacker, iDamage, vPoint, iMod, vDir,
	                                                static_cast<T4::engine::hitLocation_t>(hitLoc));

	self->sentient->lastAttacker = pAttacker;

	// Actor_WasAttackedBy
	sentient_s* const attacker = pAttacker->sentient;
	if (attacker)
		T4M::Actor_SentientInfo(actor, SentientIndex(attacker))->iLastAttackMeTime = T4::engine::level->time;

	if (!actor->sentient->syncedMeleeEnt.number && actor->allowPain)
	{
		if (T4::game::Actor_PushState(actor, T4::engine::AIS_PAIN))
			T4::game::Actor_KillAnimScript(actor);
	}
}

// ==========================================================
// sub_4BB7F0 — Actor_InFixedNodeExposedCombat
// ==========================================================

// @modified — sub_4BB7F0 / CoD4 game/actor.cpp Actor_InFixedNodeExposedCombat. 1:1 with vanilla except sentientInfo/vis_blockers go through the T4M accessors.
//   Actor_GetTargetEntity and Actor_PointNearPoint are inlined, as in vanilla.
//   DETOURED — do not call directly.
bool T4_Reconstructed::Actor_InFixedNodeExposedCombat(actor_s* self)
{
	const unsigned short targetNum = self->sentient->targetEnt.number;
	if (!targetNum)
		return false;

	const gentity_s* const target = &T4::engine::g_entities.get()[targetNum - 1];
	const sentient_s* const sentient = target->sentient;
	if (sentient)
	{
		const int since = static_cast<int>(static_cast<unsigned int>(T4::engine::level->time)
		                - static_cast<unsigned int>(T4M::Actor_SentientInfo(self, SentientIndex(sentient))->lastKnownPosTime));
		if (since > 10000)
			return false;
	}

	float buffer = 64.0f;
	const float radius = self->codeGoal.radius;
	if (64.0f - radius >= 0.0f)                     // comiss ; jb: NaN keeps 64
		buffer = radius;

	if (self->pAnimScriptFunc != T4::engine::g_scr_data_anim.get())
		return false;

	const float* const origin = self->ent->r.currentOrigin;
	const float dz = origin[2] - self->codeGoal.pos[2];
	if (dz * dz > 6400.0f)
		return false;

	const float dy = self->codeGoal.pos[1] - origin[1];
	const float dx = self->codeGoal.pos[0] - origin[0];
	const float distSq = dy * dy + dx * dx;
	return buffer * buffer >= distSq;               // comiss ; jb: NaN -> false
}

// @wrapper — usercall(self@ecx) -> bool@al ; retn. Vanilla keeps ecx, ebx, ebp, esi, edi and xmm4-7.
__declspec(naked) void T4M::Actor_InFixedNodeExposedCombat_Wrapper()
{
	__asm
	{
		push    ecx
		push    edx
	}
	T4M_SAVE_XMM
	__asm
	{
		push    ecx                         ; self
		call    T4_Reconstructed::Actor_InFixedNodeExposedCombat
		add     esp, 4
	}
	T4M_RESTORE_XMM
	__asm
	{
		pop     edx
		pop     ecx
		retn
	}
}

// ==========================================================
// sub_4BE5D0 — Actor_UpdateGoalPos
// ==========================================================

// @modified — sub_4BE5D0 / CoD4 game/actor.cpp Actor_UpdateGoalPos. 1:1 with vanilla except sentientInfo/vis_blockers go through the T4M accessors.
//   Actor_GetTargetSentient, Sentient_EnemyTeam, Actor_SetGoalRadius/Height and the last
//   Actor_CheckOverridePos are inlined, as in vanilla. WaW also copies scriptGoal.ang and
//   sets codeGoal.node on the friendly-chain path.
//   DETOURED — do not call directly.
void T4_Reconstructed::Actor_UpdateGoalPos(actor_s* self)
{
	float prevGoalPos[3];
	prevGoalPos[0] = self->codeGoal.pos[0];
	prevGoalPos[1] = self->codeGoal.pos[1];
	prevGoalPos[2] = self->codeGoal.pos[2];

	gentity_s* const ents = T4::engine::g_entities.get();

	if (self->useEnemyGoal)
	{
		const unsigned short targetNum = self->sentient->targetEnt.number;
		const sentient_s* const enemy = targetNum ? ents[targetNum - 1].sentient : nullptr;
		const sentient_info_t* const info = T4M::Actor_SentientInfo(self, SentientIndex(enemy));

		CopyFloatX87(&self->codeGoal.pos[0], &info->vLastKnownPos[0]);
		CopyFloatX87(&self->codeGoal.pos[1], &info->vLastKnownPos[1]);
		CopyFloatX87(&self->codeGoal.pos[2], &info->vLastKnownPos[2]);

		float radius = self->pathEnemyFightDist;
		self->codeGoalSrc = T4::engine::AI_GOAL_SRC_ENEMY;
		self->codeGoal.node = nullptr;
		self->codeGoal.volume = nullptr;
		if (4.0f > radius)
			radius = 4.0f;
		self->codeGoal.radius = radius;
	}
	else
	{
		const unsigned short goalEntNum = self->scriptGoalEnt.number;
		if (goalEntNum)
		{
			self->codeGoal.node = nullptr;
			self->codeGoal.volume = nullptr;

			const sentient_s* const goalSentient = ents[goalEntNum - 1].sentient;
			if (goalSentient)
			{
				const int enemyTeam[5] = { 0, 2, 1, 0, 0 };   // Sentient_EnemyTeam, a stack table in vanilla
				if (goalSentient->eTeam != enemyTeam[self->sentient->eTeam]
				 && self->iFollowMin <= self->iFollowMax)
				{
					self->codeGoalSrc = T4::engine::AI_GOAL_SRC_FRIENDLY_CHAIN;

					pathnode_t* const chainPos = self->pDesiredChainPos;
					if (!chainPos)
					{
						const float* const origin = self->ent->r.currentOrigin;
						CopyFloatX87(&self->codeGoal.pos[0], &origin[0]);
						CopyFloatX87(&self->codeGoal.pos[1], &origin[1]);
						CopyFloatX87(&self->codeGoal.pos[2], &origin[2]);
						T4::game::Actor_CheckOverridePos(self, prevGoalPos);
						return;
					}

					CopyFloatX87(&self->codeGoal.pos[0], &chainPos->constant.vOrigin[0]);
					CopyFloatX87(&self->codeGoal.pos[1], &chainPos->constant.vOrigin[1]);
					CopyFloatX87(&self->codeGoal.pos[2], &chainPos->constant.vOrigin[2]);

					float radius = chainPos->constant.fRadius;
					if (radius != 0.0f)             // ucomiss ; jnp: NaN counts as non-zero
					{
						if (4.0f > radius)
							radius = 4.0f;
						self->codeGoal.radius = radius;

						float height = self->scriptGoal.height;
						if (80.0f > height)
							height = 80.0f;
						self->codeGoal.height = height;
					}
					self->codeGoal.node = chainPos;
					T4::game::Actor_CheckOverridePos(self, prevGoalPos);
					return;
				}
			}

			const float* const origin = ents[goalEntNum - 1].r.currentOrigin;
			CopyFloatX87(&self->codeGoal.pos[0], &origin[0]);
			CopyFloatX87(&self->codeGoal.pos[1], &origin[1]);
			CopyFloatX87(&self->codeGoal.pos[2], &origin[2]);
			self->codeGoalSrc = T4::engine::AI_GOAL_SRC_SCRIPT_ENTITY_GOAL;
		}
		else
		{
			CopyFloatX87(&self->codeGoal.pos[0], &self->scriptGoal.pos[0]);
			CopyFloatX87(&self->codeGoal.pos[1], &self->scriptGoal.pos[1]);
			CopyFloatX87(&self->codeGoal.pos[2], &self->scriptGoal.pos[2]);
			CopyFloatX87(&self->codeGoal.ang[0], &self->scriptGoal.ang[0]);
			CopyFloatX87(&self->codeGoal.ang[1], &self->scriptGoal.ang[1]);
			CopyFloatX87(&self->codeGoal.ang[2], &self->scriptGoal.ang[2]);
			pathnode_t* const node = self->scriptGoal.node;
			gentity_s* const volume = self->scriptGoal.volume;
			self->codeGoalSrc = T4::engine::AI_GOAL_SRC_SCRIPT_GOAL;
			self->codeGoal.node = node;
			self->codeGoal.volume = volume;
		}

		const sentient_s* const sentient = self->sentient;
		float radius = self->scriptGoal.radius;
		if (sentient && sentient->bInMeleeCharge && radius > 64.0f)
			radius = 64.0f;
		if (4.0f > radius)
			radius = 4.0f;
		self->codeGoal.radius = radius;

		float height = self->scriptGoal.height;
		if (80.0f > height)
			height = 80.0f;
		self->codeGoal.height = height;
	}

	// Actor_CheckOverridePos, inlined here by vanilla.
	if (self->arrivalInfo.animscriptOverrideRunTo
	 && (self->codeGoal.pos[0] != prevGoalPos[0]
	  || self->codeGoal.pos[1] != prevGoalPos[1]
	  || self->codeGoal.pos[2] != prevGoalPos[2]))
	{
		self->arrivalInfo.animscriptOverrideRunTo = 0;
	}
}

// @wrapper — usercall(self@ecx) ; retn. Vanilla keeps ecx, ebx, ebp, esi, edi and xmm5-7;
//   sub_4BB8F0 and sub_4BBB50 read xmm5-7 right after the call.
__declspec(naked) void T4M::Actor_UpdateGoalPos_Wrapper()
{
	__asm
	{
		push    ecx
		push    edx
	}
	T4M_SAVE_XMM
	__asm
	{
		push    ecx                         ; self
		call    T4_Reconstructed::Actor_UpdateGoalPos
		add     esp, 4
	}
	T4M_RESTORE_XMM
	__asm
	{
		pop     edx
		pop     ecx
		retn
	}
}

// ==========================================================
// sub_4C4170 — Actor_Cover_CheckWithEnemy
// ==========================================================

// @modified — sub_4C4170 / CoD4 game/actor_cover.cpp Actor_Cover_CheckWithEnemy. 1:1 with vanilla except sentientInfo/vis_blockers go through the T4M accessors.
//   Actor_GetTargetEntity/Sentient, Actor_CanSeeEntity and Actor_CheckIgnore are inlined, as in
//   vanilla. WaW keeps a single pathnodeRange_t and reads the grenade position at gentity_s + 0x258.
//   DETOURED — do not call directly.
bool T4_Reconstructed::Actor_Cover_CheckWithEnemy(actor_s* self, const pathnode_t* node, bool checkEnemyRange)
{
	pathnodeRange_t range;
	T4::game::Actor_Cover_InitRange(node, &range);

	const sentient_s* const selfSentient = self->sentient;
	gentity_s* const ents = T4::engine::g_entities.get();
	const unsigned short targetNum = selfSentient->targetEnt.number;
	gentity_s*  const targetEnt      = targetNum ? &ents[targetNum - 1] : nullptr;
	sentient_s* const targetSentient = targetNum ? ents[targetNum - 1].sentient : nullptr;

	if (targetEnt)
	{
		if (!targetSentient)
		{
			if (!checkEnemyRange
			 && !T4_Reconstructed::Actor_CanSeeEntityEx(self, targetEnt, self->fovDot, self->fMaxSightDistSqrd))
				return true;
			return T4::game::Actor_Cover_NodeRangeValid(targetEnt->r.currentOrigin, node, &range);
		}
	}
	else if (!targetSentient)
	{
		if (!checkEnemyRange)
			return true;
		const unsigned short grenadeNum = self->pGrenade.number;
		if (!grenadeNum)
			return true;
		const float* const grenadePos = reinterpret_cast<const float*>(&ents[grenadeNum - 1].u);
		return T4::game::Actor_Cover_NodeRangeValid(grenadePos, node, &range);
	}

	if (targetSentient->ent->flags & 4)
		return true;
	if (targetSentient->bIgnoreMe)
		return true;
	if (T4::engine::g_threatBias->threatTable[targetSentient->iThreatBiasGroupIndex][selfSentient->iThreatBiasGroupIndex]
	    == static_cast<int>(0x80000000))
		return true;

	const sentient_info_t* const info = T4M::Actor_SentientInfo(self, SentientIndex(targetSentient));
	if (info->lastKnownPosTime <= 0)
		return true;
	if (!checkEnemyRange && !T4::game::Actor_CanSeeEnemy(self))
		return true;
	if (T4::game::Actor_Cover_NodeRangeValid(targetSentient->ent->r.currentOrigin, node, &range))
		return true;
	return T4::game::Actor_Cover_NodeRangeValid(info->vLastKnownPos, node, &range);
}

// @wrapper — usercall(self@eax, node@stack0, checkEnemyRange@stack1) -> eax (0/1, tested as a dword
//   by sub_4C4300) ; retn 8. Vanilla keeps ebx, ebp, esi, edi.
__declspec(naked) void T4M::Actor_Cover_CheckWithEnemy_Wrapper()
{
	__asm
	{
		push    ecx
		push    edx
	}
	T4M_SAVE_XMM
	__asm
	{
		; [esp+88h] ret, [esp+8Ch] node, [esp+90h] checkEnemyRange
		movzx   ecx, byte ptr [esp+90h]
		push    ecx                         ; checkEnemyRange
		push    dword ptr [esp+90h]         ; node
		push    eax                         ; self
		call    T4_Reconstructed::Actor_Cover_CheckWithEnemy
		add     esp, 0Ch
		movzx   eax, al
	}
	T4M_RESTORE_XMM
	__asm
	{
		pop     edx
		pop     ecx
		retn    8
	}
}

void PatchT4MAM_SentientInfo_Actor()
{
	Detours::X86::DetourFunction(T4M::GetAddress("SentientInfo_Copy"),
	                             reinterpret_cast<uintptr_t>(&T4M::SentientInfo_Copy_Wrapper),
	                             Detours::X86Option::USE_JUMP);
	Detours::X86::DetourFunction(T4M::GetAddress("Actor_DissociateSentient"),
	                             reinterpret_cast<uintptr_t>(&T4M::Actor_DissociateSentient_Wrapper),
	                             Detours::X86Option::USE_JUMP);
	Detours::X86::DetourFunction(T4M::GetAddress("Actor_Pain"),
	                             reinterpret_cast<uintptr_t>(&T4_Reconstructed::Actor_Pain),
	                             Detours::X86Option::USE_JUMP);
	Detours::X86::DetourFunction(T4M::GetAddress("Actor_InFixedNodeExposedCombat"),
	                             reinterpret_cast<uintptr_t>(&T4M::Actor_InFixedNodeExposedCombat_Wrapper),
	                             Detours::X86Option::USE_JUMP);
	Detours::X86::DetourFunction(T4M::GetAddress("Actor_UpdateGoalPos"),
	                             reinterpret_cast<uintptr_t>(&T4M::Actor_UpdateGoalPos_Wrapper),
	                             Detours::X86Option::USE_JUMP);
	Detours::X86::DetourFunction(T4M::GetAddress("Actor_Cover_CheckWithEnemy"),
	                             reinterpret_cast<uintptr_t>(&T4M::Actor_Cover_CheckWithEnemy_Wrapper),
	                             Detours::X86Option::USE_JUMP);
}
