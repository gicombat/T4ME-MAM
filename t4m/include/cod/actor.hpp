#pragma once

// Actor / Sentient subsystem (SP only). CoD4 reference: src/game/actor.cpp,
// src/game/sentient.cpp, src/game/enthandle.cpp, src/game/actor_event_listeners.cpp.
//
// Bucketing: Actor_* / Sentient_* / G_* are game-module gameplay code -> T4::game.
// Data globals -> T4::engine, matching globals.hpp.
//
// Vanilla caps, for reference (see R&D analysis/actor_sentient_RE.md):
//   MAX_ACTORS = 32, MAX_SENTIENTS = 36 (32 actors + 4 SP clients)
//   sizeof(actor_s) = 0x31B8, sizeof(sentient_s) = 0x88

namespace T4
{
	namespace game
	{
		// DETOURED by T4_Reconstructed when the AI limit patch is active.
		WEAK engine::symbol<engine::actor_s*()>    Actor_Alloc{ "Actor_Alloc" };
		WEAK engine::symbol<engine::sentient_s*()> Sentient_Alloc{ "Sentient_Alloc" };
		WEAK engine::symbol<void()>                G_InitActors{ "G_InitActors" };
		WEAK engine::symbol<void()>                Sentient_ClearAll{ "Sentient_ClearAll" };

		// WaW sub_4B5340 — usercall(actor@eax) -> void ; no stack args.
		inline void Actor_SetDefaults(engine::actor_s* actor)
		{
			static void* fn = reinterpret_cast<void*>(T4M::GetAddress("Actor_SetDefaults"));
			__asm
			{
				mov  eax, actor
				call fn
			}
		}

		// WaW sub_4D7970 — usercall(obj@eax) -> void ; leaf, no stack args, no ambient
		// registers. Constructs the sub-object embedded at actor_s + 0xD94, writing a
		// dispatch pointer at +0x10C plus float constants. The exe's CRT static
		// initializer (0x7E8A52) runs it over the 32 vanilla slots long before
		// Sys_RunInit, so a grown pool has to build its extra slots itself to match what
		// vanilla's never-allocated slots hold (Actor_Alloc memsets the whole actor_s, so
		// live actors carry it zeroed regardless). Writes constants only: idempotent.
		inline void Actor_ConstructSubObj_D94(void* subObj)
		{
			static void* fn = reinterpret_cast<void*>(T4M::GetAddress("Actor_ConstructSubObj_D94"));
			__asm
			{
				mov  eax, subObj
				call fn
			}
		}

		// --- savegame (CoD4 game/g_save.cpp) --------------------------------------
		// ReadActor (0x50F3F0) is not exposed: it is detoured, see T4_Reconstructed.
		WEAK engine::symbol<void(const engine::saveField_t* fields, unsigned char* dest, int tempsize, engine::SaveGame* save)>
			G_ReadStruct{ "G_ReadStruct" };

		// WaW sub_50F370 — usercall(save@eax, actor@stack) ; retn, caller cleans.
		inline void ReadActorPotentialCoverNodes(engine::actor_s* actor, engine::SaveGame* save)
		{
			static void* fn = reinterpret_cast<void*>(T4M::GetAddress("ReadActorPotentialCoverNodes"));
			__asm
			{
				push actor
				mov  eax, save
				call fn
				add  esp, 4
			}
		}

		// --- AI limit phase 8.2 (sentientInfo readers/writers: vanilla callees) ------------------------------
		// WaW sub_4E1670 — usercall(self@esi, pSuppressor@edi) ; retn, no stack args.
		inline void Actor_DissociateSuppressor(engine::actor_s* self, engine::sentient_s* pSuppressor)
		{
			static void* fn = reinterpret_cast<void*>(T4M::GetAddress("Actor_DissociateSuppressor"));
			__asm
			{
				mov  esi, self
				mov  edi, pSuppressor
				call fn
			}
		}

		// WaW sub_4C6F10 — usercall(sentient@ecx, eType@stack0) ; retn 4, callee cleans.
		inline void Actor_BroadcastTeamEvent(engine::sentient_s* sentient, int eType)
		{
			static void* fn = reinterpret_cast<void*>(T4M::GetAddress("Actor_BroadcastTeamEvent"));
			__asm
			{
				push eType
				mov  ecx, sentient
				call fn
			}
		}

		// WaW sub_4E6D90 — usercall(ent@eax, this@stack0) ; retn 4, callee cleans. CoD4 EntHandle::setEnt.
		inline void EntHandle_setEnt(engine::EntHandle* handle, engine::gentity_s* ent)
		{
			static void* fn = reinterpret_cast<void*>(T4M::GetAddress("EntHandle_setEnt"));
			__asm
			{
				push handle
				mov  eax, ent
				call fn
			}
		}

		// WaW sub_4BE580 — usercall(self@ecx, prevGoalPos@edx) ; retn, no stack args.
		inline void Actor_CheckOverridePos(engine::actor_s* self, const float* prevGoalPos)
		{
			static void* fn = reinterpret_cast<void*>(T4M::GetAddress("Actor_CheckOverridePos"));
			__asm
			{
				mov  ecx, self
				mov  edx, prevGoalPos
				call fn
			}
		}

		// WaW sub_4E0EB0 — usercall(self@esi, eState@edi) -> int@eax ; retn, no stack args.
		inline int Actor_PushState(engine::actor_s* self, engine::ai_state_t eState)
		{
			static void* fn = reinterpret_cast<void*>(T4M::GetAddress("Actor_PushState"));
			int result;
			__asm
			{
				mov  esi, self
				mov  edi, eState
				call fn
				mov  result, eax
			}
			return result;
		}

		// WaW sub_4C08D0 — usercall(self@esi) ; retn, no stack args.
		inline void Actor_KillAnimScript(engine::actor_s* self)
		{
			static void* fn = reinterpret_cast<void*>(T4M::GetAddress("Actor_KillAnimScript"));
			__asm
			{
				mov  esi, self
				call fn
			}
		}

		// WaW sub_4C3EB0 — usercall(node@eax, rangeOut@esi) ; retn, no stack args.
		// Case 18 forwards eax (node) to sub_5591D0 — set by this wrapper already.
		inline void Actor_Cover_InitRange(const engine::pathnode_t* node, engine::pathnodeRange_t* rangeOut)
		{
			static void* fn = reinterpret_cast<void*>(T4M::GetAddress("Actor_Cover_InitRange"));
			__asm
			{
				mov  eax, node
				mov  esi, rangeOut
				call fn
			}
		}

		// WaW sub_4C3DD0 — usercall(pos@eax, node@esi, range@edi) -> bool@al ; retn, no stack args.
		inline bool Actor_Cover_NodeRangeValid(const float* pos, const engine::pathnode_t* node, engine::pathnodeRange_t* range)
		{
			static void* fn = reinterpret_cast<void*>(T4M::GetAddress("Actor_Cover_NodeRangeValid"));
			bool result;
			__asm
			{
				mov  eax, pos
				mov  esi, node
				mov  edi, range
				call fn
				mov  result, al
			}
			return result;
		}

		// WaW sub_4DF870 — usercall(self@eax) -> bool@al ; retn, no stack args.
		inline bool Actor_CanSeeEnemy(engine::actor_s* self)
		{
			static void* fn = reinterpret_cast<void*>(T4M::GetAddress("Actor_CanSeeEnemy"));
			bool result;
			__asm
			{
				mov  eax, self
				call fn
				mov  result, al
			}
			return result;
		}

		WEAK engine::symbol<bool()> CreateDebugStringsIfNeeded{ "CreateDebugStringsIfNeeded" };   // sub_474740
		WEAK engine::symbol<bool()> CreateDebugLinesIfNeeded{ "CreateDebugLinesIfNeeded" };       // sub_474980

		// sub_558F40 — cdecl.
		WEAK engine::symbol<void(const float* xyz, const float* color, float scale, const char* text)>
			G_AddDebugString{ "G_AddDebugString" };

		// sub_4C5190 — stdcall (retn 4).
		WEAK engine::symbol<void __stdcall(engine::actor_s* actor)>
			Actor_DebugDrawNodesInVolume{ "Actor_DebugDrawNodesInVolume" };

		// WaW sub_4748B0 — usercall(list@edi, xyz@ecx, color@eax, scale@xmm0, text, duration) ; caller cleans 8.
		inline void AddDebugStringToList(void* list, const float* xyz, const float* color, float scale, const char* text, int duration)
		{
			static void* fn = reinterpret_cast<void*>(T4M::GetAddress("AddDebugStringToList"));
			__asm
			{
				push duration
				push text
				mov  edi, list
				mov  ecx, xyz
				mov  eax, color
				movss xmm0, scale
				call fn
				add  esp, 8
			}
		}

		// WaW sub_474B80 — usercall(list@ecx, start@edi, color@edx, end, depthTest, duration) ; caller cleans 0xC.
		inline void AddDebugLineToList(void* list, const float* startPt, const float* endPt, const float* color, int depthTest, int duration)
		{
			static void* fn = reinterpret_cast<void*>(T4M::GetAddress("AddDebugLineToList"));
			__asm
			{
				push duration
				push depthTest
				push endPt
				mov  ecx, list
				mov  edi, startPt
				mov  edx, color
				call fn
				add  esp, 0Ch
			}
		}

		// WaW sub_4F7B10 — usercall(start@edi, end, color, depthTest) ; caller cleans 0xC.
		inline void G_DebugLine(const float* startPt, const float* endPt, const float* color, int depthTest)
		{
			static void* fn = reinterpret_cast<void*>(T4M::GetAddress("G_DebugLine"));
			__asm
			{
				push depthTest
				push color
				push endPt
				mov  edi, startPt
				call fn
				add  esp, 0Ch
			}
		}

		// WaW sub_4F7B90 — usercall(origin@edx, maxs@eax, yaw@xmm0, mins, color, depthTest, duration) ; caller cleans 0x10.
		inline void G_DebugBox(const float* origin, const float* mins, const float* maxs, float yaw, const float* color, int depthTest, int duration)
		{
			static void* fn = reinterpret_cast<void*>(T4M::GetAddress("G_DebugBox"));
			__asm
			{
				push duration
				push depthTest
				push color
				push mins
				mov  edx, origin
				mov  eax, maxs
				movss xmm0, yaw
				call fn
				add  esp, 10h
			}
		}

		// WaW sub_4F7D20 — usercall(up@eax, center@ecx, radius, color, depthTest, duration) ; caller cleans 0x10.
		inline void G_DebugCircle(const float* center, float radius, const float* color, int depthTest, int duration, const float* up)
		{
			static void* fn = reinterpret_cast<void*>(T4M::GetAddress("G_DebugCircle"));
			__asm
			{
				push duration
				push depthTest
				push color
				push dword ptr radius
				mov  ecx, center
				mov  eax, up
				call fn
				add  esp, 10h
			}
		}

		// WaW sub_4F8020 — usercall(center@eax, endAngle@xmm0, radius, startAngle, color) ; caller cleans 0xC.
		inline void G_DebugArc(const float* center, float radius, float startAngle, float endAngle, const float* color)
		{
			static void* fn = reinterpret_cast<void*>(T4M::GetAddress("G_DebugArc"));
			__asm
			{
				push color
				push dword ptr startAngle
				push dword ptr radius
				mov  eax, center
				movss xmm0, endAngle
				call fn
				add  esp, 0Ch
			}
		}

		// WaW sub_566340 — usercall(sentient@eax, eyePos@edx) ; no stack args.
		inline void Sentient_GetDebugEyePosition(engine::sentient_s* sentient, float* eyePos)
		{
			static void* fn = reinterpret_cast<void*>(T4M::GetAddress("Sentient_GetDebugEyePosition"));
			__asm
			{
				mov  eax, sentient
				mov  edx, eyePos
				call fn
			}
		}

		// WaW sub_4E0080 — usercall(actor@ecx, eyePos@edx) ; no stack args.
		inline void Actor_GetDebugEyePosition(engine::actor_s* actor, float* eyePos)
		{
			static void* fn = reinterpret_cast<void*>(T4M::GetAddress("Actor_GetDebugEyePosition"));
			__asm
			{
				mov  ecx, actor
				mov  edx, eyePos
				call fn
			}
		}

		// WaW sub_4EC4E0 — usercall(eyePos@edi, client@stack) ; caller cleans 4.
		inline void G_GetPlayerEyePosition(float* eyePos, void* client)
		{
			static void* fn = reinterpret_cast<void*>(T4M::GetAddress("G_GetPlayerEyePosition"));
			__asm
			{
				push client
				mov  edi, eyePos
				call fn
				add  esp, 4
			}
		}

		// WaW sub_4B6D60 — usercall(actor@ecx) -> int@eax.
		inline int usingCodeGoal(engine::actor_s* actor)
		{
			static void* fn = reinterpret_cast<void*>(T4M::GetAddress("usingCodeGoal"));
			int result;
			__asm
			{
				mov  ecx, actor
				call fn
				mov  result, eax
			}
			return result;
		}

		// WaW sub_55B120 — usercall(node@eax, viewPos@stack) ; caller cleans 4.
		inline void Path_DrawDebugNode(void* node, const float* viewPos)
		{
			static void* fn = reinterpret_cast<void*>(T4M::GetAddress("Path_DrawDebugNode"));
			__asm
			{
				push viewPos
				mov  eax, node
				call fn
				add  esp, 4
			}
		}

		// WaW sub_4D0650 — usercall(path@ecx, dist@xmm1) -> int@eax.
		inline int Path_DistanceGreaterThan(void* path, float dist)
		{
			static void* fn = reinterpret_cast<void*>(T4M::GetAddress("Path_DistanceGreaterThan"));
			int result;
			__asm
			{
				mov  ecx, path
				movss xmm1, dist
				call fn
				mov  result, eax
			}
			return result;
		}

		// WaW sub_4E17A0 — usercall(actor@eax) -> int@eax.
		inline int Actor_IsMoveSuppressed(engine::actor_s* actor)
		{
			static void* fn = reinterpret_cast<void*>(T4M::GetAddress("Actor_IsMoveSuppressed"));
			int result;
			__asm
			{
				mov  eax, actor
				call fn
				mov  result, eax
			}
			return result;
		}

		// WaW sub_4E1120 — usercall(self@ecx, vStart@eax, sentient@stack, vEnd@stack) ; retn 8.
		// CoD4 actor_suppression.cpp Actor_AddSuppressionLine.
		inline void Actor_AddSuppressionLine(engine::actor_s* self, const float* vStart,
		                                     engine::sentient_s* sentient, const float* vEnd)
		{
			static void* fn = reinterpret_cast<void*>(T4M::GetAddress("Actor_AddSuppressionLine"));
			__asm
			{
				push vEnd
				push sentient
				mov  eax, vStart
				mov  ecx, self
				call fn
			}
		}

		// WaW sub_4E5440 — usercall(self@ecx) -> int@eax (0/1, callers test al)
		inline int Actor_IsUsingTurret(engine::actor_s* self)
		{
			static void* fn = reinterpret_cast<void*>(T4M::GetAddress("Actor_IsUsingTurret"));
			int result;
			__asm
			{
				mov  ecx, self
				call fn
				mov  result, eax
			}
			return result;
		}

		// WaW sub_4E3250 — __stdcall, retn 4. Tail of CoD4 Actor_CaresAboutInfo once WaW has
		// inlined the sentientInfo read: returns level.time - lastKnownPosTime >= 2000.
		WEAK engine::symbol<int __stdcall(int lastKnownPosTime)> Actor_CaresAboutInfoTime{ "Actor_CaresAboutInfoTime" };

		// WaW sub_4C7840 — usercall(sentient@ecx, self@edi) ; retn.
		// CoD4 actor_events.cpp Actor_EventNewEnemy (argument = originator->sentient).
		inline void Actor_EventNewEnemy(engine::actor_s* self, engine::sentient_s* originator)
		{
			static void* fn = reinterpret_cast<void*>(T4M::GetAddress("Actor_EventNewEnemy"));
			__asm
			{
				mov  ecx, originator
				mov  edi, self
				call fn
			}
		}

		// WaW sub_4C78D0 — usercall(originator@eax, self@stack, vOrigin@stack) ; retn, caller cleans 8.
		// CoD4 actor_events.cpp Actor_EventExplosion.
		inline void Actor_EventExplosion(engine::gentity_s* originator, engine::actor_s* self, const float* vOrigin)
		{
			static void* fn = reinterpret_cast<void*>(T4M::GetAddress("Actor_EventExplosion"));
			__asm
			{
				push vOrigin
				push self
				mov  eax, originator
				call fn
				add  esp, 8
			}
		}

		// WaW sub_4CCE80 — usercall(originator@eax, self@stack) ; retn 4.
		// CoD4 actor_grenade.cpp Actor_GrenadePing (Actor_EventGrenadePing inlined into it).
		inline void Actor_GrenadePing(engine::gentity_s* originator, engine::actor_s* self)
		{
			static void* fn = reinterpret_cast<void*>(T4M::GetAddress("Actor_GrenadePing"));
			__asm
			{
				push self
				mov  eax, originator
				call fn
			}
		}

		// WaW sub_4C0D30 — usercall(self@eax) ; retn. CoD4 actor.cpp Actor_AnimCombat.
		inline void Actor_AnimCombat(engine::actor_s* self)
		{
			static void* fn = reinterpret_cast<void*>(T4M::GetAddress("Actor_AnimCombat"));
			__asm
			{
				mov  eax, self
				call fn
			}
		}

		// WaW sub_4BA420 — usercall(self@eax) -> al ; retn. CoD4 actor.cpp Actor_IsAtGoal.
		inline bool Actor_IsAtGoal(engine::actor_s* self)
		{
			static void* fn = reinterpret_cast<void*>(T4M::GetAddress("Actor_IsAtGoal"));
			unsigned char r;
			__asm
			{
				mov  eax, self
				call fn
				mov  r, al
			}
			return r != 0;
		}

		// WaW sub_4C7CF0 — usercall(self@eax) ; retn. CoD4 actor_exposed.cpp Actor_Exposed_CheckLockGoal.
		inline void Actor_Exposed_CheckLockGoal(engine::actor_s* self)
		{
			static void* fn = reinterpret_cast<void*>(T4M::GetAddress("Actor_Exposed_CheckLockGoal"));
			__asm
			{
				mov  eax, self
				call fn
			}
		}

		// WaW sub_4C4300 — usercall(ppScript@eax, self@edx, node@edi, flag@stack) -> eax ; retn 4.
		// CoD4 actor_cover.cpp Actor_Cover_PickAttackScript (self, node, 1, &script).
		inline int Actor_Cover_PickAttackScript(engine::actor_s* self, engine::pathnode_t* node, int flag,
		                                        void** ppScript)
		{
			static void* fn = reinterpret_cast<void*>(T4M::GetAddress("Actor_Cover_PickAttackScript"));
			int r;
			__asm
			{
				push flag
				mov  eax, ppScript
				mov  edx, self
				mov  edi, node
				call fn
				mov  r, eax
			}
			return r;
		}

		// WaW sub_4D62C0 — usercall(self@eax, mode@edx) ; retn. Leaf.
		// CoD4 actor_orientation.cpp Actor_SetOrientMode.
		inline void Actor_SetOrientMode(engine::actor_s* self, engine::ai_orient_mode_t mode)
		{
			static void* fn = reinterpret_cast<void*>(T4M::GetAddress("Actor_SetOrientMode"));
			__asm
			{
				mov  eax, self
				mov  edx, mode
				call fn
			}
		}

		// WaW sub_4D5CF0 — usercall(self@edi, pOrient@eax) ; retn.
		// CoD4 actor_orientation.cpp Actor_FaceLikelyEnemyPath.
		inline void Actor_FaceLikelyEnemyPath(engine::actor_s* self, engine::ai_orient_t* pOrient)
		{
			static void* fn = reinterpret_cast<void*>(T4M::GetAddress("Actor_FaceLikelyEnemyPath"));
			__asm
			{
				mov  edi, self
				mov  eax, pOrient
				call fn
			}
		}

		// WaW sub_4CFCF0 — usercall(pInfo@edi, sentient@stack, enemy@stack) -> eax ; retn 8.
		// CoD4 pathnode.cpp Path_FindFacingNode. pInfo is a sentientInfo row (or null):
		// always pass what T4M::Actor_SentientInfo returned.
		inline engine::pathnode_t* Path_FindFacingNode(engine::sentient_s* sentient, engine::sentient_s* enemy,
		                                               engine::sentient_info_t* pInfo)
		{
			static void* fn = reinterpret_cast<void*>(T4M::GetAddress("Path_FindFacingNode"));
			engine::pathnode_t* r;
			__asm
			{
				push enemy
				push sentient
				mov  edi, pInfo
				call fn
				mov  r, eax
			}
			return r;
		}

		// WaW sub_4D5460 — usercall(v@eax, pOrient@stack) ; retn 4.
		// CoD4 actor_orientation.cpp Actor_FaceVector.
		inline void Actor_FaceVector(const float* v, engine::ai_orient_t* pOrient)
		{
			static void* fn = reinterpret_cast<void*>(T4M::GetAddress("Actor_FaceVector"));
			__asm
			{
				push pOrient
				mov  eax, v
				call fn
			}
		}

		// WaW sub_4D4F40 — usercall(pOrient@esi, pitch@xmm3, yaw@stack) ; retn 4.
		// CoD4 actor_orientation.cpp Actor_SetDesiredAngles. Vanilla callers pass yaw through
		// fld / push ecx / fstp [esp]; reproduced as is.
		inline void Actor_SetDesiredAngles(engine::ai_orient_t* pOrient, float pitch, float yaw)
		{
			static void* fn = reinterpret_cast<void*>(T4M::GetAddress("Actor_SetDesiredAngles"));
			__asm
			{
				fld    yaw
				mov    esi, pOrient
				movss  xmm3, pitch
				push   ecx
				fstp   dword ptr [esp]
				call   fn
			}
		}

		// WaW sub_5663D0 — usercall(sentient@esi) -> pathnode_t*@eax ; retn. Keeps edi.
		inline engine::pathnode_t* Sentient_NearestNode(engine::sentient_s* sentient)
		{
			static void* fn = reinterpret_cast<void*>(T4M::GetAddress("Sentient_NearestNode"));
			engine::pathnode_t* result;
			__asm
			{
				mov  esi, sentient
				call fn
				mov  result, eax
			}
			return result;
		}

		// WaW sub_55C150 — usercall(nodeA@eax, nodeB@ecx) -> int@eax ; retn. Leaf, symmetric.
		inline int Path_NodesVisible(const engine::pathnode_t* nodeA, const engine::pathnode_t* nodeB)
		{
			static void* fn = reinterpret_cast<void*>(T4M::GetAddress("Path_NodesVisible"));
			int result;
			__asm
			{
				mov  eax, nodeA
				mov  ecx, nodeB
				call fn
				mov  result, eax
			}
			return result;
		}

		// WaW sub_4D55E0 — usercall(self@edx) ; retn. Keeps esi/edi.
		inline void Actor_SetAnglesToLikelyEnemyPath(engine::actor_s* self)
		{
			static void* fn = reinterpret_cast<void*>(T4M::GetAddress("Actor_SetAnglesToLikelyEnemyPath"));
			__asm
			{
				mov  edx, self
				call fn
			}
		}

		// WaW sub_5662F0 — usercall(sentient@eax, outPos@esi) ; retn.
		inline void Sentient_GetEyePosition(engine::sentient_s* sentient, float* outPos)
		{
			static void* fn = reinterpret_cast<void*>(T4M::GetAddress("Sentient_GetEyePosition"));
			__asm
			{
				mov  eax, sentient
				mov  esi, outPos
				call fn
			}
		}

		// WaW sub_4DFEF0 — usercall(self@edi) ; retn. Refreshes actor_s::eyeInfo once per frame.
		inline void Actor_UpdateEyeInformation(engine::actor_s* self)
		{
			static void* fn = reinterpret_cast<void*>(T4M::GetAddress("Actor_UpdateEyeInformation"));
			__asm
			{
				mov  edi, self
				call fn
			}
		}

		// WaW sub_4E0050 — usercall(self@eax, outPos@esi) ; retn.
		inline void Actor_GetEyePosition(engine::actor_s* self, float* outPos)
		{
			static void* fn = reinterpret_cast<void*>(T4M::GetAddress("Actor_GetEyePosition"));
			__asm
			{
				mov  eax, self
				mov  esi, outPos
				call fn
			}
		}

		// WaW sub_569030 — usercall(point@eax, turret@stack, viewOut@stack, anglesOut@stack)
		// -> int@eax ; retn, caller cleans 0xC.
		inline int turret_CanTargetPoint(const float* point, engine::gentity_s* turret, float* viewOut, float* anglesOut)
		{
			static void* fn = reinterpret_cast<void*>(T4M::GetAddress("turret_CanTargetPoint"));
			int result;
			__asm
			{
				push anglesOut
				push viewOut
				push turret
				mov  eax, point
				call fn
				add  esp, 0Ch
				mov  result, eax
			}
			return result;
		}

		// WaW sub_4DF170 — usercall(point@eax, eyePos@ecx, self@stack, fovDot@stack,
		// fMaxDistSqrd@stack) -> int@eax ; retn 0Ch.
		inline int PointInFovAndRange(const float* point, const float* eyePos, engine::actor_s* self, float fovDot, float fMaxDistSqrd)
		{
			static void* fn = reinterpret_cast<void*>(T4M::GetAddress("PointInFovAndRange"));
			int result;
			__asm
			{
				push dword ptr fMaxDistSqrd
				push dword ptr fovDot
				push self
				mov  eax, point
				mov  ecx, eyePos
				call fn
				mov  result, eax
			}
			return result;
		}

		// WaW sub_4DF360 — usercall(bVisible@al, pInfo@ecx, self@edi, ent@stack) ; retn 4.
		// Calls Actor_UpdateLastKnownPos (sub_4DFE10, detoured) on a visible transition.
		inline void Actor_UpdateVisCache(engine::actor_s* self, const engine::gentity_s* ent, engine::sentient_info_t* pInfo, unsigned char bVisible)
		{
			static void* fn = reinterpret_cast<void*>(T4M::GetAddress("Actor_UpdateVisCache"));
			__asm
			{
				push ent
				mov  ecx, pInfo
				mov  edi, self
				mov  al, bVisible
				call fn
			}
		}

		// WaW sub_54E6A0 — usercall(tagName@eax, ent@stack0, pos@stack1) -> int ; caller cleans.
		// (G_ prefix: could live in animation.hpp instead; only turret code uses it so far.)
		inline int G_DObjGetWorldTagPos(engine::gentity_s* ent, unsigned int tagName, float* pos)
		{
			static void* fn = reinterpret_cast<void*>(T4M::GetAddress("G_DObjGetWorldTagPos"));
			int result;
			__asm
			{
				push pos
				push ent
				mov  eax, tagName
				call fn
				add  esp, 8
				mov  result, eax
			}
			return result;
		}

		// WaW sub_54A690 — usercall(ent@edi, outPos@stack) ; retn, caller cleans 4.
		inline void G_EntityCentroid(const engine::gentity_s* ent, float* outPos)
		{
			static void* fn = reinterpret_cast<void*>(T4M::GetAddress("G_EntityCentroid"));
			__asm
			{
				push outPos
				mov  edi, ent
				call fn
				add  esp, 4
			}
		}

		// WaW sub_4DF270 — usercall(self@eax, point@esi, fovDot@stack, fMaxDistSqrd@stack,
		// ignoreEntNum@stack) -> int@eax ; retn 0Ch. Callers only test al.
		inline int Actor_CanSeePointEx(engine::actor_s* self, const float* point, float fovDot, float fMaxDistSqrd, int ignoreEntNum)
		{
			static void* fn = reinterpret_cast<void*>(T4M::GetAddress("Actor_CanSeePointEx"));
			int result;
			__asm
			{
				push ignoreEntNum
				push dword ptr fMaxDistSqrd
				push dword ptr fovDot
				mov  eax, self
				mov  esi, point
				call fn
				mov  result, eax
			}
			return result;
		}

		// WaW sub_4DF8D0 — usercall(self@eax) -> bool@al
		inline bool Actor_CanSeeEnemyViaClaimedNode(engine::actor_s* self)
		{
			static void* fn = reinterpret_cast<void*>(T4M::GetAddress("Actor_CanSeeEnemyViaClaimedNode"));
			unsigned char result;
			__asm
			{
				mov  eax, self
				call fn
				mov  result, al
			}
			return result != 0;
		}

		// WaW sub_566800 — usercall(teamFlags@esi) -> sentient_s*@eax ; retn.
		// Bound patched to NEW_MAX_SENTIENTS (aiCap_SentientIter_56682C).
		inline engine::sentient_s* Sentient_FirstSentient(int teamFlags)
		{
			static void* fn = reinterpret_cast<void*>(T4M::GetAddress("Sentient_FirstSentient"));
			engine::sentient_s* result;
			__asm
			{
				mov  esi, teamFlags
				call fn
				mov  result, eax
			}
			return result;
		}

		// WaW sub_566850 — usercall(sentient@eax, teamFlags@edi) -> sentient_s*@eax ; retn.
		// Bounds patched to NEW_MAX_SENTIENTS (aiCap_SentientIter_56686F / _56689C).
		inline engine::sentient_s* Sentient_NextSentient(engine::sentient_s* sentient, int teamFlags)
		{
			static void* fn = reinterpret_cast<void*>(T4M::GetAddress("Sentient_NextSentient"));
			engine::sentient_s* result;
			__asm
			{
				mov  eax, sentient
				mov  edi, teamFlags
				call fn
				mov  result, eax
			}
			return result;
		}

		// WaW sub_4C03B0 — usercall(sentient@eax, dist@stack) -> float@xmm0 ; caller cleans.
		// WaW dropped CoD4's second sentient argument.
		inline float Sentient_GetScarinessForDistance(engine::sentient_s* sentient, float dist)
		{
			static void* fn = reinterpret_cast<void*>(T4M::GetAddress("Sentient_GetScarinessForDistance"));
			float result;
			__asm
			{
				push dist
				mov  eax, sentient
				call fn
				add  esp, 4
				movss result, xmm0
			}
			return result;
		}

		// WaW sub_4E3360 — usercall(self@esi, enemy@ecx, isCurrentEnemy@edi) -> int@eax
		inline int Actor_ThreatFromAttackerCount(engine::actor_s* self, engine::sentient_s* enemy, int isCurrentEnemy)
		{
			static void* fn = reinterpret_cast<void*>(T4M::GetAddress("Actor_ThreatFromAttackerCount"));
			int result;
			__asm
			{
				mov  esi, self
				mov  ecx, enemy
				mov  edi, isCurrentEnemy
				call fn
				mov  result, eax
			}
			return result;
		}

		// WaW sub_4E33D0 — usercall(enemy@ecx, self@stack) -> int@eax ; retn 4.
		inline int Actor_ThreatCoveringFire(engine::sentient_s* enemy, engine::actor_s* self)
		{
			static void* fn = reinterpret_cast<void*>(T4M::GetAddress("Actor_ThreatCoveringFire"));
			int result;
			__asm
			{
				push self
				mov  ecx, enemy
				call fn
				mov  result, eax
			}
			return result;
		}

		// WaW sub_4E34C0 — usercall(enemy@eax) -> int@eax (0 or 200)
		inline int Actor_ThreatFlashed(engine::sentient_s* enemy)
		{
			static void* fn = reinterpret_cast<void*>(T4M::GetAddress("Actor_ThreatFlashed"));
			int result;
			__asm
			{
				mov  eax, enemy
				call fn
				mov  result, eax
			}
			return result;
		}

		// WaW sub_4E38B0 — usercall(self@ecx) -> void
		inline void Actor_IncrementThreatTime(engine::actor_s* self)
		{
			static void* fn = reinterpret_cast<void*>(T4M::GetAddress("Actor_IncrementThreatTime"));
			__asm
			{
				mov  ecx, self
				call fn
			}
		}

		// WaW sub_4E3D80 — usercall(self@esi) -> void ; ai_showPotentialThreatDir debug draw.
		inline void Actor_PotentialThreat_Debug(engine::actor_s* self)
		{
			static void* fn = reinterpret_cast<void*>(T4M::GetAddress("Actor_PotentialThreat_Debug"));
			__asm
			{
				mov  esi, self
				call fn
			}
		}

		// WaW sub_566690 — usercall(self@eax, enemy@stack0, bNotify@stack1) -> void ; retn 8.
		inline void Sentient_SetEnemy(engine::sentient_s* self, engine::gentity_s* enemy, int bNotify)
		{
			static void* fn = reinterpret_cast<void*>(T4M::GetAddress("Sentient_SetEnemy"));
			__asm
			{
				push bNotify
				push enemy
				mov  eax, self
				call fn
			}
		}

		// WaW sub_4CE560 — usercall(origin@eax, path@edi) -> void
		inline void Path_AddTrimmedAmount(engine::path_t* path, const float* origin)
		{
			static void* fn = reinterpret_cast<void*>(T4M::GetAddress("Path_AddTrimmedAmount"));
			__asm
			{
				mov  eax, origin
				mov  edi, path
				call fn
			}
		}

		// WaW sub_4D02E0 — usercall(path@ecx) -> void
		inline void Path_Clear(engine::path_t* path)
		{
			static void* fn = reinterpret_cast<void*>(T4M::GetAddress("Path_Clear"));
			__asm
			{
				mov  ecx, path
				call fn
			}
		}

		// WaW sub_4E5480 — usercall(self@esi) -> void
		inline void Actor_StopUseTurret(engine::actor_s* self)
		{
			static void* fn = reinterpret_cast<void*>(T4M::GetAddress("Actor_StopUseTurret"));
			__asm
			{
				mov  esi, self
				call fn
			}
		}

		// WaW sub_4E0E60 — usercall(self@eax, eState@esi) -> void
		inline void Actor_SetState(engine::actor_s* self, int eState)
		{
			static void* fn = reinterpret_cast<void*>(T4M::GetAddress("Actor_SetState"));
			__asm
			{
				mov  eax, self
				mov  esi, eState
				call fn
			}
		}

		// WaW sub_4B5A10 — usercall(self@esi) -> bool@al
		inline bool Actor_KeepClaimedNode(engine::actor_s* self)
		{
			static void* fn = reinterpret_cast<void*>(T4M::GetAddress("Actor_KeepClaimedNode"));
			unsigned char result;
			__asm
			{
				mov  esi, self
				call fn
				mov  result, al
			}
			return result != 0;
		}

		// WaW sub_4BE410 — stdcall(self), retn 4.
		WEAK engine::symbol<void __stdcall(engine::actor_s* self)> Actor_UpdateDesiredChainPos{ "Actor_UpdateDesiredChainPos" };

		// WaW sub_4CBFD0 — usercall(self@ecx, point@stack) -> int@eax ; retn 4.
		inline int Actor_Grenade_IsPointSafe(engine::actor_s* self, const float* point)
		{
			static void* fn = reinterpret_cast<void*>(T4M::GetAddress("Actor_Grenade_IsPointSafe"));
			int result;
			__asm
			{
				push point
				mov  ecx, self
				call fn
				mov  result, eax
			}
			return result;
		}

		// WaW sub_4B5B40 — usercall(self@edi) -> void. Calls Actor_UpdateThreat.
		inline void Actor_PreThink(engine::actor_s* self)
		{
			static void* fn = reinterpret_cast<void*>(T4M::GetAddress("Actor_PreThink"));
			__asm
			{
				mov  edi, self
				call fn
			}
		}

		// WaW sub_55D0F0 — usercall(node@edx, claimer@eax) -> int@eax
		inline int Path_CanClaimNode(engine::pathnode_t* node, engine::sentient_s* claimer)
		{
			static void* fn = reinterpret_cast<void*>(T4M::GetAddress("Path_CanClaimNode"));
			int result;
			__asm
			{
				mov  edx, node
				mov  eax, claimer
				call fn
				mov  result, eax
			}
			return result;
		}

		// WaW sub_55D2B0 — usercall(node@edi, claimer@stack) -> void ; caller cleans.
		inline void Path_ForceClaimNode(engine::pathnode_t* node, engine::sentient_s* claimer)
		{
			static void* fn = reinterpret_cast<void*>(T4M::GetAddress("Path_ForceClaimNode"));
			__asm
			{
				push claimer
				mov  edi, node
				call fn
				add  esp, 4
			}
		}

		// WaW sub_4C0930 — stdcall(self, animScript, moveMode, animMode), retn 10h.
		WEAK engine::symbol<void __stdcall(engine::actor_s* self, engine::scr_animscript_t* animScript, unsigned char moveMode, int animMode)>
			Actor_SetAnimScript{ "Actor_SetAnimScript" };

		// WaW sub_4E4190 — stdcall(self) -> actor_think_result_t@eax, retn 4.
		WEAK engine::symbol<int __stdcall(engine::actor_s* self)> Actor_Turret_PostThink{ "Actor_Turret_PostThink" };

		// WaW sub_4B88E0 — usercall(actor@eax) -> sentient_s*@eax ; leaf, inlined in several places.
		inline engine::sentient_s* Actor_GetTargetSentient(engine::actor_s* actor)
		{
			static void* fn = reinterpret_cast<void*>(T4M::GetAddress("Actor_GetTargetSentient"));
			engine::sentient_s* result;
			__asm
			{
				mov  eax, actor
				call fn
				mov  result, eax
			}
			return result;
		}

		// WaW sub_4E6E30 — usercall(sentient@eax, handle@stack) ; retn 4 (callee cleans).
		// CoD4 SentientHandle::setSentient.
		inline void SentientHandle_setSentient(engine::SentientHandle* handle, engine::sentient_s* sentient)
		{
			static void* fn = reinterpret_cast<void*>(T4M::GetAddress("SentientHandle_setSentient"));
			__asm
			{
				push handle
				mov  eax, sentient
				call fn
			}
		}

		// WaW sub_4E6C90 — usercall(oldInfoIndex@eax, entHandleList@edx) ; retn, no stack args.
		inline void RemoveEntHandleInfo(engine::EntHandleList* entHandleList, unsigned int oldInfoIndex)
		{
			static void* fn = reinterpret_cast<void*>(T4M::GetAddress("RemoveEntHandleInfo"));
			__asm
			{
				mov  eax, oldInfoIndex
				mov  edx, entHandleList
				call fn
			}
		}

		// WaW sub_4BA350 — usercall(vPoint@ecx, vGoalPos@eax, buffer@stack) -> bool@al ; retn 4.
		inline bool Actor_PointNearPoint(const float* vPoint, const float* vGoalPos, float buffer)
		{
			static void* fn = reinterpret_cast<void*>(T4M::GetAddress("Actor_PointNearPoint"));
			bool result;
			__asm
			{
				push buffer
				mov  eax, vGoalPos
				mov  ecx, vPoint
				call fn
				mov  result, al
			}
			return result;
		}
	}

	namespace engine
	{
		// level.actors / level.sentients in CoD4 terms. The game module rewrites both
		// pointers on every map load.
		WEAK symbol<actor_s*>    g_actors{ "g_actorsPtr" };
		WEAK symbol<sentient_s*> g_sentients{ "g_sentientsPtr" };

		// sizeof(actor_s), exported into svs by the game module.
		WEAK symbol<int> g_actorStride{ "g_actorStride" };

		// setailimit budget. 0 = unlimited; SpawnActor fails silently above it.
		WEAK symbol<int> g_aiLimit{ "g_aiLimit" };

		// --- tables sized by MAX_ACTORS / MAX_SENTIENTS ---------------------------
		// g_scr_data.actorXAnimTrees[MAX_ACTORS] — indexed by actor index
		// (CoD4 G_GetActorAnimTree), so it must grow with the pool.
		WEAK symbol<XAnimTree_s*> g_scr_actorXAnimTrees{ "g_scr_actorXAnimTrees" };

		// EntHandleList g_sentientsHandleList[MAX_SENTIENTS] — indexed by sentient
		// index (CoD4 SentientHandleDissociate), grows with the sentient pool.
		WEAK symbol<unsigned short> g_sentientsHandleList{ "g_sentientsHandleList" };

		// AIEventListener g_AIEVlisteners[MAX_ACTORS] + its count. Count-bounded, not
		// index-addressed: overflowing it only raises "Max listeners exceeded", it
		// cannot corrupt memory. Grown and relocated all the same - the ceiling is on
		// listening entities, and it does not get any higher just because the actor
		// pool did. The savegame archives only the used prefix (count * 8 bytes), so
		// growing it needs no savegame patch. Note the [MAX_ACTORS] above is our own
		// sizing: CoD4 declares it g_AIEVlisteners[32] against a separate constant.
		WEAK symbol<AIEventListener> g_AIEVlisteners{ "g_AIEVlisteners" };
		WEAK symbol<int>             g_listenerCount{ "g_listenerCount" };

		// g_scr_data.actorCorpseInfo — sized by the corpse budget, NOT by MAX_ACTORS
		// (CoD4 has 16 corpses for 32 actors). Do not grow it with the pool.
		WEAK symbol<BYTE> g_scr_actorCorpseInfo{ "g_scr_actorCorpseInfo" };

		// svs.snapshotActors ring depth (0x200). Per local client.
		WEAK symbol<int> svSnapshotActorsRingSize{ "svSnapshotActorsRingSize" };

		// WaW sub_5F53D0 — usercall(dest@ecx, size@edi, save@esi) ; retn, no stack args.
		// Reads save->server_memFile, which sits at offset 0.
		inline void SaveMemory_LoadRead(void* dest, int bytes, SaveGame* save)
		{
			static void* fn = reinterpret_cast<void*>(T4M::GetAddress("SaveMemory_LoadRead"));
			__asm
			{
				mov  ecx, dest
				mov  edi, bytes
				mov  esi, save
				call fn
			}
		}

		// CoD4 actorFields — WaW's copy still lists 33 sentientInfo[k].pLastKnownNode
		// (SF_PATHNODE), CoD4's MAX_SENTIENTS, not WaW's 36.
		WEAK symbol<saveField_t> actorFields{ "actorFields" };
		WEAK symbol<char>        emptyString{ "emptyString" };

		// Function-local statics of WaW's ReadActor: a prototype of the proximity visitor
		// embedded at actor_s::Physics.proximity_data, built once and torn down at exit.
		WEAK symbol<DWORD>                  ReadActor_staticGuard{ "ReadActor_staticGuard" };
		WEAK symbol<colgeom_visitor_inlined_t> ReadActor_staticProximity{ "ReadActor_staticProximity" };
		WEAK symbol<BYTE>                   ReadActor_proximityVtbl{ "ReadActor_proximityVtbl" };
		WEAK symbol<float>                  ReadActor_boundsMinInit{ "ReadActor_boundsMinInit" };   //  1e38
		WEAK symbol<float>                  ReadActor_boundsMaxInit{ "ReadActor_boundsMaxInit" };   // -1e38
		WEAK symbol<void __cdecl()>         ReadActor_staticDtor{ "ReadActor_staticDtor" };
		WEAK symbol<int __cdecl(void (__cdecl*)())> crt_atexit{ "crt_atexit" };

		// --- AI limit phase 8.2 (data) ------------------------------
		// AIFuncTable[AISpecies] -> ai_funcs_t[ai_state_t] (7 pointers per state).
		WEAK symbol<const ai_funcs_t*>  AIFuncTable{ "AIFuncTable" };

		// modNames[meansOfDeath] -> &scr_const.<mod> (CoD4 g_combat.cpp).
		WEAK symbol<unsigned short*>    modNames{ "modNames" };

		// g_HitLocConstNames[hitLocation_t] (CoD4 g_combat.cpp).
		WEAK symbol<unsigned short>     g_HitLocConstNames{ "g_HitLocConstNames" };

		// &g_scr_data.anim — compared against actor_s::pAnimScriptFunc.
		WEAK symbol<scr_animscript_t>   g_scr_data_anim{ "g_scr_data_anim" };

		// Actor_EntInfo function-local statics (pulsing goal color)
		WEAK symbol<int> Actor_EntInfo_endTime{ "Actor_EntInfo_endTime" };

		WEAK symbol<int> Actor_EntInfo_direction{ "Actor_EntInfo_direction" };        // .data, initial 1

		// .data floats read by Actor_EntInfo (0.001f and 7.0f); read through memory, as vanilla does
		WEAK symbol<float> Actor_EntInfo_timeScale{ "Actor_EntInfo_timeScale" };

		WEAK symbol<float> entInfoLineStep{ "entInfoLineStep" };

		// G_GetEntInfoScale view term (bss float, written by sub_42DF50)
		WEAK symbol<float> g_entInfoViewScale{ "g_entInfoViewScale" };

		// colorTeam[5][4]
		WEAK symbol<float> Actor_EntInfo_teamColors{ "Actor_EntInfo_teamColors" };

		// string tables
		WEAK symbol<const char*> g_entinfoAITextNames{ "g_entinfoAITextNames" };      // "all","brief",...
		WEAK symbol<const char*> aiOrientModeNames{ "aiOrientModeNames" };            // "invalid","dont_change",...
		WEAK symbol<const char*> aiAnimModeNames{ "aiAnimModeNames" };                // "none","(code)",...

		// g_threatBias.threatTable[16][16] (CoD4 threat_bias_t), row = enemy group,
		// column = self group. INT_MIN = ignore.
		WEAK symbol<int> g_threatBias_threatTable{ "g_threatBias_threatTable" };

		// scr_const.tag_eye (scr_const + 0x282).
		WEAK symbol<unsigned short> scr_const_tag_eye{ "scr_const_tag_eye" };

		// PROF_SCOPED begin/end stub (a bare retn). IDA: nullsub_3 SP, nullsub_2 ger.
		WEAK symbol<void()> Prof_Nullsub{ "nullsub_3" };

		// CRT qsort, and actor_senses.cpp compare_sentient_sort (loc_4DFC20: pe2->fMetric -
		// pe1->fMetric as integer bit patterns, descending).
		WEAK symbol<void(void* base, unsigned int num, unsigned int width, int (__cdecl* cmp)(const void*, const void*))>
			crt_qsort{ "crt_qsort" };

		WEAK symbol<int(const void*, const void*)> compare_sentient_sort{ "compare_sentient_sort" };
	}
}
