// ==========================================================
// T4M project
//
// Component: clientdll
// Purpose:   Reconstruction du sous-système CONFIG-STRING (étape vers 1024+ modèles).
//
//   Stratégie : reconstruire chaque fonction CS de façon FIDÈLE (détour
//   T4_Reconstructed::), en remplaçant les accès durs (word_2350426 / 0xBF0 /
//   word_2350424) par des GLOBAUX PARTAGÉS retargetables, qui valent les valeurs
//   VANILLA par défaut. Tant qu'on n'a pas basculé ces globaux, les détours sont
//   à comportement IDENTIQUE au vanilla → testables sans régression. Une fois TOUT
//   le sous-système reconstruit, on bascule les globaux vers une table agrandie/
//   relogée en un seul changement atomique. Voir plans/plan_configstring_reconstruction.md.
//
//   Reconstruites + vérifiées (adversarial ligne-par-ligne vs asm) :
//     - SV_GetConfigstring   (sub_6315C0, __usercall esi=idx/edi=dstSize/dst@stack)
//     - SV_ClearConfigstrings(sub_631DA0, __cdecl void)
//     - SV_SetConfigstrings  (sub_6311E0, __cdecl(pairs[],count) — writer batch)
//   À VENIR : réseau (sub_62F500/64CAE0), init (sub_510830/990/5104F0), parsers,
//   allocator, puis bascule + relocation arbre bounds/baselines.
//
//   ⚠️ Détours GATED OFF par défaut (code possédé+compilé mais inerte). Activer un
//   flag pour valider la fidélité au runtime (le jeu doit se comporter à l'identique),
//   puis bascule des globaux.
// ==========================================================

#include "StdInc.h"
#include "T4.h"
#include <cstring>   // strlen / strcmp / memcpy / memset
#include <cstdio>    // sprintf (flip status string)


// === T4M variant-aware address cache (resolved at runtime in ResolveCsAddrs) ===
static DWORD a_401000 = 0;
static DWORD a_45CE1E = 0;
static DWORD a_474620 = 0;
static DWORD a_496050 = 0;
static DWORD a_5720F0 = 0;
static DWORD a_572210 = 0;
static DWORD a_5EF390 = 0;
static DWORD a_5F69E0 = 0;
static DWORD a_5F6DF0 = 0;
static DWORD a_62F250 = 0;
static DWORD a_62F500 = 0;
static DWORD a_6311E0 = 0;
static DWORD a_6315C0 = 0;
static DWORD a_631DA0 = 0;
static DWORD a_63248C = 0;
static DWORD a_633FA0 = 0;
static DWORD a_638520 = 0;
static DWORD a_6393F0 = 0;
static DWORD a_64CAE0 = 0;
static DWORD a_674C70 = 0;
static DWORD a_674D00 = 0;
static DWORD a_674E00 = 0;
static DWORD a_674F50 = 0;
static DWORD a_675060 = 0;
static DWORD a_675430 = 0;
static DWORD a_675500 = 0;
static DWORD a_675560 = 0;
static DWORD a_6757A0 = 0;
static DWORD a_676690 = 0;
static DWORD a_676990 = 0;
static DWORD a_6772E0 = 0;
static DWORD a_678330 = 0;
static DWORD a_67AB80 = 0;
static DWORD a_68E680 = 0;
static DWORD a_6AF000 = 0;
static DWORD a_7EB000 = 0;
static DWORD a_18FABD0 = 0;
static DWORD a_190E0A8 = 0;
static DWORD a_2350424 = 0;
static DWORD a_2350426 = 0;
static DWORD a_2350F42 = 0;
static DWORD a_26AA55C = 0;
static DWORD a_26AA56C = 0;
static DWORD a_2CAD98C = 0;
static DWORD a_2CAD990 = 0;
static DWORD a_2CAD994 = 0;
static DWORD a_2CAD998 = 0;
static DWORD a_2FACDF8 = 0;
static DWORD a_2FCDA04 = 0;
static DWORD a_300DC9C = 0;
static DWORD a_300DCA0 = 0;
static DWORD a_300DCA8 = 0;
static DWORD a_300DCAC = 0;
static DWORD a_300DCB0 = 0;
static DWORD a_305A63C = 0;
static DWORD a_305BC74 = 0;
static DWORD a_305C474 = 0;
static DWORD a_305D5FC = 0;
static DWORD a_307D5FC = 0;
static DWORD a_34651D8 = 0;
static DWORD a_3467010 = 0;


// ============================================================================
// CS subsystem config — SINGLE master switch. Validated stack = CS_ENABLE 1.
//   1 = ON  : 5 CS detours + flip stage 2a (relocated 0x1000 table, model block @0xBF0)
//             + model precache cap 1024.
//   0 = OFF : vanilla CS table & behaviour (kill-switch / A-B baseline).
// The 3 gates below DERIVE from the master — kept for structure + surgical A-B (edit ONE
// line: CSF_MODEL_1024 -> 0 runs stage 2a at model cap 512 ; CS_FLIP_STAGE -> 1 is the
// pure-relocation stepping-stone). LIVE toggles, NOT dead code.
// ============================================================================
#define CS_ENABLE 1

// The 3 gates DERIVE from the master CS_ENABLE (final form). SEND/PARSE are coop-verified faithful
// (lastEntityRef fix, 2026-06-10); GET/SET/CLEAR re-enabled below; flip stage 2 + model-cap 1024 give
// the >512-model precache. For a surgical A-B, override ONE gate by hand:
//   CSF_MODEL_1024 -> 0 : stage 2a relocation at the vanilla model cap (512).
//   CS_FLIP_STAGE  -> 1 : pure-relocation stepping-stone (no model block move).
//   CS_DETOURS     -> 0 : detours off while keeping the rest (kill-switch).
#define CS_DETOURS     CS_ENABLE
#define CS_FLIP_STAGE  (CS_ENABLE ? 2 : 0)
#define CSF_MODEL_1024 CS_ENABLE




// ============================================================================
// Globaux PARTAGÉS config-string (retargetables ; défaut = valeurs vanilla).
// La bascule finale ne touchera QUE ces 3 variables + l'allocation des nouvelles
// tables ; toutes les reconstructions ci-dessous les utilisent.
// ============================================================================
extern "C" unsigned short* g_csTable    = nullptr; // word_2350426
extern "C" unsigned short* g_csSentinel = nullptr; // word_2350424
extern "C" int             g_maxCS      = 0xBF0;                        // MAX_CONFIGSTRINGS
// (next to g_csTable / g_csSentinel / g_maxCS, OUTSIDE the namespace)
// Client-side CS byte-offset store base (dword_305A63C): per-CS offset into the
// 0x20000 CS blob byte_305D5FC. Made shared so the eventual flip can retarget it.
extern "C" int* g_clientCsOffsets = nullptr;          // dword_305A63C
// Companion CS-text side-table bases — ALSO relocated by the flip stage 2, so the detoured
// CL_ParseGamestate must read them at runtime (never hardcode), like g_csTable (B3 fix).
extern "C" char* g_csData   = nullptr;               // byte_305D5FC (0x20000 blob)
extern "C" int*  g_csCursor = nullptr;               // dword_307D5FC write cursor

// Repointé par le flip Stage 1 vers le bloc modèle in-table relogé (= g_csTable + 0x58E).
// Défini dans PatchT4MAM_ModelIndex.cpp ; lu par la reconstruction G_ModelIndex (son scan).
extern "C" unsigned short* g_modelHashTbl;
// Base config-string du bloc modèle pour G_ModelIndex (0x58E vanilla, 0xBF0 après flip stage 2).
extern "C" int g_modelCsBase;
// Stage 2b: model XModel* table + cap, owned by PatchT4MAM_ModelIndex.cpp. The flip (CSF_MODEL_1024)
// relocates dword_190E0A8 to a 1024 buffer and raises the cap; defaults = vanilla (0x0190E0A8 / 512).
extern "C" void** g_modelPtrTbl;
extern "C" int    g_maxModels;
// Statut du flip, loggé une fois au runtime (1er SV_ClearConfigstrings) — jamais en init.
static char g_csFlipStatus[96] = "CS flip stage1: disabled";


using namespace T4::engine;


namespace T4_Reconstructed
{
    // String pool base = T4::engine::gScrMemTreePub->mt_buffer (dword_3702390). CS text = base + handle*0xC + 4.

    static const char* cs_assertToken = nullptr;// unk_887664 (length-prefixed)
    static const char  cs_empty[1] = { 0 };                            // chaîne vide locale

    // --- helpers vanilla (par VA) -------------------------------------------
    typedef void (__cdecl* StrncpyZ_t)(char* dst, const char* src, int count); // sub_7AA9C0
    static StrncpyZ_t cs_strncpyz = nullptr;
    typedef void (__cdecl* ComMemset_t)(void* dst, int fill, int dwordCount);  // sub_5E5100
    static ComMemset_t cs_memsetDwords = nullptr;
    typedef int  (__cdecl* SL_Intern_t)(int a, const char* str, int c, int len);
    static SL_Intern_t SL_Intern_Low = nullptr;// idx < 0x585
    static SL_Intern_t SL_Intern_High = nullptr;// idx >= 0x585
    typedef char* (__cdecl* Va_t)(const char* fmt, ...);                       // sub_5F6D80
    static Va_t cs_va = nullptr;

    static int* p_sv_state = nullptr;// ==2 => SV actif
    static int* p_sv_running = nullptr;
    static unsigned char* g_clientsBase = nullptr;// dword_2547090
    static const int CLIENT_STRIDE  = 0x58D30;
    static const char* ERR_BAD_INDEX = nullptr;// "SV_SetConfigstring: bad index %i"
    static const char* ERR_BIG_BUNDLE = nullptr;// "…big config string…"
    #define CS_STRPOOL ((unsigned char*)T4::engine::gScrMemTreePub->mt_buffer)
    static unsigned char** p_23D5C30 = nullptr;
    #define CS_SVS_PTR (*p_23D5C30)

    // ========================================================================
    // @faithful  SV_GetConfigstring (sub_6315C0). __usercall esi=idx/edi=dstSize/dst@stack.
    // ========================================================================
    extern "C" void __cdecl SV_GetConfigstring(int idx, char* dst, int dstSize)
    {
        if (idx < 0 || idx >= g_maxCS)
            T4::engine::Com_Error(ERR_DROP, cs_assertToken, idx);            // puis fall-through (fidèle)
        
        unsigned int handle = (unsigned int)g_csTable[idx];
        if ((handle & 0xFFFF) == 0) { cs_strncpyz(dst, cs_empty, dstSize - 1); dst[dstSize - 1] = 0; return; }
        if (handle == 0)            { cs_strncpyz(dst, (const char*)0, dstSize - 1); dst[dstSize - 1] = 0; return; }

        const char* src = (const char*)(CS_STRPOOL + handle * 0xC + 4);
        cs_strncpyz(dst, src, dstSize - 1);
        dst[dstSize - 1] = 0;
    }
    __declspec(naked) void SV_GetConfigstring_Naked()
    {
        __asm {
            mov     eax, [esp+4]        // dst (arg_0)
            push    edi                 // dstSize
            push    eax                 // dst
            push    esi                 // idx
            call    SV_GetConfigstring
            add     esp, 0Ch
            retn
        }
    }

    // ========================================================================
    // thunk SL_RemoveRefToString (sub_68E680). __usercall(edx=handle, esi=poolSel).
    // PRÉSERVE esi (non-volatile cdecl). Réutilisé par reset + setter (pre-free).
    // ========================================================================
    __declspec(naked) void __cdecl SL_RemoveRefToString_thunk(unsigned short /*handle*/, int /*poolSel*/)
    {
        __asm {
            push    esi
            movzx   edx, word ptr [esp+8]     // handle (+4 pour push esi)
            mov     esi, [esp+0Ch]            // poolSel
            mov     eax, a_68E680
            call    eax
            pop     esi
            retn
        }
    }

    // ========================================================================
    // @faithful  SV_ClearConfigstrings (sub_631DA0). __cdecl void.
    // ========================================================================
    extern "C" void __cdecl SV_ClearConfigstrings(void)
    {
#if CS_FLIP_STAGE >= 1
        static bool s_flipLogged = false;
        if (!s_flipLogged) { s_flipLogged = true; T4::engine::Com_Printf(0, "[T4M] %s\n", g_csFlipStatus); }
#endif
        for (int i = 0; i < g_maxCS; ++i)
        {
            unsigned short handle = g_csTable[i];
            if (handle != 0) SL_RemoveRefToString_thunk(handle, 0);
        }
        unsigned short sentinel = *g_csSentinel;
        if (sentinel != 0) SL_RemoveRefToString_thunk(sentinel, 0);

        cs_memsetDwords((void*)p_sv_state, 0, 0x21806); // 0x86018 octets (gameState)
        *(int*)a_2FCDA04 = 0;

        // Flip : la table CS vit hors gameState → le memset ci-dessus ne la couvre plus.
        // Zéroter le buffer relogé (sentinelle + g_maxCS slots) comme le faisait le memset vanilla.
        if ((void*)g_csSentinel != (void*)a_2350424)
            memset(g_csSentinel, 0, 2 + g_maxCS * 2);
    }

    // ========================================================================
    // Thunks pour SV_SendServerCommand (sub_633FA0). Convention vanilla :
    //   eax = flag (0 = broadcast tous ; sinon = client ciblé)
    //   stack = (arg_0 = 1, arg_4 = fmt, arg_8.. = varargs)
    // Le push [esp+N] auto-décalé reconstruit la pile vanilla. ecx est scratch
    // (sub_633FA0 l'écrase dès l'entrée) → on l'utilise pour l'adresse d'appel,
    // ce qui laisse eax libre pour le flag.
    // ========================================================================
    __declspec(naked) void __cdecl SV_CS_Send_cxxs(int /*one*/, const char* /*fmt*/, int /*c*/, int /*idx*/, int /*len*/, const char* /*str*/)
    {
        __asm {
            push    [esp+18h]   // str
            push    [esp+18h]   // len
            push    [esp+18h]   // idx
            push    [esp+18h]   // c
            push    [esp+18h]   // fmt
            push    [esp+18h]   // one
            xor     eax, eax              // flag = 0 (broadcast)
            mov     ecx, a_633FA0
            call    ecx
            add     esp, 18h
            retn
        }
    }
    __declspec(naked) void __cdecl SV_CS_Send_cs(int /*one*/, const char* /*fmt*/, int /*c*/, const char* /*str*/)
    {
        __asm {
            push    [esp+10h]   // str
            push    [esp+10h]   // c
            push    [esp+10h]   // fmt
            push    [esp+10h]   // one
            xor     eax, eax              // flag = 0 (broadcast)
            mov     ecx, a_633FA0
            call    ecx
            add     esp, 10h
            retn
        }
    }
    __declspec(naked) void __cdecl SV_CS_Send_cs_target(void* /*target*/, int /*one*/, const char* /*fmt*/, int /*c*/, const char* /*str*/)
    {
        __asm {
            push    [esp+14h]   // str
            push    [esp+14h]   // c
            push    [esp+14h]   // fmt
            push    [esp+14h]   // one
            mov     eax, [esp+14h]        // flag = target (envoi ciblé)
            mov     ecx, a_633FA0
            call    ecx
            add     esp, 10h
            retn
        }
    }
    // Info_ValueForKey (sub_5F6DF0). __thiscall(ecx=info, stack key) -> char*.
    __declspec(naked) char* __cdecl SV_Info_ValueForKey(const char* /*info*/, const char* /*key*/)
    {
        __asm {
            mov     ecx, [esp+4]          // info -> this
            push    [esp+8]               // key
            mov     edx, a_5F6DF0
            call    edx
            add     esp, 4
            retn
        }
    }

    // ========================================================================
    // SV_SetConfigstrings (sub_6311E0). __cdecl(pairs[], count). Writer batch.
    // Reconstruction BEHAVIORALLY-faithful (flux propre, pas la structure goto
    // littérale — comportement vérifié contre l'asm). Détour direct __cdecl.
    // ========================================================================
    struct SV_CSPair { int index; const char* str; };

    extern "C" void __cdecl SV_SetConfigstrings(const SV_CSPair* pairs, int count)
    {
        char bigbuf[0x200];   // var_404 : accumulateur "%x %x %s"
        char fragbuf[0x200];  // var_604 : fragment de string longue
        char infobuf[0x204];  // var_204 : info per-client
        int  acc_len   = 0;   // var_614
        int  lastIndex = 0;   // var_618 (survit pour la phase flush)

        for (int n = 0; n < count; ++n)
        {
            int         idx = pairs[n].index;
            const char* str = pairs[n].str;
            lastIndex = idx;

            if (idx < 0 || idx >= g_maxCS)
                T4::engine::Com_Error(ERR_DROP, ERR_BAD_INDEX, idx);

            unsigned int oldHandle = g_csTable[idx];
            if (oldHandle == 0) continue;                 // slot jamais alloué → skip

            if (str == 0) str = cs_empty;

            // skip si le nouveau texte == l'ancien
            const char* oldText = (const char*)(CS_STRPOOL + oldHandle * 0xC + 4);
            if (strcmp(str, oldText) == 0) continue;

            // pré-libère l'ancien handle, internalise le nouveau, écrit la table
            SL_RemoveRefToString_thunk((unsigned short)oldHandle, 0);
            int lenPlus1 = (int)strlen(str) + 1;
            int newHandle = (idx < 0x585) ? SL_Intern_Low(0, str, 0, lenPlus1)
                                          : SL_Intern_High(0, str, 0, lenPlus1);
            g_csTable[idx] = (unsigned short)newHandle;

            // si serveur inactif → pas de broadcast
            if (*p_sv_state != 2 && *p_sv_running == 0) continue;

            int len = (int)strlen(str);
            if (len <= 0x1EF)
            {
                if (acc_len + len + 0x11 >= 0x200)
                {
                    SV_CS_Send_cs(1, "%c %s", 'd', bigbuf);
                    acc_len = 0;
                }
                acc_len += T4::engine::crt_sprintf(bigbuf + acc_len, "%x %x %s", idx, len, str); // FIX: idx,len,str
            }
            else
            {
                int remaining = len, offset = 0;
                while (remaining > 0)
                {
                    int chunk = 0x1EF;
                    if (str[offset + 0x1EF] == ' ')       // couper sur une frontière d'espace
                    {
                        const char* pb = str + offset;
                        do {
                            --chunk;
                            if (chunk == 0) 
                                T4::engine::Com_Error(ERR_DROP, ERR_BIG_BUNDLE, 0x1EF);
                        } while (pb[chunk] == ' ');
                    }
                    cs_strncpyz(fragbuf, str + offset, chunk);
                    fragbuf[chunk] = 0;

                    if (offset == 0)
                        SV_CS_Send_cxxs(1, "%c %x %x %s", 'x', idx, len, fragbuf);
                    else if (remaining > 0x1EF)
                        SV_CS_Send_cs(1, "%c %s", 'y', fragbuf);
                    else
                        SV_CS_Send_cs(1, "%c %s", 'z', fragbuf);

                    remaining -= chunk;
                    offset    += chunk;
                }
            }
        }

        // ---- flush final --------------------------------------------------
        if (acc_len == 0) return;

        if (lastIndex != 9 && lastIndex != 0x588 && lastIndex != 0xBB5)
        {
            SV_CS_Send_cs(1, "%c %s", 'd', bigbuf);       // broadcast à tous
            return;
        }

        // index spéciaux : envoi per-client avec clientNum injecté
        unsigned char* svs = CS_SVS_PTR;
        int numClients = *(int*)(svs + 0x10);
        if (numClients <= 0) return;

        const char* lastStr = (count > 0) ? pairs[count - 1].str : cs_empty;
        for (int i = 0; i < numClients; ++i)
        {
            unsigned char* client = g_clientsBase + (size_t)i * CLIENT_STRIDE;
            if (*(int*)client < 3) continue;

            { char* d = infobuf; const char* s = lastStr; while ((*d++ = *s++) != 0) {} }
            char* value = SV_Info_ValueForKey(infobuf, cs_va("%i", i));
            int vlen = (int)strlen(value);
            T4::engine::crt_sprintf(bigbuf, "%x %x \\%d\\%s", lastIndex, vlen + 3, i, value); // FIX: lastIndex,vlen+3,i,value
            SV_CS_Send_cs_target(client, 1, "%c %s", 'd', bigbuf);
        }
    }

// NOTE: everything below lives inside `namespace T4_Reconstructed { ... }`.

    using T4::engine::msg_s;

    // ---- send-side absolutes (NOT relocated) -------------------------------
    static unsigned int* p_46E5054 = nullptr;
    #define CS_HUNK_OFF (*p_46E5054)                  // dword_46E5054 (temp Hunk offset)
    static unsigned char* cs_hunkBase = nullptr;// dword_212B2F8
    static unsigned char* cs_boundsNodes = nullptr;// unk_2351C10
    static unsigned char* cs_boundsNodesEnd = nullptr;// unk_23BCC10
    static unsigned char* cs_entBaselines = nullptr;// unk_23BCC08
    static int* cs_entBaselineCount = nullptr;// dword_23D4AE4
    static int* cs_dword_234FC20 = nullptr;
    static const int CS_BOUNDS_STRIDE   = 0x1AC;
    static const int CS_BASELINE_STRIDE = 0x118;
    // Built-in CS static table base (dword_8D55F8): stride 0x10, [+0]/[+0xC]=csNum, [+4]=defaultStr.
    static unsigned char* cs_staticTable = nullptr;// dword_8D55F8

    // Com_DPrintf format strings. Content matched to vanilla data segment
    // (aSvSendclientga / aGoingFromCsCon / aSendingIBytesI). Literals used instead
    // of hardcoded data VAs (faithful content; only address differs -> no runtime
    // impact; avoids guessing relocated data offsets).
    static const char* const cs_fmt_for     = "SV_SendClientGameState() for %s\n";
    static const char* const cs_fmt_csconn  = "Going from CS_CONNECTED to CS_CLIENTLOADING for %s\n";
    static const char* const cs_fmt_sending = "Sending %i bytes in gamestate to client: %i\n";
    static const char* const cs_String      = "";                       // vanilla "offset String" = empty string

    // ---- send-side cdecl helper VAs ----------------------------------------
    typedef void (__cdecl* GsDPrintf_t)(int channel, const char* fmt, ...);      // sub_59A310
    static GsDPrintf_t gs_dprintf = nullptr;
    typedef void (__cdecl* GsSnapshot_t)(void);                                  // sub_639910
    static GsSnapshot_t gs_snapshot = nullptr;

    // ---- send-side naked thunks (NEW; distinct from existing cs_memsetDwords) ----

    // sub_7AFF40  Com_Memset = standard __cdecl memset(dst, fill, count). Call sites push
    // dst/fill/count then `add esp,0Ch`; MSG_Init's internal memset pushes esi(=msg) as the
    // dst STACK arg. NOT __usercall esi=dst — the send verify misread that pushed esi as a
    // register input. Cross-checked vs the parse-side pg_memset + call sites @0x1448/@0x4058D6.
    typedef void* (__cdecl* GsMemset_t)(void* dst, int fill, unsigned int count);
    static GsMemset_t gs_memset = nullptr;
    // sub_678330  ResetReliableFragBuffers. edi=netchan(client+0x14). bool al (ignored). retn.
    __declspec(naked) void __cdecl gs_resetFrag(void* /*netchan*/) {
        __asm {
            push edi
            mov  edi, [esp+8]            // netchan
            mov  eax, a_678330
            call eax
            pop  edi
            retn
        }
    }
    // sub_62F250  SV_DropClient. eax=client, stack=reason. retn N (callee pops 1 arg via add esp at caller -> cdecl). Caller cleans.
    __declspec(naked) void __cdecl gs_dropClient(void* /*client*/, const char* /*reason*/) {
        __asm {
            mov  eax, [esp+4]            // client
            push dword ptr [esp+8]       // reason
            mov  ecx, a_62F250
            call ecx
            add  esp, 4
            retn
        }
    }
    // sub_674C70  MSG_Init. esi=msg, stack=(data,maxsize). retn (callee cleans its 2 internal? no: caller add esp,8). Caller cleans.
    __declspec(naked) void __cdecl gs_msgInit(msg_s* /*msg*/, char* /*data*/, int /*maxsize*/) {
        __asm {
            push esi
            mov  esi, [esp+8]            // msg
            push dword ptr [esp+10h]     // maxsize (arg_4)
            push dword ptr [esp+10h]     // data    (arg_0)
            mov  eax, a_674C70
            call eax
            add  esp, 8
            pop  esi
            retn
        }
    }
    // sub_638520  SV_WritePendingReliable. eax=msg, esi=client. retn.
    __declspec(naked) void __cdecl gs_writeReliable(msg_s* /*msg*/, void* /*client*/) {
        __asm {
            push esi
            mov  eax, [esp+8]            // msg
            mov  esi, [esp+0Ch]          // client
            mov  ecx, a_638520
            call ecx
            pop  esi
            retn
        }
    }
    // sub_5F69E0  Q_stricmpn. eax=maxcount, edx=s1, stack=s2 -> int (0 == equal). Caller add esp,4.
    __declspec(naked) int __cdecl gs_stricmpn(int /*count*/, const char* /*s1*/, const char* /*s2*/) {
        __asm {
            mov  eax, [esp+4]            // count
            mov  edx, [esp+8]            // s1
            push dword ptr [esp+0Ch]     // s2
            mov  ecx, a_5F69E0
            call ecx
            add  esp, 4
            retn
        }
    }
    // sub_674D00  MSG_WriteBits. edx=msg, stack=(value,bits). Caller add esp,8.
    __declspec(naked) void __cdecl gs_writeBits(msg_s* /*msg*/, int /*value*/, int /*bits*/) {
        __asm {
            mov  edx, [esp+4]            // msg
            push dword ptr [esp+0Ch]     // bits  (pushed first in vanilla: push 0Ch then push esi)
            push dword ptr [esp+0Ch]     // value
            mov  ecx, a_674D00
            call ecx
            add  esp, 8
            retn
        }
    }
    // sub_674E00  MSG_WriteBit0 ("next index" marker). eax=msg. retn.
    __declspec(naked) void __cdecl gs_writeBit0(msg_s* /*msg*/) {
        __asm {
            mov  eax, [esp+4]
            mov  ecx, a_674E00
            call ecx
            retn
        }
    }
    // sub_675430  MSG_WriteString. edx=str, edi=msg. retn. NOTE: derefs [str] (NULL would crash).
    __declspec(naked) void __cdecl gs_writeString(msg_s* /*msg*/, const char* /*str*/) {
        __asm {
            push edi
            mov  edi, [esp+8]            // msg
            mov  edx, [esp+0Ch]          // str
            mov  eax, a_675430
            call eax
            pop  edi
            retn
        }
    }
    // sub_67AB80  MSG_WriteDeltaStruct. eax=from, ecx=to, stack=(msg, arg4, arg8). Caller add esp,0Ch. (D4)
    __declspec(naked) void __cdecl gs_writeDelta(void* /*from*/, void* /*to*/,
                                                 msg_s* /*msg*/, int /*arg4*/, int /*arg8*/) {
        __asm {
            mov  eax, [esp+4]            // from (= zeroed varBaseline)
            mov  ecx, [esp+8]            // to   (= node / baseline)
            push dword ptr [esp+14h]     // arg8 (0x1000)
            push dword ptr [esp+14h]     // arg4 (0)
            push dword ptr [esp+14h]     // msg
            mov  edx, a_67AB80
            call edx
            add  esp, 0Ch
            retn
        }
    }
    // sub_496050 / sub_5720F0 / sub_6AF000  block serializers. esi=msg. retn.
    __declspec(naked) void __cdecl gs_writeBlock_496050(msg_s* /*msg*/) {
        __asm { push esi
                mov  esi, [esp+8]
                mov  eax, a_496050
                call eax
                pop  esi
                retn }
    }
    __declspec(naked) void __cdecl gs_writeBlock_5720F0(msg_s* /*msg*/) {
        __asm { push esi
                mov  esi, [esp+8]
                mov  eax, a_5720F0
                call eax
                pop  esi
                retn }
    }
    __declspec(naked) void __cdecl gs_writeBlock_6AF000(msg_s* /*msg*/) {
        __asm { push esi
                mov  esi, [esp+8]
                mov  eax, a_6AF000
                call eax
                pop  esi
                retn }
    }
    // sub_6393F0  SV_Netchan_Transmit. esi=client, stack=(msg, 0). Caller add esp,8 (within the 0x18 caller cleanup).
    __declspec(naked) void __cdecl gs_transmit(void* /*client*/, msg_s* /*msg*/) {
        __asm {
            push esi
            mov  esi, [esp+8]            // client
            push 0                       // arg_4 = 0
            push dword ptr [esp+10h]     // arg_0 = msg
            mov  eax, a_6393F0
            call eax
            add  esp, 8
            pop  esi
            retn
        }
    }

    // ---- helpers: vanilla clientNum + inlined MSG byte/short/long writes -----
    static const int CS_CLIENT_STRIDE = 0x58D30;
    static inline int gs_clientNum(void* client) {
        return (int)(((unsigned char*)client - g_clientsBase) / CS_CLIENT_STRIDE);
    }
    static inline void gs_rawByte(msg_s* m, unsigned char v) {
        if (m->cursize < m->maxsize) { m->data[m->cursize] = (char)v; m->cursize += 1; }
        else                          m->overflowed = 1;
    }
    static inline void gs_rawLong(msg_s* m, int v) {
        if (m->cursize + 4 <= m->maxsize) { *(int*)(m->data + m->cursize) = v; m->cursize += 4; }
        else                                m->overflowed = 1;
    }
    static inline void gs_rawShort(msg_s* m, unsigned short v) {
        if (m->cursize + 2 <= m->maxsize) { *(unsigned short*)(m->data + m->cursize) = v; m->cursize += 2; }
        else                                m->overflowed = 1;
    }
    // 12-bit index emit: byte-align pad (loc_62F867/904) then WriteBits(idx, 12).
    static inline void gs_emitIndex(msg_s* m, int idx) {
        if ((m->bit & 7) == 0) {
            if (m->cursize >= m->maxsize) m->overflowed = 1;
            else { m->data[m->cursize] = 0; m->cursize += 1; m->bit = (m->cursize - 1) * 8; }
        }
        m->bit += 1;
        gs_writeBits(m, idx, 0x0C);                      // push 0Ch = 12-bit CS index (unchanged)
    }
    // strcmp-style 2-bytes/iteration compare used by loops 1/2 for idx < 0x585 (loc_62F811).
    static inline int gs_strcmp2(const char* a, const char* b) {
        for (;;) {
            if (a[0] != b[0]) return 1;
            if (a[0] == 0)    return 0;
            if (a[1] != b[1]) return 1;
            if (a[1] == 0)    return 0;
            a += 2; b += 2;
        }
    }
    static inline const char* gs_csText(unsigned short handle) {
        return handle ? (const char*)(CS_STRPOOL + (unsigned)handle * 0xC + 4)
                      : (const char*)0;                  // vanilla: ecx=0 when handle==0
    }

    // ========================================================================
    // @faithful  SV_SendClientGameState (sub_62F500). __cdecl(client*); caller
    // cleans (push client; call; add esp,N). Direct detour (no entry wrapper).
    // ========================================================================
    extern "C" void __cdecl SV_SendClientGameState(void* client)
    {
        unsigned char* cl = (unsigned char*)client;

        // temp Hunk alloc: save offset, reserve 0x20000 for the msg buffer.
        const unsigned int savedHunkOff = CS_HUNK_OFF;
        unsigned char*     msgData      = cs_hunkBase + savedHunkOff;
        CS_HUNK_OFF = savedHunkOff + 0x20000;

        // --- preamble: reset reliable fragment buffers (loc_62F536) ---------- (D1)
        if (*(int*)cl != 0) {
            for (;;) {
                if (*(int*)(cl + 0x50) == 0) break;          // [ebx+50h]==0 -> loc_62F5A4 (re-test at loop head)
                unsigned char* netchan = cl + 0x14;
                gs_resetFrag(netchan);                       // sub_678330 (bool ignored)
                if (*(int*)(netchan + 0x3C) == 0) {          // [edi+3Ch] re-read FROM MEMORY after the call
                    int sub = *(int*)netchan & 0xF;
                    unsigned char* frag = cl + 0x11624 + sub * 0x20DC;
                    gs_memset(frag, 0, 0x20DC);
                    int cn = gs_clientNum(cl);
                    *(int*)(frag + 0x20B8) = *(int*)(a_26AA55C + cn * 4);
                    *(int*)(frag + 0x20BC) = *(int*)(a_26AA56C + cn * 4);
                }
                if (*(int*)cl == 0) break;                   // [ebx]!=0 -> loc_62F536
            }
        }

        // --- EXE_NEEDSTATS reset + drop gate (loc_62F5A4) -------------------
        if (*(int*)(cl + 0x52BFC) != 0) {
            gs_memset(cl + 0x554D5, 0, 0x2000);
            *(unsigned char*)(cl + 0x574D5) = 0x7F;
        }
        if (*(unsigned char*)(cl + 0x574D5) != 0x7F) {
            gs_dropClient(cl, "EXE_NEEDSTATS");              // aExeNeedstats (plain reason string)
            CS_HUNK_OFF = savedHunkOff;                      // restore temp + retn
            return;
        }

        // --- snapshot + traces + state transition (loc_62F5F6) -------------
        gs_snapshot();                                       // sub_639910
        gs_dprintf(0x0F, cs_fmt_for,    cl + 0x11548);
        gs_dprintf(0x0F, cs_fmt_csconn, cl + 0x11548);
        *(int*)cl             = 3;                            // CS_CLIENTLOADING
        *(int*)(cl + 0x323F0) = 0;
        *(int*)(cl + 0x11100) = *(int*)(cl + 0x14);

        msg_s msg;
        gs_msgInit(&msg, (char*)msgData, 0x20000);           // sub_674C70
        msg.lastEntityRef = -1;                              // vanilla @0x62F148: lastEntityRef=-1 after MSG_Init
                                                             // (baseline entity numbers are delta-compressed
                                                             // against +0x24; MUST seed -1 or cmd3 numbers desync) (D6)

        // --- header ---------------------------------------------------------
        gs_rawLong(&msg, *(int*)(cl + 0x11140));             // reliableAcknowledge
        gs_writeReliable(&msg, cl);                          // sub_638520
        gs_rawByte(&msg, 1);
        gs_rawLong(&msg, *(int*)(cl + 0x110F0));             // reliableSequence
        gs_rawByte(&msg, 2);                                 // svc_configstrings

        // ====================================================================
        // LOOP 1 — count CS to send (loc_62F6E3 .. cmp esi,0BF0h)
        // ====================================================================
        int count = 0;
        unsigned char* st = cs_staticTable;                  // walker dword_8D55F8
        for (int i = 0; i < g_maxCS; ++i) {
            int staticCsNum = *(int*)(st);                   // [ebx]
            if (staticCsNum == i) {
                unsigned short handle = g_csTable[i];
                const char* live = gs_csText(handle);
                const char* def  = *(const char**)(st + 4); // [ebx+4]
                int diff = (i >= 0x585) ? (gs_stricmpn(0x7FFFFFFF, def, live) != 0)
                                        : gs_strcmp2(def, live);
                if (diff) ++count;
                st += 0x10;
            } else {
                if (g_csTable[i] != *g_csSentinel) ++count;  // dynamic CS
            }
        }
        gs_rawShort(&msg, (unsigned short)count);

        // ====================================================================
        // LOOP 2 — write each CS (loc_62F7AF .. cmp esi,0BF0h)
        // ====================================================================
        int prevWritten = -1;                                // var_17C
        unsigned char* st2 = cs_staticTable - 0x0C;          // off_8D55FC - 0x10  (D2)
        for (int i = 0; i < g_maxCS; ++i) {
            int staticCsNum = *(int*)(st2 + 0xC);            // [ebx+0Ch]
            if (staticCsNum == i) {
                st2 += 0x10;                                 // add ebx,10h (before string read)
                unsigned short bHandle = g_csTable[i];
                const char* live = gs_csText(bHandle);
                const char* def  = *(const char**)(st2);     // [ebx] (post-increment)
                int diff = (i >= 0x585) ? (gs_stricmpn(0x7FFFFFFF, def, live) != 0)
                                        : gs_strcmp2(def, live);
                if (!diff) {                                 // loc_62F836 jz loc_62F947 -> skip total (D3)
                    continue;
                }
                if (bHandle == *g_csSentinel) {              // diff && handle==sentinel: 12-bit + ""
                    gs_emitIndex(&msg, i);
                    prevWritten = i;
                    gs_writeString(&msg, cs_String);         // vanilla "offset String" (empty)
                    // falls into common path, which sees handle==sentinel -> continue
                }
                // diff && handle!=sentinel: fall into common path (loc_62F8A9)
            }

            // --- common path (loc_62F8A9) -----------------------------------
            unsigned short handle = g_csTable[i];
            if (handle == *g_csSentinel) continue;           // loc_62F947 skip
            const char* text = gs_csText(handle);            // ebx = text or NULL

            if (i == prevWritten + 1)
                gs_writeBit0(&msg);                          // sub_674E00 "next index" marker
            else
                gs_emitIndex(&msg, i);                       // pad-align + 12-bit index
            prevWritten = i;
            // @faithful: vanilla passes raw edx (ebx == text, possibly NULL when handle==0;
            // sub_675430 would deref [0]). The sentinel-skip above protects this in practice.
            gs_writeString(&msg, text);
        }

        // ====================================================================
        // cmd 3 — bounds tree nodes (delta vs zeroed baseline) — loc_62F972
        // ====================================================================
        unsigned char varBaseline[0x118];
        gs_memset(varBaseline, 0, 0x118);                    // var_118 memset
        for (unsigned char* node = cs_boundsNodes; node < cs_boundsNodesEnd; node += CS_BOUNDS_STRIDE) {
            if (*(int*)node == 0) continue;
            gs_rawByte(&msg, 3);
            gs_writeDelta(varBaseline, node, &msg, 0, 0x1000); // from=baseline, to=node (D4)
        }

        // ====================================================================
        // cmd 4 — entity baselines (delta vs zeroed baseline) — loc_62FA36
        // ====================================================================
        msg.lastEntityRef = -1;                              // vanilla @0x62F9E9->0x62FA36: reset before cmd4 loop
                                                             // (cmd4 baseline numbers delta-compress fresh from -1,
                                                             //  matching the PARSE's `lastRefEntity=-1` on cmd4) (D6)
        if (*cs_entBaselineCount > 0) {
            unsigned char* base = cs_entBaselines;
            for (int n = 0; n < *cs_entBaselineCount; ++n) {
                gs_rawByte(&msg, 4);
                gs_writeDelta(varBaseline, base, &msg, 0, 0x1000); // (D4)
                base += CS_BASELINE_STRIDE;
            }
        }

        // ====================================================================
        // cmd 0xB + clientNum + dword_234FC20 + 5/6/7 blocks + final 0xB
        // ====================================================================
        gs_rawByte(&msg, 0x0B);
        gs_rawLong(&msg, gs_clientNum(cl));
        gs_rawLong(&msg, *cs_dword_234FC20);
        gs_rawByte(&msg, 5);
        gs_writeBlock_496050(&msg);
        gs_rawByte(&msg, 6);
        gs_writeBlock_5720F0(&msg);
        gs_rawByte(&msg, 7);
        gs_writeBlock_6AF000(&msg);
        gs_rawByte(&msg, 0x0B);

        // ---- trace + transmit + snapshot bookkeeping (loc_62FBAC) ----------
        gs_dprintf(0x0F, cs_fmt_sending, msg.cursize, gs_clientNum(cl));
        gs_transmit(cl, &msg);                               // sub_6393F0

        *(int*)a_2CAD998 = *(int*)a_300DCA0;
        *(int*)a_2CAD98C = *(int*)a_300DCA8;
        *(int*)a_2CAD990 = *(int*)a_300DCAC;
        *(int*)a_300DCB0 = *(int*)a_2CAD994;
        *(int*)a_300DC9C = *(int*)a_2FACDF8;

        CS_HUNK_OFF = savedHunkOff;                          // restore temp
    }


    // ========================================================================
    //  CL_ParseGamestate (sub_64CAE0) support: opaque msg + tables + helpers.
    // ========================================================================
    // PARSE uses the same msg_s (0x2C) defined above. For a READ msg the official fields map:
    //   cursize (+0x14)   = split boundary (readcount < cursize -> read from `data`)
    //   splitSize (+0x18) = size of the post-split `splitData` region
    //   readcount (+0x1C) = current read cursor ; lastEntityRef (+0x24) = entity-number delta base.
    struct rec_csAuto_t { int index; const char* name; int unk; int pad; }; // stride 0x10
    static rec_csAuto_t* g_csAuto = nullptr;// dword_8D55F8

    // g_csData (blob base) is now a file-scope relocatable global (see top, B3 fix).
    #define G_CSCURSOR (*g_csCursor)                                        // dword_307D5FC write cursor (relocatable)

    static char* BASELINE_ENTITY_TABLE = nullptr;                       // unk_3122688 stride 0x118 (absolute)
    static char* BASELINE_CLIENT_TABLE = nullptr;                       // unk_3168570 stride 0x118 (absolute)

    static char** p_1F552DC = nullptr;
    #define G_1F552DC (*p_1F552DC)
    #define G_1F552FC ((char*)*T4::engine::dvar_singlethreadRender)
    static int* p_1F552C4 = nullptr;
    #define G_1F552C4 (*p_1F552C4)
    static int* p_307D6E0 = nullptr;
    #define G_307D6E0 (*p_307D6E0)
    static int* p_3010014 = nullptr;
    #define G_3010014 (*p_3010014)
    static int* p_302012C = nullptr;
    #define G_302012C (*p_302012C)
    static int* p_300FFEC = nullptr;
    #define G_300FFEC (*p_300FFEC)
    static int* p_301011C = nullptr;
    #define G_301011C (*p_301011C)
    #define G_2122B00 ((char*)*T4::engine::fs_game)
    static int* p_2122AFC = nullptr;
    #define G_2122AFC (*p_2122AFC)
    static int* p_4DA997C = nullptr;
    #define G_4DA997C (*p_4DA997C)
    static void* G_3058528 = nullptr;// byte_3058528 (0x301654)
    static void* G_48AE508 = nullptr;// qword_48AE508
    static void* G_48AE50C = nullptr;
    static void* G_48AE510 = nullptr;

    // unk_* are LENGTH-PREFIXED error tokens consumed by cs_error (sub_59AC50):
    // pass by VA (the IDA `unk_<VA>` name IS the address). Do NOT convert to C literals.
    static const char* STR_TOO_MANY_CS = nullptr;// unk_84F750 (db 15h,"configstring > M...")
    static const char* STR_CS_OVERFLOW = nullptr;// unk_88951C (length-prefixed)
    static const char* STR_BAD_ENT = nullptr;// unk_88C220 (length-prefixed)
    static const char* STR_BAD_CLI = nullptr;// unk_88C244 (length-prefixed)
    // aClParsegamesta / aFFF are PLAIN printf/scanf strings -> C literals (content faithful).
    static const char* const STR_BAD_CMD     = "CL_ParseGamestate: bad command byte %d\n"; // aClParsegamesta
    static const char* const STR_FMT_3F      = "%f %f %f";                  // aFFF

    // ---- parse cdecl helper VAs --------------------------------------------
    typedef void* (__cdecl* pg_memset_t)(void*, int, unsigned int);
    static pg_memset_t pg_memset = nullptr;// sub_7AFF40
    typedef void* (__cdecl* pg_memcpy_t)(void*, const void*, unsigned int);
    static pg_memcpy_t pg_memcpy = nullptr;// sub_7AFFC0
    typedef void  (__cdecl* pg_preload_t)(int, int);
    static pg_preload_t pg_5F0210 = nullptr;// sub_5F0210
    typedef void  (__cdecl* pg_void0_t)(void);
    // sub_474620: __usercall (index in ESI, vanilla `xor esi,esi` -> 0). NOT void(void).
    // Modeled as the CL_PG_ResetSnapshotSlot0 naked thunk (file scope) so ESI is set.
    static pg_void0_t pg_59E970 = nullptr;
    static pg_void0_t pg_64C890 = nullptr;
    static pg_void0_t pg_642A60 = nullptr;
    typedef void  (__cdecl* pg_arg1_t)(void*);
    static pg_arg1_t pg_4962F0 = nullptr;
    static pg_arg1_t pg_6AF0F0 = nullptr;
    typedef void  (__cdecl* pg_clreset_t)(int, int);
    static pg_clreset_t pg_5DE720 = nullptr;

    // ---- parse __usercall thunks (forward declarations; bodies defined at file
    //      scope after the namespace). extern "C" -> flat names; these declarations
    //      make the call sites in CL_ParseGamestate resolve correctly. ----
    extern "C" int   CL_PG_ReadLong(void* msg);          // sub_675560 (msg=ecx)
    extern "C" int   CL_PG_ReadShort(void* msg);         // sub_675500 (msg=eax)
    extern "C" int   CL_PG_ReadBit(void* msg);           // sub_675060 (msg=edx)
    extern "C" int   CL_PG_ReadBits(void* msg, int nb);  // sub_674F50 (msg=edx, push nb)
    extern "C" char* CL_PG_ReadString(void* msg);        // sub_6757A0 (msg=edx)
    extern "C" int   CL_PG_ReadByte(void* msg);          // sub_676690 (msg=ecx)
    extern "C" int   CL_PG_ReadEntityIdx(int nb, void* msg);                       // sub_676990 (edi=nb, esi=msg)
    extern "C" void  CL_PG_ReadDeltaBaseline(int idx, void* msg, int z, void* buf, void* table); // sub_6772E0
    extern "C" void  CL_PG_5EF390(int a, int b);         // sub_5EF390 (ecx=0)
    extern "C" void  CL_PG_ReadInit576(void* msg);       // sub_572210 (msg=esi)
    extern "C" void  CL_PG_ResetSnapshotSlot0(void);     // sub_474620 (__usercall esi=index; CL_ParseGamestate passes 0)

    // ========================================================================
    // @faithful  CL_ParseGamestate (sub_64CAE0). __cdecl(msg_t*); caller cleans
    // (push msg; call; add esp,4). Direct detour (no entry wrapper).
    // ========================================================================
    extern "C" void __cdecl CL_ParseGamestate(void* msgArg)
    {
        msg_s* msg = (msg_s*)msgArg;
        char readBuf[0x118];                                 // var_118

        pg_memset(readBuf, 0, 0x118);
        int cmd = 0xB;                                       // var_124 init

        if (G_1F552DC[0x10] == 0)
            pg_5F0210(0x1000, 0);

        CL_PG_ResetSnapshotSlot0();                          // sub_474620 (esi=0)
        G_3010014 = 0;
        if (G_307D6E0 == 0)
        {
            pg_memset(G_3058528, 0, 0x301654);
            // FLIP FIX (map-reload truncated names / wrong asset type): the vanilla memset above
            // zeroes [0x3058528,0x3359B7C), which USED to contain the client side-table (offsets
            // dword_305A63C, blob byte_305D5FC, cursor dword_307D5FC). The flip relocated those three
            // into the owned `side` buffer, so this memset no longer reaches them. Without a re-zero,
            // map 2 keeps map 1's stale per-CS offsets -> a slot the new gamestate never set still
            // reads offset!=0 -> precache treats it as "set", reads mid-blob (truncated name) and
            // routes it by its index range (e.g. vehicle block 0x88E..0x951 -> "Could not load
            // vehicle file"). Mirror the vanilla coverage for the relocated tables (offsets+blob+cursor).
            if ((void*)g_clientCsOffsets != (void*)a_305A63C)
                memset(g_clientCsOffsets, 0, 0x4000 + 0x20000 + 4);   // side buffer: offsets+blob+cursor
        }

        pg_59E970();

        msg->lastEntityRef = -1;                             // [ebx+24h] = -1
        *(long long*)G_48AE508 = 0;                          // movq qword_48AE508, 0
        *(int*)G_48AE510 = 0;
        G_302012C = CL_PG_ReadLong(msg);                     // sub_675560
        G_CSCURSOR = 1;                                      // dword_307D5FC = 1

        for (;;) {                                           // loc_64CB85
            int prevCmd     = cmd;                           // edx = var_124 loaded BEFORE read (D2)
            int splitThresh = msg->cursize;                  // [ebx+14h] (read-msg split boundary)
            int total       = msg->splitSize + splitThresh;  // [ebx+18h]+[ebx+14h]
            int rc          = msg->readcount;                // [ebx+1Ch]

            if (rc >= total) {                               // jge loc_64CEAA
                msg->overflowed = 1;
                cmd = -1;
                goto bad_command;
            }

            // inline MSG_ReadByte
            int b;
            if (rc < splitThresh)                            // jl loc_64CBAB -> [ebx+8]
                b = (unsigned char)msg->data[rc];
            else                                             // fall-through -> [ebx+0Ch]-thresh
                b = (unsigned char)(msg->splitData - splitThresh)[rc];
            msg->readcount = rc + 1;
            cmd = b;

            if (cmd == 0xB)
                goto end_of_gamestate;

            if (cmd == 2) {                                  // configstrings
                int autoRow = 0;                             // ebp
                int csIndex = -1;                            // esi = 0xFFFFFFFF
                int count   = CL_PG_ReadShort(msg);          // sub_675500 -> var_120 (D1: ONLY a short)
                if (count == 0)
                    goto cs_after_batch;                     // jz loc_64CD46

                do {                                         // loc_64CBE6
                    if (CL_PG_ReadBit(msg))                  // sub_675060
                        csIndex = csIndex + 1;
                    else
                        csIndex = CL_PG_ReadBits(msg, 0xC);  // sub_674F50, 12 bits on the wire

                    if (csIndex < 0 || csIndex >= g_maxCS) { // was cmp esi,0BF0h
                        T4::engine::Com_Error(ERR_DROP, STR_TOO_MANY_CS);        // sub_59AC50
                        /* edi reloaded from dword_307D5FC; cursor unchanged */
                    }

                    // loc_64CC2D: flush auto rows whose index < csIndex (write default names)
                    while (g_csAuto[autoRow].index != 0 &&
                           g_csAuto[autoRow].index < csIndex) {
                        const char* name = g_csAuto[autoRow].name;
                        int len = (int)strlen(name);
                        g_clientCsOffsets[g_csAuto[autoRow].index] = G_CSCURSOR;
                        pg_memcpy(g_csData + G_CSCURSOR, name, len + 1);
                        G_CSCURSOR = G_CSCURSOR + len + 1;
                        ++autoRow;
                    }
                    // loc_64CCAA: consume matching auto row (skip storing its name)
                    if (g_csAuto[autoRow].index == csIndex)
                        ++autoRow;

                    // loc_64CCBA: read + store the configstring text
                    char* str = CL_PG_ReadString(msg);       // sub_6757A0
                    int slen = (int)strlen(str);
                    if (G_CSCURSOR + slen + 1 > 0x20000) {   // cmp ecx,20000h; jle
                        T4::engine::Com_Error(ERR_DROP, STR_CS_OVERFLOW);        // sub_59AC50
                        /* edi reloaded */
                    }
                    g_clientCsOffsets[csIndex] = G_CSCURSOR;
                    pg_memcpy(g_csData + G_CSCURSOR, str, slen + 1);
                    G_CSCURSOR = G_CSCURSOR + slen + 1;
                } while (--count != 0);                      // sub var_120,1; jnz

            cs_after_batch:                                  // loc_64CD46
                while (g_csAuto[autoRow].index != 0) {       // flush trailing auto rows
                    const char* name = g_csAuto[autoRow].name;
                    int len = (int)strlen(name);
                    g_clientCsOffsets[g_csAuto[autoRow].index] = G_CSCURSOR;
                    pg_memcpy(g_csData + G_CSCURSOR, name, len + 1);
                    G_CSCURSOR = G_CSCURSOR + len + 1;
                    ++autoRow;
                }
                // loc_64CDB6: parse mapCenter "%f %f %f" from CS slot 12 (dword_305A66C)
                {
                    int off = g_clientCsOffsets[12];
                    T4::engine::Com_sscanf(g_csData + off, STR_FMT_3F, G_48AE508, G_48AE50C, G_48AE510);
                }
                continue;                                    // jmp loc_64CB85
            }
            else if (cmd == 3) {                             // baseline entity
                int nb  = cmd + 7;                           // lea edi,[ecx+7] = 0xA
                int idx = CL_PG_ReadEntityIdx(nb, msg);      // sub_676990
                if (idx < 0 || idx >= 0x400)                 // absolute bound (NOT relocated)
                    T4::engine::Com_Error(ERR_DROP, STR_BAD_ENT, idx);
                char* table = BASELINE_ENTITY_TABLE + idx * 0x118;
                CL_PG_ReadDeltaBaseline(idx, msg, 0, readBuf, table); // sub_6772E0
                continue;
            }
            else if (cmd == 4) {                             // baseline client
                if (prevCmd != 4)                            // cmp edx,ecx; jz skip
                    msg->lastEntityRef = -1;                 // [ebx+24h] = -1
                int idx = CL_PG_ReadEntityIdx(0xA, msg);
                if (idx < 0 || idx > 0x15E)                  // jle -> bound is <= 0x15E
                    T4::engine::Com_Error(ERR_DROP, STR_BAD_CLI, idx);
                char* table = BASELINE_CLIENT_TABLE + idx * 0x118;
                CL_PG_ReadDeltaBaseline(idx, msg, 0, readBuf, table);
                continue;
            }
            else {
            bad_command:                                     // loc_64CEB8
                T4::engine::Com_PrintError(1, STR_BAD_CMD, cmd);     // sub_59A380
                msg->overflowed = 1;                         // [ebx]=1
                msg->cursize    = msg->readcount;            // [ebx+14h]=[ebx+1Ch]
                msg->splitSize  = 0;                         // [ebx+18h]=0
                return;
            }
        }

    end_of_gamestate:                                        // loc_64CEEA
        G_300FFEC = CL_PG_ReadLong(msg);                     // sub_675560
        G_301011C = CL_PG_ReadLong(msg);                     // sub_675560

        if (G_1F552FC[0x10] != 0) {
            T4::engine::Sys_SyncDatabase();
            T4::engine::DB_PostLoadXZone();
            T4::engine::Sys_WakeDatabase();
            T4::engine::DB_WaitForPendingLoads();
            T4::engine::DB_CheckPendingComplete();
        }

        pg_64C890();                                         // loc_64CF26
        {
            char* g  = G_2122B00;
            int flag = (unsigned char)g[0x0B];               // movzx ecx, byte [eax+0Bh]
            G_4DA997C |= flag;
            if (G_1F552DC[0x10] == 0) {                       // demo / non-listen path
                int sid = G_301011C;
                if (g[0x0B] != 0 || sid != G_2122AFC)
                    pg_5DE720(0, sid);                        // sub_5DE720(0, serverId)
            }
        }

        pg_642A60();                                          // loc_64CF64
        CL_PG_ReadByte(msg);                                  // sub_676690 resync (ret ignored)
        pg_4962F0(msg);
        CL_PG_ReadByte(msg);                                  // sub_676690
        CL_PG_ReadInit576(msg);                               // sub_572210
        CL_PG_ReadByte(msg);                                  // sub_676690
        pg_6AF0F0(msg);
        CL_PG_5EF390(G_1F552C4, 0);                           // sub_5EF390 (ecx=0; arg0=serverId, arg1=0)
    }

// (end of code that lives INSIDE namespace T4_Reconstructed — closing brace `}` is the file's existing one.)


} // namespace T4_Reconstructed
//   parse __usercall naked thunks. extern "C" -> flat linker names so the body
//   above (inside the namespace) resolves them.
extern "C" {

// sub_675560  MSG_ReadLong : msg in ECX -> EAX
__declspec(naked) int CL_PG_ReadLong(void* /*msg*/) {
    __asm { mov ecx, [esp+4]
            mov eax, a_675560
            jmp eax }                       // tail-call: callee retn, no args -> safe
}
// sub_675500  MSG_ReadShort : msg in EAX -> EAX (signed short)
__declspec(naked) int CL_PG_ReadShort(void* /*msg*/) {
    __asm { mov eax, [esp+4]
            mov edx, a_675500
            jmp edx }
}
// sub_675060  MSG_ReadBit : msg in EDX -> EAX
__declspec(naked) int CL_PG_ReadBit(void* /*msg*/) {
    __asm { mov edx, [esp+4]
            mov eax, a_675060
            jmp eax }
}
// sub_674F50  MSG_ReadBits(numBits) : msg in EDX, numBits pushed -> EAX (caller add esp,4)
__declspec(naked) int CL_PG_ReadBits(void* /*msg*/, int /*nb*/) {
    __asm { mov edx, [esp+4]                // msg
            push dword ptr [esp+8]          // nb
            mov eax, a_674F50
            call eax
            add esp, 4
            retn }
}
// sub_6757A0  MSG_ReadString : msg in EDX -> char* (static buffer)
__declspec(naked) char* CL_PG_ReadString(void* /*msg*/) {
    __asm { mov edx, [esp+4]
            mov eax, a_6757A0
            jmp eax }
}
// sub_676690  MSG_ReadByte : msg in ECX -> EAX
__declspec(naked) int CL_PG_ReadByte(void* /*msg*/) {
    __asm { mov ecx, [esp+4]
            mov eax, a_676690
            jmp eax }
}
// sub_676990  MSG_ReadEntityIndex(nb, msg) : edi=nb, esi=msg (esi ambient R/W [esi+24h]) -> EAX
__declspec(naked) int CL_PG_ReadEntityIdx(int /*nb*/, void* /*msg*/) {
    __asm { push esi
            push edi
            mov  edi, [esp+0Ch]             // nb  (after 2 pushes)
            mov  esi, [esp+10h]             // msg
            mov  eax, a_676990
            call eax
            pop  edi
            pop  esi
            retn }
}
// sub_6772E0  ReadDeltaBaseline(idx, msg, z, buf, table) : eax=idx, ecx=msg,
//   stack push order = z(arg_0), buf(arg_4), table(arg_8). Caller add esp,0Ch.
//   sig: (idx@+4, msg@+8, z@+0xC, buf@+0x10, table@+0x14).
__declspec(naked) void CL_PG_ReadDeltaBaseline(int /*idx*/, void* /*msg*/, int /*z*/, void* /*buf*/, void* /*table*/) {
    __asm { mov  eax, [esp+4]               // idx
            mov  ecx, [esp+8]               // msg
            push dword ptr [esp+14h]        // table (orig +0x14)          -> arg_8 (pushed first)
            push dword ptr [esp+14h]        // buf   (orig +0x10, now +0x14)-> arg_4
            push dword ptr [esp+14h]        // z     (orig +0x0C, now +0x14)-> arg_0
            mov  edx, a_6772E0
            call edx
            add  esp, 0Ch
            retn }
}
// sub_5EF390 : ecx=0, stack=(a@arg_0, b@arg_4) ; vanilla pushes b then a. Caller add esp,8.
__declspec(naked) void CL_PG_5EF390(int /*a*/, int /*b*/) {
    __asm { xor  ecx, ecx
            push dword ptr [esp+8]          // b (arg_4) pushed first
            push dword ptr [esp+8]          // a (arg_0)
            mov  eax, a_5EF390
            call eax
            add  esp, 8
            retn }
}
// sub_572210 : msg in ESI (ambient) -> void
__declspec(naked) void CL_PG_ReadInit576(void* /*msg*/) {
    __asm { push esi
            mov  esi, [esp+8]
            mov  eax, a_572210
            call eax
            pop  esi
            retn }
}
// sub_474620  ResetClientSnapshotSlot : index in ESI (ambient). CL_ParseGamestate passes 0
//   (vanilla loc_64CB28 `xor esi,esi`). sub_474620 ends in a bare retn (no stack args).
//   push/pop esi preserves the caller's ESI (cdecl correctness); call+retn keeps it stack-safe.
__declspec(naked) void CL_PG_ResetSnapshotSlot0(void) {
    __asm { push esi
            xor  esi, esi
            mov  eax, a_474620
            call eax
            pop  esi
            retn }
}

} // extern "C"

// (The parse thunks above are forward-declared inside namespace T4_Reconstructed,
//  right before CL_ParseGamestate, so the in-namespace call sites resolve to these
//  flat extern "C" symbols.)


// ===========================================================================
// FLIP — staged relocation of the config-string table into OWNED buffers.
//   Stage 1: pure relocation (g_maxCS=0xBF0, model block stays at offset 0x58E)
//            -> vanilla behaviour, validates the repointing machinery.
//   Stage 2: widen g_maxCS to 0x1000, re-base the model block to offset 0xBF0
//            (in-table, model cap still 512), and relocate the CS-index companions
//            (model-map dword_34651D8 + read-alias dword_3466810 ; client side-table
//            struct dword_305A63C / byte_305D5FC / cursor dword_307D5FC).
//   Every immediate/count is exe-verified — analysis/cs_flip_patch_sites_2026_06_03.md
//   + analysis/cs_flip_companion_oob_audit_2026_06_03.md. All-or-nothing: verify all,
//   then write; abort -> vanilla intact.
// ===========================================================================
#if CS_FLIP_STAGE >= 1

#define CSF_TEXT_LO ((DWORD)a_401000)
#define CSF_TEXT_HI ((DWORD)a_7EB000)// .text end (VSize 0x3EA000)
#define CSF_VANILLA_BASE ((DWORD)a_2350426)
#define CSF_VANILLA_MODELBLK ((DWORD)a_2350F42)// = base + 0x58E*2
static const int   CSF_MODELBLK_REFS    = 21;            // word_2350F42 .text refs

#if CS_FLIP_STAGE >= 2
static const DWORD CSF_MAXCS     = 0x1000;
static const DWORD CSF_MODEL_OFF = 0xBF0;    // model block re-based to the new top (entries)
#else
static const DWORD CSF_MAXCS     = 0xBF0;
static const DWORD CSF_MODEL_OFF = 0x58E;    // model block stays at vanilla offset
#endif

// Model block span (entries): 512, or 1024 with stage 2b. Drives the model-map read-window END and
// the status line; defined for all stages so the status sprintf compiles regardless of CS_FLIP_STAGE.
#if CSF_MODEL_1024
static const DWORD CSF_MODEL_COUNT = 0x400;
#else
static const DWORD CSF_MODEL_COUNT = 0x200;
#endif

// Table base/sentinel/sub-block immediates -> g_csTable + (sym - base). 27 sites, both stages.
#define RA(n) ((DWORD)T4M::GetAddress(n))   // resolve a flip table name -> variant VA
struct CsfSite { const char* immVA; const char* sym; };
static const CsfSite kCsfSites[] = {
    { "csf_imm_54A9E7", "word_2351342" }, { "csf_imm_54AAC4", "word_2351342" }, { "csf_imm_54A868", "word_2351542" },
    { "csf_imm_525DC7", "word_23516CA" }, { "csf_imm_4EFF84", "word_23518CA" }, { "csf_imm_54A934", "word_23518CA" },
    { "csf_imm_54A3E4", "word_235192A" }, { "csf_imm_54A7C4", "word_2351A6A" },
    { "csf_imm_4F3B28", "word_2350438" }, { "csf_imm_525752", "word_2350F32" }, { "csf_imm_527579", "word_2350F36" },
    { "csf_imm_5276F7", "word_2350F36" }, { "csf_imm_5278B8", "word_2350F36" }, { "csf_imm_527ABB", "word_2350F38" },
    { "csf_imm_527C28", "word_2351B8C" }, { "csf_imm_527DAB", "word_2351B8C" }, { "csf_imm_527EF8", "word_2351B8E" },
    { "csf_imm_52807B", "word_2351B8E" }, { "csf_imm_5281C8", "word_2351B90" }, { "csf_imm_52835B", "word_2351B90" },
    { "csf_imm_63246A", "word_2350424" }, { "csf_imm_63246F", "word_2350426" }, { "csf_imm_63248C", "word_2351C06" },
    { "csf_imm_631702", "word_2350426" }, { "csf_imm_631726", "word_2350424" }, { "csf_imm_63172D", "word_2350426" },
    { "strTable_base_54A20C", "word_2350428" },
};

#if CS_FLIP_STAGE >= 2
// Stage-2 fixed-immediate patches (verify old, write new). exe-verified.
struct CsfImm { const char* immVA; DWORD oldv; DWORD neu; };
static const CsfImm kCsfStage2Imm[] = {
    // A.1 — CS index bounds 0xBF0 -> 0x1000
    { "csf_imm_4596D9", 0x00000BF0, 0x00001000 }, { "csf_imm_63A926", 0x00000BF0, 0x00001000 },
    { "csf_imm_63A9A8", 0x00000BF0, 0x00001000 },
    // C — model offset 0x58E -> 0xBF0 (classifier disp is negative two's-complement)
    { "csf_imm_45947C", 0xFFFFFA72, 0xFFFFF410 }, // sub_459410 lea [esi-0x58E] -> [esi-0xBF0]
    { "csf_imm_510507", 0x0000058E, 0x00000BF0 }, // sub_5104F0 savegame restore offset
    { "csf_imm_510838", 0x0000058E, 0x00000BF0 }, // sub_510830 savegame table offset
    { "csf_imm_510992", 0x0000058E, 0x00000BF0 }, // sub_510990 savegame table offset
    // client side-table struct: offsets array grows 0x2FC0 -> 0x4000, total 0x22FC4 -> 0x24004.
    // VAs are in the serializer sub_63A7F0 / clear sub_641730 — exe-verified .text matches (B1 fix).
    { "csf_imm_63A8A8", 0x00002FC0, 0x00004000 }, // blob displacement (lea edi,[ebx+edx+2FC0h])
    { "csf_imm_63A807", 0x00022FC4, 0x00024004 }, { "csf_imm_63A84E", 0x00022FC4, 0x00024004 },
    { "csf_imm_63A861", 0x00022FC4, 0x00024004 }, { "csf_imm_6417B1", 0x00022FC4, 0x00024004 },
};
// Companion symbol relocations (scan-replace, count-asserted).
struct CsfReloc { const char* sym; int count; };
static const CsfReloc kCsfModelMap[] = { { "dword_34651D8", 1 } };          // model-map WRITE base only
static const CsfReloc kCsfSideTbl[]  = { { "dword_305A63C", 20 }, { "byte_305D5FC", 89 }, { "dword_307D5FC", 21 } };

// --- ALIAS GAP FIX (root cause of the stage-2a "Client/Server game mismatch" + downstream desync) ---
// The flip relocates each table by its BASE, but vanilla hardcodes many SUB-BLOCK alias addresses
// (base + idx*stride) as separate immediates. A base-only scan-replace misses them, so once the base
// is relocated they keep reading the STALE old table. The 2026-06-03 audit counted base refs only.
// Counts are exe-verified (symbol-prefixed grep of CoDWaW LanFixed.exe.asm).
struct CsfAlias { const char* addr; int count; };
// (1) Client CS offsets array (base 0x0305A63C, stride 4 = blob byte-offset per CS index):
//     gamename idx 2 (the mismatch trigger @sub_664570), mapCenter idx 12, VisionSet/category blocks,
//     and the MODEL block [0x58E,0x78E) which re-bases to CSF_MODEL_OFF like the server CS table.
static const CsfAlias kCsfOffAliases[] = {
    {"dword_305A640",1},{"dword_305A644",1},{"dword_305A654",2},{"dword_305A658",1},{"dword_305A65C",1},
    {"dword_305A660",1},{"dword_305A66C",3},{"dword_305A870",1},{"dword_305AA70",2},{"dword_305AA74",1},
    {"dword_305AB70",1},{"dword_305AC30",2},{"dword_305BC2C",1},{"dword_305BC54",1},{"dword_305BC58",1},
    {"dword_305BC5C",1},{"dword_305BC60",1},{"dword_305BC64",1},{"dword_305BC74",5},{"dword_305BC78",1},
    {"dword_305C474",10},{"dword_305C870",1},{"dword_305C874",1},{"dword_305C878",1},{"dword_305CB84",2},
    {"dword_305CF88",1},{"dword_305CFC4",6},{"dword_305D044",5},{"dword_305D048",1},{"dword_305D244",1},
    {"dword_305D264",2},{"dword_305D268",1},{"dword_305D284",1},{"dword_305D288",2},{"dword_305D2C4",7},
    {"dword_305D344",2},{"dword_305D348",1},{"dword_305D3C4",1},{"dword_305D3C8",1},{"dword_305D4C8",1},
    {"dword_305D508",2},{"dword_305D50C",1},{"dword_305D510",1},
};
#define CSF_OFF_BASE ((DWORD)a_305A63C)// = dword_305A63C (offsets array base)
#define CSF_OFF_MODLO ((DWORD)a_305BC74)// = offsets[0x58E] (model block start; re-base)
#define CSF_OFF_MODHI ((DWORD)a_305C474)// = offsets[0x78E] (model block end, exclusive)
// (2) Model-map (base 0x034651D8, stride 4) READ-window aliases inside the model block. The audit
//     handled only 0x3466810; all re-base to mmap + (CSF_MODEL_OFF + idx - 0x58E)*4.
static const CsfAlias kCsfMmapAliases[] = {
    {"dword_3466810",15},  // base + 0x58E*4 (model read-window start)
    {"dword_3466814", 1},  // base + 0x58F*4 (sub_6621B0 "server models" registration loop)
    {"dword_346685C", 1},  // base + 0x5A1*4
    {"dword_34669BC", 1},  // base + 0x5F9*4
};
#define CSF_MMAP_BASE ((DWORD)a_34651D8)// = dword_34651D8 (model-map base)

#if CSF_MODEL_1024
// Stage 2b model-table relocations (base scan-replace, count-asserted) + END pointer.
// NB: the render path (model-map dword_3466810, mmap) is already 0x1000 from 2a; the destructibles
// pool unk_47E87F0 is a SEPARATE limit and is intentionally NOT relocated here.
static const CsfReloc kCsfModelTbl[] = { { "dword_190E0A8", 9 }, { "dword_18FABD0", 3 } };  // XModel* + remap bases
// NB: 0x0190E8A8 (= base+512*4) is NOT the table END — it is a SEPARATE dummy "overflow" entity
// struct (fields at 190E9C8/CC...) that sub_54F1E0 returns when out of entity slots (coop path).
// It must STAY at its vanilla address (the relocated XModel* table no longer overlaps it). An
// earlier flip wrongly re-based it into modelPtrBuf -> sub_54F1E0 returned an OOB pointer ->
// crash on entity-field write [esi+0xE0] (coop-only; solo never hits the dummy path).
// Model count caps (verify old → write new). VAs exe-verified (patch-doc 2a §C) except 0x662A3E
// (sub_6621B0 server-models loop) byte-verified by the Phase-1 old-value check.
static const CsfImm kCsfModel1024Imm[] = {
    { "csf_imm_459481", 0x000001FF, 0x000003FF }, // sub_459410 classifier model range  (cmp eax,1FFh)
    { "csf_imm_662A3E", 0x000007FC, 0x00000FFC }, // sub_6621B0 server-models loop       (cmp edi,7FCh=0x1FF*4)
    { "csf_imm_51056A", 0x00000200, 0x00000400 }, // sub_5104F0 savegame restore cap     (cmp ebx,200h)
    { "csf_imm_510833", 0x00000200, 0x00000400 }, // sub_510830 savegame table count     (push 200h)
    { "csf_imm_510999", 0x00000200, 0x00000400 }, // sub_510990 savegame table count     (mov ecx,200h)
};
#endif
#endif

static int  CsfCountDword(DWORD v) {
    int c = 0;
    for (BYTE* p = (BYTE*)CSF_TEXT_LO; p < (BYTE*)(CSF_TEXT_HI - 3); ++p)
        if (*(DWORD*)p == v) ++c;
    return c;
}
static void CsfReplaceDword(DWORD oldv, DWORD neu) {
    for (BYTE* p = (BYTE*)CSF_TEXT_LO; p < (BYTE*)(CSF_TEXT_HI - 3); ++p)
        if (*(DWORD*)p == oldv) *(DWORD*)p = neu;
}
static inline DWORD CsfRebase(DWORD sym) { return (DWORD)g_csTable + (sym - CSF_VANILLA_BASE); }
static inline bool  CsfTextOk(DWORD va)  { return va >= CSF_TEXT_LO && va < CSF_TEXT_HI; } // C2 guard

static void SetupCsFlip()
{
    // ---- Phase 1: all-or-nothing read-only verify (no writes if anything is off) ----
    for (const CsfSite& s : kCsfSites)
        if (!CsfTextOk(RA(s.immVA)) || *(DWORD*)RA(s.immVA) != RA(s.sym)) {
            sprintf(g_csFlipStatus, "flip ABORT: %08X has %08X != %08X", (unsigned)RA(s.immVA),
                    (unsigned)(CsfTextOk(RA(s.immVA)) ? *(DWORD*)RA(s.immVA) : 0), (unsigned)RA(s.sym));
            return;
        }
    if (CsfCountDword(CSF_VANILLA_MODELBLK) != CSF_MODELBLK_REFS) {
        sprintf(g_csFlipStatus, "flip ABORT: modelblk count %d != %d",
                CsfCountDword(CSF_VANILLA_MODELBLK), CSF_MODELBLK_REFS);
        return;
    }
#if CS_FLIP_STAGE >= 2
    for (const CsfImm& s : kCsfStage2Imm)
        if (!CsfTextOk(RA(s.immVA)) || *(DWORD*)RA(s.immVA) != s.oldv) {
            sprintf(g_csFlipStatus, "flip ABORT: imm %08X has %08X != %08X", (unsigned)RA(s.immVA),
                    (unsigned)(CsfTextOk(RA(s.immVA)) ? *(DWORD*)RA(s.immVA) : 0), (unsigned)s.oldv);
            return;
        }
    // B2: model-map read-window END operand (sub_45CDF0 loop @0x45CE1E). Relocated separately to a
    //     RUNTIME value below; the OTHER 8 refs of 0x03467010 are effects reads -> stay vanilla.
    if (*(DWORD*)a_45CE1E != a_3467010) {
        sprintf(g_csFlipStatus, "flip ABORT: mmap-end %08X != 03467010", (unsigned)*(DWORD*)a_45CE1E);
        return;
    }
    for (const CsfReloc& r : kCsfModelMap)
        if (CsfCountDword(RA(r.sym)) != r.count) {
            sprintf(g_csFlipStatus, "flip ABORT: mmap %08X count %d != %d",
                    (unsigned)RA(r.sym), CsfCountDword(RA(r.sym)), r.count);
            return;
        }
    for (const CsfReloc& r : kCsfSideTbl)
        if (CsfCountDword(RA(r.sym)) != r.count) {
            sprintf(g_csFlipStatus, "flip ABORT: side %08X count %d != %d",
                    (unsigned)RA(r.sym), CsfCountDword(RA(r.sym)), r.count);
            return;
        }
    for (const CsfAlias& a : kCsfOffAliases)
        if (CsfCountDword(RA(a.addr)) != a.count) {
            sprintf(g_csFlipStatus, "flip ABORT: off-alias %08X count %d != %d",
                    (unsigned)RA(a.addr), CsfCountDword(RA(a.addr)), a.count);
            return;
        }
    for (const CsfAlias& a : kCsfMmapAliases)
        if (CsfCountDword(RA(a.addr)) != a.count) {
            sprintf(g_csFlipStatus, "flip ABORT: mmap-alias %08X count %d != %d",
                    (unsigned)RA(a.addr), CsfCountDword(RA(a.addr)), a.count);
            return;
        }
#if CSF_MODEL_1024
    for (const CsfReloc& r : kCsfModelTbl)
        if (CsfCountDword(RA(r.sym)) != r.count) {
            sprintf(g_csFlipStatus, "flip ABORT: modeltbl %08X count %d != %d",
                    (unsigned)RA(r.sym), CsfCountDword(RA(r.sym)), r.count);
            return;
        }
    for (const CsfImm& s : kCsfModel1024Imm)
        if (!CsfTextOk(RA(s.immVA)) || *(DWORD*)RA(s.immVA) != s.oldv) {
            sprintf(g_csFlipStatus, "flip ABORT: modelcap %08X has %08X != %08X", (unsigned)RA(s.immVA),
                    (unsigned)(CsfTextOk(RA(s.immVA)) ? *(DWORD*)RA(s.immVA) : 0), (unsigned)s.oldv);
            return;
        }
#endif
#endif

    // ---- Phase 2: allocate ALL owned buffers + null-check BEFORE committing any global (B4) ----
    BYTE* buf = (BYTE*)VirtualAlloc(NULL, 0x4000, MEM_COMMIT | MEM_RESERVE, PAGE_READWRITE);
#if CS_FLIP_STAGE >= 2
    //   model-map dword_34651D8 -> 0x1000*4 ; side-table [offsets 0x4000][blob 0x20000][cursor 4] = 0x24004.
    BYTE* mmap = (BYTE*)VirtualAlloc(NULL, 0x1000 * 4, MEM_COMMIT | MEM_RESERVE, PAGE_READWRITE);
    BYTE* side = (BYTE*)VirtualAlloc(NULL, 0x24004,    MEM_COMMIT | MEM_RESERVE, PAGE_READWRITE);
    if (!buf || !mmap || !side) { strcpy(g_csFlipStatus, "flip ABORT: VirtualAlloc failed"); return; }
#if CSF_MODEL_1024
    BYTE* modelPtrBuf = (BYTE*)VirtualAlloc(NULL, 0x400 * 4, MEM_COMMIT | MEM_RESERVE, PAGE_READWRITE);
    BYTE* remapBuf    = (BYTE*)VirtualAlloc(NULL, 0x400 * 2, MEM_COMMIT | MEM_RESERVE, PAGE_READWRITE);
    if (!modelPtrBuf || !remapBuf) { strcpy(g_csFlipStatus, "flip ABORT: model VirtualAlloc failed"); return; }
#endif
#else
    if (!buf) { strcpy(g_csFlipStatus, "flip ABORT: VirtualAlloc failed"); return; }
#endif
    memcpy(buf, (void*)a_2350424, 2 + 0xBF0 * 2); // preserve current table at the flip moment

    g_csSentinel = (unsigned short*)buf;
    g_csTable    = (unsigned short*)(buf + 2);
    g_maxCS      = (int)CSF_MAXCS;

    // ---- Phase 3: commit .text patches (single unprotect; leave RWX per project memory) ----
    DWORD oldProt;
    VirtualProtect((LPVOID)CSF_TEXT_LO, CSF_TEXT_HI - CSF_TEXT_LO, PAGE_EXECUTE_READWRITE, &oldProt);
    for (const CsfSite& s : kCsfSites)
        *(DWORD*)RA(s.immVA) = CsfRebase(RA(s.sym));
    CsfReplaceDword(CSF_VANILLA_MODELBLK, (DWORD)g_csTable + CSF_MODEL_OFF * 2); // 21 model-block refs
    // ROOT-CAUSE FIX (stage-2a crash): kCsfSites re-based the SV_SpawnServer fill-loop END
    // (sub_631F20 @0x63248C) to g_csTable+0xBF0*2 (the OLD bound), so the loop never sentinel-
    // fills the re-based model block [0xBF0,0xDF0). Unfilled slots stay 0 -> SV_SetConfigstring
    // skips them (oldHandle==0) -> model names never written -> field_1A8 NULL -> Q_stricmpn crash.
    // Override to the FULL table so every slot (incl. the model block) gets the empty sentinel.
    *(DWORD*)a_63248C = (DWORD)g_csTable + CSF_MAXCS * 2;
#if CS_FLIP_STAGE >= 2
    for (const CsfImm& s : kCsfStage2Imm)
        *(DWORD*)RA(s.immVA) = s.neu;
    CsfReplaceDword(a_34651D8, (DWORD)mmap);                          // model-map base (WRITE)
    for (const CsfAlias& a : kCsfMmapAliases)                          // model-map READ-window aliases (re-based)
        CsfReplaceDword(RA(a.addr), (DWORD)mmap + (CSF_MODEL_OFF + (RA(a.addr) - CSF_MMAP_BASE) / 4 - 0x58E) * 4);
    *(DWORD*)a_45CE1E = (DWORD)mmap + (CSF_MODEL_OFF + CSF_MODEL_COUNT) * 4; // model-map read-window END (B2; 512 or 2b:1024)
    CsfReplaceDword(a_305A63C, (DWORD)side);                          // side-table offsets base
    for (const CsfAlias& a : kCsfOffAliases) {                         // offsets-array sub-block aliases
        DWORD idx = (RA(a.addr) - CSF_OFF_BASE) / 4;
        DWORD ni  = (RA(a.addr) >= CSF_OFF_MODLO && RA(a.addr) < CSF_OFF_MODHI)
                      ? (DWORD)(CSF_MODEL_OFF + idx - 0x58E) : idx;    // model block re-bases to CSF_MODEL_OFF
        CsfReplaceDword(RA(a.addr), (DWORD)side + ni * 4);
    }
    CsfReplaceDword(a_305D5FC, (DWORD)side + 0x4000);                 // side-table blob (shifted)
    CsfReplaceDword(a_307D5FC, (DWORD)side + 0x24000);                // side-table cursor
#if CSF_MODEL_1024
    CsfReplaceDword(a_190E0A8, (DWORD)modelPtrBuf);                   // model XModel* table base (9)
    // 0x0190E8A8 is the dummy overflow entity (see kCsfModelTbl note) — intentionally NOT relocated.
    CsfReplaceDword(a_18FABD0, (DWORD)remapBuf);                      // model remap table base (3)
    for (const CsfImm& s : kCsfModel1024Imm)
        *(DWORD*)RA(s.immVA) = s.neu;                                       // model count caps 512->1024
#endif
#endif
    // .text intentionally left RWX (feedback_dont_restore_text_protection)

    // ---- Phase 4: globals our reconstructions read at runtime follow the relocated buffers ----
    // C1: the vanilla 'lea ecx,[esi+0x58E]; call SV_SetConfigstring' inside G_ModelIndex (0x54A561)
    //     is left at 0x58E ON PURPOSE — it is dead under the sub_54A480 detour. The flip is INVALID
    //     without that detour, and depends on PatchT4.cpp calling ModelIndex BEFORE ConfigStrings.
    g_modelHashTbl = (unsigned short*)((DWORD)g_csTable + CSF_MODEL_OFF * 2);
#if CS_FLIP_STAGE >= 2
    g_modelCsBase     = (int)CSF_MODEL_OFF;       // G_ModelIndex issues SV_SetConfigstring at the new offset
    g_clientCsOffsets = (int*) side;              // B3: detoured CL_ParseGamestate writes the relocated...
    g_csData          = (char*)(side + 0x4000);   //     ...offsets, blob...
    g_csCursor        = (int*) (side + 0x24000);  //     ...and cursor (NOT the hardcoded vanilla bases).
#if CSF_MODEL_1024
    g_maxModels   = 0x400;                         // G_ModelIndex recon registers up to 1024 models
    g_modelPtrTbl = (void**)modelPtrBuf;           //   into the relocated XModel* table
#endif
#endif

    sprintf(g_csFlipStatus, "flip OK stage %d%s: table=%08X maxCS=%X model@%X x%X",
            (int)CS_FLIP_STAGE, (CSF_MODEL_COUNT == 0x400 ? "b" : ""),
            (unsigned)(DWORD)g_csTable, (unsigned)CSF_MAXCS, (unsigned)CSF_MODEL_OFF, (unsigned)CSF_MODEL_COUNT);
}
#endif // CS_FLIP_STAGE >= 1




// ---------------------------------------------------------------------------
// Debug: dump the LIVE config-string table (index, handle, len, text). Optional
// [start,end) range (default = all). Model-block entries are flagged '*'. Backs the
// "listconfigstrings" console command. Runtime only (reads g_csTable / string pool live).
// ---------------------------------------------------------------------------
extern "C" void __cdecl T4M_DumpConfigStrings(int start, int end)
{
    if (start < 0)                start = 0;
    if (end < 0 || end > g_maxCS) end   = g_maxCS;
    unsigned char* pool = CS_STRPOOL;   // = T4::engine::gScrMemTreePub->mt_buffer
    int count = 0, totalLen = 0, models = 0;
    for (int i = start; i < end; ++i) {
        unsigned short handle = g_csTable[i];
        if (handle == 0 || handle == *g_csSentinel) continue;        // empty slot
        const char* text = (const char*)(pool + (unsigned)handle * 0xC + 4);
        int len = (int)strlen(text);
        bool isModel = (i >= g_modelCsBase && i < g_modelCsBase + g_maxModels);
        if (isModel) ++models;
        T4::engine::Com_Printf(0, "CS[%04X]%c h=%04X len=%-3d : %s\n",
                               i, isModel ? '*' : ' ', handle, len, text);
        ++count; totalLen += len + 1;
    }
    T4::engine::Com_Printf(0, "[T4M] %d strings (%d model), %d text bytes | g_maxCS=%X model@%X cap=%d\n",
                           count, models, totalLen, (unsigned)g_maxCS, (unsigned)g_modelCsBase, g_maxModels);
}

// ---------------------------------------------------------------------------
// Install. Gated by the master CS_ENABLE switch. NO Com_Printf (init-time).
// ---------------------------------------------------------------------------

static void ResolveCsAddrs()
{
	using namespace T4_Reconstructed;   // voir les pointeurs typés (cs_*/gs_*/pg_*) résolus ci-dessous
	a_401000 = (DWORD)T4M::GetAddress("text_start");
	a_45CE1E = (DWORD)T4M::GetAddress("csf_imm_45CE1E");
	a_474620 = (DWORD)T4M::GetAddress("ResetClientSnapshotSlot");
	a_496050 = (DWORD)T4M::GetAddress("sub_496050");
	pg_4962F0 = (pg_arg1_t)T4M::GetAddress("sub_4962F0");
	a_5720F0 = (DWORD)T4M::GetAddress("sub_5720F0");
	a_572210 = (DWORD)T4M::GetAddress("sub_572210");
	gs_dprintf = (GsDPrintf_t)T4M::GetAddress("Com_DPrintf");
	pg_59E970 = (pg_void0_t)T4M::GetAddress("sub_59E970");
	pg_5DE720 = (pg_clreset_t)T4M::GetAddress("sub_5DE720");
	cs_memsetDwords = (ComMemset_t)T4M::GetAddress("sub_5E5100");
	a_5EF390 = (DWORD)T4M::GetAddress("sub_5EF390");
	pg_5F0210 = (pg_preload_t)T4M::GetAddress("sub_5F0210");
	a_5F69E0 = (DWORD)T4M::GetAddress("Q_stricmpn");
	cs_va = (Va_t)T4M::GetAddress("Com_FormatMsg");
	a_5F6DF0 = (DWORD)T4M::GetAddress("Info_ValueForKey");
	a_62F250 = (DWORD)T4M::GetAddress("SV_DropClient");
	a_62F500 = (DWORD)T4M::GetAddress("SV_SendClientGameState");
	a_6311E0 = (DWORD)T4M::GetAddress("SV_SetConfigstrings");
	a_6315C0 = (DWORD)T4M::GetAddress("SV_GetConfigstring");
	a_631DA0 = (DWORD)T4M::GetAddress("SV_ClearConfigstrings");
	a_63248C = (DWORD)T4M::GetAddress("csf_imm_63248C");
	a_633FA0 = (DWORD)T4M::GetAddress("SV_SendServerCommand");
	a_638520 = (DWORD)T4M::GetAddress("SV_WritePendingReliable");
	a_6393F0 = (DWORD)T4M::GetAddress("SV_Netchan_Transmit");
	gs_snapshot = (GsSnapshot_t)T4M::GetAddress("sub_639910");
	pg_642A60 = (pg_void0_t)T4M::GetAddress("sub_642A60");
	pg_64C890 = (pg_void0_t)T4M::GetAddress("sub_64C890");
	a_64CAE0 = (DWORD)T4M::GetAddress("CL_ParseGamestate");
	a_674C70 = (DWORD)T4M::GetAddress("MSG_Init");
	a_674D00 = (DWORD)T4M::GetAddress("MSG_WriteBits");
	a_674E00 = (DWORD)T4M::GetAddress("MSG_WriteBit0");
	a_674F50 = (DWORD)T4M::GetAddress("MSG_ReadBits");
	a_675060 = (DWORD)T4M::GetAddress("MSG_ReadBit");
	a_675430 = (DWORD)T4M::GetAddress("MSG_WriteString");
	a_675500 = (DWORD)T4M::GetAddress("MSG_ReadShort");
	a_675560 = (DWORD)T4M::GetAddress("MSG_ReadLong");
	a_6757A0 = (DWORD)T4M::GetAddress("MSG_ReadString");
	a_676690 = (DWORD)T4M::GetAddress("MSG_ReadByte");
	a_676990 = (DWORD)T4M::GetAddress("MSG_ReadEntityIndex");
	a_6772E0 = (DWORD)T4M::GetAddress("ReadDeltaBaseline");
	a_678330 = (DWORD)T4M::GetAddress("ResetReliableFragBuffers");
	a_67AB80 = (DWORD)T4M::GetAddress("MSG_WriteDeltaStruct");
	SL_Intern_Low = (SL_Intern_t)T4M::GetAddress("SL_GetStringOfSize");
	SL_Intern_High = (SL_Intern_t)T4M::GetAddress("SL_GetLowercaseStringOfLen");
	a_68E680 = (DWORD)T4M::GetAddress("SL_RemoveRefToString");
	a_6AF000 = (DWORD)T4M::GetAddress("sub_6AF000");
	pg_6AF0F0 = (pg_arg1_t)T4M::GetAddress("sub_6AF0F0");
	cs_strncpyz = (StrncpyZ_t)T4M::GetAddress("I_strncpyz");
	gs_memset = (GsMemset_t)T4M::GetAddress("Mem_Memset");
	pg_memset = (pg_memset_t)T4M::GetAddress("Mem_Memset");
	pg_memcpy = (pg_memcpy_t)T4M::GetAddress("Sys_MemCpy");
	a_7EB000 = (DWORD)T4M::GetAddress("dword_7EB000");
	STR_TOO_MANY_CS = (const char*)T4M::GetAddress("unk_84F750");
	ERR_BAD_INDEX = (const char*)T4M::GetAddress("dword_8875C8");
	ERR_BIG_BUNDLE = (const char*)T4M::GetAddress("dword_8875F0");
	cs_assertToken = (const char*)T4M::GetAddress("unk_887664");
	STR_CS_OVERFLOW = (const char*)T4M::GetAddress("unk_88951C");
	STR_BAD_ENT = (const char*)T4M::GetAddress("unk_88C220");
	STR_BAD_CLI = (const char*)T4M::GetAddress("unk_88C244");
	cs_staticTable = (unsigned char*)T4M::GetAddress("dword_8D55F8");
	g_csAuto = (rec_csAuto_t*)T4M::GetAddress("dword_8D55F8");
	a_18FABD0 = (DWORD)T4M::GetAddress("dword_18FABD0");
	a_190E0A8 = (DWORD)T4M::GetAddress("dword_190E0A8");
	p_1F552C4 = (int*)T4M::GetAddress("cl_paused");
	p_1F552DC = (char**)T4M::GetAddress("sv_running");
	p_2122AFC = (int*)T4M::GetAddress("dword_2122AFC");
	cs_hunkBase = (unsigned char*)T4M::GetAddress("dword_212B2F8");
	p_sv_state = (int*)T4M::GetAddress("dword_234FC08");
	p_sv_running = (int*)T4M::GetAddress("dword_234FC14");
	cs_dword_234FC20 = (int*)T4M::GetAddress("dword_234FC20");
	a_2350424 = (DWORD)T4M::GetAddress("word_2350424");
	a_2350426 = (DWORD)T4M::GetAddress("word_2350426");
	a_2350F42 = (DWORD)T4M::GetAddress("word_2350F42");
	cs_boundsNodes = (unsigned char*)T4M::GetAddress("unk_2351C10");
	cs_entBaselines = (unsigned char*)T4M::GetAddress("unk_23BCC08");
	cs_boundsNodesEnd = (unsigned char*)T4M::GetAddress("unk_23BCC10");
	cs_entBaselineCount = (int*)T4M::GetAddress("dword_23D4AE4");
	p_23D5C30 = (unsigned char**)T4M::GetAddress("dword_23D5C30");
	g_clientsBase = (unsigned char*)T4M::GetAddress("dword_2547090");
	a_26AA55C = (DWORD)T4M::GetAddress("dword_26AA55C");
	a_26AA56C = (DWORD)T4M::GetAddress("dword_26AA56C");
	a_2CAD98C = (DWORD)T4M::GetAddress("dword_2CAD98C");
	a_2CAD990 = (DWORD)T4M::GetAddress("dword_2CAD990");
	a_2CAD994 = (DWORD)T4M::GetAddress("dword_2CAD994");
	a_2CAD998 = (DWORD)T4M::GetAddress("dword_2CAD998");
	a_2FACDF8 = (DWORD)T4M::GetAddress("dword_2FACDF8");
	a_2FCDA04 = (DWORD)T4M::GetAddress("dword_2FCDA04");
	a_300DC9C = (DWORD)T4M::GetAddress("dword_300DC9C");
	a_300DCA0 = (DWORD)T4M::GetAddress("dword_300DCA0");
	a_300DCA8 = (DWORD)T4M::GetAddress("dword_300DCA8");
	a_300DCAC = (DWORD)T4M::GetAddress("dword_300DCAC");
	a_300DCB0 = (DWORD)T4M::GetAddress("dword_300DCB0");
	p_300FFEC = (int*)T4M::GetAddress("dword_300FFEC");
	p_3010014 = (int*)T4M::GetAddress("dword_3010014");
	p_301011C = (int*)T4M::GetAddress("dword_301011C");
	p_302012C = (int*)T4M::GetAddress("dword_302012C");
	G_3058528 = (void*)T4M::GetAddress("byte_3058528");
	a_305A63C = (DWORD)T4M::GetAddress("dword_305A63C");
	a_305BC74 = (DWORD)T4M::GetAddress("dword_305BC74");
	a_305C474 = (DWORD)T4M::GetAddress("dword_305C474");
	a_305D5FC = (DWORD)T4M::GetAddress("byte_305D5FC");
	a_307D5FC = (DWORD)T4M::GetAddress("dword_307D5FC");
	p_307D6E0 = (int*)T4M::GetAddress("dword_307D6E0");
	BASELINE_ENTITY_TABLE = (char*)T4M::GetAddress("unk_3122688");
	BASELINE_CLIENT_TABLE = (char*)T4M::GetAddress("unk_3168570");
	a_34651D8 = (DWORD)T4M::GetAddress("dword_34651D8");
	a_3467010 = (DWORD)T4M::GetAddress("dword_3467010");
	p_46E5054 = (unsigned int*)T4M::GetAddress("dword_46E5054");
	G_48AE508 = (void*)T4M::GetAddress("dword_48AE508");
	G_48AE50C = (void*)T4M::GetAddress("dword_48AE50C");
	G_48AE510 = (void*)T4M::GetAddress("dword_48AE510");
	p_4DA997C = (int*)T4M::GetAddress("dword_4DA997C");
	g_csTable = (unsigned short*)a_2350426;
	g_csSentinel = (unsigned short*)a_2350424;
	g_clientCsOffsets = (int*)a_305A63C;
	g_csData = (char*)a_305D5FC;
	g_csCursor = (int*)a_307D5FC;
}

void PatchT4MAM_ConfigStrings()
{
	ResolveCsAddrs();   // variant-aware: must run before detours/flip read a_* / g_*
#if CS_DETOURS
    // All 5 CS reconstructions. SEND + PARSE are coop-verified faithful (the trType:13 / wrong-position
    // desync was our SV_SendClientGameState omitting the two `lastEntityRef = -1` resets — after MSG_Init
    // and before the cmd4 loop — that the baseline entity-number delta-compression needs; the PARSE resets
    // it on cmd4, so the SEND must match). GET/SET/CLEAR re-enabled (full subsystem for the flip).
    Detours::X86::DetourFunction((uintptr_t)a_6315C0, (uintptr_t)&T4_Reconstructed::SV_GetConfigstring_Naked, Detours::X86Option::USE_JUMP);
    Detours::X86::DetourFunction((uintptr_t)a_631DA0, (uintptr_t)&T4_Reconstructed::SV_ClearConfigstrings, Detours::X86Option::USE_JUMP);
    Detours::X86::DetourFunction((uintptr_t)a_6311E0, (uintptr_t)&T4_Reconstructed::SV_SetConfigstrings, Detours::X86Option::USE_JUMP);
    Detours::X86::DetourFunction((uintptr_t)a_62F500, (uintptr_t)&T4_Reconstructed::SV_SendClientGameState, Detours::X86Option::USE_JUMP);
    Detours::X86::DetourFunction((uintptr_t)a_64CAE0, (uintptr_t)&T4_Reconstructed::CL_ParseGamestate, Detours::X86Option::USE_JUMP);
#endif

#if CS_FLIP_STAGE >= 1
    SetupCsFlip(); // relocate the CS table into owned buffer(s); behaviour per CS_FLIP_STAGE
#endif
}
