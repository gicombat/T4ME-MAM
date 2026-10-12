// ==========================================================
// T4M project
//
// Purpose: add pathnodes (cover, concealment...) to a map loaded from a fastfile, and
//          rebuild links / vis / node tree with the game's own "Connect Paths" builder.
//
//   Extra nodes live in <fs_basepath>/<fs_game>/maps/<mapname>.nodes, entity-string format.
//   Zone path arrays are sized exactly, so nodes/basenodes are re-pointed to bigger copies.
//   The extra nodes are also appended to the map entity string: G_SpawnPathnodeDynamic
//   pairs node_* entities with nodes by order, and actualNodeCount (vis stride, GetNode)
//   only counts paired nodes.
//
//   Plan: "Claude R&D/T4M CoD WaW/plans/plan_runtime_pathnodes.md"
// ==========================================================

#include "StdInc.h"
#include "T4.h"
#include <cctype>
#include <cmath>
#include <cstdio>
#include <string>
#include <utility>
#include <vector>

using namespace T4::engine;

namespace
{
	constexpr unsigned PATH_MAX_NODES = 0x2000;

	struct NodeEnt
	{
		std::vector<std::pair<std::string, std::string>> kv;

		const char* Get(const char* key) const
		{
			for (const auto& p : kv)
				if (!_stricmp(p.first.c_str(), key))
					return p.second.c_str();
			return nullptr;
		}
	};

	// Replacements for the zone arrays; kept alive until the next fresh map load.
	std::vector<unsigned char> s_nodes;
	std::vector<pathbasenode_t> s_basenodes;
	std::string s_entityString;

	// pathnode_* authoring buffer for the current map.
	std::string s_editMap;
	std::vector<NodeEnt> s_edit;

	const char* DvarString(const char* name)
	{
		dvar_t* dvar = Dvars::Functions::Dvar_FindVar(name);
		return (dvar && dvar->current.string) ? dvar->current.string : "";
	}

	std::string NodeFilePath(const char* map)
	{
		std::string game = DvarString("fs_game");
		if (game.empty())
			game = "main";
		return std::string(DvarString("fs_basepath")) + "\\" + game + "\\maps\\" + map + ".nodes";
	}

	// Entity-string tokenizer: { "key" "value" ... } blocks.
	bool ParseEntities(const std::string& text, std::vector<NodeEnt>& out)
	{
		size_t i = 0;
		auto skipWs = [&]() { while (i < text.size() && isspace((unsigned char)text[i])) ++i; };
		auto quoted = [&](std::string& s) -> bool
		{
			skipWs();
			if (i >= text.size() || text[i] != '"')
				return false;
			size_t end = text.find('"', i + 1);
			if (end == std::string::npos)
				return false;
			s = text.substr(i + 1, end - i - 1);
			i = end + 1;
			return true;
		};

		for (;;)
		{
			skipWs();
			if (i >= text.size())
				return true;
			if (text[i] != '{')
				return false;
			++i;

			NodeEnt ent;
			for (;;)
			{
				skipWs();
				if (i < text.size() && text[i] == '}')
				{
					++i;
					break;
				}
				std::string key, value;
				if (!quoted(key) || !quoted(value))
					return false;
				ent.kv.emplace_back(key, value);
			}
			out.push_back(std::move(ent));
		}
	}

	std::string SerializeEntity(const NodeEnt& ent)
	{
		std::string s = "{\n";
		for (const auto& p : ent.kv)
			s += "\"" + p.first + "\" \"" + p.second + "\"\n";
		return s + "}\n";
	}

	bool ReadNodeFile(const char* map, std::vector<NodeEnt>& out)
	{
		FILE* f = fopen(NodeFilePath(map).c_str(), "rb");
		if (!f)
			return false;
		std::string text;
		char buf[4096];
		size_t got;
		while ((got = fread(buf, 1, sizeof(buf), f)) > 0)
			text.append(buf, got);
		fclose(f);
		return ParseEntities(text, out);
	}

	int FindNodeType(const char* classname)
	{
		for (const PathnodeClassType* t = pathnodeClassTypes; t->classname; ++t)
			if (!strcmp(t->classname, classname))
				return t->type;
		return 0;
	}

	// @new - extra nodes from the .nodes file, then the vanilla builder chain
	// (Path_InitPaths non-fastfile order: BuildChains, ConnectPaths, overlaps).
	void Path_AddExtraNodes()
	{
		const char* map = DvarString("mapname");
		std::vector<NodeEnt> ents;
		if (!*map || !ReadNodeFile(map, ents) || ents.empty())
			return;

		std::vector<std::pair<int, const NodeEnt*>> valid;
		for (const NodeEnt& ent : ents)
		{
			const char* classname = ent.Get("classname");
			const int type = classname ? FindNodeType(classname) : 0;
			if (!type)
			{
				Com_Printf(0, "^3[T4M] %s.nodes: unknown node classname '%s', skipped\n", map, classname ? classname : "");
				continue;
			}
			valid.emplace_back(type, &ent);
		}

		PathData& path = gameWorldSp->path;
		const unsigned n = path.nodeCount;
		if (valid.empty() || n + valid.size() > PATH_MAX_NODES)
		{
			Com_Printf(0, "^3[T4M] %s.nodes: %u + %u nodes, nothing added (max %u)\n", map, n, (unsigned)valid.size(), PATH_MAX_NODES);
			return;
		}

		const size_t total = n + valid.size();
		s_nodes.assign(total * sizeof(pathnode_t), 0);
		memcpy(s_nodes.data(), path.nodes, n * sizeof(pathnode_t));
		s_basenodes.assign(total, pathbasenode_t{});
		if (path.basenodes)
			memcpy(s_basenodes.data(), path.basenodes, n * sizeof(pathbasenode_t));
		path.nodes = reinterpret_cast<pathnode_t*>(s_nodes.data());
		path.basenodes = s_basenodes.data();

		static SpawnVar spawnVar;
		for (const auto& v : valid)
		{
			memset(&spawnVar, 0, sizeof(spawnVar));
			for (const auto& p : v.second->kv)
			{
				if (!_stricmp(p.first.c_str(), "classname") || spawnVar.numSpawnVars >= 64)
					continue;
				spawnVar.spawnVars[spawnVar.numSpawnVars][0] = const_cast<char*>(p.first.c_str());
				spawnVar.spawnVars[spawnVar.numSpawnVars][1] = const_cast<char*>(p.second.c_str());
				++spawnVar.numSpawnVars;
			}
			spawnVar.spawnVarsValid = true;
			T4::game::Path_SpawnNodeStatic(v.first, &spawnVar);
			T4::game::Path_InitNodeDynamic(&path.nodes[path.nodeCount - 1]);
		}

		// Builder preconditions: no cached links/tree/vis, link counts from zero.
		for (unsigned i = 0; i < path.nodeCount; ++i)
		{
			path.nodes[i].constant.totalLinkCount = 0;
			path.nodes[i].constant.Links = nullptr;
		}
		*g_pathLinkPool = nullptr;
		*g_pathTreeNodeIndices = nullptr;
		path.chainNodeCount = 0;
		path.chainNodeForNode = nullptr;
		path.nodeForChainNode = nullptr;
		path.visBytes = 0;
		path.pathVis = nullptr;
		path.nodeTreeCount = 0;
		path.nodeTree = nullptr;

		const DWORD start = GetTickCount();
		T4::game::Path_BuildChains();
		T4::game::Path_ConnectPaths();
		T4::game::Path_CheckNodeOverlaps();

		MapEnts* mapEnts = *cm_mapEnts;
		s_entityString = mapEnts->entityString;
		for (const auto& v : valid)
			s_entityString += SerializeEntity(*v.second);
		mapEnts->entityString = &s_entityString[0];
		mapEnts->numEntityChars = (int)s_entityString.size() + 1;
		*g_entityBeginParsePoint = mapEnts->entityString;
		*g_entityEndParsePoint = mapEnts->entityString;

		Com_Printf(0, "[T4M] %s: %u extra pathnodes, %u total, paths rebuilt in %u ms\n",
			map, (unsigned)valid.size(), path.nodeCount, GetTickCount() - start);
	}

	// ---- Authoring commands ----------------------------------------------------

	bool SyncEditBuffer()
	{
		const char* map = DvarString("mapname");
		if (!*map)
		{
			Com_Printf(0, "No map loaded.\n");
			return false;
		}
		if (s_editMap != map)
		{
			s_editMap = map;
			s_edit.clear();
			ReadNodeFile(map, s_edit);
		}
		return true;
	}

	gclient_s* LocalClient()
	{
		gentity_s* ent = &g_entities[0];
		return ent->client;
	}

	void ParseVec3(const char* s, float* v)
	{
		v[0] = v[1] = v[2] = 0.0f;
		if (s)
			sscanf(s, "%f %f %f", &v[0], &v[1], &v[2]);
	}

	void Cmd_PathnodeAdd_f()
	{
		if (Cmd_Argc() < 2)
		{
			Com_Printf(0, "usage: pathnode_add <type> [key value ...]   e.g. pathnode_add cover_stand\n");
			return;
		}
		gclient_s* client = LocalClient();
		if (!SyncEditBuffer() || !client)
			return;

		std::string classname = Cmd_Argv(1);
		if (classname.compare(0, 5, "node_"))
			classname = "node_" + classname;
		if (!FindNodeType(classname.c_str()))
		{
			Com_Printf(0, "Unknown node type '%s'. Types:", classname.c_str());
			for (const PathnodeClassType* t = pathnodeClassTypes; t->classname; ++t)
				Com_Printf(0, " %s", t->classname + 5);
			Com_Printf(0, "\n");
			return;
		}

		char buf[128];
		NodeEnt ent;
		ent.kv.emplace_back("classname", classname);
		sprintf_s(buf, "%g %g %g", client->ps.origin[0], client->ps.origin[1], client->ps.origin[2]);
		ent.kv.emplace_back("origin", buf);
		sprintf_s(buf, "0 %g 0", client->ps.viewangles[1]);
		ent.kv.emplace_back("angles", buf);
		for (int i = 2; i + 1 < Cmd_Argc(); i += 2)
			ent.kv.emplace_back(Cmd_Argv(i), Cmd_Argv(i + 1));

		s_edit.push_back(std::move(ent));
		Com_Printf(0, "Added %s #%u (unsaved; pathnode_save, then reload the map)\n", classname.c_str(), (unsigned)s_edit.size() - 1);
	}

	void Cmd_PathnodeRemove_f()
	{
		gclient_s* client = LocalClient();
		if (!SyncEditBuffer() || !client)
			return;

		int best = -1;
		float bestDistSq = 64.0f * 64.0f;
		for (size_t i = 0; i < s_edit.size(); ++i)
		{
			float o[3];
			ParseVec3(s_edit[i].Get("origin"), o);
			const float dx = o[0] - client->ps.origin[0], dy = o[1] - client->ps.origin[1], dz = o[2] - client->ps.origin[2];
			const float d = dx * dx + dy * dy + dz * dz;
			if (d < bestDistSq)
			{
				bestDistSq = d;
				best = (int)i;
			}
		}
		if (best < 0)
		{
			Com_Printf(0, "No added node within 64 units.\n");
			return;
		}
		Com_Printf(0, "Removed %s #%d at %.0f units (unsaved)\n", s_edit[best].Get("classname"), best, sqrtf(bestDistSq));
		s_edit.erase(s_edit.begin() + best);
	}

	void Cmd_PathnodeList_f()
	{
		if (!SyncEditBuffer())
			return;
		for (size_t i = 0; i < s_edit.size(); ++i)
			Com_Printf(0, "#%u %s  origin (%s)  angles (%s)\n", (unsigned)i,
				s_edit[i].Get("classname"), s_edit[i].Get("origin"), s_edit[i].Get("angles"));
		Com_Printf(0, "%u added node(s) for %s; %u nodes in the loaded map, %u paired with entities%s\n",
			(unsigned)s_edit.size(), s_editMap.c_str(), gameWorldSp->path.nodeCount, *actualNodeCount,
			*actualNodeCount == gameWorldSp->path.nodeCount ? "" : " ^1(MISMATCH)");
	}

	void Cmd_PathnodeSave_f()
	{
		if (!SyncEditBuffer())
			return;
		const std::string path = NodeFilePath(s_editMap.c_str());
		const std::string dir = path.substr(0, path.find_last_of('\\'));
		CreateDirectoryA(dir.c_str(), nullptr);

		FILE* f = fopen(path.c_str(), "wb");
		if (!f)
		{
			Com_Printf(0, "^1Cannot write %s\n", path.c_str());
			return;
		}
		for (const NodeEnt& ent : s_edit)
		{
			const std::string s = SerializeEntity(ent);
			fwrite(s.data(), 1, s.size(), f);
		}
		fclose(f);
		Com_Printf(0, "Saved %u node(s) to %s. Reload the map (map %s) to rebuild paths.\n",
			(unsigned)s_edit.size(), path.c_str(), s_editMap.c_str());
	}
}

// @modified - sub_55A2D0 (Path_InitLinkCounts). Called by G_InitGame after the
// Path_InitNodeDynamic loop and before DropToFloor / entity spawn: on a fresh fastfile
// load, extra nodes are added and paths rebuilt first. A restart keeps the rebuilt arrays.
void __cdecl T4M::Path_InitLinkCounts()
{
	PathData& path = gameWorldSp->path;
	if ((*useFastFile)->current.enabled && path.nodes != reinterpret_cast<pathnode_t*>(s_nodes.data()))
		Path_AddExtraNodes();

	for (unsigned i = 0; i < path.nodeCount; ++i)
	{
		pathnode_t* node = &path.nodes[i];
		node->dynamic.wLinkCount = node->constant.totalLinkCount;
		for (unsigned j = 0; j < node->constant.totalLinkCount; ++j)
		{
			pathlink_s* link = &node->constant.Links[j];
			link->ubBadPlaceCount[0] = 0;
			link->ubBadPlaceCount[1] = 0;
			link->ubBadPlaceCount[2] = 0;
			link->ubBadPlaceCount[3] = 0;
		}
	}
}

void PatchT4MAM_PathNodes()
{
	Detours::X86::DetourFunction((uintptr_t)T4M::GetAddress("Path_InitLinkCounts"), (uintptr_t)&T4M::Path_InitLinkCounts, Detours::X86Option::USE_JUMP);

	T4M::Cmd_AddCommand("pathnode_add", Cmd_PathnodeAdd_f);
	T4M::Cmd_AddCommand("pathnode_remove", Cmd_PathnodeRemove_f);
	T4M::Cmd_AddCommand("pathnode_list", Cmd_PathnodeList_f);
	T4M::Cmd_AddCommand("pathnode_save", Cmd_PathnodeSave_f);
}
