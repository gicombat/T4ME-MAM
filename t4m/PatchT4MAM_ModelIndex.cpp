// ==========================================================
// T4M project
//
// Component: clientdll
// Purpose:   G_ModelIndex (sub_54A480) faithful reconstruction + detour.
//
//   Vanilla: int __usercall G_ModelIndex@<eax>(const char* name@<edi>) @ 0x54A480
//   Error  : "G_ModelIndex: Overflow" (unk_86874C) when the model table fills.
//   __usercall: name arrives in EDI -> naked shim pushes it for the cdecl body.
//
//   This file ONLY reconstructs + detours G_ModelIndex, parameterised by the exported
//   globals g_modelHashTbl / g_modelPtrTbl / g_maxModels / g_modelCsBase. The model-cap
//   512 -> 1024 expansion is OWNED by the config-string flip (PatchT4MAM_ConfigStrings.cpp,
//   gate CSF_MODEL_1024): the flip repoints the model name-hash block in-table (0xBF0),
//   relocates dword_190E0A8 + word_18FABD0 to 1024 buffers, bumps the model count caps,
//   and writes g_maxModels / g_modelPtrTbl here. One system (the flip), not two — the old
//   out-of-table SetupModelCap1024 attempt is retired.
//
//   The name handle is always written by SV_SetConfigstring (g_modelHashTbl is a live CS
//   block); the recon never writes it directly (would break the configstring ref-count).
// ==========================================================

#include "StdInc.h"
#include "T4.h"

static const int VANILLA_MAX_MODELS = 0x200;   // 512 — g_maxModels default; the CS flip raises it to 1024

// Model config-string base offset. Default 0x58E (vanilla). The CS flip stage 2 re-bases
// the model block to the top of the enlarged table (0xBF0) and sets this so G_ModelIndex
// issues SV_SetConfigstring at the new offset. Exported for the flip to write.
extern "C" int g_modelCsBase = 0x58E;

namespace ModelIndexEngine
{
    typedef unsigned int (__cdecl* SL_FindLowercaseString_t)(const char* str, int inst);     // sub_68DD50
    typedef char*        (__cdecl* Com_FormatMsg_t)         (const char* fmt, ...);          // sub_5F6D80
    typedef void         (__cdecl* Strncpyz_t)              (char* dst, const char* src, int size); // sub_7AA9C0
    typedef void         (__cdecl* Com_Error_t)             (int code, const char* fmt, ...);// sub_59AC50
    typedef void*        (__cdecl* XModelFinder_t)          (const char* name, void* cb1, void* cb2); // sub_618EB0/618E20
    typedef void         (__cdecl* SV_SetConfigstring_t)    (const void* pairs, int count);  // sub_6311E0

    // All resolved at runtime in PatchT4MAM_ModelIndex() (variant-aware; avoids static-init
    // before the AddrMap is loaded). CSV keys noted per line.
    static SL_FindLowercaseString_t SL_FindLowercaseString = nullptr; // SL_FindLowercaseString (sub_68DD50)
    static Com_FormatMsg_t          Com_FormatMsg          = nullptr; // va (sub_5F6D80)
    static Strncpyz_t               Strncpyz               = nullptr; // I_strncpyz (sub_7AA9C0)
    static Com_Error_t              Com_Error              = nullptr; // sub_59AC50
    static XModelFinder_t           XModelFinder_Sync      = nullptr; // sub_618EB0
    static XModelFinder_t           XModelFinder_Deferred  = nullptr; // sub_618E20
    static SV_SetConfigstring_t     SV_SetConfigstring     = nullptr; // SV_SetConfigstrings (sub_6311E0)

    static unsigned short* g_scrConst0    = nullptr; // word_1F33B90  (empty-string id marker)
    static int*            g_precacheOpen = nullptr; // dword_18F6DB8 (precache window flag)
    static unsigned char** g_dvar1F552FC  = nullptr; // dword_1F552FC (byte[+0x10] selects finder)
    static int*            g_errBufInited  = nullptr; // dword_3882B7C
    static char*           g_errBuf        = nullptr; // byte_3BE1E30
    static char*           g_byte3BE222F   = nullptr; // byte_3BE222F
    static char*           g_byte3BD4716   = nullptr; // byte_3BD4716
    static void*           kFinderCallback = nullptr; // sub_6D9A30
    static const char*     kOverflowMsg    = nullptr; // unk_86874C ("\x15G_ModelIndex: Overflow")
}

// --- model-table state, all EXPORTED so the config-string flip (PatchT4MAM_ConfigStrings.cpp) owns
// the 512->1024 expansion: it repoints g_modelHashTbl at the in-table model block, relocates
// g_modelPtrTbl to a 1024 buffer (gate CSF_MODEL_1024) and raises g_maxModels. Defaults = vanilla 512.
// Defaults resolved variant-aware in PatchT4MAM_ModelIndex() (runs before the CS flip, which
// overwrites these when CS_ENABLE). word_2350F42 / dword_190E0A8.
extern "C" unsigned short* g_modelHashTbl = nullptr; // word_2350F42 (-> in-table block by the flip)
extern "C" void**          g_modelPtrTbl  = nullptr; // dword_190E0A8 (-> 1024 buffer by the flip)
extern "C" int             g_maxModels    = VANILLA_MAX_MODELS;

extern "C" int __cdecl T4M_G_ModelIndex_recon(const char* name);

// Scr_ErrorInternal (sub_693CF0) is __usercall(eax = scriptId). Vanilla's not-precached path
// does `xor eax,eax` before the call (script instance 0). Calling it via a plain function
// pointer left eax = the function address -> edx = eax*0x18048 -> OOB byte_3882B78[edx] ->
// access violation @0x693CF9 (observed: EAX=0x693CF0). This thunk passes eax=0 like vanilla;
// with eax=0 it returns cleanly (no script error) and the model is lazily registered below.
static void* p_Scr_ErrorInternal = nullptr;   // sub_693CF0 (resolved in PatchT4MAM_ModelIndex)
static __declspec(naked) void Scr_ErrorInternal_scriptId0()
{
    __asm
    {
        xor  eax, eax
        call p_Scr_ErrorInternal   // sub_693CF0, __usercall eax=scriptId(0)
        retn
    }
}

// ---------------------------------------------------------------------------
// Reconstruction of sub_54A480. __cdecl(name)->eax. Uniform across vanilla and
// 1024 modes (the globals above select the tables/cap).
// ---------------------------------------------------------------------------
extern "C" int __cdecl T4M_G_ModelIndex_recon(const char* name)
{
    using namespace ModelIndexEngine;

    static bool s_logged = false;
    if (!s_logged)
    {
        s_logged = true;
        T4::engine::Com_Printf(0, "[T4M] G_ModelIndex active, cap=%d\n", g_maxModels);
    }

    if (name[0] == '\0')
        return 0;

    unsigned int myId        = SL_FindLowercaseString(name, 0);
    unsigned int emptyMarker = *g_scrConst0;

    int esi = 1;
    for (; esi < g_maxModels; ++esi)
    {
        unsigned int slot = g_modelHashTbl[esi];
        if (slot == emptyMarker || slot == 0) break; // free slot (relocated buffer is zeroed)
        if (slot == myId)                      return esi; // already registered
    }

    if (*g_precacheOpen == 0)
    {
        char* msg = Com_FormatMsg("model '%s' not precached", name);
        if (*g_errBufInited == 0)
        {
            Strncpyz(g_errBuf, msg, 0x3FF);
            *g_byte3BE222F  = 0;
            *g_errBufInited = (int)g_errBuf;
        }
        *g_byte3BD4716 = 0;
        Scr_ErrorInternal_scriptId0();   // sub_693CF0 with eax=0 (returns cleanly; warns, no longjmp)
    }

    if (esi == g_maxModels)
    {
        T4::engine::Com_Printf(0,
            "^1[T4M] G_ModelIndex OVERFLOW at %d models, requesting '%s'\n", esi, name);
        Com_Error(1, kOverflowMsg);
    }

    XModelFinder_t finder = (g_dvar1F552FC[0][0x10] != 0) ? XModelFinder_Sync
                                                          : XModelFinder_Deferred;
    void* model = finder(name, kFinderCallback, kFinderCallback);
    g_modelPtrTbl[esi]  = model;
    // The name handle in g_modelHashTbl is written by SV_SetConfigstring below — g_modelHashTbl is
    // always a LIVE CS block (vanilla 0x2350F42 or the flip's in-table model block), so writing it
    // here without a string ref would corrupt the configstring ref-count -> dangling handle.

    // Issue the config-string for every registered model. With the flip the model block lives at
    // [g_modelCsBase, g_modelCsBase + g_maxModels) which fits inside g_maxCS (0xBF0+0x400=0xFF0<0x1000),
    // so no overflow even at 1024. (CS_REGISTER_LIMIT, the old out-of-table 512 guard, is retired.)
    if (esi < g_maxModels)
    {
        struct { int csIndex; const char* name; } pair;
        pair.csIndex = esi + g_modelCsBase;   // 0x58E vanilla, 0xBF0 after CS flip stage 2
        pair.name    = name;
        SV_SetConfigstring(&pair, 1);
    }

    {
        static int s_hwm = 0;
        if (esi > s_hwm)
        {
            s_hwm = esi;
            if ((esi % 64) == 0 || esi >= g_maxModels - 8)
                T4::engine::Com_Printf(0, "[T4M] G_ModelIndex high-water: %d / %d\n", esi, g_maxModels);
        }
    }

    return esi;
}

// __usercall(edi=name) -> cdecl shim. Detour target for 0x54A480.
static __declspec(naked) void T4M_G_ModelIndex_usercall()
{
    __asm
    {
        push    edi
        call    T4M_G_ModelIndex_recon
        add     esp, 4
        retn
    }
}

// ---------------------------------------------------------------------------
// Install. Init-time: wires the detour. The 512->1024 model-cap expansion is owned
// by the config-string flip (PatchT4MAM_ConfigStrings.cpp, gate CSF_MODEL_1024): it
// relocates dword_190E0A8 + word_18FABD0 and sets g_maxModels / g_modelPtrTbl here.
// NO engine prints here (Sys_RunInit constraint) — status logged at first recon call.
// ---------------------------------------------------------------------------
void PatchT4MAM_ModelIndex()
{
    using namespace ModelIndexEngine;

    // Resolve all vanilla deps variant-aware (runs at PatchT4 time, AddrMap loaded).
    SL_FindLowercaseString = (SL_FindLowercaseString_t)T4M::GetAddress("SL_FindLowercaseString");
    Com_FormatMsg          = (Com_FormatMsg_t)         T4M::GetAddress("va");
    Strncpyz               = (Strncpyz_t)              T4M::GetAddress("I_strncpyz");
    Com_Error              = (Com_Error_t)             T4M::GetAddress("sub_59AC50");
    XModelFinder_Sync      = (XModelFinder_t)          T4M::GetAddress("sub_618EB0");
    XModelFinder_Deferred  = (XModelFinder_t)          T4M::GetAddress("sub_618E20");
    SV_SetConfigstring     = (SV_SetConfigstring_t)    T4M::GetAddress("SV_SetConfigstrings");
    g_scrConst0    = (unsigned short*)T4M::GetAddress("word_1F33B90");
    g_precacheOpen = (int*)           T4M::GetAddress("dword_18F6DB8");
    g_dvar1F552FC  = (unsigned char**)T4M::GetAddress("dword_1F552FC");
    g_errBufInited = (int*)           T4M::GetAddress("dword_3882B7C");
    g_errBuf       = (char*)          T4M::GetAddress("byte_3BE1E30");
    g_byte3BE222F  = (char*)          T4M::GetAddress("byte_3BE222F");
    g_byte3BD4716  = (char*)          T4M::GetAddress("byte_3BD4716");
    kFinderCallback = (void*)         T4M::GetAddress("sub_6D9A30");
    kOverflowMsg    = (const char*)   T4M::GetAddress("unk_86874C");
    p_Scr_ErrorInternal = (void*)     T4M::GetAddress("Scr_ErrorInternal");

    // Flip-shared model-table defaults (the CS flip overwrites these later, see file head).
    g_modelHashTbl = (unsigned short*)T4M::GetAddress("word_2350F42");
    g_modelPtrTbl  = (void**)         T4M::GetAddress("dword_190E0A8");

    Detours::X86::DetourFunction((uintptr_t)T4M::GetAddress("G_ModelIndex"),
                                 (uintptr_t)&T4M_G_ModelIndex_usercall,
                                 Detours::X86Option::USE_JUMP);
}
