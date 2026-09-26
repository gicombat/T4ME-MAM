// ==========================================================
// T4M project
//
// Component: clientdll
// Purpose: AI limit phase 8.2 - reconstructions detoured so that their accesses to
//          actor_s::sentientInfo / vis_blockers go through T4M::Actor_SentientInfo /
//          T4M::Actor_VisBlocker (side table, see PatchT4MAM_ActorSentientInfo.cpp).
//          CoD4 actor_senses.cpp / actor_orientation.cpp: Actor_GetAnglesToLikelyEnemyPath, Actor_CanSeeEntityEx,
//          Actor_CanSeeSentient, Actor_CanSeeEnemyExtended, Actor_KnowAboutEnemy, Actor_UpdateSight,
//          Actor_UpdateLastKnownPos, Actor_UpdateLastEnemySightPos
//
//          R&D analysis/actor_sentientinfo_sidetable.md
//
// Started: 2026-09-19
// ==========================================================

#include "StdInc.h"

#include <cfloat>
#include <climits>
#include <cstring>

using T4::engine::actor_s;
using T4::engine::sentient_s;
using T4::engine::sentient_info_t;
using T4::engine::gentity_s;
using T4::engine::pathnode_t;

static_assert(sizeof(sentient_s) == 0x88, "sentient_s layout");
static_assert(sizeof(gentity_s) == 0x378, "gentity_s layout");

namespace
{
	// (sentient - level.sentients) / 0x88, as vanilla's imul 78787879h / sar 6.
	int SentientIndex(const sentient_s* sentient)
	{
		return static_cast<int>(sentient - *T4::engine::g_sentients);
	}

	// EntHandle::ent(): &g_entities[number - 1], unchecked. Vanilla evaluates it for
	// number == 0 too (g_entities - 0x378) wherever it does not test the handle first.
	gentity_s* EntityFromHandle(unsigned short number)
	{
		return reinterpret_cast<gentity_s*>(reinterpret_cast<char*>(T4::engine::g_entities.get())
		                                    + (static_cast<int>(number) - 1) * 0x378);
	}

	// SentientHandle::sentient(). Vanilla adds the sentient pool BSS base (0x18E73C0,
	// relocated by PatchT4MAM_ActorLimit's scan); level.sentients holds the same base.
	sentient_s* SentientFromHandle(unsigned short number)
	{
		return *T4::engine::g_sentients + (static_cast<int>(number) - 1);
	}

	// Actor_IsUsingTurret, inlined by vanilla in Actor_CanSeeEntityEx.
	bool IsUsingTurretInline(const actor_s* self)
	{
		const gentity_s* turret = self->pTurret;
		if (!turret)
			return false;
		const unsigned short owner = turret->r.ownerNum.number;
		if (!owner)
			return false;
		return EntityFromHandle(owner) == self->ent;
	}

	struct sentient_sort_t
	{
		sentient_s* sentient;
		float       fMetric;
	};
	static_assert(sizeof(sentient_sort_t) == 8, "qsort element size is 8 in vanilla");
}

// ==========================================================
// sub_4D5690
// ==========================================================

// @modified — sub_4D5690 / CoD4 game/actor_orientation.cpp Actor_GetAnglesToLikelyEnemyPath.
//   1:1 with vanilla except sentientInfo goes through the T4M accessor, and the inlined
//   Sentient_FirstSentient/NextSentient walk (vanilla: 6x-unrolled, 36 sentients) runs to
//   NEW_MAX_SENTIENTS like the patched out-of-line iterators (aiCap_SentientIter_*).
//   stdcall(self), retn 4, bool in al.
//   DETOURED — do not call directly.
bool __stdcall T4_Reconstructed::Actor_GetAnglesToLikelyEnemyPath(actor_s* self)
{
	sentient_s* sentient = self->sentient;
	if (sentient->bIgnoreAll)
		return false;

	// Sentient_EnemyTeam, inlined as a stack table.
	int enemyTeamOf[5];
	enemyTeamOf[0] = 0;
	enemyTeamOf[1] = 2;
	enemyTeamOf[2] = 1;
	enemyTeamOf[3] = 0;
	enemyTeamOf[4] = 0;
	const int enemyTeam = enemyTeamOf[sentient->eTeam];
	if (!enemyTeam)
		return false;

	if (self->faceLikelyEnemyPathNode)
	{
		if (self->faceLikelyEnemyPathNeedCheckTime > *T4::engine::level_time)
			return true;

		if (self->faceLikelyEnemyPathNeedRecalculateTime > *T4::engine::level_time)
		{
			pathnode_t* nearest = T4::game::Sentient_NearestNode(sentient);
			if (T4::game::Path_NodesVisible(self->faceLikelyEnemyPathNode, nearest))
			{
				T4::game::Actor_SetAnglesToLikelyEnemyPath(self);
				self->faceLikelyEnemyPathNeedCheckTime = *T4::engine::level_time + 500;
				return true;
			}
		}
	}
	else if (self->faceLikelyEnemyPathNeedRecalculateTime > *T4::engine::level_time)
	{
		return false;
	}

	float bestDistSq = FLT_MAX;             // flt_8AF43C
	const float clientScale = 0.25f;        // dword_83FCCC
	self->faceLikelyEnemyPathNode = nullptr;
	sentient_s* best = nullptr;
	const int teamFlags = 1 << enemyTeam;

	sentient_s* other = *T4::engine::g_sentients;
	for (int i = 0; i < NEW_MAX_SENTIENTS; ++i, ++other)   // @modified: vanilla 36
	{
		if (!other->inuse)
			continue;
		if (!(teamFlags & (1 << other->eTeam)))
			continue;
		// Actor_CheckIgnore, inlined.
		if (other->bIgnoreMe)
			continue;
		if (T4::engine::g_threatBias_threatTable.get()[other->iThreatBiasGroupIndex * 16 + self->sentient->iThreatBiasGroupIndex] == INT_MIN)
			continue;

		const gentity_s* otherEnt = other->ent;
		const float* org = self->ent->r.currentOrigin;
		const float dx = org[0] - otherEnt->r.currentOrigin[0];
		const float dy = org[1] - otherEnt->r.currentOrigin[1];
		const float dz = org[2] - otherEnt->r.currentOrigin[2];
		float distSq = (dx * dx + dz * dz) + dy * dy;
		if (otherEnt->client)
			distSq = distSq * clientScale;

		if (bestDistSq > distSq)
		{
			bestDistSq = distSq;
			best = other;
		}
	}

	if (!best)
	{
		const int t = *T4::engine::level_time + 500;
		self->faceLikelyEnemyPathNeedRecalculateTime = t;
		self->faceLikelyEnemyPathNeedCheckTime = t;
		return false;
	}

	sentient_info_t* info;
	if (bestDistSq > best->maxVisibleDist * best->maxVisibleDist)
		info = T4M::Actor_SentientInfo(self, SentientIndex(best));   // @modified
	else
		info = nullptr;

	pathnode_t* node = T4::game::Path_FindFacingNode(self->sentient, best, info);
	if (!node)
	{
		const int t = *T4::engine::level_time + 3000;
		self->faceLikelyEnemyPathNeedRecalculateTime = t;
		self->faceLikelyEnemyPathNeedCheckTime = t;
		return false;
	}

	self->faceLikelyEnemyPathNode = node;
	T4::game::Actor_SetAnglesToLikelyEnemyPath(self);
	self->faceLikelyEnemyPathNeedRecalculateTime = *T4::engine::level_time + 3000;
	self->faceLikelyEnemyPathNeedCheckTime = *T4::engine::level_time + 500;
	return true;
}

// ==========================================================
// sub_4DF440
// ==========================================================

// @modified — sub_4DF440 / CoD4 game/actor_senses.cpp Actor_CanSeeEntityEx.
//   1:1 with vanilla except sentientInfo goes through the T4M accessor (2 sites).
//   stdcall(self, ent, fovDot, fMaxDistSqrd), retn 10h, bool in al.
//   DETOURED — do not call directly.
bool __stdcall T4_Reconstructed::Actor_CanSeeEntityEx(actor_s* self, gentity_s* ent, float fovDot, float fMaxDistSqrd)
{
	float vViewPos[3];
	float vDestPos[3];
	float localAngles[2];
	sentient_info_t* pInfo;
	unsigned char bVisible;

	sentient_s* sentient = ent->sentient;
	if (sentient)
	{
		pInfo = T4M::Actor_SentientInfo(self, SentientIndex(sentient));   // @modified
		T4::game::Sentient_GetEyePosition(sentient, vDestPos);

		if (IsUsingTurretInline(self))
		{
			if (!T4::game::turret_CanTargetPoint(vDestPos, self->pTurret, vViewPos, localAngles)
			    && !T4::game::turret_CanTargetSentient(self->pTurret, sentient, vDestPos, vViewPos, localAngles))
			{
				if (*T4::engine::level_time - pInfo->lastKnownPosTime >= 1000)
				{
					const float* org = self->ent->r.currentOrigin;
					const float dy = org[1] - vDestPos[1];
					const float dx = org[0] - vDestPos[0];
					if (dy * dy + dx * dx >= 262144.0f)   // dword_8AFCD8
						return false;
				}
				T4::game::Actor_GetEyePosition(self, vViewPos);
			}
		}
		else
		{
			// Actor_GetEyePosition, inlined.
			T4::game::Actor_UpdateEyeInformation(self);
			vViewPos[0] = self->eyeInfo.pos[0];
			vViewPos[1] = self->eyeInfo.pos[1];
			vViewPos[2] = self->eyeInfo.pos[2];
		}

		float fovDotUse = fovDot;

		// Actor_GetTargetSentient, inlined.
		const unsigned short targetNum = self->sentient->targetEnt.number;
		sentient_s* target = targetNum ? EntityFromHandle(targetNum)->sentient : nullptr;
		if (target == sentient)
		{
			fovDotUse = 0.0f;
		}
		else
		{
			const unsigned short favNum = self->pFavoriteEnemy.number;
			if (favNum && SentientFromHandle(favNum) == sentient)
				fovDotUse = 0.0f;
		}

		const float maxVisSq = sentient->maxVisibleDist * sentient->maxVisibleDist;
		if (fMaxDistSqrd - maxVisSq >= 0.0f)
			fMaxDistSqrd = maxVisSq;

		// Actor_CanSeePointExInternal, inlined.
		const int ignoreEntNum = ent->s.number;
		if (T4::game::PointInFovAndRange(vDestPos, vViewPos, self, fovDotUse, fMaxDistSqrd))
			bVisible = static_cast<unsigned char>(T4_Reconstructed::Actor_SightTrace(self, vViewPos, vDestPos, ignoreEntNum));
		else
			bVisible = 0;
	}
	else
	{
		if (!T4::game::G_DObjGetWorldTagPos(ent, *T4::engine::scr_const_tag_eye, vDestPos))
			T4::game::G_EntityCentroid(ent, vDestPos);
		pInfo = nullptr;
		bVisible = static_cast<unsigned char>(T4::game::Actor_CanSeePointEx(self, vDestPos, fovDot, fMaxDistSqrd, ent->s.number));
	}

	bool bCacheable;
	if (bVisible)
	{
		bCacheable = fovDot >= self->fovDot && self->fMaxSightDistSqrd >= fMaxDistSqrd;
	}
	else
	{
		if (!(self->fovDot >= fovDot))
			return false;
		if (!(fMaxDistSqrd >= self->fMaxSightDistSqrd))
			return false;
		bCacheable = true;
	}

	if (bCacheable && sentient)
		T4::game::Actor_UpdateVisCache(self, ent, pInfo, bVisible);

	if (!bVisible)
		return false;

	actor_s* other = ent->actor;
	if (!other)
		return true;
	if (static_cast<unsigned char>(T4::game::Actor_IsUsingTurret(self)))
		return true;
	if (static_cast<unsigned char>(T4::game::Actor_IsUsingTurret(other)))
		return true;

	unsigned char bOtherVisible = 1;
	const float selfMaxVisSq = self->sentient->maxVisibleDist * self->sentient->maxVisibleDist;
	const float otherMaxSight = other->fMaxSightDistSqrd;
	const float otherMaxDistSqrd = (otherMaxSight - selfMaxVisSq >= 0.0f) ? selfMaxVisSq : otherMaxSight;

	if (!T4::game::PointInFovAndRange(vViewPos, vDestPos, other, other->fovDot, otherMaxDistSqrd))
		bOtherVisible = 0;

	sentient_info_t* otherInfo = T4M::Actor_SentientInfo(other, SentientIndex(self->sentient));   // @modified
	T4::game::Actor_UpdateVisCache(other, self->ent, otherInfo, bOtherVisible);
	return true;
}

// ==========================================================
// sub_4DF800
// ==========================================================

// @modified — sub_4DF800 / CoD4 game/actor_senses.cpp Actor_CanSeeSentient.
//   1:1 with vanilla except sentientInfo goes through the T4M accessor.
//   Reached through T4M::Actor_CanSeeSentient_Wrapper.
//   DETOURED — do not call directly.
bool T4_Reconstructed::Actor_CanSeeSentient(actor_s* self, sentient_s* sentient, int iMaxLatency)
{
	sentient_info_t* info = T4M::Actor_SentientInfo(self, SentientIndex(sentient));   // @modified
	const int iLastUpdateTime = info->VisCache.iLastUpdateTime;
	if (iLastUpdateTime && iLastUpdateTime + iMaxLatency >= *T4::engine::level_time)
		return info->VisCache.bVisible;

	return T4_Reconstructed::Actor_CanSeeEntityEx(self, sentient->ent, self->fovDot, self->fMaxSightDistSqrd);
}

// @wrapper — usercall(sentient@eax, self@esi, iMaxLatency@stack) ; retn 4.
//   Vanilla preserves ecx on every path (push ecx / pop ecx frame slot).
__declspec(naked) void T4M::Actor_CanSeeSentient_Wrapper()
{
	__asm
	{
		push    ecx
		push    dword ptr [esp+8]           ; iMaxLatency
		push    eax                         ; sentient
		push    esi                         ; self
		call    T4_Reconstructed::Actor_CanSeeSentient
		add     esp, 0Ch
		pop     ecx
		retn    4
	}
}

// ==========================================================
// sub_4DF930
// ==========================================================

// @modified — sub_4DF930 / CoD4 game/actor_senses.cpp Actor_CanSeeEnemyExtended, with
//   useClaimedNode folded to 1 by the compiler (single caller, sub_4C7EB0).
//   1:1 with vanilla except sentientInfo goes through the T4M accessor.
//   Reached through T4M::Actor_CanSeeEnemyExtended_Wrapper.
//   DETOURED — do not call directly.
bool T4_Reconstructed::Actor_CanSeeEnemyExtended(actor_s* self)
{
	const unsigned short targetNum = self->sentient->targetEnt.number;
	if (targetNum)
	{
		sentient_s* target = EntityFromHandle(targetNum)->sentient;
		if (target)
		{
			if (static_cast<unsigned char>(T4::game::Actor_CanSeeEnemyViaClaimedNode(self)))
				return true;

			const int iLastVisTime = T4M::Actor_SentientInfo(self, SentientIndex(target))->VisCache.iLastVisTime;   // @modified
			if (iLastVisTime && *T4::engine::level_time - iLastVisTime < 10000)
				return true;
			return false;
		}
	}

	return T4_Reconstructed::Actor_CanSeeEntityEx(self, EntityFromHandle(targetNum), self->fovDot, self->fMaxSightDistSqrd);
}

// @wrapper — usercall(self@edi) ; retn. ecx/edx are clobbered by vanilla on every path.
__declspec(naked) void T4M::Actor_CanSeeEnemyExtended_Wrapper()
{
	__asm
	{
		push    edi                         ; self
		call    T4_Reconstructed::Actor_CanSeeEnemyExtended
		add     esp, 4
		retn
	}
}

// ==========================================================
// sub_4DF9D0
// ==========================================================

// @modified — sub_4DF9D0 / CoD4 game/actor_senses.cpp Actor_KnowAboutEnemy.
//   1:1 with vanilla except sentientInfo goes through the T4M accessor.
//   Reached through T4M::Actor_KnowAboutEnemy_Wrapper.
//   DETOURED — do not call directly.
bool T4_Reconstructed::Actor_KnowAboutEnemy(actor_s* self, int hadPath)
{
	const unsigned short targetNum = self->sentient->targetEnt.number;
	if (!targetNum)
		return false;
	sentient_s* enemy = EntityFromHandle(targetNum)->sentient;
	if (!enemy)
		return false;

	if (!hadPath && static_cast<unsigned char>(T4::game::Actor_CanSeeEnemyViaClaimedNode(self)))
		return true;

	const int lastKnownPosTime = T4M::Actor_SentientInfo(self, SentientIndex(enemy))->lastKnownPosTime;   // @modified
	if (lastKnownPosTime && *T4::engine::level_time - lastKnownPosTime < 10000)
		return true;
	return false;
}

// @wrapper — usercall(self@edi, hadPath@stack) ; retn 4.
//   Vanilla preserves ecx on every path (push ecx / pop ecx frame slot).
__declspec(naked) void T4M::Actor_KnowAboutEnemy_Wrapper()
{
	__asm
	{
		push    ecx
		push    dword ptr [esp+8]           ; hadPath
		push    edi                         ; self
		call    T4_Reconstructed::Actor_KnowAboutEnemy
		add     esp, 8
		pop     ecx
		retn    4
	}
}

// ==========================================================
// sub_4DFC30
// ==========================================================

// @modified — sub_4DFC30 / CoD4 game/actor_senses.cpp Actor_UpdateSight.
//   1:1 with vanilla except sentientInfo goes through the T4M accessor, and the
//   check[] stack array holds NEW_MAX_SENTIENTS entries: vanilla's is 36 x 8 bytes
//   and the (patched) team iterator can now yield up to 67 candidates.
//   stdcall(self), retn 4.
//   DETOURED — do not call directly.
void __stdcall T4_Reconstructed::Actor_UpdateSight(actor_s* self)
{
	sentient_sort_t check[NEW_MAX_SENTIENTS];   // @modified: vanilla [36]
	int iCheckCount = 0;

	int enemyTeamOf[5];
	enemyTeamOf[0] = 0;
	enemyTeamOf[1] = 2;
	enemyTeamOf[2] = 1;
	enemyTeamOf[3] = 0;
	enemyTeamOf[4] = 0;
	const int enemyTeam = enemyTeamOf[self->sentient->eTeam];
	if (!enemyTeam)
		return;
	const int iTeamFlags = 1 << enemyTeam;

	T4::engine::Prof_Nullsub();   // PROF_SCOPED("sight 1")

	sentient_s* sentient = T4::game::Sentient_FirstSentient(iTeamFlags);
	if (sentient)
	{
		const float threeHalfs = 1.5f;   // dword_8E46E4
		const float half = 0.5f;         // dword_82B678
		const float* org = self->ent->r.currentOrigin;

		do
		{
			const float* sorg = sentient->ent->r.currentOrigin;
			const float dx = sorg[0] - org[0];
			const float dz = sorg[2] - org[2];
			const float dy = sorg[1] - org[1];
			const float fDistSqrd = (dz * dz + dx * dx) + dy * dy;

			if (fDistSqrd != 0.0f)
			{
				const sentient_info_t* info = T4M::Actor_SentientInfo(self, SentientIndex(sentient));   // @modified
				const int age = *T4::engine::level_time - info->VisCache.iLastUpdateTime - 100;
				const int staleness = ~(age >> 31) & age;

				// I_rsqrt, inlined.
				int bits;
				memcpy(&bits, &fDistSqrd, sizeof(bits));
				const int yBits = 0x5F3759DF - (bits >> 1);
				float y;
				memcpy(&y, &yBits, sizeof(y));
				const float rsqrt = (threeHalfs - ((fDistSqrd * half) * y) * y) * y;

				check[iCheckCount].fMetric = rsqrt * static_cast<float>(staleness);
				check[iCheckCount].sentient = sentient;
				++iCheckCount;
			}

			sentient = T4::game::Sentient_NextSentient(sentient, iTeamFlags);
		} while (sentient);

		if (iCheckCount > 1)
		{
			T4::engine::crt_qsort(check, iCheckCount, sizeof(sentient_sort_t), T4::engine::compare_sentient_sort.get());
		}
	}

	T4::engine::Prof_Nullsub();   // PROF_SCOPED("sight 2")

	const int iOldTraceCount = self->iTraceCount;
	for (int i = 0; i < iCheckCount; ++i)
	{
		T4_Reconstructed::Actor_CanSeeSentient(self, check[i].sentient, 0);
		if (self->iTraceCount != iOldTraceCount)
			break;
	}
}

// ==========================================================
// sub_4DFE10
// ==========================================================

// @modified — sub_4DFE10 / CoD4 game/actor_senses.cpp Actor_UpdateLastKnownPos.
//   1:1 with vanilla except sentientInfo goes through the T4M accessor.
//   Sentient_GetOrigin inlined as other->ent->r.currentOrigin.
//   Reached through T4M::Actor_UpdateLastKnownPos_Wrapper.
//   DETOURED — do not call directly.
void T4_Reconstructed::Actor_UpdateLastKnownPos(actor_s* self, sentient_s* other)
{
	sentient_info_t* info = T4M::Actor_SentientInfo(self, SentientIndex(other));   // @modified
	info->lastKnownPosTime = *T4::engine::level_time;
	const float* origin = other->ent->r.currentOrigin;
	info->vLastKnownPos[0] = origin[0];
	info->vLastKnownPos[1] = origin[1];
	info->vLastKnownPos[2] = origin[2];
	info->pLastKnownNode = other->pNearestNode;
}

// @wrapper — usercall(other@esi, self@stack) ; retn 4.
__declspec(naked) void T4M::Actor_UpdateLastKnownPos_Wrapper()
{
	__asm
	{
		push    esi                         ; other
		push    dword ptr [esp+8]           ; self
		call    T4_Reconstructed::Actor_UpdateLastKnownPos
		add     esp, 8
		retn    4
	}
}

// ==========================================================
// sub_4DFE70
// ==========================================================

// @modified — sub_4DFE70 / CoD4 game/actor_senses.cpp Actor_UpdateLastEnemySightPos.
//   1:1 with vanilla except sentientInfo goes through the T4M accessor.
//   Reached through T4M::Actor_UpdateLastEnemySightPos_Wrapper.
//   DETOURED — do not call directly.
void T4_Reconstructed::Actor_UpdateLastEnemySightPos(actor_s* self)
{
	const unsigned short targetNum = self->sentient->targetEnt.number;
	if (!targetNum)
		return;
	sentient_s* target = EntityFromHandle(targetNum)->sentient;
	if (!target)
		return;

	const sentient_info_t* info = T4M::Actor_SentientInfo(self, SentientIndex(target));   // @modified
	if (!info->VisCache.bVisible)
		return;
	if (info->VisCache.iLastVisTime != *T4::engine::level_time)
		return;

	self->lastEnemySightPosValid = true;
	T4::game::Sentient_GetEyePosition(target, self->lastEnemySightPos);
}

// @wrapper — usercall(self@eax) ; retn. Vanilla preserves ecx on every path.
__declspec(naked) void T4M::Actor_UpdateLastEnemySightPos_Wrapper()
{
	__asm
	{
		push    ecx
		push    eax                         ; self
		call    T4_Reconstructed::Actor_UpdateLastEnemySightPos
		add     esp, 4
		pop     ecx
		retn
	}
}

void PatchT4MAM_SentientInfo_Senses()
{
	Detours::X86::DetourFunction(T4M::GetAddress("Actor_GetAnglesToLikelyEnemyPath"),
	                             reinterpret_cast<uintptr_t>(&T4_Reconstructed::Actor_GetAnglesToLikelyEnemyPath),
	                             Detours::X86Option::USE_JUMP);
	Detours::X86::DetourFunction(T4M::GetAddress("Actor_CanSeeEntityEx"),
	                             reinterpret_cast<uintptr_t>(&T4_Reconstructed::Actor_CanSeeEntityEx),
	                             Detours::X86Option::USE_JUMP);
	Detours::X86::DetourFunction(T4M::GetAddress("Actor_CanSeeSentient"),
	                             reinterpret_cast<uintptr_t>(&T4M::Actor_CanSeeSentient_Wrapper),
	                             Detours::X86Option::USE_JUMP);
	Detours::X86::DetourFunction(T4M::GetAddress("Actor_CanSeeEnemyExtended"),
	                             reinterpret_cast<uintptr_t>(&T4M::Actor_CanSeeEnemyExtended_Wrapper),
	                             Detours::X86Option::USE_JUMP);
	Detours::X86::DetourFunction(T4M::GetAddress("Actor_KnowAboutEnemy"),
	                             reinterpret_cast<uintptr_t>(&T4M::Actor_KnowAboutEnemy_Wrapper),
	                             Detours::X86Option::USE_JUMP);
	Detours::X86::DetourFunction(T4M::GetAddress("Actor_UpdateSight"),
	                             reinterpret_cast<uintptr_t>(&T4_Reconstructed::Actor_UpdateSight),
	                             Detours::X86Option::USE_JUMP);
	Detours::X86::DetourFunction(T4M::GetAddress("Actor_UpdateLastKnownPos"),
	                             reinterpret_cast<uintptr_t>(&T4M::Actor_UpdateLastKnownPos_Wrapper),
	                             Detours::X86Option::USE_JUMP);
	Detours::X86::DetourFunction(T4M::GetAddress("Actor_UpdateLastEnemySightPos"),
	                             reinterpret_cast<uintptr_t>(&T4M::Actor_UpdateLastEnemySightPos_Wrapper),
	                             Detours::X86Option::USE_JUMP);
}
