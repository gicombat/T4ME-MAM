// ==========================================================
// T4M project
//
// Component: clientdll
// Purpose: AI limit expansion (MAX_ACTORS / MAX_SENTIENTS above the vanilla 32 / 36)
//          Phases 1-2: both pools and the two index-addressed side tables relocated,
//          every count bound either detoured (preferred) or patched. Validated in game
//          at the vanilla 32 before phase 3 raised the cap.
//          Phase 3: NEW_MAX_ACTORS raised to 64.
//          Phase 4: the client half of the chain. Snapshot actor index widened to 6
//          bits, CL_GetSnapshot's silent clamp to 32 raised, the pair of snapshot_t
//          globals relocated and grown (their actors[] array is fixed-size and ends
//          exactly on the next field), and the two per-actor animation-tree arrays
//          (client and server DObj worlds) relocated.
//          Always on: there is no toggle. A gated build could never be exercised - the
//          gate was a dvar, and dvars are still at their defaults this early in
//          Sys_RunInit, so it read 0 on every run.
//          Phase 5: client actor ring left at its vanilla 512 entries (8 snapshots of
//          history at 64 actors) with an opt-in watchdog, t4m_aiRingWatch.
//          Phase 6: savegames are format-incompatible with vanilla ones, by design.
//          Phase 7: setailimit's clamp raised, t4m_aiMaxActors exposed to script.
//
//          Design + full RE: R&D plans/plan_increase_ai_limit.md
//                            R&D analysis/actor_sentient_RE.md
//
// Started: 2026-08-21
// ==========================================================

#include "StdInc.h"
#include <safetyhook.hpp>

#define NEW_MAX_ACTORS       64
#define SP_MAX_CLIENTS       4                                  // dword_18F6DC0
#define NEW_MAX_SENTIENTS    (NEW_MAX_ACTORS + SP_MAX_CLIENTS)

#define ACTOR_SIZE           0x31B8
#define SENTIENT_SIZE        0x88

// Client snapshot path. snapshot_t is a pair of consecutive .bss globals; actors[] is
// its last big block and ends exactly where the trailing dword starts, so growing it
// means relocating the pair, widening the stride and moving that one field.
#define ACTORSTATE_SIZE      0x78
#define SNAP_ACTORS_OFFSET   0x5471C
#define VANILLA_SNAP_TAIL    0x5561C
#define VANILLA_SNAP_SIZE    0x55620
#define NEW_SNAP_TAIL        (SNAP_ACTORS_OFFSET + NEW_MAX_ACTORS * ACTORSTATE_SIZE)
#define NEW_SNAP_SIZE        (NEW_SNAP_TAIL + 4)

// Per-actor animation-tree slot. One array per DObj world, both at the same offset
// inside their world block, which is what sub_42A2D0 relies on.
#define ACTORTREE_SIZE       0x458
#define ACTORTREE_BLOCK_OFS  0x89078
// The client array is walked from its tree-pointer field; the server one, from +8.
#define ACTORTREE_CG_BIAS    0x3DC
// Three more fields of the same element are reached by their own absolute form,
// and one is reached through the cg struct instead of the DObj world. None of
// them is findable by looking for the array base: tools/xref.py listed them.
#define ACTORTREE_F30        0x30
#define ACTORTREE_F440       0x440
#define ACTORTREE_F3AC       0x3AC
#define ACTORTREE_SV_BIAS    0x8

// Sub-object the CRT static initializer at 0x7E8A52 constructs in every actor slot.
// It is the only part of an actor_s that is not valid when zeroed, so it is the only
// one a freshly allocated slot has to be given.
#define ACTOR_SUBOBJ_OFFSET  0xD94

#define VANILLA_MAX_ACTORS    32
#define VANILLA_MAX_SENTIENTS 36
#define VANILLA_MAX_LISTENERS 32

// AI event listeners are per listening *entity*, not per actor, and the array is
// purely count-bounded (no pointer end marker anywhere). CoD4 sizes it with a literal
// of its own - MAX_AI_EV_LISTENERS, "AIEventListener g_AIEVlisteners[32]" - which is
// NOT MAX_ACTORS: the two just happen to be 32 in both games. Reusing the actor cap
// here is a T4M choice, made to keep one knob; nothing in the engine ties them.
#define NEW_MAX_LISTENERS     NEW_MAX_ACTORS
#define LISTENER_SIZE         0x8

// Every vanilla count bound is "cmp reg, imm8" followed by a signed jcc, so the
// immediate is sign-extended: 128 reads as -128 and the loop exits at once.
static_assert(NEW_MAX_ACTORS    <= 127, "count bounds are imm8 + signed jcc; see plan_increase_ai_limit.md");
static_assert(NEW_MAX_SENTIENTS <= 127, "same cliff applies to the sentient bounds");
static_assert(NEW_MAX_ACTORS    >= VANILLA_MAX_ACTORS, "shrinking below vanilla is not supported");

// The snapshot actor index is 6 bits wide after phase 4 (MSG_Write/ReadBits at
// 0x67ADC1 / 0x64BB77). Going past 64 needs 7 bits and a matching pair of patches.
static_assert(NEW_MAX_ACTORS <= 64, "the snapshot actor index is 6 bits; see plan_increase_ai_limit.md phase 4");

// The vanilla snapshot_t layout, restated as arithmetic so a typo cannot pass unnoticed.
static_assert(VANILLA_SNAP_TAIL == SNAP_ACTORS_OFFSET + VANILLA_MAX_ACTORS * ACTORSTATE_SIZE,
              "snapshot_t.actors[32] must end exactly on the trailing field");
static_assert(VANILLA_SNAP_SIZE == VANILLA_SNAP_TAIL + 4, "the trailing field is the last one");

// ==========================================================
// Reconstructions detoured at the vanilla VAs. Preferred over patching the
// baked count immediate: we own the whole function, it reads as the CoD4
// source it came from, and it is not bound by the imm8 sign-extension cliff.
// ==========================================================

// @modified — sub_4B4CA0 / CoD4 src/game/actor.cpp Actor_Alloc.
//   1:1 with vanilla except the walk stops at NEW_MAX_ACTORS.
//   DETOURED — do not call directly.
T4::engine::actor_s* T4_Reconstructed::Actor_Alloc()
{
	T4::engine::actor_s* actor = *T4::engine::g_actors;

	for (int i = 0; i < NEW_MAX_ACTORS; ++i, ++actor)
	{
		if (!actor->inuse)
		{
			memset(actor, 0, sizeof(T4::engine::actor_s));
			actor->inuse = 1;
			T4::game::Actor_SetDefaults(actor);
			return actor;
		}
	}

	T4::engine::Com_DPrintf(18, "Actor allocation failed\n");
	return nullptr;
}

// @modified — sub_566030 / CoD4 src/game/sentient.cpp Sentient_Alloc.
//   DETOURED — do not call directly.
T4::engine::sentient_s* T4_Reconstructed::Sentient_Alloc()
{
	T4::engine::sentient_s* sentient = *T4::engine::g_sentients;

	for (int i = 0; i < NEW_MAX_SENTIENTS; ++i, ++sentient)
	{
		if (!sentient->inuse)
		{
			memset(sentient, 0, sizeof(T4::engine::sentient_s));
			sentient->inuse = 1;
			return sentient;
		}
	}

	T4::engine::Com_DPrintf(15, "Sentient allocation failed\n");
	return nullptr;
}

// @modified — sub_4B52C0 / CoD4 src/game/actor.cpp G_InitActors, which inlines
//   Actor_EventListener_Init (actor_event_listeners.cpp) exactly like vanilla.
//   DETOURED — do not call directly.
void T4_Reconstructed::G_InitActors()
{
	T4::engine::actor_s* actors = *T4::engine::g_actors;
	for (int i = 0; i < NEW_MAX_ACTORS; ++i)
		actors[i].inuse = 0;

	// Actor_EventListener_Init, over the grown array. ENTITYNUM_NONE is written as
	// the literal 0x3FF the binary uses, not as MAX_GENTITIES-1, which T4M has since
	// moved. The symbol is re-pointed by ApplyActorLimit before this ever runs.
	*T4::engine::g_listenerCount = 0;
	T4::engine::AIEventListener* listeners = T4::engine::g_AIEVlisteners;
	for (int i = 0; i < NEW_MAX_LISTENERS; ++i)
	{
		listeners[i].entIndex = 0x3FF;
		listeners[i].events   = 0;
	}

	// @modified — the one part of the growth that cannot be a patch. G_InitActorSystem
	// clears g_sentientsHandleList a few instructions before calling us, with a memset
	// whose size is a push imm8 ("6A 48" = the vanilla 36 entries) at 0x50206B. 68 entries
	// is 0x88 bytes, which an imm8 sign-extends to -120, and no 2-byte encoding pushes a
	// larger positive value. Clearing the tail here — same call, a few instructions later,
	// before anything reads the table — is what the widened memset would have done.
	// Sentient_ClearAll does not touch this table, so without it the entries above 35
	// would carry stale handles across a map change.
	unsigned short* handles = T4::engine::g_sentientsHandleList;
	for (int i = VANILLA_MAX_SENTIENTS; i < NEW_MAX_SENTIENTS; ++i)
		handles[i] = 0;
}

// Vanilla's range message is a .rdata literal that names 32. Built once at install
// time instead, so it names whatever cap is actually in force.
static char g_setAiLimitError[96] = "SetAILimit must take a value between 0 and 32 inclusive.";

// @modified - WaW 0x52F000, the GSC builtin behind setailimit(). 1:1 with vanilla
//   except for the bound and the message naming it. Two vanilla oddities kept as-is:
//   the parameter error is inlined rather than calling Scr_ParamError (error_index
//   then Scr_Error), and the value is stored even on the error path, which is
//   harmless only because Scr_Error does not return.
//   Detoured rather than patched: owning the function is what lets the text move,
//   and it drops the bound out of the imm8 range entirely.
//   DETOURED - do not call directly.
void T4_Reconstructed::GScr_SetAILimit()
{
	const int limit = T4::engine::Scr_GetInt(T4::engine::SCRIPTINSTANCE_SERVER, 0);

	if (limit < 0 || limit > NEW_MAX_ACTORS)
	{
		T4::engine::gScrVarPub->error_index = 1;
		T4::engine::Scr_Error(g_setAiLimitError, T4::engine::SCRIPTINSTANCE_SERVER, 0);
	}

	*T4::engine::g_aiLimit = limit;
}

namespace T4M
{
	static BYTE* g_newActorPool    = nullptr;
	static BYTE* g_newSentientPool = nullptr;
	static BYTE* g_newXAnimTrees   = nullptr;
	static BYTE* g_newHandleList   = nullptr;
	static BYTE* g_newListeners    = nullptr;   // g_AIEVlisteners, count-bounded

	// What the console version line reports. Branding runs before this patch, but it
	// reports through a detour that the engine calls later, so the tag is up to date by
	// the time anyone reads it.
	static enum { AI_TAG_OFF, AI_TAG_FAILED, AI_TAG_ACTIVE } g_aiStatus = AI_TAG_OFF;
	static BYTE* g_newSnapPair     = nullptr;   // phase 4: the two snapshot_t globals
	static BYTE* g_treeReserve     = nullptr;   // phase 4: holds both actor-tree arrays
	static BYTE* g_newCgActorTrees = nullptr;
	static BYTE* g_newSvActorTrees = nullptr;

	static void ReleasePools()
	{
		if (g_newActorPool)    { VirtualFree(g_newActorPool, 0, MEM_RELEASE);    g_newActorPool = nullptr; }
		if (g_newSentientPool) { VirtualFree(g_newSentientPool, 0, MEM_RELEASE); g_newSentientPool = nullptr; }
		if (g_newXAnimTrees)   { VirtualFree(g_newXAnimTrees, 0, MEM_RELEASE);   g_newXAnimTrees = nullptr; }
		if (g_newHandleList)   { VirtualFree(g_newHandleList, 0, MEM_RELEASE);   g_newHandleList = nullptr; }
		if (g_newListeners)    { VirtualFree(g_newListeners, 0, MEM_RELEASE);    g_newListeners = nullptr; }
		if (g_newSnapPair)     { VirtualFree(g_newSnapPair, 0, MEM_RELEASE);     g_newSnapPair = nullptr; }
		if (g_treeReserve)     { VirtualFree(g_treeReserve, 0, MEM_RELEASE);     g_treeReserve = nullptr; }
		g_newCgActorTrees = nullptr;
		g_newSvActorTrees = nullptr;
	}

	// Init-safe only: PatchT4_* runs before the console exists, so no Com_Printf here.
	// Also mirrored to a file next to the exe, because the one line that matters is
	// emitted at DLL load - long before anyone thinks to attach a debugger.
	static void AiDbg(const char* msg)
	{
		OutputDebugStringA(msg);

		char path[MAX_PATH];
		if (!GetModuleFileNameA(NULL, path, MAX_PATH))
			return;

		char* const slash = strrchr(path, '\\');
		if (!slash)
			return;
		lstrcpyA(slash + 1, "t4m_ai.log");

		// First write of the run truncates: a log that only ever grows is a log nobody
		// reads to the end.
		static bool truncated = false;
		const HANDLE h = CreateFileA(path, FILE_APPEND_DATA, FILE_SHARE_READ, NULL,
		                             truncated ? OPEN_ALWAYS : CREATE_ALWAYS,
		                             FILE_ATTRIBUTE_NORMAL, NULL);
		if (h == INVALID_HANDLE_VALUE)
			return;
		truncated = true;

		DWORD written = 0;
		WriteFile(h, msg, static_cast<DWORD>(lstrlenA(msg)), &written, NULL);
		CloseHandle(h);
	}

	// -----------------------------------------------------------------
	// .text bounds, read from the running exe's PE headers rather than
	// hardcoded, so the SP and GER images are both covered.
	// -----------------------------------------------------------------
	static bool GetTextSection(BYTE** outBase, size_t* outSize)
	{
		BYTE* module = reinterpret_cast<BYTE*>(GetModuleHandle(NULL));
		if (!module)
			return false;

		const IMAGE_DOS_HEADER* dos = reinterpret_cast<const IMAGE_DOS_HEADER*>(module);
		if (dos->e_magic != IMAGE_DOS_SIGNATURE)
			return false;

		const IMAGE_NT_HEADERS* nt = reinterpret_cast<const IMAGE_NT_HEADERS*>(module + dos->e_lfanew);
		if (nt->Signature != IMAGE_NT_SIGNATURE)
			return false;

		const IMAGE_SECTION_HEADER* sec = IMAGE_FIRST_SECTION(nt);
		for (WORD i = 0; i < nt->FileHeader.NumberOfSections; ++i, ++sec)
		{
			if (memcmp(sec->Name, ".text", 5) == 0)
			{
				*outBase = module + sec->VirtualAddress;
				*outSize = sec->Misc.VirtualSize;
				return true;
			}
		}
		return false;
	}

	// -----------------------------------------------------------------
	// Scanned relocations: a whole-.text search for one 32-bit value.
	// Only used for pool base addresses, whose values are unique enough that
	// a byte-granular scan cannot collide. Sizes are NOT scanned: 0x1320 also
	// occurs at 0x74B05C as an esp displacement (fmul dword ptr [esp+1320h]).
	// -----------------------------------------------------------------
	struct ScanReloc
	{
		DWORD       from;
		DWORD       to;
		int         expected;   // exact occurrence count, verified in both exes
		int         found;
		const char* what;
		// Some constants are also used as the *end marker* of a neighbouring array,
		// where relocating them would run the neighbour's loop off into our buffer.
		// Such a site is recognised by the opcode in front of the immediate and left
		// alone; its count is verified just like the patched one.
		// 0 = no exclusion. Two-byte opcodes are given in memory order (81 FE -> 0xFE81);
		// a value below 0x100 is a one-byte opcode compared against the byte at -1,
		// which is what "cmp eax, imm32" (3D) needs since it has no modrm.
		WORD        skipOpcode;
		int         skipExpected;
		int         skipped;
	};

	static bool SkipThisSite(const ScanReloc& r, const BYTE* textBase, const BYTE* at)
	{
		if (!r.skipOpcode)
			return false;
		if (r.skipOpcode < 0x100)
			return at >= textBase + 1 && at[-1] == static_cast<BYTE>(r.skipOpcode);
		return at >= textBase + 2 && *reinterpret_cast<const WORD*>(at - 2) == r.skipOpcode;
	}

	// -----------------------------------------------------------------
	// Immediate patches addressed by VA (CSV key = instruction start).
	// -----------------------------------------------------------------
	struct ImmPatch
	{
		const char* key;
		BYTE        immOfs;     // immediate offset inside the instruction
		BYTE        immSize;    // 1 or 4
		DWORD       expect;     // vanilla value, verified before writing
		DWORD       value;
	};

	// Sites still handled by an immediate: either the bound lives inside a large
	// function we do not want to own (savegame, snapshot build), or it is a pure
	// size constant rather than a behavioural bound. Everything that could be
	// reconstructed cheaply is a detour instead — see T4_Reconstructed above.
	static const ImmPatch kImmPatches[] =
	{
		// --- actor count (cmp reg, 32) ----------------------------------
		{ "aiCap_ActorFirstTeam_4B5882",  2, 1, VANILLA_MAX_ACTORS,    NEW_MAX_ACTORS },
		{ "aiCap_ActorNextTeam_4B58BF",   2, 1, VANILLA_MAX_ACTORS,    NEW_MAX_ACTORS },
		{ "aiCap_ActorNextTeam_4B58F2",   2, 1, VANILLA_MAX_ACTORS,    NEW_MAX_ACTORS },
		{ "aiCap_ActorScan_4BD511",       2, 1, VANILLA_MAX_ACTORS,    NEW_MAX_ACTORS },
		{ "aiCap_ActorIdx_50E600",        2, 1, VANILLA_MAX_ACTORS,    NEW_MAX_ACTORS },
		{ "aiCap_ActorIdx_50EA56",        2, 1, VANILLA_MAX_ACTORS,    NEW_MAX_ACTORS },
		{ "aiCap_Save_51202D",            2, 1, VANILLA_MAX_ACTORS,    NEW_MAX_ACTORS },
		{ "aiCap_Restore_51368E",         2, 1, VANILLA_MAX_ACTORS,    NEW_MAX_ACTORS },
		// Deliberately absent: aiCap_Save_5122A1 and aiCap_Restore_513870. Both read
		// as actor bounds because they are "cmp reg, 32" inside the savegame, but the
		// loops they close walk g_scr_actorCorpseInfo (dword_190B648, stride 0x20,
		// corpse-indexed and sized by the corpse budget, not by MAX_ACTORS). Growing
		// them archives 64 records out of a 32-entry table. Both bounds stay at 32.

		// --- sentient count (cmp reg, 36) -------------------------------
		{ "aiCap_SpawnActor_4E06C3",      2, 1, VANILLA_MAX_SENTIENTS, NEW_MAX_SENTIENTS },
		{ "aiCap_SentientIdx_50E648",     2, 1, VANILLA_MAX_SENTIENTS, NEW_MAX_SENTIENTS },
		{ "aiCap_SentientIdx_50EA92",     2, 1, VANILLA_MAX_SENTIENTS, NEW_MAX_SENTIENTS },
		{ "aiCap_SentientIdx_50EACF",     2, 1, VANILLA_MAX_SENTIENTS, NEW_MAX_SENTIENTS },
		{ "aiCap_Save_5120BD",            2, 1, VANILLA_MAX_SENTIENTS, NEW_MAX_SENTIENTS },
		{ "aiCap_Restore_5136FF",         2, 1, VANILLA_MAX_SENTIENTS, NEW_MAX_SENTIENTS },
		{ "aiCap_SentientIter_56682C",    2, 1, VANILLA_MAX_SENTIENTS, NEW_MAX_SENTIENTS },
		{ "aiCap_SentientIter_56686F",    2, 1, VANILLA_MAX_SENTIENTS, NEW_MAX_SENTIENTS },
		{ "aiCap_SentientIter_56689C",    2, 1, VANILLA_MAX_SENTIENTS, NEW_MAX_SENTIENTS },

		// --- actor pool byte size (0x63700) -----------------------------
		{ "aiCap_ActorFree_4B4E1F",       2, 4, VANILLA_MAX_ACTORS * ACTOR_SIZE, NEW_MAX_ACTORS * ACTOR_SIZE },
		{ "aiCap_ActorScan_4B92A2",       2, 4, VANILLA_MAX_ACTORS * ACTOR_SIZE, NEW_MAX_ACTORS * ACTOR_SIZE },
		{ "aiCap_ActorScan_54ECEA",       1, 4, VANILLA_MAX_ACTORS * ACTOR_SIZE, NEW_MAX_ACTORS * ACTOR_SIZE },
		{ "aiCap_SentientFree_5660BD",    1, 4, VANILLA_MAX_ACTORS * ACTOR_SIZE, NEW_MAX_ACTORS * ACTOR_SIZE },
		{ "aiCap_SvSnapshot_63916A",      2, 4, VANILLA_MAX_ACTORS * ACTOR_SIZE, NEW_MAX_ACTORS * ACTOR_SIZE },

		// --- sentient pool byte size (0x1320) ---------------------------
		{ "aiCap_SentientClear_566B4F",   1, 4, VANILLA_MAX_SENTIENTS * SENTIENT_SIZE, NEW_MAX_SENTIENTS * SENTIENT_SIZE },
		{ "aiSize_SentientPool_4BB5AA",   2, 4, VANILLA_MAX_SENTIENTS * SENTIENT_SIZE, NEW_MAX_SENTIENTS * SENTIENT_SIZE },
		{ "aiSize_SentientPool_4E2951",   2, 4, VANILLA_MAX_SENTIENTS * SENTIENT_SIZE, NEW_MAX_SENTIENTS * SENTIENT_SIZE },
		{ "aiSize_SentientPool_4E29C6",   2, 4, VANILLA_MAX_SENTIENTS * SENTIENT_SIZE, NEW_MAX_SENTIENTS * SENTIENT_SIZE },

		// CRT static-init loop that constructs actors[i] + 0xD94 (imm32 count, no imm8
		// cliff here). Whether it still runs after this patch is reported by AiDbg below.
		{ "aiCap_ActorCtorLoop_7E8A52",   1, 4, VANILLA_MAX_ACTORS - 1, NEW_MAX_ACTORS - 1 },

		// g_scr_data.actorXAnimTrees[] alloc/free loops: byte-size bounds (imm32).
		{ "aiSize_XAnimTrees_5019C2",     2, 4, VANILLA_MAX_ACTORS * 4, NEW_MAX_ACTORS * 4 },
		{ "aiSize_XAnimTrees_501AC9",     2, 4, VANILLA_MAX_ACTORS * 4, NEW_MAX_ACTORS * 4 },

		// --- phase 4: client snapshot ------------------------------------
		// CL_GetSnapshot clamps numActors to 32 and, unlike the entity path, says
		// nothing when it truncates. Without this the wire could carry 64 actors and
		// the cgame would still see 32.
		{ "aiCap_ClSnapClampCmp_63A6AA",  2, 1, VANILLA_MAX_ACTORS, NEW_MAX_ACTORS },
		{ "aiCap_ClSnapClampMov_63A6AF",  1, 4, VANILLA_MAX_ACTORS, NEW_MAX_ACTORS },

		// --- phase 4: snapshot actor index 5 -> 6 bits --------------------
		// Both sides must move together or the whole bitstream desynchronises; they
		// are in the same all-or-nothing batch as everything else here. Not to be
		// confused with 0x67ADF0, also "push 5", which is MSG_WriteDeltaStruct's
		// field-index width (ceil(log2(29 fields))).
		{ "aiNet_WriteIndexBits_67ADC1",  1, 1, 5, 6 },
		{ "aiNet_ReadIndexBits_64BB77",   1, 1, 5, 6 },

		// --- AI event listener count ------------------------------------
		// Actor_EventListener_Add refuses a new listener past this and raises a script
		// error. Nothing else bounds the array: it is a compacted list walked against
		// g_listenerCount, with no pointer end marker to relocate. CoD4's release build
		// agrees - its only other bound, in RemoveSwapWithLast, is an assert.
		{ "aiCap_ListenerAdd_4C6CC0",     2, 1, VANILLA_MAX_LISTENERS, NEW_MAX_LISTENERS },

		// Phase 7's setailimit bound is not here: the whole builtin is reconstructed
		// and detoured (T4_Reconstructed::GScr_SetAILimit), which also lets its range
		// message name the real cap instead of the .rdata literal that says 32.
	};

	// Same mechanism, but the value is only known once the replacement buffers exist.
	// Kept in a second table so the pre-flight check can still verify every `expect`
	// before a single allocation happens. Order matters: see FillDynPatchValues.
	enum DynPatch
	{
		DYN_SNAP_CMP_BASE = 0,   // cmp cg.snap, offset snapshots[0]
		DYN_SNAP_ADD_BASE,       // add ecx, offset snapshots[0]
		DYN_SNAP_STRIDE,         // imul ecx, sizeof(snapshot_t)
		DYN_SNAP_TAIL_A,         // the one field that lives past actors[]
		DYN_SNAP_TAIL_B,
		DYN_SNAP_TAIL_C,
		DYN_TREE_BLOCK_OFS,      // sub_42A2D0's world-relative offset of the tree array
		DYN_TREE_SV_SAVE_START,  // savegame archive loop over the server tree array
		DYN_TREE_SV_SAVE_END,
		DYN_TREE_SV_LOAD_START,
		DYN_TREE_SV_LOAD_END,
		DYN_TREE_CG_DISP_BASE,   // cg struct + 0x146210  -> element field +0x000
		DYN_TREE_CG_DISP_3AC,    // cg struct + 0x1465BC  -> element field +0x3AC
		DYN_COUNT
	};

	static ImmPatch g_dynPatches[DYN_COUNT] =
	{
		{ "aiSnap_CmpBase_674412",     6, 4, 0x034732E4, 0 },
		{ "aiSnap_AddBase_67442E",     2, 4, 0x034732E4, 0 },
		{ "aiSnap_Stride_674428",      2, 4, VANILLA_SNAP_SIZE, 0 },
		{ "aiSnap_TailOfs_44A637",     2, 4, VANILLA_SNAP_TAIL, 0 },
		{ "aiSnap_TailOfs_63A52D",     2, 4, VANILLA_SNAP_TAIL, 0 },
		{ "aiSnap_TailOfs_6735B8",     2, 4, VANILLA_SNAP_TAIL, 0 },
		{ "aiTree_BlockOfs_42A318",    3, 4, ACTORTREE_BLOCK_OFS, 0 },
		// The four savegame sites below are addressed by VA rather than scanned: the
		// constants they hold are each shared with a site that must NOT move - the array
		// start (0x018DE8B0) doubles as the previous array's end marker, and the end
		// marker (0x018E73B0) doubles as a plain global written by sub_500C30.
		{ "aiTree_SvSaveStart_511669", 1, 4, 0x018DE8B0, 0 },
		{ "aiTree_SvSaveEnd_511A0C",   2, 4, 0x018E73B0, 0 },
		{ "aiTree_SvLoadStart_51300E", 1, 4, 0x018DE8B0, 0 },
		{ "aiTree_SvLoadEnd_5130F8",   2, 4, 0x018E73B0, 0 },
		// The array is a member of the cg struct as well as of the DObj world, so it
		// is also reached as "cgStructBase + 0x146210". Two sites, and the second is
		// the one that crashed the first 64-actor build.
		{ "aiTree_CgDisp_403B77",      3, 4, 0x00146210, 0 },
		{ "aiTree_CgDisp_42A492",      3, 4, 0x001465BC, 0 },
	};

	static bool ImmSiteMatches(const ImmPatch& p, DWORD* outVa)
	{
		const uintptr_t va = T4M::GetAddress(p.key);
		*outVa = static_cast<DWORD>(va);
		if (!va)
			return false;

		const BYTE* at = reinterpret_cast<const BYTE*>(va + p.immOfs);
		return (p.immSize == 1) ? (*at == static_cast<BYTE>(p.expect))
		                        : (*reinterpret_cast<const DWORD*>(at) == p.expect);
	}

	// Allocate both pools, then rewrite every base address and every count bound.
	// All-or-nothing: everything is verified first, and a single mismatch aborts before
	// the first byte is written. A half-patched pool is far worse than none.
	static bool ApplyActorLimit()
	{
		const size_t actorBytes    = static_cast<size_t>(NEW_MAX_ACTORS)    * ACTOR_SIZE;
		const size_t sentientBytes = static_cast<size_t>(NEW_MAX_SENTIENTS) * SENTIENT_SIZE;

		BYTE*  textBase = nullptr;
		size_t textSize = 0;
		if (!GetTextSection(&textBase, &textSize))
		{
			AiDbg("[T4M][ai] .text section not found, aborting\n");
			return false;
		}

		const DWORD vanillaActorBase    = static_cast<DWORD>(T4M::GetAddress("g_actorPoolBss"));
		const DWORD vanillaSentientBase = static_cast<DWORD>(T4M::GetAddress("g_sentientPoolBss"));
		if (!vanillaActorBase || !vanillaSentientBase)
		{
			AiDbg("[T4M][ai] pool base keys missing from the address map, aborting\n");
			return false;
		}

		// Verify every immediate site before touching anything.
		for (const ImmPatch& p : kImmPatches)
		{
			DWORD va = 0;
			if (!ImmSiteMatches(p, &va))
			{
				char buf[192];
				wsprintfA(buf, "[T4M][ai] site %s (0x%08X) does not hold the vanilla value, aborting\n", p.key, va);
				AiDbg(buf);
				return false;
			}
		}

		for (const ImmPatch& p : g_dynPatches)
		{
			DWORD va = 0;
			if (!ImmSiteMatches(p, &va))
			{
				char buf[192];
				wsprintfA(buf, "[T4M][ai] site %s (0x%08X) does not hold the vanilla value, aborting\n", p.key, va);
				AiDbg(buf);
				return false;
			}
		}

		// Index-addressed side tables grow with the pools; a 33rd actor would otherwise
		// write past them. Count-bounded lists (g_AIEVlisteners) and budget-sized ones
		// (g_scr_actorCorpseInfo) deliberately stay put — see cod/actor.hpp.
		const DWORD vanillaXAnimTrees = static_cast<DWORD>(T4M::GetAddress("g_scr_actorXAnimTrees"));
		const DWORD vanillaHandleList = static_cast<DWORD>(T4M::GetAddress("g_sentientsHandleList"));
		// g_AIEVlisteners is count-bounded rather than actor-indexed, so it cannot corrupt
		// anything by overflowing - it just starts refusing listeners with a script error.
		// It still has to grow: 32 listening entities is the same ceiling whether there are
		// 32 actors or 64. flt_1732200 sits immediately after it, so growing means moving.
		const DWORD vanillaListeners  = static_cast<DWORD>(T4M::GetAddress("g_AIEVlisteners"));

		g_newActorPool    = static_cast<BYTE*>(VirtualAlloc(NULL, actorBytes,    MEM_COMMIT | MEM_RESERVE, PAGE_READWRITE));
		g_newSentientPool = static_cast<BYTE*>(VirtualAlloc(NULL, sentientBytes, MEM_COMMIT | MEM_RESERVE, PAGE_READWRITE));
		g_newXAnimTrees   = static_cast<BYTE*>(VirtualAlloc(NULL, NEW_MAX_ACTORS * 4,    MEM_COMMIT | MEM_RESERVE, PAGE_READWRITE));
		g_newHandleList   = static_cast<BYTE*>(VirtualAlloc(NULL, NEW_MAX_SENTIENTS * 2, MEM_COMMIT | MEM_RESERVE, PAGE_READWRITE));
		g_newListeners    = static_cast<BYTE*>(VirtualAlloc(NULL, NEW_MAX_LISTENERS * LISTENER_SIZE, MEM_COMMIT | MEM_RESERVE, PAGE_READWRITE));
		if (!g_newActorPool || !g_newSentientPool || !g_newXAnimTrees || !g_newHandleList || !g_newListeners ||
		    !vanillaXAnimTrees || !vanillaHandleList || !vanillaListeners)
		{
			AiDbg("[T4M][ai] pool VirtualAlloc failed, aborting\n");
			ReleasePools();
			return false;
		}

		memcpy(g_newXAnimTrees, reinterpret_cast<void*>(vanillaXAnimTrees), VANILLA_MAX_ACTORS    * 4);
		memcpy(g_newHandleList, reinterpret_cast<void*>(vanillaHandleList), VANILLA_MAX_SENTIENTS * 2);
		memcpy(g_newListeners,  reinterpret_cast<void*>(vanillaListeners),  VANILLA_MAX_LISTENERS * LISTENER_SIZE);

		// A CRT static initializer constructs a sub-object at actors[i]+0xD94 (it writes a
		// dispatch-table pointer at +0x10C, not just zeros), and Actor_ClearAll only resets
		// the inuse flag. A blank pool is therefore NOT equivalent to the vanilla one:
		// copy the initialized BSS state over, exactly like the entity pool relocation does.
		memcpy(g_newActorPool,    reinterpret_cast<void*>(vanillaActorBase),    VANILLA_MAX_ACTORS    * ACTOR_SIZE);
		memcpy(g_newSentientPool, reinterpret_cast<void*>(vanillaSentientBase), VANILLA_MAX_SENTIENTS * SENTIENT_SIZE);

		// --- phase 4 -----------------------------------------------------
		// The client snapshot pair and the two per-actor animation-tree arrays. Every
		// address involved is read from the map, then checked against the layout this
		// file assumes, so a wrong CSV entry aborts instead of writing somewhere random.
		const DWORD vanillaSnapPair = static_cast<DWORD>(T4M::GetAddress("cgSnapshotPair"));
		const DWORD cgBlock         = static_cast<DWORD>(T4M::GetAddress("cgDObjBlockBase"));
		const DWORD svBlock         = static_cast<DWORD>(T4M::GetAddress("svDObjBlockBase"));
		const DWORD vanillaCgTrees  = static_cast<DWORD>(T4M::GetAddress("cgActorAnimTrees"));
		const DWORD vanillaSvTrees  = static_cast<DWORD>(T4M::GetAddress("svActorAnimTrees"));
		if (!vanillaSnapPair || !cgBlock || !svBlock || !vanillaCgTrees || !vanillaSvTrees)
		{
			AiDbg("[T4M][ai] phase 4 keys missing from the address map, aborting\n");
			ReleasePools();
			return false;
		}

		// sub_42A2D0 computes "currentWorld + 0x89078 + idx * 0x458" and falls back to
		// the absolute client array when no world is selected. Both paths must keep
		// agreeing, so the two replacements are placed at the exact distance the two
		// world blocks have: one reservation, one commit at each end. The middle is
		// reserved and never committed, so it costs address space and nothing else.
		if (vanillaCgTrees != cgBlock + ACTORTREE_BLOCK_OFS || vanillaSvTrees != svBlock + ACTORTREE_BLOCK_OFS)
		{
			AiDbg("[T4M][ai] actor tree arrays are not at world + 0x89078, aborting\n");
			ReleasePools();
			return false;
		}

		const DWORD  worldGap  = cgBlock - svBlock;
		const size_t treeBytes = static_cast<size_t>(NEW_MAX_ACTORS) * ACTORTREE_SIZE;

		g_newSnapPair = static_cast<BYTE*>(VirtualAlloc(NULL, 2u * NEW_SNAP_SIZE, MEM_COMMIT | MEM_RESERVE, PAGE_READWRITE));
		g_treeReserve = static_cast<BYTE*>(VirtualAlloc(NULL, worldGap + treeBytes, MEM_RESERVE, PAGE_NOACCESS));
		if (g_treeReserve)
		{
			// The distance is the invariant here, and it is not page-aligned, so the
			// addresses are the ones we asked for, not the page-rounded ones VirtualAlloc
			// hands back. Committing rounds outwards, which still covers both ranges.
			BYTE* const svWant = g_treeReserve;
			BYTE* const cgWant = g_treeReserve + worldGap;
			if (VirtualAlloc(svWant, treeBytes, MEM_COMMIT, PAGE_READWRITE) &&
			    VirtualAlloc(cgWant, treeBytes, MEM_COMMIT, PAGE_READWRITE))
			{
				g_newSvActorTrees = svWant;
				g_newCgActorTrees = cgWant;
			}
		}
		if (!g_newSnapPair || !g_newSvActorTrees || !g_newCgActorTrees)
		{
			AiDbg("[T4M][ai] phase 4 VirtualAlloc failed, aborting\n");
			ReleasePools();
			return false;
		}

		// Carry the vanilla content over. Everything up to the end of actors[32] keeps
		// its offset; only the single field that used to follow it moves.
		for (int i = 0; i < 2; ++i)
		{
			const BYTE* src = reinterpret_cast<const BYTE*>(vanillaSnapPair) + i * VANILLA_SNAP_SIZE;
			BYTE*       dst = g_newSnapPair + i * NEW_SNAP_SIZE;
			memcpy(dst, src, VANILLA_SNAP_TAIL);
			*reinterpret_cast<DWORD*>(dst + NEW_SNAP_TAIL) = *reinterpret_cast<const DWORD*>(src + VANILLA_SNAP_TAIL);
		}
		memcpy(g_newCgActorTrees, reinterpret_cast<void*>(vanillaCgTrees), VANILLA_MAX_ACTORS * ACTORTREE_SIZE);
		memcpy(g_newSvActorTrees, reinterpret_cast<void*>(vanillaSvTrees), VANILLA_MAX_ACTORS * ACTORTREE_SIZE);

		const DWORD newCgTrees = reinterpret_cast<DWORD>(g_newCgActorTrees);
		const DWORD newSvTrees = reinterpret_cast<DWORD>(g_newSvActorTrees);

		g_dynPatches[DYN_SNAP_CMP_BASE].value      = reinterpret_cast<DWORD>(g_newSnapPair);
		g_dynPatches[DYN_SNAP_ADD_BASE].value      = reinterpret_cast<DWORD>(g_newSnapPair);
		g_dynPatches[DYN_SNAP_STRIDE].value        = NEW_SNAP_SIZE;
		g_dynPatches[DYN_SNAP_TAIL_A].value        = NEW_SNAP_TAIL;
		g_dynPatches[DYN_SNAP_TAIL_B].value        = NEW_SNAP_TAIL;
		g_dynPatches[DYN_SNAP_TAIL_C].value        = NEW_SNAP_TAIL;
		g_dynPatches[DYN_TREE_BLOCK_OFS].value     = newCgTrees - cgBlock;
		g_dynPatches[DYN_TREE_SV_SAVE_START].value = newSvTrees + ACTORTREE_SV_BIAS;
		g_dynPatches[DYN_TREE_SV_SAVE_END].value   = newSvTrees + ACTORTREE_SV_BIAS + NEW_MAX_ACTORS * ACTORTREE_SIZE;
		g_dynPatches[DYN_TREE_SV_LOAD_START].value = g_dynPatches[DYN_TREE_SV_SAVE_START].value;
		g_dynPatches[DYN_TREE_SV_LOAD_END].value   = g_dynPatches[DYN_TREE_SV_SAVE_END].value;

		const DWORD cgStruct = static_cast<DWORD>(T4M::GetAddress("cgStructBase"));
		if (!cgStruct || vanillaCgTrees != cgStruct + 0x146210)
		{
			AiDbg("[T4M][ai] cg struct base does not sit 0x146210 below the tree array, aborting\n");
			ReleasePools();
			return false;
		}
		g_dynPatches[DYN_TREE_CG_DISP_BASE].value = newCgTrees - cgStruct;
		g_dynPatches[DYN_TREE_CG_DISP_3AC].value  = newCgTrees + ACTORTREE_F3AC - cgStruct;

		// The single displacement in that lea has to describe both worlds at once.
		if (newSvTrees - svBlock != g_dynPatches[DYN_TREE_BLOCK_OFS].value)
		{
			AiDbg("[T4M][ai] tree arrays did not land at the world distance, aborting\n");
			ReleasePools();
			return false;
		}

		const DWORD newActorBase    = reinterpret_cast<DWORD>(g_newActorPool);
		const DWORD newSentientBase = reinterpret_cast<DWORD>(g_newSentientPool);

		// Only values proven to be array bases are relocated. Deliberately excluded:
		//   0x046E7F44 — end marker of an unrelated 0x808-stride array that happens to
		//                land inside the actor pool range;
		//   0x018E73A8..0x018E73BC — the 33rd, standalone element of the server actor
		//                tree array, addressed absolutely and never as trees[32]. One of
		//                them, 0x018E73B0, is also that array's loop end marker: that
		//                use goes through the VA table, this one stays put;
		//   0x018E86E0.. — standalone globals right after the sentient array.
		// Counts were verified identical in the SP and GER images.
		ScanReloc relocs[] =
		{
			{ vanillaActorBase,                       newActorBase,                        1,  0, "actor pool base" },
			// CRT ctor loop start: &actors[0] + 0xD94.
			{ vanillaActorBase + 0xD94,               newActorBase + 0xD94,                1,  0, "actor ctor pointer" },
			{ vanillaSentientBase,                    newSentientBase,                     18, 0, "sentient pool base" },
			// SentientHandle number is index+1, so the engine also carries base - 0x88,
			// including two field-biased forms of it.
			{ vanillaSentientBase - SENTIENT_SIZE,        newSentientBase - SENTIENT_SIZE,        26, 0, "sentient handle base" },
			{ vanillaSentientBase - SENTIENT_SIZE + 0x04, newSentientBase - SENTIENT_SIZE + 0x04, 2,  0, "sentient handle base +0x04" },
			{ vanillaSentientBase - SENTIENT_SIZE + 0x58, newSentientBase - SENTIENT_SIZE + 0x58, 1,  0, "sentient handle base +0x58" },
			{ vanillaXAnimTrees, reinterpret_cast<DWORD>(g_newXAnimTrees), 18, 0, "actorXAnimTrees" },
			{ vanillaHandleList, reinterpret_cast<DWORD>(g_newHandleList), 9,  0, "sentientsHandleList" },

			// g_AIEVlisteners. Both fields of the entry are addressed independently, and the
			// base has one reference that is NOT ours: sub_4C1520 uses it as the end marker
			// of the 0x2C-stride array at 0x172F500, which ends exactly here. That one is a
			// bare "cmp eax, imm32" (opcode 3D, no modrm), hence the one-byte rule.
			{ vanillaListeners,     reinterpret_cast<DWORD>(g_newListeners),     18, 0, "AIEVlisteners", 0x3D, 1, 0 },
			{ vanillaListeners + 4, reinterpret_cast<DWORD>(g_newListeners) + 4, 10, 0, "AIEVlisteners +4" },

			// Phase 4 — the per-actor animation trees. The client base has a sixth
			// reference that is NOT one of ours: sub_655830 uses it as the end marker of
			// the cgame client array (stride 0x594) that ends exactly where this one
			// starts. It is recognised by its "cmp esi, imm32" opcode and left in place;
			// relocating it would send that loop walking into our buffer.
			{ vanillaCgTrees, newCgTrees, 5, 0, "cgame actor trees", 0xFE81, 1, 0 },
			{ vanillaCgTrees + ACTORTREE_CG_BIAS, newCgTrees + ACTORTREE_CG_BIAS, 11, 0, "cgame actor trees +0x3DC" },
			{ vanillaCgTrees + ACTORTREE_F30,  newCgTrees + ACTORTREE_F30,  5, 0, "cgame actor trees +0x30" },
			{ vanillaCgTrees + ACTORTREE_F440, newCgTrees + ACTORTREE_F440, 1, 0, "cgame actor trees +0x440" },
			{ vanillaCgTrees + ACTORTREE_CG_BIAS + VANILLA_MAX_ACTORS * ACTORTREE_SIZE,
			  newCgTrees     + ACTORTREE_CG_BIAS + NEW_MAX_ACTORS     * ACTORTREE_SIZE, 3, 0, "cgame actor trees end" },
			// The server base is clean (two indexed forms). Its +8 and end-marker forms
			// are not: both collide with an unrelated site, so they go through the VA
			// table instead — see g_dynPatches.
			{ vanillaSvTrees, newSvTrees, 2, 0, "server actor trees" },
		};

		DWORD oldProtect = 0;
		if (!VirtualProtect(textBase, textSize, PAGE_EXECUTE_READWRITE, &oldProtect))
		{
			AiDbg("[T4M][ai] VirtualProtect failed, aborting\n");
			ReleasePools();
			return false;
		}

		// Pass 1 — count.
		for (BYTE* p = textBase; p <= textBase + textSize - sizeof(DWORD); ++p)
		{
			const DWORD v = *reinterpret_cast<DWORD*>(p);
			for (ScanReloc& r : relocs)
				if (v == r.from)
				{
					if (SkipThisSite(r, textBase, p))
						++r.skipped;
					else
						++r.found;
				}
		}

		bool countsOk = true;
		for (const ScanReloc& r : relocs)
		{
			if (r.found != r.expected || r.skipped != r.skipExpected)
			{
				char buf[224];
				wsprintfA(buf, "[T4M][ai] %s: %d refs found (%d expected), %d excluded (%d expected), aborting\n",
				          r.what, r.found, r.expected, r.skipped, r.skipExpected);
				AiDbg(buf);
				countsOk = false;
			}
		}

		if (countsOk)
		{
			// Pass 2 — write.
			for (BYTE* p = textBase; p <= textBase + textSize - sizeof(DWORD); ++p)
			{
				DWORD& v = *reinterpret_cast<DWORD*>(p);
				for (const ScanReloc& r : relocs)
				{
					if (v == r.from)
					{
						if (SkipThisSite(r, textBase, p))
							break;
						v = r.to;
						break;
					}
				}
			}

			for (const ImmPatch& p : kImmPatches)
			{
				BYTE* at = reinterpret_cast<BYTE*>(T4M::GetAddress(p.key) + p.immOfs);
				if (p.immSize == 1)
					*at = static_cast<BYTE>(p.value);
				else
					*reinterpret_cast<DWORD*>(at) = p.value;
			}

			for (const ImmPatch& p : g_dynPatches)
			{
				BYTE* at = reinterpret_cast<BYTE*>(T4M::GetAddress(p.key) + p.immOfs);
				if (p.immSize == 1)
					*at = static_cast<BYTE>(p.value);
				else
					*reinterpret_cast<DWORD*>(at) = p.value;
			}
		}

		VirtualProtect(textBase, textSize, oldProtect, &oldProtect);
		FlushInstructionCache(GetCurrentProcess(), textBase, textSize);

		if (!countsOk)
		{
			ReleasePools();
			return false;
		}

		// Slots 32..N-1 came out of VirtualAlloc as raw zeroes, while the memcpy above
		// carried slots 0..31 over in the state the exe's CRT static initializer (0x7E8A52)
		// left them: a dispatch pointer at +0xD94+0x10C plus float constants. That state is
		// not load-bearing — Actor_Alloc memsets the whole actor_s and Actor_SetDefaults
		// never rewrites it, so every actually allocated actor runs with it zeroed — but it
		// is what vanilla's never-allocated slots hold, and the pool-wide scans do walk
		// those. Build the new slots with the very constructor that loop called, so the
		// whole pool is uniform. Unconditional, not gated on the "has it run yet" test
		// below: the constructor writes constants only, so building twice is a no-op.
		for (int i = VANILLA_MAX_ACTORS; i < NEW_MAX_ACTORS; ++i)
			T4::game::Actor_ConstructSubObj_D94(g_newActorPool + i * ACTOR_SIZE + ACTOR_SUBOBJ_OFFSET);

		// The .text immediates now carry the new bases, but symbol<> still caches the
		// vanilla ones. Re-point them so our own reconstructions read the grown tables.
		T4::engine::g_scr_actorXAnimTrees.set(reinterpret_cast<size_t>(g_newXAnimTrees));
		T4::engine::g_sentientsHandleList.set(reinterpret_cast<size_t>(g_newHandleList));
		T4::engine::g_AIEVlisteners.set(reinterpret_cast<size_t>(g_newListeners));

		// Reported for the record: the CRT loop is expected to have already run, which is
		// why the slots above are built by hand. If this ever says "not run yet", the
		// patched loop rebuilds every slot on our base afterwards and the two agree.
		const DWORD ctorMark = *reinterpret_cast<const DWORD*>(newActorBase + ACTOR_SUBOBJ_OFFSET + 0x10C);

		char buf[320];
		wsprintfA(buf, "[T4M][ai] actors %d @0x%08X, sentients %d @0x%08X, %d immediates, CRT ctor %s\n",
		          NEW_MAX_ACTORS, newActorBase, NEW_MAX_SENTIENTS, newSentientBase,
		          static_cast<int>(_countof(kImmPatches) + _countof(g_dynPatches)),
		          ctorMark ? "already ran" : "not run yet");
		AiDbg(buf);
		wsprintfA(buf, "[T4M][ai] snapshots @0x%08X stride 0x%X, actor trees cg @0x%08X sv @0x%08X, wire index 6 bits\n",
		          reinterpret_cast<DWORD>(g_newSnapPair), NEW_SNAP_SIZE, newCgTrees, newSvTrees);
		AiDbg(buf);
		return true;
	}

	// -----------------------------------------------------------------
	// Phase 5 — client actor ring watchdog (observation only).
	//
	// cl.parseActors holds 512 actorState_s. CL_GetSnapshot drops a snapshot whose
	// firstActor has fallen more than a full ring behind, which at 64 actors leaves
	// 8 snapshots of history instead of vanilla's 16. That is still more headroom
	// than the entity ring gives in a dense map, so the ring is deliberately left
	// alone; this only says so out loud if it ever does bite. Growing it is a
	// 23-immediate job written up in plans/plan_increase_ai_limit.md, phase 5-B.
	// -----------------------------------------------------------------
	#define CL_ACTOR_RING_ENTRIES 512

	static int g_ringDropCount = 0;

	static void RingWatchTick(SafetyHookContext& ctx)
	{
		const int* parseActorsNum = reinterpret_cast<const int*>(T4M::GetAddress("cl_parseActorsNum"));
		if (!parseActorsNum || !ctx.ebx)
			return;

		// ebx is the cl.snapshots[] entry CL_GetSnapshot is about to hand out; +0x20DC
		// is its firstActor, the ring cursor the actors were parsed at.
		const int firstActor = *reinterpret_cast<const int*>(ctx.ebx + 0x20DC);
		if (*parseActorsNum - firstActor < CL_ACTOR_RING_ENTRIES)
			return;

		if (++g_ringDropCount % 60 == 1)
			T4::engine::Com_Printf(14, "[T4M][ai] actor ring exhausted, snapshot dropped (%d so far)\n", g_ringDropCount);
	}

	// Reported in the console version line so the loaded DLL identifies itself, and says
	// what the AI patch actually did rather than merely that it was compiled in.
	static char g_aiTag[16] = "";
}

void PatchT4MAM_ActorLimit()
{
	// Unconditional: there is no toggle. Logged on entry all the same, so "the patch
	// never ran" and "the patch aborted" can never look like the same silence again.
	{
		char buf[128];
		wsprintfA(buf, "[T4M][ai] entry: target %d actors, %d sentients\n",
		          NEW_MAX_ACTORS, NEW_MAX_SENTIENTS);
		T4M::AiDbg(buf);
	}

	// The reconstructions below walk the pool up to NEW_MAX_ACTORS. Installing them
	// over a pool that failed to grow would be worse than doing nothing at all.
	if (!T4M::ApplyActorLimit())
	{
		T4M::g_aiStatus = T4M::AI_TAG_FAILED;
		lstrcpyA(T4M::g_aiTag, " -ai:FAILED");
		return;
	}
	T4M::g_aiStatus = T4M::AI_TAG_ACTIVE;
	wsprintfA(T4M::g_aiTag, " -ai:%d", NEW_MAX_ACTORS);

	Detours::X86::DetourFunction(T4M::GetAddress("Actor_Alloc"),
	                             reinterpret_cast<uintptr_t>(&T4_Reconstructed::Actor_Alloc),
	                             Detours::X86Option::USE_JUMP);
	Detours::X86::DetourFunction(T4M::GetAddress("Sentient_Alloc"),
	                             reinterpret_cast<uintptr_t>(&T4_Reconstructed::Sentient_Alloc),
	                             Detours::X86Option::USE_JUMP);
	Detours::X86::DetourFunction(T4M::GetAddress("G_InitActors"),
	                             reinterpret_cast<uintptr_t>(&T4_Reconstructed::G_InitActors),
	                             Detours::X86Option::USE_JUMP);

	// Phase 7: the setailimit builtin, message included.
	wsprintfA(g_setAiLimitError, "SetAILimit must take a value between 0 and %d inclusive.", NEW_MAX_ACTORS);
	Detours::X86::DetourFunction(T4M::GetAddress("GScr_SetAILimit"),
	                             reinterpret_cast<uintptr_t>(&T4_Reconstructed::GScr_SetAILimit),
	                             Detours::X86Option::USE_JUMP);

	// Phase 7: let GSC read the real cap instead of hardcoding 32. Registered ROM in
	// PatchT4_Console with the vanilla value; only a successful patch raises it.
	if (ai_max_actors)
		ai_max_actors->current.integer = NEW_MAX_ACTORS;

	// Phase 5: opt-in, and observation only — it never alters the flow.
	if (ai_ring_watch && ai_ring_watch->current.boolean)
	{
		static auto ring_watch_hook = safetyhook::create_mid(T4M::GetAddress("aiRing_ClActorHistory_63A4FD"),
		                                                    &T4M::RingWatchTick);
	}
}

// @new - lets the console version line prove which DLL is loaded and what the AI patch
// did with it: " -ai:64" when the pool really grew, " -ai:FAILED" when it aborted (the
// reason is in t4m_ai.log). Empty only if the patch never ran at all. Never null.
const char* T4M::AiLimitStatusTag()
{
	return T4M::g_aiTag;
}
