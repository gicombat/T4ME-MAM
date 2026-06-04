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

// --- Gates de détour (validation incrémentale ; 0 = inerte) -----------------
#define CS_DETOUR_GETCONFIGSTRING   1
#define CS_DETOUR_CLEARCONFIGSTRING 1
#define CS_DETOUR_SETCONFIGSTRING   1
// (next to CS_DETOUR_GETCONFIGSTRING / CS_DETOUR_CLEARCONFIGSTRING / CS_DETOUR_SETCONFIGSTRING)
#define CS_DETOUR_SENDGAMESTATE  1
#define CS_DETOUR_PARSEGAMESTATE 1

// --- Bascule (flip) de la table CS vers un buffer relogé possédé. 0 = table vanilla en place.
//   Stage 1 = relocation PURE : g_maxCS reste 0xBF0, bloc modèle reste à l'offset 0x58E.
//   Comportement IDENTIQUE au vanilla → valide la machinerie de repointing sans régression.
//   (Stage 2, séparé, élargira g_maxCS→0x1000 + re-base modèle→0xBF0 + cap 1024.)
// 0 = off (table vanilla). 1 = Stage 1 (pure relocation, vanilla behaviour, 0xBF0).
// 2 = Stage 2a (widen g_maxCS->0x1000, re-base model->0xBF0 cap 512, relocate CS-index companions).
#define CS_FLIP_STAGE 2

// Stage 2b: raise the MODEL cap 512 -> 1024 on top of stage 2a (relocate the model XModel*/remap
// tables to 1024-entry buffers + bump the model count caps + g_maxModels). Requires CS_FLIP_STAGE>=2.
// 0 = model cap stays 512. The destructibles pool (unk_47E87F0) is a SEPARATE 512 limit -> not touched.
#define CSF_MODEL_1024 1




// ============================================================================
// Globaux PARTAGÉS config-string (retargetables ; défaut = valeurs vanilla).
// La bascule finale ne touchera QUE ces 3 variables + l'allocation des nouvelles
// tables ; toutes les reconstructions ci-dessous les utilisent.
// ============================================================================
extern "C" unsigned short* g_csTable    = (unsigned short*)0x02350426; // word_2350426
extern "C" unsigned short* g_csSentinel = (unsigned short*)0x02350424; // word_2350424
extern "C" int             g_maxCS      = 0xBF0;                        // MAX_CONFIGSTRINGS
// (next to g_csTable / g_csSentinel / g_maxCS, OUTSIDE the namespace)
// Client-side CS byte-offset store base (dword_305A63C): per-CS offset into the
// 0x20000 CS blob byte_305D5FC. Made shared so the eventual flip can retarget it.
extern "C" int* g_clientCsOffsets = (int*)0x0305A63C;          // dword_305A63C
// Companion CS-text side-table bases — ALSO relocated by the flip stage 2, so the detoured
// CL_ParseGamestate must read them at runtime (never hardcode), like g_csTable (B3 fix).
extern "C" char* g_csData   = (char*)0x0305D5FC;               // byte_305D5FC (0x20000 blob)
extern "C" int*  g_csCursor = (int*) 0x0307D5FC;               // dword_307D5FC write cursor

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




namespace T4_Reconstructed
{
    // dword_3702390 : POINTEUR runtime vers la base du pool de strings internées
    // (stride 0xC ; texte d'un CS = (*ptr) + handle*0xC + 4). On lit la VALEUR.
    static unsigned char** const cs_stringPoolPtr = (unsigned char**)0x03702390;

    static const char* const cs_assertToken = (const char*)0x00887664; // unk_887664 (length-prefixed)
    static const char  cs_empty[1] = { 0 };                            // chaîne vide locale

    // --- helpers vanilla (par VA) -------------------------------------------
    typedef void (__cdecl* CsErr_t)(int level, const void* msg, ...);  // sub_59AC50
    static const CsErr_t   cs_error    = (CsErr_t)0x0059AC50;
    typedef void (__cdecl* StrncpyZ_t)(char* dst, const char* src, int count); // sub_7AA9C0
    static const StrncpyZ_t cs_strncpyz = (StrncpyZ_t)0x007AA9C0;
    typedef void (__cdecl* ComMemset_t)(void* dst, int fill, int dwordCount);  // sub_5E5100
    static const ComMemset_t cs_memsetDwords = (ComMemset_t)0x005E5100;
    typedef int  (__cdecl* SL_Intern_t)(int a, const char* str, int c, int len);
    static const SL_Intern_t SL_Intern_Low  = (SL_Intern_t)0x0068DE50; // idx < 0x585
    static const SL_Intern_t SL_Intern_High = (SL_Intern_t)0x0068E390; // idx >= 0x585
    typedef int  (__cdecl* ComSprintf_t)(char* dst, const char* fmt, ...);     // sub_7AA926
    static const ComSprintf_t cs_sprintf = (ComSprintf_t)0x007AA926;
    typedef char* (__cdecl* Va_t)(const char* fmt, ...);                       // sub_5F6D80
    static const Va_t cs_va = (Va_t)0x005F6D80;

    static int* const p_sv_state    = (int*)0x00234FC08;  // ==2 => SV actif
    static int* const p_sv_running  = (int*)0x00234FC14;
    static unsigned char* const g_clientsBase = (unsigned char*)0x02547090; // dword_2547090
    static const int CLIENT_STRIDE  = 0x58D30;
    static const void* const ERR_BAD_INDEX  = (const void*)0x008875C8;  // "SV_SetConfigstring: bad index %i"
    static const void* const ERR_BIG_BUNDLE = (const void*)0x008875F0;  // "…big config string…"
    #define CS_STRPOOL (*(unsigned char**)0x03702390)
    #define CS_SVS_PTR (*(unsigned char**)0x023D5C30)

    // ========================================================================
    // @faithful  SV_GetConfigstring (sub_6315C0). __usercall esi=idx/edi=dstSize/dst@stack.
    // ========================================================================
    extern "C" void __cdecl SV_GetConfigstring(int idx, char* dst, int dstSize)
    {
        if (idx < 0 || idx >= g_maxCS)
            cs_error(1, cs_assertToken, idx);            // puis fall-through (fidèle)

        unsigned int handle = (unsigned int)g_csTable[idx];
        if ((handle & 0xFFFF) == 0) { cs_strncpyz(dst, cs_empty, dstSize - 1); dst[dstSize - 1] = 0; return; }
        if (handle == 0)            { cs_strncpyz(dst, (const char*)0, dstSize - 1); dst[dstSize - 1] = 0; return; }

        const char* src = (const char*)(*cs_stringPoolPtr + handle * 0xC + 4);
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
            mov     eax, 0x0068E680
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

        cs_memsetDwords((void*)0x00234FC08, 0, 0x21806); // 0x86018 octets (gameState)
        *(int*)0x02FCDA04 = 0;

        // Flip : la table CS vit hors gameState → le memset ci-dessus ne la couvre plus.
        // Zéroter le buffer relogé (sentinelle + g_maxCS slots) comme le faisait le memset vanilla.
        if ((void*)g_csSentinel != (void*)0x02350424)
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
            mov     ecx, 0x00633FA0
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
            mov     ecx, 0x00633FA0
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
            mov     ecx, 0x00633FA0
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
            mov     edx, 0x005F6DF0
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
                cs_error(1, ERR_BAD_INDEX, idx);

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
                acc_len += cs_sprintf(bigbuf + acc_len, "%x %x %s", idx, len, str); // FIX: idx,len,str
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
                            if (chunk == 0) cs_error(1, ERR_BIG_BUNDLE, 0x1EF);
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
            cs_sprintf(bigbuf, "%x %x \\%d\\%s", lastIndex, vlen + 3, i, value); // FIX: lastIndex,vlen+3,i,value
            SV_CS_Send_cs_target(client, 1, "%c %s", 'd', bigbuf);
        }
    }

// NOTE: everything below lives inside `namespace T4_Reconstructed { ... }`.

    // ========================================================================
    //  cs_msg_t — vanilla MSG message struct (built on the stack).
    //  MSG_Init (sub_674C70) memsets 0x2C bytes before populating fields, so the
    //  struct MUST be >= 0x2C bytes (trailing pad). Byte offsets are load-bearing:
    //  helpers (sub_67AB80 etc.) read [+8]=data, [+0x10]=maxsize, [+0x14]=cursize,
    //  [+0x20]=bit, [+0]=overflowed.
    // ========================================================================
    struct cs_msg_t {
        int   overflowed;   // +0x00
        int   field_4;      // +0x04
        char* data;         // +0x08
        int   field_C;      // +0x0C
        int   maxsize;      // +0x10
        int   cursize;      // +0x14
        int   field_18;     // +0x18
        int   field_1C;     // +0x1C
        int   bit;          // +0x20  (aliased var_154 in vanilla asm)
        int   field_24;     // +0x24
        int   field_28;     // +0x28  PAD: MSG_Init zeroes 0x2C bytes  (D5)
    };

    // ---- send-side absolutes (NOT relocated) -------------------------------
    #define CS_HUNK_OFF   (*(unsigned int*)0x046E5054)                  // dword_46E5054 (temp Hunk offset)
    static unsigned char* const cs_hunkBase        = (unsigned char*)0x0212B2F8; // dword_212B2F8
    static unsigned char* const cs_boundsNodes     = (unsigned char*)0x02351C10; // unk_2351C10
    static unsigned char* const cs_boundsNodesEnd  = (unsigned char*)0x023BCC10; // unk_23BCC10
    static unsigned char* const cs_entBaselines    = (unsigned char*)0x023BCC08; // unk_23BCC08
    static int* const           cs_entBaselineCount = (int*)0x023D4AE4;          // dword_23D4AE4
    static int* const           cs_dword_234FC20    = (int*)0x0234FC20;
    static const int CS_BOUNDS_STRIDE   = 0x1AC;
    static const int CS_BASELINE_STRIDE = 0x118;
    // Built-in CS static table base (dword_8D55F8): stride 0x10, [+0]/[+0xC]=csNum, [+4]=defaultStr.
    static unsigned char* const cs_staticTable = (unsigned char*)0x008D55F8;     // dword_8D55F8

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
    static const GsDPrintf_t gs_dprintf  = (GsDPrintf_t)0x0059A310;
    typedef void (__cdecl* GsSnapshot_t)(void);                                  // sub_639910
    static const GsSnapshot_t gs_snapshot = (GsSnapshot_t)0x00639910;

    // ---- send-side naked thunks (NEW; distinct from existing cs_memsetDwords) ----

    // sub_7AFF40  Com_Memset = standard __cdecl memset(dst, fill, count). Call sites push
    // dst/fill/count then `add esp,0Ch`; MSG_Init's internal memset pushes esi(=msg) as the
    // dst STACK arg. NOT __usercall esi=dst — the send verify misread that pushed esi as a
    // register input. Cross-checked vs the parse-side pg_memset + call sites @0x1448/@0x4058D6.
    typedef void* (__cdecl* GsMemset_t)(void* dst, int fill, unsigned int count);
    static const GsMemset_t gs_memset = (GsMemset_t)0x007AFF40;
    // sub_678330  ResetReliableFragBuffers. edi=netchan(client+0x14). bool al (ignored). retn.
    __declspec(naked) void __cdecl gs_resetFrag(void* /*netchan*/) {
        __asm {
            push edi
            mov  edi, [esp+8]            // netchan
            mov  eax, 0x00678330
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
            mov  ecx, 0x0062F250
            call ecx
            add  esp, 4
            retn
        }
    }
    // sub_674C70  MSG_Init. esi=msg, stack=(data,maxsize). retn (callee cleans its 2 internal? no: caller add esp,8). Caller cleans.
    __declspec(naked) void __cdecl gs_msgInit(cs_msg_t* /*msg*/, char* /*data*/, int /*maxsize*/) {
        __asm {
            push esi
            mov  esi, [esp+8]            // msg
            push dword ptr [esp+10h]     // maxsize (arg_4)
            push dword ptr [esp+10h]     // data    (arg_0)
            mov  eax, 0x00674C70
            call eax
            add  esp, 8
            pop  esi
            retn
        }
    }
    // sub_638520  SV_WritePendingReliable. eax=msg, esi=client. retn.
    __declspec(naked) void __cdecl gs_writeReliable(cs_msg_t* /*msg*/, void* /*client*/) {
        __asm {
            push esi
            mov  eax, [esp+8]            // msg
            mov  esi, [esp+0Ch]          // client
            mov  ecx, 0x00638520
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
            mov  ecx, 0x005F69E0
            call ecx
            add  esp, 4
            retn
        }
    }
    // sub_674D00  MSG_WriteBits. edx=msg, stack=(value,bits). Caller add esp,8.
    __declspec(naked) void __cdecl gs_writeBits(cs_msg_t* /*msg*/, int /*value*/, int /*bits*/) {
        __asm {
            mov  edx, [esp+4]            // msg
            push dword ptr [esp+0Ch]     // bits  (pushed first in vanilla: push 0Ch then push esi)
            push dword ptr [esp+0Ch]     // value
            mov  ecx, 0x00674D00
            call ecx
            add  esp, 8
            retn
        }
    }
    // sub_674E00  MSG_WriteBit0 ("next index" marker). eax=msg. retn.
    __declspec(naked) void __cdecl gs_writeBit0(cs_msg_t* /*msg*/) {
        __asm {
            mov  eax, [esp+4]
            mov  ecx, 0x00674E00
            call ecx
            retn
        }
    }
    // sub_675430  MSG_WriteString. edx=str, edi=msg. retn. NOTE: derefs [str] (NULL would crash).
    __declspec(naked) void __cdecl gs_writeString(cs_msg_t* /*msg*/, const char* /*str*/) {
        __asm {
            push edi
            mov  edi, [esp+8]            // msg
            mov  edx, [esp+0Ch]          // str
            mov  eax, 0x00675430
            call eax
            pop  edi
            retn
        }
    }
    // sub_67AB80  MSG_WriteDeltaStruct. eax=from, ecx=to, stack=(msg, arg4, arg8). Caller add esp,0Ch. (D4)
    __declspec(naked) void __cdecl gs_writeDelta(void* /*from*/, void* /*to*/,
                                                 cs_msg_t* /*msg*/, int /*arg4*/, int /*arg8*/) {
        __asm {
            mov  eax, [esp+4]            // from (= zeroed varBaseline)
            mov  ecx, [esp+8]            // to   (= node / baseline)
            push dword ptr [esp+14h]     // arg8 (0x1000)
            push dword ptr [esp+14h]     // arg4 (0)
            push dword ptr [esp+14h]     // msg
            mov  edx, 0x0067AB80
            call edx
            add  esp, 0Ch
            retn
        }
    }
    // sub_496050 / sub_5720F0 / sub_6AF000  block serializers. esi=msg. retn.
    __declspec(naked) void __cdecl gs_writeBlock_496050(cs_msg_t* /*msg*/) {
        __asm { push esi
                mov  esi, [esp+8]
                mov  eax, 0x00496050
                call eax
                pop  esi
                retn }
    }
    __declspec(naked) void __cdecl gs_writeBlock_5720F0(cs_msg_t* /*msg*/) {
        __asm { push esi
                mov  esi, [esp+8]
                mov  eax, 0x005720F0
                call eax
                pop  esi
                retn }
    }
    __declspec(naked) void __cdecl gs_writeBlock_6AF000(cs_msg_t* /*msg*/) {
        __asm { push esi
                mov  esi, [esp+8]
                mov  eax, 0x006AF000
                call eax
                pop  esi
                retn }
    }
    // sub_6393F0  SV_Netchan_Transmit. esi=client, stack=(msg, 0). Caller add esp,8 (within the 0x18 caller cleanup).
    __declspec(naked) void __cdecl gs_transmit(void* /*client*/, cs_msg_t* /*msg*/) {
        __asm {
            push esi
            mov  esi, [esp+8]            // client
            push 0                       // arg_4 = 0
            push dword ptr [esp+10h]     // arg_0 = msg
            mov  eax, 0x006393F0
            call eax
            add  esp, 8
            pop  esi
            retn
        }
    }

    // ---- helpers: vanilla clientNum + inlined MSG byte/short/long writes -----
    static const int CS_CLIENT_STRIDE = 0x58D30;
    static inline int gs_clientNum(void* client) {
        return (int)(((unsigned char*)client - (unsigned char*)0x02547090) / CS_CLIENT_STRIDE);
    }
    static inline void gs_rawByte(cs_msg_t* m, unsigned char v) {
        if (m->cursize < m->maxsize) { m->data[m->cursize] = (char)v; m->cursize += 1; }
        else                          m->overflowed = 1;
    }
    static inline void gs_rawLong(cs_msg_t* m, int v) {
        if (m->cursize + 4 <= m->maxsize) { *(int*)(m->data + m->cursize) = v; m->cursize += 4; }
        else                                m->overflowed = 1;
    }
    static inline void gs_rawShort(cs_msg_t* m, unsigned short v) {
        if (m->cursize + 2 <= m->maxsize) { *(unsigned short*)(m->data + m->cursize) = v; m->cursize += 2; }
        else                                m->overflowed = 1;
    }
    // 12-bit index emit: byte-align pad (loc_62F867/904) then WriteBits(idx, 12).
    static inline void gs_emitIndex(cs_msg_t* m, int idx) {
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
        return handle ? (const char*)(*cs_stringPoolPtr + (unsigned)handle * 0xC + 4)
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
                    *(int*)(frag + 0x20B8) = *(int*)(0x026AA55C + cn * 4);
                    *(int*)(frag + 0x20BC) = *(int*)(0x026AA56C + cn * 4);
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

        cs_msg_t msg;
        gs_msgInit(&msg, (char*)msgData, 0x20000);           // sub_674C70

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
                if (!diff) continue;                         // loc_62F836 jz loc_62F947 -> skip total (D3)
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
            // Safe alternative (commented): gs_writeString(&msg, text ? text : cs_empty);
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

        *(int*)0x02CAD998 = *(int*)0x0300DCA0;
        *(int*)0x02CAD98C = *(int*)0x0300DCA8;
        *(int*)0x02CAD990 = *(int*)0x0300DCAC;
        *(int*)0x0300DCB0 = *(int*)0x02CAD994;
        *(int*)0x0300DC9C = *(int*)0x02FACDF8;

        CS_HUNK_OFF = savedHunkOff;                          // restore temp
    }


    // ========================================================================
    //  CL_ParseGamestate (sub_64CAE0) support: opaque msg + tables + helpers.
    // ========================================================================
    struct rec_msg_t {
        int   overflowed;   // +0
        int   pad04;        // +4
        char* data;         // +8   (read when readcount < splitThresh)
        char* splitData;    // +0xC (read when readcount >= splitThresh)
        int   maxsize;      // +0x10  REQUIRED filler: without it splitThresh..lastRefEntity
                            //         land 4 bytes low and the inline ReadByte desyncs from
                            //         the CL_PG_* thunks (which use the real [+0x1C] readcount).
        int   splitThresh;  // +0x14
        int   cursize;      // +0x18
        int   readcount;    // +0x1C
        int   bit;          // +0x20
        int   lastRefEntity;// +0x24
    };
    struct rec_csAuto_t { int index; const char* name; int unk; int pad; }; // stride 0x10
    static rec_csAuto_t* const g_csAuto = (rec_csAuto_t*)0x008D55F8;         // dword_8D55F8

    // g_csData (blob base) is now a file-scope relocatable global (see top, B3 fix).
    #define G_CSCURSOR (*g_csCursor)                                        // dword_307D5FC write cursor (relocatable)

    #define BASELINE_ENTITY_TABLE ((char*)0x03122688)                       // unk_3122688 stride 0x118 (absolute)
    #define BASELINE_CLIENT_TABLE ((char*)0x03168570)                       // unk_3168570 stride 0x118 (absolute)

    #define G_1F552DC (*(char**)0x01F552DC)
    #define G_1F552FC (*(char**)0x01F552FC)
    #define G_1F552C4 (*(int*)0x01F552C4)
    #define G_307D6E0 (*(int*)0x0307D6E0)
    #define G_3010014 (*(int*)0x03010014)
    #define G_302012C (*(int*)0x0302012C)
    #define G_300FFEC (*(int*)0x0300FFEC)
    #define G_301011C (*(int*)0x0301011C)
    #define G_2122B00 (*(char**)0x02122B00)
    #define G_2122AFC (*(int*)0x02122AFC)
    #define G_4DA997C (*(int*)0x04DA997C)
    static void* const G_3058528 = (void*)0x03058528;                       // byte_3058528 (0x301654)
    static void* const G_48AE508 = (void*)0x048AE508;                       // qword_48AE508
    static void* const G_48AE50C = (void*)0x048AE50C;
    static void* const G_48AE510 = (void*)0x048AE510;

    // unk_* are LENGTH-PREFIXED error tokens consumed by cs_error (sub_59AC50):
    // pass by VA (the IDA `unk_<VA>` name IS the address). Do NOT convert to C literals.
    static const char* const STR_TOO_MANY_CS = (const char*)0x0084F750;     // unk_84F750 (db 15h,"configstring > M...")
    static const char* const STR_CS_OVERFLOW = (const char*)0x0088951C;     // unk_88951C (length-prefixed)
    static const char* const STR_BAD_ENT     = (const char*)0x0088C220;     // unk_88C220 (length-prefixed)
    static const char* const STR_BAD_CLI     = (const char*)0x0088C244;     // unk_88C244 (length-prefixed)
    // aClParsegamesta / aFFF are PLAIN printf/scanf strings -> C literals (content faithful).
    static const char* const STR_BAD_CMD     = "CL_ParseGamestate: bad command byte %d\n"; // aClParsegamesta
    static const char* const STR_FMT_3F      = "%f %f %f";                  // aFFF

    // ---- parse cdecl helper VAs --------------------------------------------
    typedef void* (__cdecl* pg_memset_t)(void*, int, unsigned int);
    static const pg_memset_t pg_memset = (pg_memset_t)0x007AFF40;            // sub_7AFF40
    typedef void* (__cdecl* pg_memcpy_t)(void*, const void*, unsigned int);
    static const pg_memcpy_t pg_memcpy = (pg_memcpy_t)0x007AFFC0;            // sub_7AFFC0
    typedef int   (__cdecl* pg_sscanf_t)(const char*, const char*, ...);
    static const pg_sscanf_t pg_sscanf = (pg_sscanf_t)0x007AB559;           // sub_7AB559
    typedef void  (__cdecl* pg_preload_t)(int, int);
    static const pg_preload_t pg_5F0210 = (pg_preload_t)0x005F0210;         // sub_5F0210
    typedef void  (__cdecl* pg_void0_t)(void);
    // sub_474620: __usercall (index in ESI, vanilla `xor esi,esi` -> 0). NOT void(void).
    // Modeled as the CL_PG_ResetSnapshotSlot0 naked thunk (file scope) so ESI is set.
    static const pg_void0_t pg_59E970 = (pg_void0_t)0x0059E970;
    static const pg_void0_t pg_64C890 = (pg_void0_t)0x0064C890;
    static const pg_void0_t pg_642A60 = (pg_void0_t)0x00642A60;
    static const pg_void0_t pg_6F6CE0 = (pg_void0_t)0x006F6CE0;
    static const pg_void0_t pg_5A3320 = (pg_void0_t)0x005A3320;
    static const pg_void0_t pg_6F6D60 = (pg_void0_t)0x006F6D60;
    static const pg_void0_t pg_5FDBF0 = (pg_void0_t)0x005FDBF0;
    static const pg_void0_t pg_48E560 = (pg_void0_t)0x0048E560;
    typedef void  (__cdecl* pg_arg1_t)(void*);
    static const pg_arg1_t pg_4962F0 = (pg_arg1_t)0x004962F0;
    static const pg_arg1_t pg_6AF0F0 = (pg_arg1_t)0x006AF0F0;
    typedef void  (__cdecl* pg_err_t)(int, const char*, ...);
    static const pg_err_t pg_59A380 = (pg_err_t)0x0059A380;                 // Com_Error (bad cmd / drop)
    typedef void  (__cdecl* pg_clreset_t)(int, int);
    static const pg_clreset_t pg_5DE720 = (pg_clreset_t)0x005DE720;
    // (cs_error == sub_59AC50 reused for configstring/baseline range errors.)

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
        rec_msg_t* msg = (rec_msg_t*)msgArg;
        char readBuf[0x118];                                 // var_118

        pg_memset(readBuf, 0, 0x118);
        int cmd = 0xB;                                       // var_124 init

        if (G_1F552DC[0x10] == 0)
            pg_5F0210(0x1000, 0);

        CL_PG_ResetSnapshotSlot0();                          // sub_474620 (esi=0)
        G_3010014 = 0;
        if (G_307D6E0 == 0)
            pg_memset(G_3058528, 0, 0x301654);

        pg_59E970();

        msg->lastRefEntity = -1;                             // [ebx+24h] = -1
        *(long long*)G_48AE508 = 0;                          // movq qword_48AE508, 0
        *(int*)G_48AE510 = 0;
        G_302012C = CL_PG_ReadLong(msg);                     // sub_675560
        G_CSCURSOR = 1;                                      // dword_307D5FC = 1

        for (;;) {                                           // loc_64CB85
            int prevCmd     = cmd;                           // edx = var_124 loaded BEFORE read (D2)
            int splitThresh = msg->splitThresh;              // [ebx+14h]
            int total       = msg->cursize + splitThresh;    // [ebx+18h]+[ebx+14h]
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
                        cs_error(1, STR_TOO_MANY_CS);        // sub_59AC50
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
                        cs_error(1, STR_CS_OVERFLOW);        // sub_59AC50
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
                    pg_sscanf(g_csData + off, STR_FMT_3F, G_48AE508, G_48AE50C, G_48AE510);
                }
                continue;                                    // jmp loc_64CB85
            }
            else if (cmd == 3) {                             // baseline entity
                int nb  = cmd + 7;                           // lea edi,[ecx+7] = 0xA
                int idx = CL_PG_ReadEntityIdx(nb, msg);      // sub_676990
                if (idx < 0 || idx >= 0x400)                 // absolute bound (NOT relocated)
                    cs_error(1, STR_BAD_ENT, idx);
                char* table = BASELINE_ENTITY_TABLE + idx * 0x118;
                CL_PG_ReadDeltaBaseline(idx, msg, 0, readBuf, table); // sub_6772E0
                continue;
            }
            else if (cmd == 4) {                             // baseline client
                if (prevCmd != 4)                            // cmp edx,ecx; jz skip
                    msg->lastRefEntity = -1;                 // [ebx+24h] = -1
                int idx = CL_PG_ReadEntityIdx(0xA, msg);
                if (idx < 0 || idx > 0x15E)                  // jle -> bound is <= 0x15E
                    cs_error(1, STR_BAD_CLI, idx);
                char* table = BASELINE_CLIENT_TABLE + idx * 0x118;
                CL_PG_ReadDeltaBaseline(idx, msg, 0, readBuf, table);
                continue;
            }
            else {
            bad_command:                                     // loc_64CEB8
                pg_59A380(1, STR_BAD_CMD, cmd);              // Com_Error(1, fmt, cmd)
                msg->overflowed  = 1;                        // [ebx]=1
                msg->splitThresh = msg->readcount;           // [ebx+14h]=[ebx+1Ch]
                msg->cursize     = 0;                        // [ebx+18h]=0
                return;
            }
        }

    end_of_gamestate:                                        // loc_64CEEA
        G_300FFEC = CL_PG_ReadLong(msg);                     // sub_675560
        G_301011C = CL_PG_ReadLong(msg);                     // sub_675560

        if (G_1F552FC[0x10] != 0) {
            pg_6F6CE0();
            pg_5A3320();
            pg_6F6D60();
            pg_5FDBF0();
            pg_48E560();
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
            mov eax, 0x00675560
            jmp eax }                       // tail-call: callee retn, no args -> safe
}
// sub_675500  MSG_ReadShort : msg in EAX -> EAX (signed short)
__declspec(naked) int CL_PG_ReadShort(void* /*msg*/) {
    __asm { mov eax, [esp+4]
            mov edx, 0x00675500
            jmp edx }
}
// sub_675060  MSG_ReadBit : msg in EDX -> EAX
__declspec(naked) int CL_PG_ReadBit(void* /*msg*/) {
    __asm { mov edx, [esp+4]
            mov eax, 0x00675060
            jmp eax }
}
// sub_674F50  MSG_ReadBits(numBits) : msg in EDX, numBits pushed -> EAX (caller add esp,4)
__declspec(naked) int CL_PG_ReadBits(void* /*msg*/, int /*nb*/) {
    __asm { mov edx, [esp+4]                // msg
            push dword ptr [esp+8]          // nb
            mov eax, 0x00674F50
            call eax
            add esp, 4
            retn }
}
// sub_6757A0  MSG_ReadString : msg in EDX -> char* (static buffer)
__declspec(naked) char* CL_PG_ReadString(void* /*msg*/) {
    __asm { mov edx, [esp+4]
            mov eax, 0x006757A0
            jmp eax }
}
// sub_676690  MSG_ReadByte : msg in ECX -> EAX
__declspec(naked) int CL_PG_ReadByte(void* /*msg*/) {
    __asm { mov ecx, [esp+4]
            mov eax, 0x00676690
            jmp eax }
}
// sub_676990  MSG_ReadEntityIndex(nb, msg) : edi=nb, esi=msg (esi ambient R/W [esi+24h]) -> EAX
__declspec(naked) int CL_PG_ReadEntityIdx(int /*nb*/, void* /*msg*/) {
    __asm { push esi
            push edi
            mov  edi, [esp+0Ch]             // nb  (after 2 pushes)
            mov  esi, [esp+10h]             // msg
            mov  eax, 0x00676990
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
            mov  edx, 0x006772E0
            call edx
            add  esp, 0Ch
            retn }
}
// sub_5EF390 : ecx=0, stack=(a@arg_0, b@arg_4) ; vanilla pushes b then a. Caller add esp,8.
__declspec(naked) void CL_PG_5EF390(int /*a*/, int /*b*/) {
    __asm { xor  ecx, ecx
            push dword ptr [esp+8]          // b (arg_4) pushed first
            push dword ptr [esp+8]          // a (arg_0)
            mov  eax, 0x005EF390
            call eax
            add  esp, 8
            retn }
}
// sub_572210 : msg in ESI (ambient) -> void
__declspec(naked) void CL_PG_ReadInit576(void* /*msg*/) {
    __asm { push esi
            mov  esi, [esp+8]
            mov  eax, 0x00572210
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
            mov  eax, 0x00474620
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

static const DWORD CSF_TEXT_LO          = 0x00401000U;
static const DWORD CSF_TEXT_HI          = 0x007EB000U;   // .text end (VSize 0x3EA000)
static const DWORD CSF_VANILLA_BASE     = 0x02350426U;
static const DWORD CSF_VANILLA_MODELBLK = 0x02350F42U;   // = base + 0x58E*2
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
struct CsfSite { DWORD immVA; DWORD sym; };
static const CsfSite kCsfSites[] = {
    { 0x0054A9E7, 0x02351342 }, { 0x0054AAC4, 0x02351342 }, { 0x0054A868, 0x02351542 },
    { 0x00525DC7, 0x023516CA }, { 0x004EFF84, 0x023518CA }, { 0x0054A934, 0x023518CA },
    { 0x0054A3E4, 0x0235192A }, { 0x0054A7C4, 0x02351A6A },
    { 0x004F3B28, 0x02350438 }, { 0x00525752, 0x02350F32 }, { 0x00527579, 0x02350F36 },
    { 0x005276F7, 0x02350F36 }, { 0x005278B8, 0x02350F36 }, { 0x00527ABB, 0x02350F38 },
    { 0x00527C28, 0x02351B8C }, { 0x00527DAB, 0x02351B8C }, { 0x00527EF8, 0x02351B8E },
    { 0x0052807B, 0x02351B8E }, { 0x005281C8, 0x02351B90 }, { 0x0052835B, 0x02351B90 },
    { 0x0063246A, 0x02350424 }, { 0x0063246F, 0x02350426 }, { 0x0063248C, 0x02351C06 },
    { 0x00631702, 0x02350426 }, { 0x00631726, 0x02350424 }, { 0x0063172D, 0x02350426 },
    { 0x0054A20C, 0x02350428 },
};

#if CS_FLIP_STAGE >= 2
// Stage-2 fixed-immediate patches (verify old, write new). exe-verified.
struct CsfImm { DWORD immVA; DWORD oldv; DWORD neu; };
static const CsfImm kCsfStage2Imm[] = {
    // A.1 — CS index bounds 0xBF0 -> 0x1000
    { 0x004596D9, 0x00000BF0, 0x00001000 }, { 0x0063A926, 0x00000BF0, 0x00001000 },
    { 0x0063A9A8, 0x00000BF0, 0x00001000 },
    // C — model offset 0x58E -> 0xBF0 (classifier disp is negative two's-complement)
    { 0x0045947C, 0xFFFFFA72, 0xFFFFF410 }, // sub_459410 lea [esi-0x58E] -> [esi-0xBF0]
    { 0x00510507, 0x0000058E, 0x00000BF0 }, // sub_5104F0 savegame restore offset
    { 0x00510838, 0x0000058E, 0x00000BF0 }, // sub_510830 savegame table offset
    { 0x00510992, 0x0000058E, 0x00000BF0 }, // sub_510990 savegame table offset
    // client side-table struct: offsets array grows 0x2FC0 -> 0x4000, total 0x22FC4 -> 0x24004.
    // VAs are in the serializer sub_63A7F0 / clear sub_641730 — exe-verified .text matches (B1 fix).
    { 0x0063A8A8, 0x00002FC0, 0x00004000 }, // blob displacement (lea edi,[ebx+edx+2FC0h])
    { 0x0063A807, 0x00022FC4, 0x00024004 }, { 0x0063A84E, 0x00022FC4, 0x00024004 },
    { 0x0063A861, 0x00022FC4, 0x00024004 }, { 0x006417B1, 0x00022FC4, 0x00024004 },
};
// Companion symbol relocations (scan-replace, count-asserted).
struct CsfReloc { DWORD sym; int count; };
static const CsfReloc kCsfModelMap[] = { { 0x034651D8, 1 } };          // model-map WRITE base only
static const CsfReloc kCsfSideTbl[]  = { { 0x0305A63C, 20 }, { 0x0305D5FC, 89 }, { 0x0307D5FC, 21 } };

// --- ALIAS GAP FIX (root cause of the stage-2a "Client/Server game mismatch" + downstream desync) ---
// The flip relocates each table by its BASE, but vanilla hardcodes many SUB-BLOCK alias addresses
// (base + idx*stride) as separate immediates. A base-only scan-replace misses them, so once the base
// is relocated they keep reading the STALE old table. The 2026-06-03 audit counted base refs only.
// Counts are exe-verified (symbol-prefixed grep of CoDWaW LanFixed.exe.asm).
struct CsfAlias { DWORD addr; int count; };
// (1) Client CS offsets array (base 0x0305A63C, stride 4 = blob byte-offset per CS index):
//     gamename idx 2 (the mismatch trigger @sub_664570), mapCenter idx 12, VisionSet/category blocks,
//     and the MODEL block [0x58E,0x78E) which re-bases to CSF_MODEL_OFF like the server CS table.
static const CsfAlias kCsfOffAliases[] = {
    {0x0305A640,1},{0x0305A644,1},{0x0305A654,2},{0x0305A658,1},{0x0305A65C,1},
    {0x0305A660,1},{0x0305A66C,3},{0x0305A870,1},{0x0305AA70,2},{0x0305AA74,1},
    {0x0305AB70,1},{0x0305AC30,2},{0x0305BC2C,1},{0x0305BC54,1},{0x0305BC58,1},
    {0x0305BC5C,1},{0x0305BC60,1},{0x0305BC64,1},{0x0305BC74,5},{0x0305BC78,1},
    {0x0305C474,10},{0x0305C870,1},{0x0305C874,1},{0x0305C878,1},{0x0305CB84,2},
    {0x0305CF88,1},{0x0305CFC4,6},{0x0305D044,5},{0x0305D048,1},{0x0305D244,1},
    {0x0305D264,2},{0x0305D268,1},{0x0305D284,1},{0x0305D288,2},{0x0305D2C4,7},
    {0x0305D344,2},{0x0305D348,1},{0x0305D3C4,1},{0x0305D3C8,1},{0x0305D4C8,1},
    {0x0305D508,2},{0x0305D50C,1},{0x0305D510,1},
};
static const DWORD CSF_OFF_BASE  = 0x0305A63C;   // = dword_305A63C (offsets array base)
static const DWORD CSF_OFF_MODLO = 0x0305BC74;   // = offsets[0x58E] (model block start; re-base)
static const DWORD CSF_OFF_MODHI = 0x0305C474;   // = offsets[0x78E] (model block end, exclusive)
// (2) Model-map (base 0x034651D8, stride 4) READ-window aliases inside the model block. The audit
//     handled only 0x3466810; all re-base to mmap + (CSF_MODEL_OFF + idx - 0x58E)*4.
static const CsfAlias kCsfMmapAliases[] = {
    {0x03466810,15},  // base + 0x58E*4 (model read-window start)
    {0x03466814, 1},  // base + 0x58F*4 (sub_6621B0 "server models" registration loop)
    {0x0346685C, 1},  // base + 0x5A1*4
    {0x034669BC, 1},  // base + 0x5F9*4
};
static const DWORD CSF_MMAP_BASE = 0x034651D8;   // = dword_34651D8 (model-map base)

#if CSF_MODEL_1024
// Stage 2b model-table relocations (base scan-replace, count-asserted) + END pointer.
// NB: the render path (model-map dword_3466810, mmap) is already 0x1000 from 2a; the destructibles
// pool unk_47E87F0 is a SEPARATE limit and is intentionally NOT relocated here.
static const CsfReloc kCsfModelTbl[] = { { 0x0190E0A8, 9 }, { 0x018FABD0, 3 } };  // XModel* + remap bases
static const DWORD CSF_MODELPTR_END = 0x0190E8A8;  // dword_190E0A8 END (=base+512*4, 1 ref @sub_54F1E0)
// Model count caps (verify old → write new). VAs exe-verified (patch-doc 2a §C) except 0x662A3E
// (sub_6621B0 server-models loop) byte-verified by the Phase-1 old-value check.
static const CsfImm kCsfModel1024Imm[] = {
    { 0x00459481, 0x000001FF, 0x000003FF }, // sub_459410 classifier model range  (cmp eax,1FFh)
    { 0x00662A3E, 0x000007FC, 0x00000FFC }, // sub_6621B0 server-models loop       (cmp edi,7FCh=0x1FF*4)
    { 0x0051056A, 0x00000200, 0x00000400 }, // sub_5104F0 savegame restore cap     (cmp ebx,200h)
    { 0x00510833, 0x00000200, 0x00000400 }, // sub_510830 savegame table count     (push 200h)
    { 0x00510999, 0x00000200, 0x00000400 }, // sub_510990 savegame table count     (mov ecx,200h)
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
        if (!CsfTextOk(s.immVA) || *(DWORD*)s.immVA != s.sym) {
            sprintf(g_csFlipStatus, "flip ABORT: %08X has %08X != %08X", (unsigned)s.immVA,
                    (unsigned)(CsfTextOk(s.immVA) ? *(DWORD*)s.immVA : 0), (unsigned)s.sym);
            return;
        }
    if (CsfCountDword(CSF_VANILLA_MODELBLK) != CSF_MODELBLK_REFS) {
        sprintf(g_csFlipStatus, "flip ABORT: modelblk count %d != %d",
                CsfCountDword(CSF_VANILLA_MODELBLK), CSF_MODELBLK_REFS);
        return;
    }
#if CS_FLIP_STAGE >= 2
    for (const CsfImm& s : kCsfStage2Imm)
        if (!CsfTextOk(s.immVA) || *(DWORD*)s.immVA != s.oldv) {
            sprintf(g_csFlipStatus, "flip ABORT: imm %08X has %08X != %08X", (unsigned)s.immVA,
                    (unsigned)(CsfTextOk(s.immVA) ? *(DWORD*)s.immVA : 0), (unsigned)s.oldv);
            return;
        }
    // B2: model-map read-window END operand (sub_45CDF0 loop @0x45CE1E). Relocated separately to a
    //     RUNTIME value below; the OTHER 8 refs of 0x03467010 are effects reads -> stay vanilla.
    if (*(DWORD*)0x0045CE1E != 0x03467010) {
        sprintf(g_csFlipStatus, "flip ABORT: mmap-end %08X != 03467010", (unsigned)*(DWORD*)0x0045CE1E);
        return;
    }
    for (const CsfReloc& r : kCsfModelMap)
        if (CsfCountDword(r.sym) != r.count) {
            sprintf(g_csFlipStatus, "flip ABORT: mmap %08X count %d != %d",
                    (unsigned)r.sym, CsfCountDword(r.sym), r.count);
            return;
        }
    for (const CsfReloc& r : kCsfSideTbl)
        if (CsfCountDword(r.sym) != r.count) {
            sprintf(g_csFlipStatus, "flip ABORT: side %08X count %d != %d",
                    (unsigned)r.sym, CsfCountDword(r.sym), r.count);
            return;
        }
    for (const CsfAlias& a : kCsfOffAliases)
        if (CsfCountDword(a.addr) != a.count) {
            sprintf(g_csFlipStatus, "flip ABORT: off-alias %08X count %d != %d",
                    (unsigned)a.addr, CsfCountDword(a.addr), a.count);
            return;
        }
    for (const CsfAlias& a : kCsfMmapAliases)
        if (CsfCountDword(a.addr) != a.count) {
            sprintf(g_csFlipStatus, "flip ABORT: mmap-alias %08X count %d != %d",
                    (unsigned)a.addr, CsfCountDword(a.addr), a.count);
            return;
        }
#if CSF_MODEL_1024
    for (const CsfReloc& r : kCsfModelTbl)
        if (CsfCountDword(r.sym) != r.count) {
            sprintf(g_csFlipStatus, "flip ABORT: modeltbl %08X count %d != %d",
                    (unsigned)r.sym, CsfCountDword(r.sym), r.count);
            return;
        }
    if (CsfCountDword(CSF_MODELPTR_END) != 1) {
        sprintf(g_csFlipStatus, "flip ABORT: modelptr-end count %d != 1", CsfCountDword(CSF_MODELPTR_END));
        return;
    }
    for (const CsfImm& s : kCsfModel1024Imm)
        if (!CsfTextOk(s.immVA) || *(DWORD*)s.immVA != s.oldv) {
            sprintf(g_csFlipStatus, "flip ABORT: modelcap %08X has %08X != %08X", (unsigned)s.immVA,
                    (unsigned)(CsfTextOk(s.immVA) ? *(DWORD*)s.immVA : 0), (unsigned)s.oldv);
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
    memcpy(buf, (void*)0x02350424, 2 + 0xBF0 * 2); // preserve current table at the flip moment

    g_csSentinel = (unsigned short*)buf;
    g_csTable    = (unsigned short*)(buf + 2);
    g_maxCS      = (int)CSF_MAXCS;

    // ---- Phase 3: commit .text patches (single unprotect; leave RWX per project memory) ----
    DWORD oldProt;
    VirtualProtect((LPVOID)CSF_TEXT_LO, CSF_TEXT_HI - CSF_TEXT_LO, PAGE_EXECUTE_READWRITE, &oldProt);
    for (const CsfSite& s : kCsfSites)
        *(DWORD*)s.immVA = CsfRebase(s.sym);
    CsfReplaceDword(CSF_VANILLA_MODELBLK, (DWORD)g_csTable + CSF_MODEL_OFF * 2); // 21 model-block refs
    // ROOT-CAUSE FIX (stage-2a crash): kCsfSites re-based the SV_SpawnServer fill-loop END
    // (sub_631F20 @0x63248C) to g_csTable+0xBF0*2 (the OLD bound), so the loop never sentinel-
    // fills the re-based model block [0xBF0,0xDF0). Unfilled slots stay 0 -> SV_SetConfigstring
    // skips them (oldHandle==0) -> model names never written -> field_1A8 NULL -> Q_stricmpn crash.
    // Override to the FULL table so every slot (incl. the model block) gets the empty sentinel.
    *(DWORD*)0x0063248C = (DWORD)g_csTable + CSF_MAXCS * 2;
#if CS_FLIP_STAGE >= 2
    for (const CsfImm& s : kCsfStage2Imm)
        *(DWORD*)s.immVA = s.neu;
    CsfReplaceDword(0x034651D8, (DWORD)mmap);                          // model-map base (WRITE)
    for (const CsfAlias& a : kCsfMmapAliases)                          // model-map READ-window aliases (re-based)
        CsfReplaceDword(a.addr, (DWORD)mmap + (CSF_MODEL_OFF + (a.addr - CSF_MMAP_BASE) / 4 - 0x58E) * 4);
    *(DWORD*)0x0045CE1E = (DWORD)mmap + (CSF_MODEL_OFF + CSF_MODEL_COUNT) * 4; // model-map read-window END (B2; 512 or 2b:1024)
    CsfReplaceDword(0x0305A63C, (DWORD)side);                          // side-table offsets base
    for (const CsfAlias& a : kCsfOffAliases) {                         // offsets-array sub-block aliases
        DWORD idx = (a.addr - CSF_OFF_BASE) / 4;
        DWORD ni  = (a.addr >= CSF_OFF_MODLO && a.addr < CSF_OFF_MODHI)
                      ? (DWORD)(CSF_MODEL_OFF + idx - 0x58E) : idx;    // model block re-bases to CSF_MODEL_OFF
        CsfReplaceDword(a.addr, (DWORD)side + ni * 4);
    }
    CsfReplaceDword(0x0305D5FC, (DWORD)side + 0x4000);                 // side-table blob (shifted)
    CsfReplaceDword(0x0307D5FC, (DWORD)side + 0x24000);                // side-table cursor
#if CSF_MODEL_1024
    CsfReplaceDword(0x0190E0A8, (DWORD)modelPtrBuf);                   // model XModel* table base (9)
    CsfReplaceDword(CSF_MODELPTR_END, (DWORD)modelPtrBuf + 0x400 * 4); // its END pointer unk_190E8A8 (1)
    CsfReplaceDword(0x018FABD0, (DWORD)remapBuf);                      // model remap table base (3)
    for (const CsfImm& s : kCsfModel1024Imm)
        *(DWORD*)s.immVA = s.neu;                                       // model count caps 512->1024
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
// Install. Détours gated (validation incrémentale). NO Com_Printf (init-time).
// ---------------------------------------------------------------------------
void PatchT4MAM_ConfigStrings()
{
#if CS_DETOUR_GETCONFIGSTRING
    Detours::X86::DetourFunction((uintptr_t)0x006315C0, (uintptr_t)&T4_Reconstructed::SV_GetConfigstring_Naked, Detours::X86Option::USE_JUMP);
#endif
#if CS_DETOUR_CLEARCONFIGSTRING
    Detours::X86::DetourFunction((uintptr_t)0x00631DA0, (uintptr_t)&T4_Reconstructed::SV_ClearConfigstrings, Detours::X86Option::USE_JUMP);
#endif
#if CS_DETOUR_SETCONFIGSTRING
    Detours::X86::DetourFunction((uintptr_t)0x006311E0, (uintptr_t)&T4_Reconstructed::SV_SetConfigstrings, Detours::X86Option::USE_JUMP);
#endif

#if CS_DETOUR_SENDGAMESTATE
    Detours::X86::DetourFunction((uintptr_t)0x0062F500, (uintptr_t)&T4_Reconstructed::SV_SendClientGameState, Detours::X86Option::USE_JUMP);
#endif
#if CS_DETOUR_PARSEGAMESTATE
    Detours::X86::DetourFunction((uintptr_t)0x0064CAE0, (uintptr_t)&T4_Reconstructed::CL_ParseGamestate, Detours::X86Option::USE_JUMP);
#endif

#if CS_FLIP_STAGE >= 1
    SetupCsFlip(); // relocate the CS table into owned buffer(s); behaviour per CS_FLIP_STAGE
#endif
}
