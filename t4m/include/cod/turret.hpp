#pragma once

// Turret subsystem (SP). CoD4 reference: src/game/turret.cpp.
// turret_think_auto (0x56A280), turret_think_manual (0x56AB00) and turret_canuse_auto (0x56B420)
// are not exposed: they are detoured, see T4_Reconstructed.

namespace T4
{
	namespace game
	{
		// WaW sub_569160 — cdecl(self, sentient, targetPos[3] out, muzzlePos[3] out, localAngles[2] out) -> int.
		WEAK engine::symbol<int(engine::gentity_s* self, engine::sentient_s* sentient, float* targetPos, float* muzzlePos, float* localAngles)>
			turret_CanTargetSentient{ "turret_CanTargetSentient" };

		// WaW sub_568D30 — cdecl(self, angles[2], bManned) -> int.
		WEAK engine::symbol<int(engine::gentity_s* self, float* desiredAngles, int bManned)>
			turret_UpdateTargetAngles{ "turret_UpdateTargetAngles" };

		// WaW sub_568F30 — cdecl(self).
		WEAK engine::symbol<void(engine::gentity_s* self)> turret_ClearTargetEnt{ "turret_ClearTargetEnt" };

		// WaW sub_568FC0 — usercall(self@esi, bManned@stack) -> int ; caller cleans.
		inline int turret_ReturnToDefaultPos(engine::gentity_s* self, int bManned)
		{
			static void* fn = reinterpret_cast<void*>(T4M::GetAddress("turret_ReturnToDefaultPos"));
			int result;
			__asm
			{
				push bManned
				mov  esi, self
				call fn
				add  esp, 4
				mov  result, eax
			}
			return result;
		}

		// WaW sub_569710 — usercall(self@eax, desiredAngles@edi, origin@stack0, bShoot@stack1) -> int ; caller cleans.
		inline int turret_aimat_vector(engine::gentity_s* self, float* origin, int bShoot, float* desiredAngles)
		{
			static void* fn = reinterpret_cast<void*>(T4M::GetAddress("turret_aimat_vector"));
			int result;
			__asm
			{
				push bShoot
				push origin
				mov  edi, desiredAngles
				mov  eax, self
				call fn
				add  esp, 8
				mov  result, eax
			}
			return result;
		}

		// WaW sub_569760 — usercall(enemy@eax, self@stack0, bShoot@stack1, missTime@stack2, desiredAngles@stack3) -> int ; caller cleans.
		inline int turret_aimat_Sentient_Internal(engine::sentient_s* enemy, engine::gentity_s* self, int bShoot, int missTime, float* desiredAngles)
		{
			static void* fn = reinterpret_cast<void*>(T4M::GetAddress("turret_aimat_Sentient_Internal"));
			int result;
			__asm
			{
				push desiredAngles
				push missTime
				push bShoot
				push self
				mov  eax, enemy
				call fn
				add  esp, 10h
				mov  result, eax
			}
			return result;
		}

		// WaW sub_569A90 — usercall(self@eax, ent@stack0, bShoot@stack1) -> int ; caller cleans.
		inline int turret_aimat_Ent(engine::gentity_s* self, engine::gentity_s* ent, int bShoot)
		{
			static void* fn = reinterpret_cast<void*>(T4M::GetAddress("turret_aimat_Ent"));
			int result;
			__asm
			{
				push bShoot
				push ent
				mov  eax, self
				call fn
				add  esp, 8
				mov  result, eax
			}
			return result;
		}

		// WaW sub_569B40 — usercall(start@stack0, end@stack1, passEnt1@ecx, passEnt2@eax) -> int ; caller cleans.
		inline int turret_SightTrace(const float* traceStart, const float* traceEnd, int passEnt1, int passEnt2)
		{
			static void* fn = reinterpret_cast<void*>(T4M::GetAddress("turret_SightTrace"));
			int result;
			__asm
			{
				push traceEnd
				push traceStart
				mov  ecx, passEnt1
				mov  eax, passEnt2
				call fn
				add  esp, 8
				mov  result, eax
			}
			return result;
		}

		// WaW sub_56B3A0 — usercall(turret@ecx, actor@edx) -> 0/1 in eax ; leaf. Callers test al.
		inline bool Actor_IsTurretCloserThenCurrent(engine::actor_s* actor, engine::gentity_s* turret)
		{
			static void* fn = reinterpret_cast<void*>(T4M::GetAddress("Actor_IsTurretCloserThenCurrent"));
			unsigned char result;
			__asm
			{
				mov  ecx, turret
				mov  edx, actor
				call fn
				mov  result, al
			}
			return result != 0;
		}
	}
}
