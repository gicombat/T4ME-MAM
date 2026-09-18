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
	}
}
