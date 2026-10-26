// Script string table diagnostics — observation only (no behaviour change).
//   sl_dump              : writes t4m_sl_dump_<n>.txt (summary per instance / user bit, biggest name
//                          families, then every live string with its refcount and user mask)
//   SL_GetStringOfSize   : both "exceeded maximum number of script strings" sites dump once, just
//                          before the vanilla error (map A -> map B overflow, 2026-10-14).
//   MT_Error / MT_AllocIndex : same dump on "failed memory allocation for script usage".
//   Each dump also reports the memory tree (0x10000 nodes of 12 bytes, 16-bit indices: already the cap):
//   free nodes per buddy list, and the nodes held by live strings.
#include "StdInc.h"
#include <safetyhook.hpp>
#include <algorithm>

namespace
{
	constexpr int      kInstances   = 2;            // 0 = server GSC, 1 = client CSC
	constexpr int      kHashEntries = 25000;        // scrStringGlob_t::hashTable
	constexpr unsigned kStatMask    = 0x30000;      // HASH_STAT_MASK (0 = free)
	constexpr int      kNodeSize    = 0xC;          // RefString stride in mt_buffer
	constexpr int      kTopFamilies = 40;
	constexpr int      kNodeBits    = 16;           // MT_NODE_BITS

	int s_dumpIndex = 0;
	bool s_overflowDumped = false;
	bool s_mtErrorDumped = false;

	int StringNodes(const char* s)
	{
		int n = (int)(strlen(s) + 1 + 4 + kNodeSize - 1) / kNodeSize;   // MT_AllocIndex(len + 4), len with the NUL
		int b = 1;
		while (b < n)
			b <<= 1;
		return b;
	}

	void DumpMemoryTree(FILE* fh, int inst)
	{
		const T4::engine::scrMemTreeGlob_t* mt = (&T4::engine::gScrMemTreePub)[inst].mt_buffer;
		int free = 0;
		fprintf(fh, "  memory tree : ");
		std::vector<unsigned short> stack;
		for (int b = 0; b <= kNodeBits; ++b)
		{
			int count = 0;                              // free blocks of this size: a binary tree (prev / next = children)
			stack.assign(1, mt->head[b]);
			while (!stack.empty() && count < (1 << kNodeBits))
			{
				unsigned short node = stack.back();
				stack.pop_back();
				if (!node)
					continue;
				++count;
				stack.push_back(mt->nodes[node].prev);
				stack.push_back(mt->nodes[node].next);
			}
			free += count << b;
			if (count)
				fprintf(fh, "[%d]x%d ", 1 << b, count);
		}
		fprintf(fh, "\n  memory tree : %d / %d nodes free\n", free, 1 << kNodeBits);
	}

	const T4::engine::RefString* RefAt(int inst, unsigned int index)
	{
		const char* mt = reinterpret_cast<const char*>((&T4::engine::gScrMemTreePub)[inst].mt_buffer);
		return reinterpret_cast<const T4::engine::RefString*>(mt + index * kNodeSize);
	}

	std::string Family(const char* s)
	{
		std::string f(s);
		size_t cut = f.find_first_of("_/\\. ", 1);
		if (cut != std::string::npos)
			f.resize(cut);
		return f;
	}

	void Dump(const char* reason)
	{
		char path[64];
		_snprintf_s(path, sizeof(path), _TRUNCATE, "t4m_sl_dump_%d.txt", s_dumpIndex++);
		FILE* fh = nullptr;
		if (fopen_s(&fh, path, "w") != 0 || !fh)
			return;
		fprintf(fh, "// %s\n", reason);
		for (int inst = 0; inst < kInstances; ++inst)
		{
			const T4::engine::HashEntry* table = (&T4::engine::gScrStringGlob)[inst].hashTable;
			int used = 0, nodes = 0;
			std::map<int, int> users;
			std::map<std::string, int> families;
			for (int h = 1; h < kHashEntries; ++h)
			{
				if (!(table[h].status_next & kStatMask))
					continue;
				const T4::engine::RefString* ref = RefAt(inst, table[h].u.prev);
				++used;
				nodes += StringNodes(ref->str);
				++users[ref->u.s.user];
				++families[Family(ref->str)];
			}
			fprintf(fh, "instance %d : %d / %d entries used, %d memory-tree nodes held by them\n", inst, used,
					kHashEntries - 1, nodes);
			DumpMemoryTree(fh, inst);
			for (const auto& u : users)
				fprintf(fh, "  user 0x%02X : %d\n", u.first, u.second);
			std::vector<std::pair<int, std::string>> top;
			for (const auto& f : families)
				top.emplace_back(f.second, f.first);
			std::sort(top.rbegin(), top.rend());
			for (int i = 0; i < (int)top.size() && i < kTopFamilies; ++i)
				fprintf(fh, "  %6d  %s\n", top[i].first, top[i].second.c_str());
		}
		for (int inst = 0; inst < kInstances; ++inst)
		{
			const T4::engine::HashEntry* table = (&T4::engine::gScrStringGlob)[inst].hashTable;
			fprintf(fh, "\n// instance %d : refcount user string\n", inst);
			for (int h = 1; h < kHashEntries; ++h)
			{
				if (!(table[h].status_next & kStatMask))
					continue;
				const T4::engine::RefString* ref = RefAt(inst, table[h].u.prev);
				fprintf(fh, "%d\t%5u\t0x%02X\t%s\n", inst, ref->u.s.refCount, ref->u.s.user, ref->str);
			}
		}
		fclose(fh);
	}

	void OnOverflow(SafetyHookContext&)
	{
		if (s_overflowDumped)
			return;
		s_overflowDumped = true;
		Dump("exceeded maximum number of script strings");
	}

	void OnMemoryTreeError(SafetyHookContext&)
	{
		if (s_mtErrorDumped)
			return;
		s_mtErrorDumped = true;
		Dump("failed memory allocation for script usage");
	}
}

namespace T4M
{
	// @new — `sl_dump`
	void SL_DumpStrings_f()
	{
		Dump("sl_dump");
		T4::engine::Com_Printf(0, "[T4M] script strings dumped to t4m_sl_dump_%d.txt\n", s_dumpIndex - 1);
	}
}

void PatchT4MAM_ScriptStrings()
{
	static auto overflow_a = safetyhook::create_mid(T4M::GetAddress("SL_GetStringOfSize_full_68E0CD"), &OnOverflow);
	static auto overflow_b = safetyhook::create_mid(T4M::GetAddress("SL_GetStringOfSize_full_68E255"), &OnOverflow);
	static auto mt_error = safetyhook::create_mid(T4M::GetAddress("MT_Error"), &OnMemoryTreeError);
	static auto mt_alloc_full = safetyhook::create_mid(T4M::GetAddress("MT_AllocIndex_full_68A67A"), &OnMemoryTreeError);
}
