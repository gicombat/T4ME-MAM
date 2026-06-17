#include "StdInc.h"
#include "T4.h"
#include "MemoryMgr.h"
#include <cstdio>
#include <cstring>
#include <cstdlib>

// =====================================================================
// Cinematic loading-movie selection + path resolution.
//
//   Deux détours :
//     • CL_MapLoading_CalcMovieToPlay (sub_6410F0) — le VM qui interprète
//       video/cin_levels.txt : mapname (arg_4) → nom de .bik (arg_8).
//     • R_Cinematic_BinkOpen (sub_6EB5C0) — construit le chemin du .bik
//       (bik/<n>.bik en fastfile, sinon <cwd>\main\video\<n>.bik puis
//       <cwd>\raw\video\<n>.bik) et délègue à R_Cinematic_BinkOpenPath.
//
//   Noms/structure portés des sources CoD4 (cl_main.cpp / r_cinematic.cpp).
//   R_Cinematic_BinkOpenPath (sub_6EB3B0) reste vanilla (plomberie Bink) ;
//   on l'appelle par VA via un thunk __usercall.
// =====================================================================

using namespace T4::engine;

// ── Table d'opcodes du VM (port CoD4 s_movieToPlayScriptOpInfo) ───────
//   { op, opName, inValues, outValues }. opName == nullptr → LITERAL (défaut).
namespace
{
	struct MovieToPlayScriptOpInfo
	{
		MovieToPlayScriptOp op;
		const char*         opName;
		unsigned int        inValues;
		unsigned int        outValues;
	};

	const MovieToPlayScriptOpInfo s_movieToPlayScriptOpInfo[18] =
	{
		{ MTPSOP_PLUS,       "+",          2u, 1u },
		{ MTPSOP_MINUS,      "-",          2u, 1u },
		{ MTPSOP_MUL,        "*",          2u, 1u },
		{ MTPSOP_GT,         ">",          2u, 1u },
		{ MTPSOP_LT,         "<",          2u, 1u },
		{ MTPSOP_EQ,         "==",         2u, 1u },
		{ MTPSOP_STRCMP,     "strcmp",     2u, 1u },
		{ MTPSOP_STRCAT,     "strcat",     2u, 1u },
		{ MTPSOP_NOT,        "!",          1u, 1u },
		{ MTPSOP_DUP,        "dup",        1u, 2u },
		{ MTPSOP_DROP,       "drop",       1u, 0u },
		{ MTPSOP_SWAP,       "swap",       2u, 2u },
		{ MTPSOP_GETDVAR,    "getdvar",    1u, 1u },
		{ MTPSOP_GETMAPNAME, "getmapname", 0u, 1u },
		{ MTPSOP_IF,         "if",         1u, 0u },
		{ MTPSOP_THEN,       "then",       0u, 0u },
		{ MTPSOP_PLAY,       "play",       1u, 0u },
		{ MTPSOP_LITERAL,    nullptr,      0u, 1u },
	};

	// I_strncpyz (vanilla sub_7AA9C0) — recréé : copie ≤ destSize-1 chars,
	// null-termine toujours. (CRT strncpy ne garantit pas le terminateur.)
	void I_strncpyz(char* dst, const char* src, int destSize)
	{
		if (destSize <= 0)
			return;
		strncpy(dst, src, destSize - 1);
		dst[destSize - 1] = 0;
	}

	// Sys_Cwd (sub_7AE02B) — recréé. Vanilla renvoie le répertoire de travail
	// du process (où tournent les .bik loose : <cwd>\main\video\…).
	char* Sys_Cwd()
	{
		static char s_cwd[256];
		GetCurrentDirectoryA(static_cast<DWORD>(sizeof(s_cwd)), s_cwd);
		return s_cwd;
	}

	// ── Helpers moteur appelés par VA (variant-aware via addr_mapping.csv) ──
	symbol<void(const char*)>        Com_BeginParseSession_e{ "Com_BeginParseSession" };
	// Com_ParseExt(&data, allowLineBreaks) → parseInfo_t* ; token = champ @ +0
	// (char[1024]) → on type le retour comme char* (= le token directement).
	symbol<char*(const char**, int)> Com_ParseExt_e{ "Com_ParseExt" };
	symbol<void()>                   Com_EndParseSession_e{ "Com_EndParseSession" };
	symbol<const char*(const char*)> Dvar_GetVariantString_e{ "Dvar_GetVariantString" };

	// R_Cinematic_BinkOpenPath (sub_6EB3B0) — VANILLA, conservé. __usercall :
	//   eax = filepath, [esp+4] = playbackFlags (byte), [esp+8] = errText.
	//   Retour al. La fonction se termine par `retn` → l'appelant nettoie.
	char Call_R_Cinematic_BinkOpenPath(const char* filepath, unsigned int flags, char* errText)
	{
		static void* fn = reinterpret_cast<void*>(T4M::GetAddress("R_Cinematic_BinkOpenPath"));
		char result;
		__asm
		{
			mov     eax, filepath          // usercall: path in eax
			push    errText                // arg_4 = errText
			push    flags                  // arg_0 = flags (lu en byte côté vanilla)
			call    fn
			add     esp, 8                 // appelant nettoie (vanilla retn)
			mov     result, al
		}
		return result;
	}
} // anonymous namespace

// =====================================================================
// T4_Reconstructed::R_Cinematic_BinkOpen — sub_6EB5C0
// @faithful (port CoD4 R_Cinematic_BinkOpen, sous-dossier WaW "bik/").
//   __cdecl(filename, errText, playbackFlags) → char (1 = ouvert).
//   Le bridge __usercall (esi/edi) est T4M::R_Cinematic_BinkOpen_Wrapper.
//   errTextSize = 0x80 est implicite côté R_Cinematic_BinkOpenPath (vanilla).
// =====================================================================
char T4M::R_Cinematic_BinkOpen(const char* filename, char* errText, unsigned int playbackFlags)
{
	char* cwd = Sys_Cwd();
	char  filepath[3][512];

	filepath[1][0] = 0;
	filepath[2][0] = 0;
	if (playbackFlags & CINEMATIC_PLAYBACK_FROM_FASTFILE)
	{
		// vanilla : rawfile dans un .ff chargé (impose FROM_MEMORY).
		_snprintf(filepath[0], 0x200u, "bik/%s.%s", filename, "bik");
	}
	else
	{
		// fichiers disque : main\video puis fallback raw\video.
		_snprintf(filepath[0], 0x200u, "%s\\main\\video\\%s.%s", cwd, filename, "bik");
		_snprintf(filepath[1], 0x200u, "%s\\raw\\video\\%s.%s", cwd, filename, "bik");
		if ((*T4::engine::fs_game)->current.string[0] != '\0')
		{
			_snprintf(filepath[2], 0x200u, "%s\\video\\%s.%s", va("%s\\%s\\", (*T4::engine::fs_localAppData)->current.string, (*T4::engine::fs_game)->current.string), filename, "bik");
		}
	}

	if ((*T4::engine::fs_game)->current.string[0] != '\0')
	{
		if (Call_R_Cinematic_BinkOpenPath(filepath[2], playbackFlags, errText))
			return 1;
	}
	if (Call_R_Cinematic_BinkOpenPath(filepath[0], playbackFlags, errText))
		return 1;
	return filepath[1][0] && Call_R_Cinematic_BinkOpenPath(filepath[1], playbackFlags, errText);
}

// @wrapper — __usercall → __cdecl bridge pour R_Cinematic_BinkOpen (sub_6EB5C0).
//   À l'entrée : esi = name, edi = errText, [esp+4] = playbackFlags.
//   Vanilla se termine par `ret` ; l'appelant (sub_6EB730) nettoie son arg
//   flags avec `add esp, 4` → on ne fait PAS `ret 4`.
__declspec(naked) void T4M::R_Cinematic_BinkOpen_Wrapper()
{
	__asm
	{
		push    dword ptr [esp+4]    // arg_2 = playbackFlags (arg pile)
		push    edi                  // arg_1 = errText (edi)
		push    esi                  // arg_0 = name (esi)
		call    T4M::R_Cinematic_BinkOpen   // cdecl(name, errText, flags) → al
		add     esp, 0Ch             // nettoie nos 3 pushes
		ret                          // l'appelant nettoie son arg flags
	}
}

// =====================================================================
// Installation
// =====================================================================
void PatchT4MAM_Cinematic()
{
	// Path builder — détour via wrapper naked (__usercall esi/edi).
	Detours::X86::DetourFunction((uintptr_t)T4M::GetAddress("R_Cinematic_BinkOpen"),
		(uintptr_t)&T4M::R_Cinematic_BinkOpen_Wrapper, Detours::X86Option::USE_JUMP);
}
