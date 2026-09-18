// ==========================================================
// T4M project
//
// Component: clientdll
// Purpose: C++ migration of the g_assetEntryPool DB layer — replaces the
//          PatchT4MemoryLimits byte-patches of the 25 hardcoded 0xA51C50 pool
//          references with faithful reconstructions that read the C++ pointer
//          T4::engine::g_assetEntryPool. Once all 13 functions are detoured, the
//          pool base/size are 100% C++ and the byte-patches are removed.
//
// Plan  : plans/plan_memory_full_detour.md  (§0 vision élargie)
// Invent: analysis/asset_pool_cpp_migration_inventory.md
//
// Each function is a byte-faithful port of its vanilla body; the ONLY change is
// that "entry = 0xA51C50 + index*0x10" becomes "&g_assetEntryPool[index].entry".
// The db hash table (db_hashTable, 0x8000 uint16 buckets), the free list
// (g_freeAssetEntries) and the reader lock (DB_ReaderAcquire/Release) are reused
// from cod/ — never re-inlined.
//
// Detours are wired in one shot at the end (PatchT4_AssetPool), matching the
// "all 13 at once" decision; each is on its own line so a crash can be bisected.
// ==========================================================

#include "StdInc.h"
#include "T4.h"
#include "Hooking.h"

using namespace T4::engine;

namespace
{
	const int kHashBuckets = 0x8000; // db_hashTable size (0x10000 bytes / 2)
	const int kAssetTypeCount = 0x8C / 4; // 35 asset types (vanilla loop 0..0x8C step 4)
	// MUST equal NEW_ASSET_ENTRY_POOL_SIZE in PatchT4MemoryLimits.cpp (the pool is
	// VirtualAlloc'd there with size+1 entries; here we build its free list).
	const int kAssetEntryPoolSize = 65535;

	// entry = &g_assetEntryPool[index]  (the vanilla "shl 4; add 0xA51C50").
	inline XAssetEntry* Entry(unsigned int index)
	{
		return &g_assetEntryPool[index].entry;
	}
}

namespace T4_Reconstructed
{
	// WaW sub_48F6E0 — cdecl(). Clears the inuse flag on every loaded asset entry
	// (called under the writer lock during a reload). Reference: 0x48F704.
	extern "C" void __cdecl DB_ClearInuseFlags()
	{
		DB_PostLoadXZone(); // sub_5A3320
		for (int h = 0; h < kHashBuckets; ++h)
		{
			unsigned short idx = db_hashTable[h];
			while (idx)
			{
				XAssetEntry* e = Entry(idx);
				e->inuse = false;
				idx = e->nextHash;
			}
		}
	}

	// WaW sub_48DF60 — cdecl(type, out). Fills out[] with the header of every entry
	// of the given type (out may be NULL to only count) and returns the count.
	// Reference: 0x48DFB4.
	extern "C" int __cdecl DB_GetAllXAssetOfType(int type, void** out)
	{
		T4M::DB_ReaderAcquire();
		int count = 0;
		for (int h = 0; h < kHashBuckets; ++h)
		{
			unsigned short idx = db_hashTable[h];
			while (idx)
			{
				XAssetEntry* e = Entry(idx);
				if ((int)e->asset.type == type)
				{
					if (out)
						out[count] = e->asset.header.data;
					++count;
				}
				idx = e->nextHash;
			}
		}
		T4M::DB_ReaderRelease();
		return count;
	}

	// WaW sub_48D560 — cdecl(type, callback, data, includeOverride). Calls
	// callback(header, data) for every entry of the given type, and (if
	// includeOverride) for each entry down its nextOverride chain.
	// References: 0x48D5B8, 0x48D5E5.
	extern "C" void __cdecl DB_EnumXAssets(
		int type, void(__cdecl* callback)(void* header, void* data), void* data, char includeOverride)
	{
		T4M::DB_ReaderAcquire();
		for (int h = 0; h < kHashBuckets; ++h)
		{
			unsigned short idx = db_hashTable[h];
			while (idx)
			{
				XAssetEntry* e = Entry(idx);
				if ((int)e->asset.type == type)
				{
					callback(e->asset.header.data, data);
					if (includeOverride)
					{
						unsigned short ov = e->nextOverride;
						while (ov)
						{
							XAssetEntry* oe = Entry(ov);
							callback(oe->asset.header.data, data);
							ov = oe->nextOverride;
						}
					}
				}
				idx = e->nextHash;
			}
		}
		T4M::DB_ReaderRelease();
	}

	// WaW sub_48DEA0 — cdecl(type, name). Returns 1 if no zone-owned entry for
	// (type, name) exists (not found, or found with zoneIndex 0). Reference: 0x48DEF4.
	extern "C" char __cdecl DB_AssetIsUnowned(int type, const char* name)
	{
		unsigned int bucket = T4_Reconstructed::DB_HashAssetName(type, name);
		T4M::DB_ReaderAcquire();
		unsigned short idx = db_hashTable[bucket];
		while (idx)
		{
			XAssetEntry* e = Entry(idx);
			if ((int)e->asset.type == type && T4M::Q_stricmpn(T4M::GetName(e), name, 0x7FFFFFFF) == 0)
			{
				char unowned = (char)(e->zoneIndex == 0);
				T4M::DB_ReaderRelease();
				return unowned;
			}
			idx = e->nextHash;
		}
		T4M::DB_ReaderRelease();
		return 1;
	}

	// WaW sub_48E370 — usercall(type@esi, header@stack). Marks the entry matching
	// (type, header) as inuse. Reference: 0x48E3A6.
	extern "C" void __cdecl DB_SetInuseByHeader(int type, void* header)
	{
		XAssetHeader h;
		h.data = header;
		const char* name = DB_XAssetGetNameHandlers[type](&h);
		unsigned int bucket = T4_Reconstructed::DB_HashAssetName(type, name);
		unsigned short idx = db_hashTable[bucket];
		while (idx)
		{
			XAssetEntry* e = Entry(idx);
			if ((int)e->asset.type == type && e->asset.header.data == header)
			{
				e->inuse = true;
				return;
			}
			idx = e->nextHash;
		}
	}

	// WaW sub_48F670 — cdecl(). Frees every loaded asset (per-type free handler),
	// returns every entry to the free list and clears the hash table. Called under
	// the writer lock during a full unload. Reference: 0x48F68C.
	extern "C" void __cdecl DB_CleanupAssetRefs()
	{
		for (int b = 0; b < kHashBuckets; ++b)
		{
			unsigned short idx = db_hashTable[b];
			while (idx)
			{
				*g_assetRefCount -= 1;
				XAssetEntryPoolEntry* pe = &g_assetEntryPool[idx];
				XAssetEntry*          e  = &pe->entry;
				int                   type   = (int)e->asset.type;
				void*                 header = e->asset.header.data;
				unsigned short        next   = e->nextHash;

				DB_XAssetFreeHandlers[type](DB_XAssetPool[type], header);

				XAssetEntryPoolEntry* oldHead = *g_freeAssetEntries;
				*g_freeAssetEntries = pe;
				pe->next = oldHead;
				idx = next;
			}
			db_hashTable[b] = 0;
		}
	}

	// WaW sub_48D340 — cdecl(). Re-inits the per-type asset pools and (re)builds the
	// g_assetEntryPool free list. This is the size keystone: the pool base and the
	// entry count are now C++ constants (kAssetEntryPoolSize) instead of the .text
	// immediates 0xA51C50 / 0x7FFF0. References: 0x48D371/382/388/390/398.
	extern "C" void __cdecl DB_InitAssetEntryPool()
	{
		for (int type = 0; type < kAssetTypeCount; ++type)
			if (DB_XAssetPool[type])
				DB_XAssetPoolInitHandlers[type](DB_XAssetPool[type], (int)g_poolSize[type]);

		XAssetEntryPoolEntry* pool = &g_assetEntryPool[0];
		*g_freeAssetEntries = &pool[1];                       // g_freeAssetEntries = &pool[1]
		for (int i = 1; i < kAssetEntryPoolSize; ++i)
			pool[i].next = &pool[i + 1];
		pool[kAssetEntryPoolSize].next = nullptr;             // free-list terminator
	}

	// WaW sub_48F9B0 — cdecl(). Post-unload cleanup: dispatch the in-use handler on
	// every still-owned entry, then compact each hash chain in place — free the
	// dead+unreferenced entries, clear the name registration on dead-but-referenced
	// ones, keep owned ones. References: 0x48F9D4, 0x48FA28.
	extern "C" void __cdecl DB_PostUnloadCleanup()
	{
		SL_TransferSystem();

		// Pass 1 — in-use handler dispatch for still-owned entries.
		for (int b = 0; b < kHashBuckets; ++b)
		{
			unsigned short idx = db_hashTable[b];
			while (idx)
			{
				XAssetEntry* e = Entry(idx);
				if (e->zoneIndex != 0)
				{
					*g_inuseEntry  = e;
					*g_inuseHeader = &e->asset.header;
					DB_InUseHandlerDispatch();
				}
				idx = e->nextHash;
			}
		}

		// Pass 2 — compact each chain. `link` points at the uint16 that references
		// the current entry (bucket head, then successive nextHash fields).
		for (int b = 0; b < kHashBuckets; ++b)
		{
			unsigned short* link = &db_hashTable[b];
			while (*link)
			{
				XAssetEntryPoolEntry* pe = &g_assetEntryPool[*link];
				XAssetEntry*          e  = &pe->entry;

				if (e->zoneIndex != 0)
				{
					link = &e->nextHash;               // owned: keep
				}
				else if (e->inuse)
				{
					// Dead but still referenced: clear its name registration.
					int         type   = (int)e->asset.type;
					const char* name   = DB_XAssetGetNameHandlers[type](&e->asset.header);
					int         len    = (int)strlen(name) + 1;
					int         handle = (int)SL_GetStringOfSize((scriptInstance_t)0, name, 4, (unsigned int)len);
					const char* nameRef = handle ? ((char*)gScrMemTreePub->mt_buffer + handle * 0xC + 4) : nullptr;
					DB_XAssetSetNameHandlers[type](&e->asset.header, nameRef);
					link = &e->nextHash;               // keep
				}
				else
				{
					// Dead + unreferenced: unlink, free, return to the free list.
					*link = e->nextHash;
					*g_assetRefCount -= 1;
					int type = (int)e->asset.type;
					DB_XAssetFreeHandlers[type](DB_XAssetPool[type], e->asset.header.data);
					XAssetEntryPoolEntry* oldHead = *g_freeAssetEntries;
					*g_freeAssetEntries = pe;
					pe->next = oldHead;
				}
			}
		}
	}
}

// @wrapper — usercall(type@esi, header@stack) -> cdecl. Detoured over sub_48E370.
__declspec(naked) void W_DB_SetInuseByHeader()
{
	__asm
	{
		mov  ecx, [esp + 4]   // header (caller's stack arg)
		push ecx
		push esi              // type
		call T4_Reconstructed::DB_SetInuseByHeader
		add  esp, 8
		ret
	}
}

// @wrapper — usercall(type@edi) -> cdecl. Detoured over sub_48D7D0. The
// reconstruction (T4_Reconstructed::DB_FindDefaultAsset, T4.cpp) already existed
// but was never detoured; this wires it, spilling edi to the stack.
__declspec(naked) void W_DB_FindDefaultAsset()
{
	__asm
	{
		push edi              // type
		call T4_Reconstructed::DB_FindDefaultAsset
		add  esp, 4
		ret
	}
}

// Wire the 8 reconstructed g_assetEntryPool functions in one shot (the "all at
// once" decision, minus the deferred sub_48F9B0). Each USE_JUMP detour makes the
// vanilla body — with its now-redundant MemoryLimits byte-patch — unreachable, so
// the matching byte-patches are removed in PatchT4MemoryLimits.
void PatchT4_AssetPool()
{
	using Detours::X86::DetourFunction;
	const Detours::X86Option J = Detours::X86Option::USE_JUMP;

	DetourFunction(T4M::GetAddress("DB_EnumXAssets_48D560"),   (uintptr_t)&T4_Reconstructed::DB_EnumXAssets, J);
	DetourFunction(T4M::GetAddress("DB_GetAllXAssetOfType"),   (uintptr_t)&T4_Reconstructed::DB_GetAllXAssetOfType, J);
	DetourFunction(T4M::GetAddress("DB_ClearInuseFlags"),      (uintptr_t)&T4_Reconstructed::DB_ClearInuseFlags, J);
	DetourFunction(T4M::GetAddress("DB_EnumXAssets_FastFile"), (uintptr_t)&T4_Reconstructed::DB_AssetIsUnowned, J); // 0x48DEA0
	DetourFunction(T4M::GetAddress("DB_CleanupAssetRefs"),     (uintptr_t)&T4_Reconstructed::DB_CleanupAssetRefs, J);
	DetourFunction(T4M::GetAddress("DB_InitAssetEntryPool"),   (uintptr_t)&T4_Reconstructed::DB_InitAssetEntryPool, J);
	DetourFunction(T4M::GetAddress("DB_PostUnloadCleanup"),    (uintptr_t)&T4_Reconstructed::DB_PostUnloadCleanup, J);
	DetourFunction(T4M::GetAddress("DB_SetInuseByHeader"),     (uintptr_t)&W_DB_SetInuseByHeader, J); // usercall type@esi
	DetourFunction(T4M::GetAddress("DB_FindDefaultAsset"),     (uintptr_t)&W_DB_FindDefaultAsset, J); // usercall type@edi
}
