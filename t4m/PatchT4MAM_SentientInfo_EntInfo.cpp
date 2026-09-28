// ==========================================================
// T4M project
//
// Component: clientdll
// Purpose: AI limit phase 8.2 - reconstructions detoured so that their accesses to
//          actor_s::sentientInfo / vis_blockers go through T4M::Actor_SentientInfo /
//          T4M::Actor_VisBlocker (side table, see PatchT4MAM_ActorSentientInfo.cpp).
//          CoD4 actor.cpp Actor_EntInfo (g_entinfo debug drawing; reads sentientInfo for the enemy line color)
//
//          R&D analysis/actor_sentientinfo_sidetable.md
//
// Started: 2026-09-19
// ==========================================================

#include "StdInc.h"
#include <bit>

using T4::engine::actor_s;
using T4::engine::sentient_info_t;

namespace
{
	// .rdata float constants of sub_4B6DD0 whose decimal form is not exact.
	constexpr float kEntInfoScaleK = std::bit_cast<float>(0x3FCB6482u);   // dword_8AF7DC
	constexpr float kInv384        = std::bit_cast<float>(0x3B2AAAABu);   // dword_8AF9A8
	constexpr float kPoint6        = std::bit_cast<float>(0x3F19999Au);   // dword_83DA64
	constexpr float kDegToRad      = std::bit_cast<float>(0x3C8EFA35u);   // dword_8AF760
	constexpr float kInv360        = std::bit_cast<float>(0x3B360B61u);   // dword_8AF918

	template <typename T>
	inline T& At(const void* base, int ofs)
	{
		return *reinterpret_cast<T*>(reinterpret_cast<uintptr_t>(base) + ofs);
	}

	// Vanilla's inlined sincosf: fld / fsincos / fstp cos / fstp sin.
	void X87SinCos(float angle, float* sinOut, float* cosOut)
	{
		__asm
		{
			fld     angle
			fsincos
			mov     ecx, cosOut
			mov     eax, sinOut
			fstp    dword ptr [ecx]
			fstp    dword ptr [eax]
		}
	}

	// EntHandle stores entnum + 1; vanilla indexes g_entities with a -0x378 displacement.
	inline T4::engine::gentity_s* EntHandleEnt(unsigned short number)
	{
		return reinterpret_cast<T4::engine::gentity_s*>(
			reinterpret_cast<char*>(T4::engine::g_entities.get()) + (static_cast<int>(number) - 1) * 0x378);
	}

	// Inlined body of G_AddDebugString (0x558F40) as vanilla carries it at 4B7005.
	inline void Inline_G_AddDebugString(const float* xyz, const float* color, float scale, const char* text, int duration)
	{
		if (*T4::engine::g_48AE4D4_inCgame == 0)
			return;
		if (!T4::game::CreateDebugStringsIfNeeded())
			return;
		T4::game::AddDebugStringToList(T4::engine::clsDebug_svStrings, xyz, color, scale, text, duration);
		*T4::engine::clsDebug_fromServer = 1;
	}

	// Inlined body of G_DebugLine (0x4F7B10) as vanilla carries it at 4B7275 / 4B72F4.
	inline void Inline_G_DebugLine(const float* startPt, const float* endPt, const float* color, int depthTest)
	{
		if (*T4::engine::g_48AE4D4_inCgame == 0)
			return;
		if (!T4::game::CreateDebugLinesIfNeeded())
			return;
		T4::game::AddDebugLineToList(T4::engine::clsDebug_svLines, startPt, endPt, color, depthTest, 0);
		*T4::engine::clsDebug_fromServer = 1;
	}
}

// @modified — sub_4B6DD0 / CoD4 game/actor.cpp Actor_EntInfo. 1:1 with vanilla except sentientInfo/vis_blockers go through the T4M accessors.
//   DETOURED — do not call directly.
//   Plain cdecl (self, source), retn; reached only through entityHandlers[].entinfo (0x8DBD20).
void T4_Reconstructed::Actor_EntInfo(T4::engine::gentity_s* self, float* source)
{
	using namespace T4::engine;
	using namespace T4::game;

	const float kZero = 0.0f;                                                  // String2

	bool drawLines = true;              // var_865
	bool drawGoalLineRadius = false;    // var_84D

	float xyz[3];                       // var_878
	float vec[3];                       // var_834 (eye pos of enemy / forward / up / view pos)
	float pos[3];                       // var_84C
	float color[4];                     // var_824
	float yawBase;                      // var_828
	float s0, s1, s2, s3;               // var_87C / var_86C / var_864 / var_85C
	float tmp858, tmp854, tmp83C;
	char  header[0x24];                 // var_824 as a char buffer (mode 7)
	char  buffer[0x800];                // var_800 (left uninitialized, as in vanilla)

	const int entNum = At<int>(self, 0x0);                                  // s.number
	const unsigned short objIdx = T4::engine::serverObjMap[entNum];
	if (!objIdx)
		return;
	BYTE* obj = reinterpret_cast<BYTE*>(T4::engine::objBuf.get()) + static_cast<short>(objIdx) * 0x68;
	if (!obj)
		return;

	actor_s* const actor = At<actor_s*>(self, 0x184);                          // ebx
	float* const selfOrigin = &At<float>(self, 0x160);                         // var_858 (r.currentOrigin)

	float dist;
	{
		const float dz = source[2] - At<float>(self, 0x168);
		const float dy = source[1] - At<float>(self, 0x164);
		const float dx = source[0] - At<float>(self, 0x160);
		dist = I_sqrt(dz * dz + dy * dy + dx * dx);
	}

	const float maxDist = (*g_entinfo_maxdist)->current.value;
	if (maxDist > kZero && dist > maxDist)
		return;

	const float infoScale = (*g_entinfo_scale)->current.value * *g_entInfoViewScale * kEntInfoScaleK * dist
	                      * kInv384;                    // var_860

	{
		// Sentient_GetDebugEyePosition(actor->ent->sentient, xyz), inlined.
		gentity_s* const sentEnt = At<gentity_s*>(At<sentient_s*>(At<gentity_s*>(actor, 0x0), 0x188), 0x0);
		actor_s* const sentActor = At<actor_s*>(sentEnt, 0x184);
		if (sentActor)
			Actor_GetDebugEyePosition(sentActor, xyz);
		else
			G_GetPlayerEyePosition(xyz, At<void*>(sentEnt, 0x180));             // ent->client
	}

	const int entinfo = (*g_entinfo)->current.integer;
	if (entinfo == 7)
	{
		// Anim tree dump of the ai_debugEntIndex entity.
		if ((*ai_debugEntIndex)->current.integer != entNum)
			return;
		const unsigned short idx7 = T4::engine::serverObjMap[entNum];
		if (!idx7)
			return;
		BYTE* obj7 = reinterpret_cast<BYTE*>(T4::engine::objBuf.get()) + static_cast<short>(idx7) * 0x68;
		if (!obj7)
			return;

		crt_sprintf(header, "Entity %d\n\n", entNum);
		DObjDisplayAnimToBuffer(obj7, header, buffer);
		char* line = crt_strtok(buffer, "\n");
		if (!line)
			return;

		const float step = infoScale * *entInfoLineStep;                       // var_86C
		for (;;)
		{
			unsigned int i = 0;
			if (strlen(line) != 0)
			{
				do
				{
					if (line[i] == '^')
					{
						line[i] = ' ';
						line[i + 1] = ' ';
						i += 2;
					}
					i += 1;
				} while (i < strlen(line));
			}

			xyz[2] = xyz[2] - step;
			Inline_G_AddDebugString(xyz, colorWhite, infoScale * kPoint6, line, 1);

			line = crt_strtok(nullptr, "\n");
			if (!line)
				return;
		}
	}

	if (entinfo == 3)
		drawLines = false;
	if (entinfo == 4 || entinfo == 5)
	{
		drawLines = false;
		drawGoalLineRadius = true;
	}

	// Actor_GetTargetEntity / Actor_GetTargetSentient, inlined on one read of sentient->targetEnt.
	const unsigned short targetNum = At<unsigned short>(At<sentient_s*>(actor, 0x4), 0x34);
	gentity_s* const target = targetNum ? EntHandleEnt(targetNum) : nullptr;          // var_804
	sentient_s* const enemy = targetNum ? At<sentient_s*>(EntHandleEnt(targetNum), 0x188) : nullptr;

	if (drawLines)
	{
		if (enemy)
		{
			const int sentientIndex = static_cast<int>(reinterpret_cast<char*>(enemy) - reinterpret_cast<char*>(*g_sentients)) / 0x88;
			sentient_info_t* const info = T4M::Actor_SentientInfo(actor, sentientIndex);   // @modified (was actor+0x1A88)
			const float* lineColor;
			if (!info->VisCache.bVisible)
				lineColor = colorRed;
			else
			{
				lineColor = colorGreen;
				if (*level_time - info->VisCache.iLastUpdateTime > 250)
					lineColor = colorYellow;
			}
			Sentient_GetDebugEyePosition(enemy, vec);
			G_DebugLine(xyz, vec, lineColor, 1);
		}
		else if (target)
		{
			G_DebugLine(xyz, &At<float>(target, 0x160), colorBlue, 1);
		}
	}

	if ((*g_entinfo)->current.integer != 3)
		G_DebugBox(selfOrigin, &At<float>(self, 0x12C), &At<float>(self, 0x138), At<float>(self, 0x170), colorMagenta, 1, 0);

	// eyeInfo.dir
	vec[0] = At<float>(actor, 0xCF8) * 48.0f + xyz[0];
	vec[1] = At<float>(actor, 0xCFC) * 48.0f + xyz[1];
	vec[2] = At<float>(actor, 0xD00) * 48.0f + xyz[2];
	if (drawLines)
		G_DebugLine(xyz, vec, colorBlue, 1);

	// Path.lookaheadDir
	pos[0] = At<float>(actor, 0x18EC) * 48.0f + xyz[0];
	pos[1] = At<float>(actor, 0x18F0) * 48.0f + xyz[1];
	pos[2] = xyz[2];
	Inline_G_DebugLine(xyz, pos, colorMagenta, 1);

	// prevMoveDir
	pos[0] = At<float>(actor, 0x1978) * 48.0f + xyz[0];
	pos[1] = At<float>(actor, 0x197C) * 48.0f + xyz[1];
	pos[2] = xyz[2];
	Inline_G_DebugLine(xyz, pos, colorOrange, 1);

	if (drawLines)
	{
		// lookAtInfo.bDoLookAt / fLookAtTurnAngle
		if (At<char>(actor, 0xD46) || At<float>(actor, 0xD2C) != kZero)
		{
			yawBase = At<float>(self, 0x170);                                  // r.currentAngles[1]

			pos[0] = At<float>(actor, 0xD20) - xyz[0];
			pos[1] = At<float>(actor, 0xD24) - xyz[1];
			pos[2] = At<float>(actor, 0xD28) - xyz[2];
			vectoangles_vanilla(pos, vec);                                             // vec = vAngles

			color[0] = 1.0f;
			color[1] = 1.0f;
			color[2] = 0.25f;
			color[3] = 0.5f;

			pos[0] = At<float>(actor, 0xD20);
			pos[1] = At<float>(actor, 0xD24);
			pos[2] = At<float>(actor, 0xD28) - 2.0f;
			G_DebugLine(xyz, pos, color, 1);

			color[3] = 0.75f;
			tmp83C = AngleNormalize360(yawBase - At<float>(actor, 0xD2C)) * kDegToRad;
			X87SinCos(tmp83C, &s0, &s1);                  // sy -> var_87C, cy -> var_86C
			tmp83C = 0.0f;
			X87SinCos(tmp83C, &s3, &s2);                  // sp -> var_85C, cp -> var_864
			pos[0] = s2 * s1 * 20.0f + xyz[0];
			pos[1] = s2 * s0 * 20.0f + xyz[1];
			pos[2] = (-0.0f - s3) * 20.0f + xyz[2];
			G_DebugLine(xyz, pos, color, 1);

			color[0] = 0.5f;
			color[3] = 0.75f;
			color[1] = 0.5f;
			color[2] = 0.0f;

			tmp858 = vec[1] * kDegToRad;
			X87SinCos(tmp858, &s1, &s2);                  // sy -> var_86C, cy -> var_864
			tmp858 = 0.0f;
			X87SinCos(tmp858, &s0, &s3);                  // sp -> var_87C, cp -> var_85C
			pos[0] = s3 * s2 * 20.0f + xyz[0];
			pos[1] = s3 * s1 * 20.0f + xyz[1];
			pos[2] = (-0.0f - s0) * 20.0f + xyz[2];
			G_DebugLine(xyz, pos, color, 1);

			tmp858 = yawBase * kDegToRad;
			X87SinCos(tmp858, &s1, &s2);                  // sy -> var_86C, cy -> var_864
			tmp858 = 0.0f;
			X87SinCos(tmp858, &s0, &s3);                  // sp -> var_87C, cp -> var_85C
			pos[0] = s3 * s2 * 20.0f + xyz[0];
			pos[1] = s3 * s1 * 20.0f + xyz[1];
			pos[2] = (-0.0f - s0) * 20.0f + xyz[2];
			G_DebugLine(xyz, pos, color, 1);

			// AngleNormalize180(yawBase - vAngles[1]), inlined.
			tmp854 = (yawBase - vec[1]) * kInv360;
			const float fl = floorf_sse(tmp854 + 0.5f);
			if ((tmp854 - fl) * 360.0f > kZero)
				G_DebugArc(xyz, 16.0f, vec[1], yawBase, color);
			else
				G_DebugArc(xyz, 16.0f, yawBase, vec[1], color);
		}

		// pGrenade
		if (At<unsigned short>(actor, 0x20E8))
		{
			G_DebugLine(xyz, &At<float>(EntHandleEnt(At<unsigned short>(actor, 0x20E8)), 0x258), colorOrange, 1);

			vec[0] = 0.0f;
			vec[1] = 0.0f;
			vec[2] = 1.0f;
			G_DebugCircle(&At<float>(EntHandleEnt(At<unsigned short>(actor, 0x20E8)), 0x258), 8.0f, colorOrange, 0, 0, vec);

			gentity_s* const grenade = EntHandleEnt(At<unsigned short>(actor, 0x20E8));
			const int weapon = At<int>(grenade, 0xE0);                         // s.weapon
			BYTE* const weapDef = reinterpret_cast<BYTE*>(bg_weaponDefs[weapon]);
			vec[0] = 0.0f;
			vec[1] = 0.0f;
			vec[2] = 1.0f;
			G_DebugCircle(&At<float>(grenade, 0x258), static_cast<float>(At<int>(weapDef, 0x650)), colorOrange, 0, 0, vec);
		}
	}
	else if (!drawGoalLineRadius)
	{
		goto label_4B7D58;
	}

	// loc_4B790A — pulsing goal color.
	{
		const int levelTime = *level_time;
		int endTime = *Actor_EntInfo_endTime;
		color[3] = 1.0f;
		if (levelTime > endTime)
		{
			const int dir = (*Actor_EntInfo_direction == 0);
			endTime = levelTime + 1000;
			*Actor_EntInfo_direction = dir;
			*Actor_EntInfo_endTime = endTime;
		}
		else if (endTime - levelTime > 1000)
		{
			endTime = 0;
			*Actor_EntInfo_endTime = endTime;
		}

		const float fact = static_cast<float>(endTime - levelTime) * *Actor_EntInfo_timeScale;
		if (*Actor_EntInfo_direction != 0)
		{
			color[2] = fact;
			color[1] = fact;
			color[0] = fact;
		}
		else
		{
			const float inv = 1.0f - fact;
			color[2] = inv;
			color[1] = inv;
			color[0] = inv;
		}
	}

	// scriptGoalEnt
	if (At<unsigned short>(actor, 0x19E0) == 0)
	{
		pos[0] = At<float>(actor, 0x19B8);
		pos[1] = At<float>(actor, 0x19BC);
		pos[2] = At<float>(actor, 0x19C0) + 16.0f;
		if (At<void*>(actor, 0x1A08) == nullptr)                               // pDesiredChainPos
			G_DebugLine(xyz, pos, color, 0);
		vec[0] = 0.0f;
		vec[1] = 0.0f;
		vec[2] = 1.0f;
		G_DebugCircle(pos, At<float>(actor, 0x19D0), color, 0, 0, vec);         // scriptGoal.radius
	}

	// fixedNode
	if (At<char>(actor, 0x19F5))
	{
		pos[0] = At<float>(actor, 0x19B8);
		pos[1] = At<float>(actor, 0x19BC);
		pos[2] = At<float>(actor, 0x19C0) + 16.0f;
		color[2] = 0.0f;
		color[0] = 0.0f;
		G_DebugLine(xyz, pos, color, 0);
		vec[0] = 0.0f;
		vec[1] = 0.0f;
		vec[2] = 1.0f;
		G_DebugCircle(pos, At<float>(actor, 0x19F8), color, 0, 0, vec);         // fixedNodeSafeRadius
	}

	if (usingCodeGoal(actor))
	{
		pos[0] = At<float>(actor, 0x198C);
		pos[1] = At<float>(actor, 0x1990);
		pos[2] = At<float>(actor, 0x1994) + 16.0f;
		G_DebugLine(xyz, pos, colorMagenta, 0);

		const int src = At<int>(actor, 0x19B4);                                // codeGoalSrc
		if (src == 1)
		{
			color[2] = 1.0f;
			color[1] = 0.0f;
			color[0] = 0.0f;
		}
		else if (src == 2)
		{
			color[1] = 1.0f;
			color[2] = 0.0f;
			color[0] = 0.0f;
		}
		else if (src == 3)
		{
			color[0] = 1.0f;
			color[1] = 0.0f;
			color[2] = 1.0f;
		}
		else
		{
			color[2] = 0.0f;
			color[1] = 0.0f;
			color[0] = 0.0f;
		}
		color[3] = 1.0f;
		vec[0] = 0.0f;
		vec[1] = 0.0f;
		vec[2] = 1.0f;
		G_DebugCircle(pos, At<float>(actor, 0x19A4), color, 0, 0, vec);         // codeGoal.radius
	}

	// codeGoal.volume
	if (At<void*>(actor, 0x19B0) && (*ai_showRegion)->current.enabled)
		Actor_DebugDrawNodesInVolume(actor);

	{
		void* const claimed = At<void*>(At<sentient_s*>(self, 0x188), 0x58);  // sentient->pClaimedNode
		if (claimed
		 && (*ai_showClaimedNode)->current.enabled
		 && !(*ai_debugCoverSelection)->current.enabled
		 && !(*ai_debugThreatSelection)->current.enabled)
		{
			// CL_GetViewPos, inlined.
			vec[0] = clViewPos[0];
			vec[1] = clViewPos[1];
			vec[2] = clViewPos[2];
			G_DebugLine(xyz, &At<float>(claimed, 0x14), colorBlue, 0);           // constant.vOrigin
			Path_DrawDebugNode(At<void*>(At<sentient_s*>(self, 0x188), 0x58), vec);
		}
	}

	if (drawLines)
	{
		if (At<void*>(At<sentient_s*>(self, 0x188), 0x58) && At<void*>(actor, 0x1A08))
			G_DebugLine(xyz, &At<float>(At<void*>(actor, 0x1A08), 0x14), colorYellow, 0);
		else if (At<void*>(actor, 0x1A08))
			G_DebugLine(xyz, &At<float>(At<void*>(actor, 0x1A08), 0x14), colorCyan, 0);
	}

label_4B7D58:
	const float* textColor;                                                    // var_828
	{
		const int team = At<int>(At<sentient_s*>(actor, 0x4), 0x4);           // eTeam
		if (team >= 0 && team < 5)
			textColor = &Actor_EntInfo_teamColors[team * 4];
		else
			textColor = colorYellow;
	}

	if (drawGoalLineRadius)
	{
		xyz[2] = infoScale * 3.5f + xyz[2];
		const char* const s = Com_FormatMsg("%i", At<int>(self, 0x0));
		G_AddDebugString(xyz, textColor, infoScale * kPoint6, s);
		return;
	}

	xyz[2] = infoScale * 70.0f + xyz[2];

	if ((*ai_debugAccuracy)->current.enabled && (*ai_debugEntIndex)->current.integer == At<int>(self, 0x0))
		return;

	int textMode;                                                              // var_864
	float textScale;                                                           // var_87C
	int aiText;
	if ((*g_entinfo)->current.integer != 2 && (aiText = (*g_entinfo_AItext)->current.integer) != 0)
	{
		const unsigned short tn = At<unsigned short>(self, 0x1A8);             // targetname
		const char* const name = tn ? SL_ConvertToString(tn, SCRIPTINSTANCE_SERVER) : "<noname>";
		textScale = infoScale * kPoint6;
		const char* const s = Com_FormatMsg("%i : %s (%s)", At<int>(self, 0x0), name, g_entinfoAITextNames[aiText]);
		G_AddDebugString(xyz, textColor, textScale, s);
		textMode = 1 << ((*g_entinfo_AItext)->current.integer & 31);       // shl eax, cl
	}
	else
	{
		const unsigned short tn = At<unsigned short>(self, 0x1A8);
		const char* const name = tn ? SL_ConvertToString(tn, SCRIPTINSTANCE_SERVER) : "<noname>";
		textScale = infoScale * kPoint6;
		const char* const s = Com_FormatMsg("%i : %s", At<int>(self, 0x0), name);
		G_AddDebugString(xyz, textColor, textScale, s);
		textMode = 0x1E;
	}

	float lineStep;                                                            // var_86C

	if (textMode & 6)
	{
		float range = 0.0f;                                                    // var_858
		const int showCombat = textMode & 4;                                   // var_854
		if (showCombat)
		{
			xyz[2] = xyz[2] - infoScale * *entInfoLineStep;
			const char* const s = Com_FormatMsg("health: %i", At<int>(self, 0x1C8));
			G_AddDebugString(xyz, textColor, textScale, s);
		}

		const float* targetColor;
		if (target)
		{
			// Sentient_GetOrigin(actor->sentient), inlined.
			const gentity_s* const sentEnt = At<gentity_s*>(At<sentient_s*>(actor, 0x4), 0x0);
			const float dx = At<float>(sentEnt, 0x160) - At<float>(target, 0x160);
			const float dz = At<float>(sentEnt, 0x168) - At<float>(target, 0x168);
			const float dy = At<float>(sentEnt, 0x164) - At<float>(target, 0x164);
			range = I_sqrt(dz * dz + dy * dy + dx * dx);
			targetColor = colorRed;
		}
		else
		{
			targetColor = colorYellow;
		}

		lineStep = infoScale * *entInfoLineStep;
		xyz[2] = xyz[2] - lineStep;

		const char* targetName;
		int targetNumber;
		if (!target)
		{
			targetName = "no target";
			targetNumber = 0;
		}
		else
		{
			const unsigned short tn = At<unsigned short>(target, 0x1A8);
			targetName = tn ? SL_ConvertToString(tn, SCRIPTINSTANCE_SERVER) : "<noname target>";
			targetNumber = At<int>(target, 0x0);
		}
		{
			const char* const s = Com_FormatMsg("%i : %s", targetNumber, targetName);
			G_AddDebugString(xyz, targetColor, textScale, s);
		}

		if (showCombat)
		{
			const int missCount = At<int>(actor, 0xC44);
			xyz[2] = xyz[2] - lineStep;
			unsigned int count = static_cast<unsigned int>(missCount);
			const char* hit;
			if (missCount != 0)
				hit = "MISS";
			else
			{
				count = static_cast<unsigned int>(At<int>(actor, 0xC48));      // hitCount
				hit = "HIT";
			}
			const char* s = Com_FormatMsg("range: %.2f ac: %.2f %s %u",
			                              static_cast<double>(range), static_cast<double>(At<float>(actor, 0xC4C)), hit, count);
			G_AddDebugString(xyz, colorWhite, textScale, s);

			const int talkTo = At<int>(actor, 0x202C);
			xyz[2] = xyz[2] - lineStep;
			s = Com_FormatMsg("talkto: %d", talkTo);
			G_AddDebugString(xyz, colorWhite, textScale, s);
		}
	}

	const int showState = textMode & 0x10;                                     // edi
	if (showState)
	{
		const int lvl = At<int>(actor, 0xBB4);                                 // stateLevel
		xyz[2] = xyz[2] - infoScale * *entInfoLineStep;
		const char* const s = Com_FormatMsg("(%i)%i:%i = %s", lvl,
		                                    At<int>(actor, 0xB8C + lvl * 4), At<int>(actor, 0xBA0 + lvl * 4),
		                                    At<const char*>(actor, 0x21CC));
		G_AddDebugString(xyz, colorWhite, textScale, s);
	}

	if (textMode & 0x12)
	{
		xyz[2] = xyz[2] - infoScale * *entInfoLineStep;
		const void* const func = At<void*>(actor, 0xD78);                      // pAnimScriptFunc
		const char* const funcName = func ? SL_ConvertToString(At<unsigned short>(func, 0x4), SCRIPTINSTANCE_SERVER) : "<none>";
		const char* const state = SL_ConvertToString(At<unsigned short>(actor, 0x215C), SCRIPTINSTANCE_SERVER);
		const char* const s = Com_FormatMsg("%s [%s]", funcName, state);
		G_AddDebugString(xyz, colorWhite, textScale, s);
	}

	if (showState)
	{
		xyz[2] = xyz[2] - infoScale * *entInfoLineStep;
		const char* const reason = SL_ConvertToString(At<unsigned short>(actor, 0x2160), SCRIPTINSTANCE_SERVER);
		const char* const last = SL_ConvertToString(At<unsigned short>(actor, 0x215E), SCRIPTINSTANCE_SERVER);
		const char* const s = Com_FormatMsg("<-  %s [%s]", last, reason);
		G_AddDebugString(xyz, colorWhite, textScale, s);
	}

	if (textMode & 8)
	{
		lineStep = infoScale * *entInfoLineStep;
		xyz[2] = xyz[2] - lineStep;

		const int scriptMode = At<int>(actor, 0xC98);                          // ScriptOrient.eMode
		const float* orientColor;
		if (scriptMode == 0)
			orientColor = colorGreen;
		else if (scriptMode == At<int>(actor, 0xC88))                          // CodeOrient.eMode
			orientColor = colorYellow;
		else
			orientColor = colorRed;

		const char* s;
		const float yaw = At<float>(At<gentity_s*>(actor, 0x0), 0x170);
		if (scriptMode)
			s = Com_FormatMsg("orient: %s (%s <- script) [%3.2f --> %3.2f]",
			                  aiOrientModeNames[At<int>(actor, 0xC88)], aiOrientModeNames[scriptMode],
			                  static_cast<double>(At<float>(actor, 0xCA4)), static_cast<double>(yaw));
		else
			s = Com_FormatMsg("orient: %s [%3.2f --> %3.2f]",
			                  aiOrientModeNames[At<int>(actor, 0xC88)],
			                  static_cast<double>(At<float>(actor, 0xC94)), static_cast<double>(yaw));
		G_AddDebugString(xyz, orientColor, textScale, s);

		if (At<short>(actor, 0x18D4) > 0)                                      // Path.wPathLen
		{
			xyz[2] = xyz[2] - lineStep;
			G_AddDebugString(xyz, colorWhite, textScale, "has path");
			if (At<short>(actor, 0x18D4) > 0
			 && !Path_DistanceGreaterThan(&At<BYTE>(actor, 0x1554), 128.0f))
			{
				xyz[2] = xyz[2] - lineStep;
				G_AddDebugString(xyz, colorWhite, textScale, "minDistForceFaceEnemy");
			}
		}

		if (At<void*>(actor, 0x195C))                                          // pPileUpActor
		{
			const int blocker = At<int>(At<void*>(actor, 0x1960), 0x0);        // pPileUpEnt->s.number
			const int blockee = At<int>(At<void*>(At<void*>(actor, 0x195C), 0x0), 0x0);
			xyz[2] = xyz[2] - lineStep;
			s = Com_FormatMsg("blockee: %d, blocker: %d", blockee, blocker);
			G_AddDebugString(xyz, colorWhite, textScale, s);
		}

		if (At<unsigned short>(actor, 0x2162))                                 // pCloseEnt
		{
			xyz[2] = xyz[2] - lineStep;
			s = Com_FormatMsg("closeEnt: %d", At<int>(EntHandleEnt(At<unsigned short>(actor, 0x2162)), 0x0));
			G_AddDebugString(xyz, colorWhite, textScale, s);
		}

		if (At<int>(actor, 0x1964))                                            // bDontAvoidPlayer
		{
			xyz[2] = xyz[2] - lineStep;
			G_AddDebugString(xyz, colorWhite, textScale, "dontavoidplayer");
		}

		if (!(At<int>(actor, 0xDFC) & 0x2000000))                              // Physics.iTraceMask
		{
			xyz[2] = xyz[2] - lineStep;
			G_AddDebugString(xyz, colorWhite, textScale, "pushPlayer");
		}

		xyz[2] = xyz[2] - lineStep;
		s = Com_FormatMsg("physics %d", At<int>(actor, 0xDD4));                // Physics.ePhysicsType
		G_AddDebugString(xyz, colorWhite, textScale, s);
	}

	if (textMode & 0xA)
	{
		xyz[2] = xyz[2] - infoScale * *entInfoLineStep;
		const char* const s = Com_FormatMsg("animmode %s script: %s",
		                                    aiAnimModeNames[At<int>(actor, 0xD8C)], aiAnimModeNames[At<int>(actor, 0xD90)]);
		G_AddDebugString(xyz, colorWhite, textScale, s);
	}

	if (textMode & 4)
	{
		if (At<char>(At<gentity_s*>(actor, 0x0), 0x19B) == 0)                 // actor->ent->takedamage
		{
			xyz[2] = xyz[2] - infoScale * *entInfoLineStep;
			G_AddDebugString(xyz, colorRed, textScale, "Invulnerable");
		}
		if (At<char>(actor, 0xC3B))                                            // provideCoveringFire
		{
			xyz[2] = xyz[2] - infoScale * *entInfoLineStep;
			G_AddDebugString(xyz, colorWhite, textScale, "Covering fire");
		}
		if (At<int>(actor, 0x20B8))                                            // ignoreSuppression
		{
			xyz[2] = xyz[2] - infoScale * *entInfoLineStep;
			G_AddDebugString(xyz, colorRed, textScale, "Ignore suppression");
		}
		if (At<int>(actor, 0x20C4) > 0 || At<float>(actor, 0x20C8) > kZero)    // suppressionStartTime / suppressionMeter
		{
			xyz[2] = xyz[2] - infoScale * *entInfoLineStep;
			const char* const s = Com_FormatMsg("Suppressed %0.2f", static_cast<double>(At<float>(actor, 0x20C8)));
			G_AddDebugString(xyz, colorRed, textScale, s);
		}
		else if (Actor_IsMoveSuppressed(actor))
		{
			xyz[2] = xyz[2] - infoScale * *entInfoLineStep;
			G_AddDebugString(xyz, colorCyan, textScale, "Move Suppressed");
		}
	}

	lineStep = infoScale * *entInfoLineStep;
	xyz[2] = xyz[2] - lineStep;
	{
		const int badPlace = At<int>(actor, 0x2144);                           // aiBadPlace
		const char* bp;
		if (badPlace == 0)
			bp = "NONE";
		else if (badPlace == 1)
			bp = "YES-Normal";
		else
			bp = "Yes-ReallyBad";
		const char* s = Com_FormatMsg("IN BADPLACE: %s AWARENESS:%3.2f", bp, static_cast<double>(At<float>(actor, 0x2148)));
		G_AddDebugString(xyz, colorWhite, textScale, s);

		xyz[2] = xyz[2] - lineStep;
		s = Com_FormatMsg("IGNOREALL FLAG: %s", At<char>(At<sentient_s*>(actor, 0x4), 0x11) ? "ON" : "OFF");
		G_AddDebugString(xyz, colorWhite, textScale, s);

		xyz[2] = xyz[2] - lineStep;
		// Vanilla passes the float radius as a double to "%d"; kept.
		s = Com_FormatMsg("GoalRadius: %d", static_cast<double>(At<float>(actor, 0x19D0)));
		G_AddDebugString(xyz, colorWhite, textScale, s);
	}
}

void PatchT4MAM_SentientInfo_EntInfo()
{
	Detours::X86::DetourFunction(T4M::GetAddress("Actor_EntInfo"),
	                             reinterpret_cast<uintptr_t>(&T4_Reconstructed::Actor_EntInfo),
	                             Detours::X86Option::USE_JUMP);
}
