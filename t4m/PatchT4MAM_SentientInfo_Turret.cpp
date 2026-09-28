// ==========================================================
// T4M project
//
// Component: clientdll
// Purpose: AI limit phase 8.2 - reconstructions detoured so that their accesses to
//          actor_s::sentientInfo / vis_blockers go through T4M::Actor_SentientInfo /
//          T4M::Actor_VisBlocker (side table, see PatchT4MAM_ActorSentientInfo.cpp).
//          CoD4 turret.cpp: turret_think_auto, turret_think_manual, turret_canuse_auto
//
//          R&D analysis/actor_sentientinfo_sidetable.md
//
// Started: 2026-09-19
// ==========================================================

#include "StdInc.h"

using T4::engine::actor_s;
using T4::engine::sentient_info_t;

using T4::engine::gentity_s;
using T4::engine::sentient_s;
using T4::engine::TurretInfo;

static_assert(offsetof(T4::engine::level_locals_s, time) == 0x1040, "level.time");
static_assert(sizeof(sentient_s) == 0x88, "sentient_s layout");
static_assert(offsetof(sentient_s, bIgnoreMe) == 0x10, "sentient_s::bIgnoreMe");
static_assert(offsetof(sentient_s, targetEnt) == 0x34, "sentient_s::targetEnt");
static_assert(sizeof(gentity_s) == 0x378, "gentity_s layout");
static_assert(offsetof(gentity_s, r) + offsetof(T4::engine::entityShared_s, currentOrigin) == 0x160, "gentity_s::r.currentOrigin");
static_assert(offsetof(gentity_s, r) + offsetof(T4::engine::entityShared_s, currentAngles) == 0x16C, "gentity_s::r.currentAngles");
static_assert(offsetof(gentity_s, r) + offsetof(T4::engine::entityShared_s, ownerNum) == 0x178, "gentity_s::r.ownerNum");
static_assert(offsetof(gentity_s, sentient) == 0x188, "gentity_s::sentient");
static_assert(offsetof(gentity_s, pTurretInfo) == 0x190, "gentity_s::pTurretInfo");
static_assert(offsetof(actor_s, iStateTime) == 0xBB8, "actor_s::iStateTime");
static_assert(offsetof(actor_s, pTurret) == 0x2134, "actor_s::pTurret");
static_assert(offsetof(TurretInfo, flags) == 0x8 && offsetof(TurretInfo, manualTarget) == 0x10
           && offsetof(TurretInfo, target) == 0x14 && offsetof(TurretInfo, targetPos) == 0x18
           && offsetof(TurretInfo, forwardAngleDot) == 0x4C && offsetof(TurretInfo, dropPitch) == 0x50
           && offsetof(TurretInfo, convergenceTime) == 0x54 && offsetof(TurretInfo, suppressTime) == 0x5C
           && offsetof(TurretInfo, maxRangeSquared) == 0x60 && offsetof(TurretInfo, detachSentient) == 0x64
           && offsetof(TurretInfo, originError) == 0x9C, "TurretInfo layout");
// word_1F33CE4 / word_1F33C5A / word_1F33C38 = scr_const (0x1F33B90) + these.
static_assert(offsetof(T4::engine::scr_const_t, turretstatechange) == 0x154
           && offsetof(T4::engine::scr_const_t, tag_weapon) == 0xCA
           && offsetof(T4::engine::scr_const_t, tag_aim) == 0xA8, "scr_const_t layout");

namespace
{
	// Vanilla: (s - level.sentients) * 0x78787879 >> 38, rounded toward zero == signed / 0x88.
	int SentientIndex(const sentient_s* s)
	{
		return static_cast<int>(reinterpret_cast<const char*>(s) - reinterpret_cast<const char*>(*T4::engine::g_sentients))
		     / static_cast<int>(sizeof(sentient_s));
	}

	// SentientHandle -> sentient. Vanilla uses the pool immediate (0x18E7338 / 0x18E73C0, both
	// relocated by PatchT4MAM_ActorLimit); level.sentients holds that same base.
	sentient_s* SentientFromHandle(unsigned short number)
	{
		return *T4::engine::g_sentients + (number - 1);
	}

	gentity_s* EntFromHandle(unsigned short number)
	{
		return &T4::engine::g_entities.get()[number - 1];
	}

	// Inlined AngleVectors: fld float / fsincos / fstp cos / fstp sin, x87 precision.
	void FSinCos(float angle, float* s, float* c)
	{
		__asm
		{
			fld     angle
			fsincos
			mov     ecx, c
			fstp    dword ptr [ecx]
			mov     ecx, s
			fstp    dword ptr [ecx]
		}
	}

	// forward only; vanilla: fwd2 = -0.0f (dword_8AF218) - sin(pitch).
	void TurretForward(const gentity_s* self, float* fwd)
	{
		float sy, cy, sp, cp;
		FSinCos(self->r.currentAngles[1] * 0.0174532924f, &sy, &cy);   // dword_8AF760
		FSinCos(self->r.currentAngles[0] * 0.0174532924f, &sp, &cp);
		fwd[0] = cp * cy;
		fwd[1] = cp * sy;
		fwd[2] = -0.0f - sp;
	}
}

// @modified — sub_56A280 / CoD4 game/turret.cpp turret_think_auto. 1:1 with vanilla except sentientInfo/vis_blockers go through the T4M accessors.
//   DETOURED — do not call directly.
int T4_Reconstructed::turret_think_auto(gentity_s* self, actor_s* actor)
{
	TurretInfo* const ti = self->pTurretInfo;
	sentient_s* target;
	gentity_s* manualEnt;             // var_40
	sentient_info_t* info;            // ebp
	sentient_s* tsent;
	float delta[3];                   // var_C
	float distSq;                     // var_20
	float fwd[3];
	float dot;
	int inCone = 0;                   // var_24
	float desiredAngles[3];           // var_18
	float eyeSelf[3];                 // var_18
	float eyeTarget[3];               // var_C
	float targetPos[3];               // var_C
	float dropAngles[2];

	if (!(ti->flags & 0x20)
	    || ti->originError[0] != 0.0f
	    || ti->originError[1] != 0.0f
	    || ti->originError[2] != 0.0f)
		goto notReady;

	target = T4::game::Actor_GetTargetSentient(actor);
	if ((ti->flags & 0x10) && ti->detachSentient.number)
		target = SentientFromHandle(ti->detachSentient.number);

	manualEnt = ti->manualTarget.number ? EntFromHandle(ti->manualTarget.number) : nullptr;

	if (!target)
	{
		info = nullptr;
		if (ti->detachSentient.number
		    && T4M::Actor_SentientInfo(actor, SentientIndex(SentientFromHandle(ti->detachSentient.number)))->attackTime   // 0x56A36D
		       <= T4::engine::level->time)
			T4::game::SentientHandle_setSentient(&ti->detachSentient, nullptr);
		T4_Reconstructed::Actor_CanAttackAll(actor);
		goto targetEnt;
	}

	info = T4M::Actor_SentientInfo(actor, SentientIndex(target));   // 0x56A3B5
	info->attackTime = T4::engine::level->time + 2000;

	delta[1] = target->ent->r.currentOrigin[1] - self->r.currentOrigin[1];
	delta[2] = target->ent->r.currentOrigin[2] - self->r.currentOrigin[2];
	delta[0] = target->ent->r.currentOrigin[0] - self->r.currentOrigin[0];
	distSq = delta[2] * delta[2] + delta[1] * delta[1] + delta[0] * delta[0];

	if (target->bIgnoreMe || !(ti->maxRangeSquared > distSq))
	{
		info = nullptr;   // loc_56A907
		goto clearManual;
	}

	if (info->VisCache.bVisible
	    && T4::game::turret_aimat_Sentient_Internal(target, self, 1, ti->convergenceTime[1], desiredAngles))
	{
		T4::game::turret_UpdateTargetAngles(self, desiredAngles, 1);
		ti->flags &= ~0x10;
		info->attackTime = 0;
		return 1;
	}

	if (T4::engine::level->time - actor->iStateTime < 1000)
		goto clearManual;
	if (ti->flags & 0x2000)
		goto clearManual;

	TurretForward(self, fwd);
	T4::engine::Vec3Normalize(delta);
	dot = fwd[2] * delta[2] + fwd[1] * delta[1] + fwd[0] * delta[0];
	inCone = 1;
	if (!(dot >= ti->forwardAngleDot))
		inCone = 0;

	// loc_56A5AF
	if (ti->detachSentient.number)
	{
		if (SentientFromHandle(ti->detachSentient.number) == target)
			goto detachCheck;
		if (T4M::Actor_SentientInfo(actor, SentientIndex(SentientFromHandle(ti->detachSentient.number)))->attackTime   // 0x56A602
		    > T4::engine::level->time)
			goto detachCheck;
	}
	if (!info->VisCache.bVisible && inCone)
		goto detachCheck;
	if (T4::engine::level->time - info->lastKnownPosTime >= ti->suppressTime)
		goto detachCheck;
	{
		const float dy = info->vLastKnownPos[1] - target->ent->r.currentOrigin[1];
		const float dx = info->vLastKnownPos[0] - target->ent->r.currentOrigin[0];
		if (!(4096.0f > dy * dy + dx * dx))   // dword_83C770
			goto detachCheck;
	}
	T4::game::Sentient_GetEyePosition(actor->sentient, eyeSelf);
	T4::game::Sentient_GetEyePosition(target, eyeTarget);
	if (T4::game::turret_SightTrace(eyeSelf, eyeTarget, actor->ent->s.number, target->ent->s.number))
		goto detachCheck;
	T4::game::SentientHandle_setSentient(&ti->detachSentient, target);
	if (65536.0f > distSq)   // dword_8AF4B0
		info->attackTime = 0;
	goto clearManual;

detachCheck:   // loc_56A6DD
	if (!ti->detachSentient.number)
		goto clearManual;
	{
		sentient_info_t* const dinfo = T4M::Actor_SentientInfo(actor, SentientIndex(SentientFromHandle(ti->detachSentient.number)));   // 0x56A719 / 0x56A720
		if (T4::engine::level->time - dinfo->lastKnownPosTime >= ti->suppressTime)
			goto dropDetach;
		const gentity_s* const dent = SentientFromHandle(ti->detachSentient.number)->ent;
		const float dy = dinfo->vLastKnownPos[1] - dent->r.currentOrigin[1];
		const float dx = dinfo->vLastKnownPos[0] - dent->r.currentOrigin[0];
		if (!(4096.0f > dy * dy + dx * dx))
			goto dropDetach;
	}
	T4::game::Sentient_GetEyePosition(actor->sentient, eyeSelf);
	T4::game::Sentient_GetEyePosition(SentientFromHandle(ti->detachSentient.number), eyeTarget);
	if (T4::game::turret_SightTrace(eyeSelf, eyeTarget, actor->ent->s.number,
	                                SentientFromHandle(ti->detachSentient.number)->ent->s.number))
		goto dropDetach;

	// loc_56A891
	if (SentientFromHandle(ti->detachSentient.number) != target)
		goto clearManual;
	if ((ti->flags & 0x10) || !inCone || T4::game::turret_ReturnToDefaultPos(self, 1))
	{
		info->attackTime = 0;   // loc_56A8ED
		return 0;
	}
	if (!(65536.0f > distSq))
		return 1;
	info->attackTime = 0;
	return 1;

dropDetach:    // loc_56A8FE
	T4::game::SentientHandle_setSentient(&ti->detachSentient, nullptr);

clearManual:   // loc_56A7E9
	if (manualEnt == target->ent)
		manualEnt = nullptr;

targetEnt:     // loc_56A7F9
	if (!ti->target.number)
		goto noTargetEnt;
	tsent = EntFromHandle(ti->target.number)->sentient;
	if (!tsent)
		goto noTargetEnt;

	info = T4M::Actor_SentientInfo(actor, SentientIndex(tsent));   // 0x56A849
	if (tsent->bIgnoreMe || T4::engine::level->time - info->lastKnownPosTime >= ti->suppressTime)
	{
		info->lastKnownPosTime = 0;   // loc_56A98F
		goto manual;
	}
	targetPos[0] = info->vLastKnownPos[0];
	targetPos[1] = info->vLastKnownPos[1];
	if (ti->flags & 0x40)
		targetPos[2] = ti->targetPos[2];
	else
		targetPos[2] = info->vLastKnownPos[2] + 32.0f;   // dword_84BC9C
	if (!T4::game::turret_aimat_vector(self, targetPos, 1, desiredAngles))
		goto manual;
	T4::game::turret_UpdateTargetAngles(self, desiredAngles, 1);
	info->attackTime = 0;
	if (ti->detachSentient.number && SentientFromHandle(ti->detachSentient.number) == tsent)
		T4::game::SentientHandle_setSentient(&ti->detachSentient, nullptr);
	return 1;

noTargetEnt:   // loc_56AA3E
	if (info)
		info->lastKnownPosTime = 0;

manual:        // loc_56A99A
	if (manualEnt)
	{
		const float dz = self->r.currentOrigin[2] - manualEnt->r.currentOrigin[2];
		const float dy = self->r.currentOrigin[1] - manualEnt->r.currentOrigin[1];
		const float dx = self->r.currentOrigin[0] - manualEnt->r.currentOrigin[0];
		if (ti->maxRangeSquared > dz * dz + dy * dy + dx * dx)
		{
			sentient_s* const ms = manualEnt->sentient;
			if (!ms)
			{
				if (T4::game::turret_aimat_Ent(self, manualEnt, 1))
					return 1;
			}
			else if (T4M::Actor_SentientInfo(actor, SentientIndex(ms))->VisCache.bVisible   // 0x56AA72
			         && T4::game::turret_aimat_Sentient_Internal(ms, self, 1, ti->convergenceTime[1], desiredAngles))
			{
				T4::game::turret_UpdateTargetAngles(self, desiredAngles, 1);
				return 1;
			}
		}
	}
	// loc_56AA14
	if (!info || !info->VisCache.bVisible)
		T4::game::turret_ClearTargetEnt(self);
	T4::game::turret_ReturnToDefaultPos(self, 1);
	return 1;

notReady:      // loc_56AA9E
	dropAngles[0] = ti->dropPitch;
	dropAngles[1] = 0.0f;
	if (ti->state)
		T4::engine::Scr_NotifyNum_Internal(T4::engine::SCRIPTINSTANCE_SERVER, self->s.number, 0,
		                                   T4::engine::scr_const->turretstatechange, 0);
	ti->state = 0;
	T4::game::turret_UpdateTargetAngles(self, dropAngles, 0);
	return 1;
}

// @modified — sub_56AB00 / CoD4 game/turret.cpp turret_think_manual. 1:1 with vanilla except sentientInfo/vis_blockers go through the T4M accessors.
//   DETOURED — do not call directly. Reached through T4M::turret_think_manual_Wrapper.
int T4_Reconstructed::turret_think_manual(gentity_s* self, actor_s* actor)
{
	TurretInfo* const ti = self->pTurretInfo;
	int flags;
	sentient_s* target;
	int bShoot;
	gentity_s* manualEnt;
	float angles[3];      // var_8
	float dropAngles[2];  // var_8

	if (!actor)
		goto manual;

	flags = ti->flags;
	if (!(flags & 0x20)
	    || ti->originError[0] != 0.0f
	    || ti->originError[1] != 0.0f
	    || ti->originError[2] != 0.0f)
		goto notReady;

	if (ti->detachSentient.number)
		return turret_think_auto(self, actor);   // vanilla: call sub_56A280

	target = T4::game::Actor_GetTargetSentient(actor);
	if (target)
	{
		const gentity_s* const tent = target->ent;
		const float dz = self->r.currentOrigin[2] - tent->r.currentOrigin[2];
		const float dy = self->r.currentOrigin[1] - tent->r.currentOrigin[1];
		const float dx = self->r.currentOrigin[0] - tent->r.currentOrigin[0];
		if (ti->maxRangeSquared > dz * dz + dy * dy + dx * dx)
		{
			const gentity_s* const aent = actor->ent;
			const float az = aent->r.currentOrigin[2] - tent->r.currentOrigin[2];
			const float ay = aent->r.currentOrigin[1] - tent->r.currentOrigin[1];
			const float ax = aent->r.currentOrigin[0] - tent->r.currentOrigin[0];
			if (65536.0f > az * az + ay * ay + ax * ax)   // dword_8AF4B0
				return turret_think_auto(self, actor);
		}
	}

	// loc_56AC51 — writes back the flags read at entry.
	ti->flags = flags & ~0x10;
	if (target)
		T4M::Actor_SentientInfo(actor, SentientIndex(target))->attackTime = T4::engine::level->time + 2000;   // 0x56AD3C
	else
		T4_Reconstructed::Actor_CanAttackAll(actor);

manual:        // loc_56AC65
	bShoot = (ti->flags >> 2) & 1;
	if (!ti->manualTarget.number)
	{
		manualEnt = nullptr;   // loc_56AD9C
		goto fallback;
	}
	manualEnt = EntFromHandle(ti->manualTarget.number);
	if (!manualEnt)
		goto fallback;
	{
		const float dz = manualEnt->r.currentOrigin[2] - self->r.currentOrigin[2];
		const float dy = manualEnt->r.currentOrigin[1] - self->r.currentOrigin[1];
		const float dx = manualEnt->r.currentOrigin[0] - self->r.currentOrigin[0];
		if (!(ti->maxRangeSquared > dz * dz + dy * dy + dx * dx))
			goto fallback;
	}
	if (!manualEnt->sentient)
	{
		T4::game::turret_aimat_Ent(self, manualEnt, bShoot);
		return 1;
	}
	if (T4::game::turret_aimat_Sentient_Internal(manualEnt->sentient, self, bShoot, 0, angles))
	{
		T4::game::turret_UpdateTargetAngles(self, angles, 1);
		return 1;
	}

fallback:      // loc_56AD9E
	if (actor && manualEnt)
	{
		T4::game::turret_ReturnToDefaultPos(self, 1);
		return 1;
	}
	T4::game::turret_ClearTargetEnt(self);
	return 1;

notReady:      // loc_56AD48
	dropAngles[0] = ti->dropPitch;
	dropAngles[1] = 0.0f;
	if (ti->state)
		T4::engine::Scr_NotifyNum_Internal(T4::engine::SCRIPTINSTANCE_SERVER, self->s.number, 0,
		                                   T4::engine::scr_const->turretstatechange, 0);
	ti->state = 0;
	T4::game::turret_UpdateTargetAngles(self, dropAngles, 0);
	return 1;
}

// @wrapper — usercall(self@eax, actor@stack) ; vanilla ends in a plain retn, caller cleans.
__declspec(naked) void T4M::turret_think_manual_Wrapper()
{
	__asm
	{
		push    dword ptr [esp+4]           ; actor (original arg_0)
		push    eax                         ; self
		call    T4_Reconstructed::turret_think_manual
		add     esp, 8
		retn
	}
}

// @modified — sub_56B420 / CoD4 game/turret.cpp turret_canuse_auto. 1:1 with vanilla except sentientInfo/vis_blockers go through the T4M accessors.
//   DETOURED — do not call directly. Returns a full 0/1 in eax (callers test eax).
int T4_Reconstructed::turret_canuse_auto(gentity_s* self, actor_s* actor)
{
	TurretInfo* const ti = self->pTurretInfo;
	gentity_s* const cur = actor->pTurret;
	sentient_s* target;
	sentient_info_t* info;
	float delta[3];                   // var_18
	float fwd[3];
	float dot;
	float eyePos[3];                  // var_C
	float vSource[3];                 // var_18
	float localAngles[2];             // var_20
	float tagWeapon[3];               // var_C
	float tagAim[3];                  // var_C
	float start[3];                   // var_18
	float dir[2];                     // var_20

	// Inlined Actor_IsUsingTurret.
	if (cur
	    && cur->r.ownerNum.number
	    && EntFromHandle(cur->r.ownerNum.number) == actor->ent
	    && cur != self
	    && !T4::game::Actor_IsTurretCloserThenCurrent(actor, self))
		return 0;

	// loc_56B481 — inlined Actor_GetTargetSentient.
	target = actor->sentient->targetEnt.number ? EntFromHandle(actor->sentient->targetEnt.number)->sentient : nullptr;
	if (!target)
		return 1;

	info = T4M::Actor_SentientInfo(actor, SentientIndex(target));   // 0x56B4CA / 0x56B4D1
	if (T4::engine::level->time - info->lastKnownPosTime >= ti->suppressTime)
		return 1;

	delta[2] = target->ent->r.currentOrigin[2] - self->r.currentOrigin[2];
	delta[1] = target->ent->r.currentOrigin[1] - self->r.currentOrigin[1];
	delta[0] = target->ent->r.currentOrigin[0] - self->r.currentOrigin[0];
	if (delta[2] * delta[2] + delta[1] * delta[1] + delta[0] * delta[0] >= ti->maxRangeSquared)
		return 1;
	{
		const float dy = info->vLastKnownPos[1] - target->ent->r.currentOrigin[1];
		const float dx = info->vLastKnownPos[0] - target->ent->r.currentOrigin[0];
		if (dy * dy + dx * dx >= 4096.0f)   // dword_83C770
			return 1;
	}

	if (info->VisCache.bVisible)
	{
		T4::game::Sentient_GetEyePosition(target, eyePos);
		if (T4::game::turret_CanTargetSentient(self, target, eyePos, vSource, localAngles))
			return 1;
	}
	else
	{
		// loc_56B5BB
		TurretForward(self, fwd);
		T4::engine::Vec3Normalize(delta);
		dot = fwd[2] * delta[2] + fwd[1] * delta[1] + fwd[0] * delta[0];
		if (dot >= ti->forwardAngleDot)
			return 1;
	}

	// loc_56B6AB
	if (!T4::game::G_DObjGetWorldTagPos(self, T4::engine::scr_const->tag_weapon, tagWeapon))
		return 0;
	start[0] = tagWeapon[0];
	start[1] = tagWeapon[1];
	if (!T4::game::G_DObjGetWorldTagPos(self, T4::engine::scr_const->tag_aim, tagAim))
		return 0;
	start[2] = tagAim[2];
	dir[0] = start[0] - tagAim[0];
	dir[1] = start[1] - tagAim[1];
	T4::engine::Vec2Normalize(dir);
	start[0] = dir[0] * 30.0f + start[0];   // dword_7F0428
	start[1] = dir[1] * 30.0f + start[1];
	T4::game::Sentient_GetEyePosition(target, eyePos);
	return T4::game::turret_SightTrace(start, eyePos, actor->ent->s.number, target->ent->s.number) != 0 ? 1 : 0;
}

void PatchT4MAM_SentientInfo_Turret()
{
	Detours::X86::DetourFunction(T4M::GetAddress("turret_think_auto"),
	                             reinterpret_cast<uintptr_t>(&T4_Reconstructed::turret_think_auto),
	                             Detours::X86Option::USE_JUMP);
	Detours::X86::DetourFunction(T4M::GetAddress("turret_think_manual"),
	                             reinterpret_cast<uintptr_t>(&T4M::turret_think_manual_Wrapper),
	                             Detours::X86Option::USE_JUMP);
	Detours::X86::DetourFunction(T4M::GetAddress("turret_canuse_auto"),
	                             reinterpret_cast<uintptr_t>(&T4_Reconstructed::turret_canuse_auto),
	                             Detours::X86Option::USE_JUMP);
}
