// ==========================================================
// T4M project
//
// Component: clientdll
// Purpose:   Let large (>31-fragment) gamestates load over the SP/Local loopback link,
//            fixing the infinite "dropped gamestate, resending" loop on maps that precache
//            many distinct models (>~520). TWO independent fixes:
//
//   Fix A — completion bug (byte patch, installer below): vanilla Netchan_Process (sub_6786A0)
//     checks each fragment bit with `test mem,(1<<i)` then `setnle` (signed > 0), which misreads
//     bit 31 (the sign bit) as "not set" -> any gamestate needing >=32 fragments (last index >=31)
//     NEVER completes reassembly. Vanilla never hit it (<512 models => <32 fragments); our model
//     expansion produces 33-fragment gamestates. setnle(0F 9F)->setnz(0F 95) @0x678B59.
//
//   Fix B — enlarge the loopback packet queue 16 -> 128 slots: SV_SendClientGameState's own
//     resend-drain (sub_62F500 loc_62F536) flushes ALL pending fragments on each resend, bursting
//     up to ~33 fragments into the loopback queue at once; the vanilla 16-slot queue drop-oldest
//     would lose them. 128 = the fragmenter's own ceiling (cmp esi,24600h). Delivery timing stays
//     vanilla (no flush hook) so the loading cinematic is unaffected.
//
//   With both fixes, the gamestate reassembles on the natural vanilla resend-drain.
//
//   Queue detail: vanilla loopbacks[2] @ 0x3695368, MAX_LOOPBACK = 16, slot 0x4F8 (payload
//   MAX_PACKETLEN 0x4F0). N_SLOTS raised to 128.
//
//   The static loopbacks[] @ 0x3695368 is referenced ONLY by NET_GetLoopPacket (sub_678F30)
//   and NET_SendLoopPacket (sub_678FF0). We fully replace both (USE_JUMP detour, no
//   trampoline) and point them at a VirtualAlloc'd 128-slot buffer -> the vanilla static
//   array becomes dead and never desyncs.
//
//   Both functions are __usercall with ambient registers:
//     NET_GetLoopPacket  : sock@<edx>, net_message@<edi>, from(stack)        -> eax
//     NET_SendLoopPacket : length@<eax>, netsrc/data/netadr (stack)          -> void
//   Naked shims marshal the register args into the cdecl reconstructions.
//
//   @modified — faithful to vanilla except the queue depth (16 -> 128) and the relocated
//   base. Slot/payload geometry (0x4F8 / 0x4F0) is unchanged; we only add slots.
// ==========================================================

#include "StdInc.h"
#include "T4.h"
#include "MemoryMgr.h"
#include <cstring>

namespace LoopbackEngine
{
    // --- queue geometry. Only N_SLOTS changes vs vanilla (16); slot stride / payload stay.
    static const int N_SLOTS     = 128;                  // MAX_LOOPBACK (vanilla 16) = fragmenter cap (0x24600/0x48C)
    static const int MASK        = N_SLOTS - 1;          // 0x7F  (vanilla 0x0F)
    static const int SLOT_SIZE   = 0x4F8;                // loopmsg_t stride (unchanged)
    static const int PAYLOAD     = 0x4F0;                // MAX_PACKETLEN (unchanged)
    static const int GET_OFF     = N_SLOTS * SLOT_SIZE;  // 0x27C00 (vanilla 0x4F80) -> loopback_t.get
    static const int SEND_OFF    = GET_OFF + 4;          // 0x27C04 (vanilla 0x4F84) -> loopback_t.send
    static const int LOOP_STRIDE = GET_OFF + 8;          // 0x27C08 (vanilla 0x4F88) -> per-direction stride

    // VirtualAlloc'd 2 * LOOP_STRIDE (loopbacks[0] = client->server, [1] = server->client). Zero-init.
    static char* g_loopbackBase = nullptr;

    // Huffman warm-up (timing) on first send — faithful to vanilla sub_678FF0 head.
    // Resolved at runtime in PatchT4MAM_Loopback() (variant-aware; avoids static-init before AddrMap).
    typedef void (__cdecl* HuffWarm_t)(void);
    static HuffWarm_t HuffWarmup   = nullptr;   // 0x677BC0 MSG_InitHuffman
    static int*       g_huffInited = nullptr;   // 0x368FCFC msg_huffmanInited
}

extern "C" int  __cdecl T4M_NET_GetLoopPacket_recon (int sock, char* msg, char* from);
extern "C" void __cdecl T4M_NET_SendLoopPacket_recon(int length, int netsrc, void* data, int adr10);

// ---------------------------------------------------------------------------
// NET_GetLoopPacket reconstruction (sub_678F30). Dequeue one packet into net_message.
// __usercall(edx=sock, edi=net_message, stack=from) -> eax. cdecl body below.
// ---------------------------------------------------------------------------
extern "C" int __cdecl T4M_NET_GetLoopPacket_recon(int sock, char* msg, char* from)
{
    using namespace LoopbackEngine;

    char* loop  = g_loopbackBase + sock * LOOP_STRIDE;
    int*  pget  = (int*)(loop + GET_OFF);
    int*  psend = (int*)(loop + SEND_OFF);

    // drop-oldest: if more than the queue holds is pending, keep only the newest N_SLOTS
    if (*psend - *pget > N_SLOTS)
        *pget = *psend - N_SLOTS;
    if (*pget >= *psend)
        return 0;                                   // queue empty

    int   i        = *pget & MASK;
    (*pget)++;                                      // vanilla: lock xadd [get],1 (loopback is single-threaded)
    char* slot     = loop + i * SLOT_SIZE;
    int   datalen  = *(int*)  (slot + PAYLOAD);     // slot+0x4F0
    short typeword = *(short*)(slot + PAYLOAD + 4); // slot+0x4F4

    char* mdata = *(char**)(msg + 8);               // net_message->data
    memcpy(mdata, slot, datalen);

    // from = NA_LOOPBACK netadr (0x18): type(dword)=2 @+0, port word @+8, rest zero
    memset(from, 0, 0x18);
    *(int*)  (from + 0) = 2;
    *(short*)(from + 8) = typeword;

    // strip the leading sock/header byte the sender prepended, then cursize = datalen-1
    memmove(mdata, mdata + 1, datalen - 1);
    *(int*)(msg + 0x14) = datalen - 1;              // net_message->cursize
    return 1;
}

// ---------------------------------------------------------------------------
// NET_SendLoopPacket reconstruction (sub_678FF0). Enqueue one packet.
// __usercall(eax=length, stack=netsrc, data, netadr...) -> void. cdecl body below.
//   adr10 = the netadr dword at vanilla arg_10 (netadr+8); its low word is the loopback sock.
// ---------------------------------------------------------------------------
extern "C" void __cdecl T4M_NET_SendLoopPacket_recon(int length, int netsrc, void* data, int adr10)
{
    using namespace LoopbackEngine;

    if (*g_huffInited == 0) { *g_huffInited = 1; HuffWarmup(); }

    unsigned char payload[PAYLOAD];                 // 0x4F0 scratch (vanilla var_4F0)
    int curlen = 0;

    // prepend the 1-byte sock/header (low byte of netsrc)
    if (curlen < PAYLOAD)
        payload[curlen++] = (unsigned char)netsrc;

    // append the data (vanilla skips if it would exceed MAX_PACKETLEN -> truncate)
    int total = curlen + length;
    if (total <= PAYLOAD)
    {
        memcpy(&payload[curlen], data, length);
        curlen = total;
    }

    // decode destination queue + stored type word (faithful to vanilla sock decode)
    int typeword = 0;
    int destSock;
    if (netsrc < 1)        { typeword = netsrc; destSock = 1; }
    else if (netsrc == 1)  { destSock = (unsigned short)adr10; }  // arg_10 = netadr+8 word
    else                   { destSock = netsrc; }

    char* loop  = g_loopbackBase + destSock * LOOP_STRIDE;
    int*  psend = (int*)(loop + SEND_OFF);
    int   i     = *psend & MASK;
    char* slot  = loop + i * SLOT_SIZE;

    memcpy(slot, payload, curlen);
    *(int*)  (slot + PAYLOAD)     = curlen;          // datalen
    *(short*)(slot + PAYLOAD + 4) = (short)typeword; // type word
    (*psend)++;                                      // vanilla: lock xadd [send],1
}

// --- __usercall shims (detour targets). Marshal register args -> cdecl recon. ---

// sub_678F30: sock@<edx>, net_message@<edi>, from(stack [esp+4]). Ends 'retn' (caller cleans 'from').
static __declspec(naked) void T4M_NET_GetLoopPacket_usercall()
{
    __asm
    {
        mov  eax, [esp+4]     // from (caller's stack arg)
        push eax              // arg3 = from
        push edi              // arg2 = net_message
        push edx              // arg1 = sock
        call T4M_NET_GetLoopPacket_recon
        add  esp, 12          // pop our 3 args (eax = return preserved)
        retn                  // caller cleans original 'from'
    }
}

// sub_678FF0: length@<eax>; caller stack [+4]=netsrc, [+8]=data, [+0x14]=netadr+8 (arg_10).
// Push all needed args FRESH (can't reuse in place — our pushed length would split the
// original retaddr from the args). Ends 'retn' (caller cleans its own args).
static __declspec(naked) void T4M_NET_SendLoopPacket_usercall()
{
    __asm
    {
        push dword ptr [esp+14h]   // arg4 = adr10 (netadr+8)         [esp now -4]
        push dword ptr [esp+0Ch]   // arg3 = data   (orig [+8])       [esp now -8]
        push dword ptr [esp+0Ch]   // arg2 = netsrc (orig [+4])       [esp now -0xC]
        push eax                   // arg1 = length
        call T4M_NET_SendLoopPacket_recon
        add  esp, 16               // pop our 4 args
        retn                       // caller cleans original stack args
    }
}

// ---------------------------------------------------------------------------
// Install. Init-time: setnz completion fix + VirtualAlloc the 128-slot queue + wire both
// detours. Delivery itself stays vanilla: SV_SendClientGameState's own resend-drain loop
// (sub_62F500 loc_62F536) already flushes all pending fragments on each resend, so with the
// setnz fix the gamestate reassembles on the natural resend — no behaviour-changing hook needed
// (an earlier flush midhook delivered the gamestate at non-vanilla timing and skipped the
// loading cinematic). If alloc fails, leave vanilla loopback (16) fully intact. NO engine
// prints here (Sys_RunInit).
// ---------------------------------------------------------------------------
void PatchT4MAM_Loopback()
{
    using namespace LoopbackEngine;

    // --- Fix vanilla Netchan_Process fragment-complete bug (sub_6786A0 loc_678B40 @0x678B58).
    // The completion loop tests bit i with `test mem,(1<<i)` then `setnle dl` (signed > 0),
    // which misreads bit 31 (the sign bit) as "not set" -> any gamestate needing >=32 fragments
    // (last index >=31) NEVER completes reassembly. Vanilla never hit it (<512 models => <32
    // fragments); our model expansion produces 33-fragment gamestates. setnle(0F 9F) -> setnz(0F 95)
    // makes it a correct `!= 0` test. Byte-pattern guarded (abort if the exe differs).
    DWORD fragSite = T4M::GetAddress("Netchan_Process_fragComplete_site");  // 0x678B58 (guard) / +1 patched
    if (*(unsigned char*)fragSite       == 0x0F &&
        *(unsigned char*)(fragSite + 1) == 0x9F &&
        *(unsigned char*)(fragSite + 2) == 0xC2)
    {
        Memory::VP::Patch<uint8_t>(fragSite + 1, 0x95);   // setnle dl -> setnz dl
    }

    // Resolve loopback recon dependencies (variant-aware; used only at runtime by the recons).
    HuffWarmup   = (HuffWarm_t)T4M::GetAddress("MSG_InitHuffman");
    g_huffInited = (int*)      T4M::GetAddress("msg_huffmanInited");

    g_loopbackBase = (char*)VirtualAlloc(NULL, (SIZE_T)LOOP_STRIDE * 2,
                                         MEM_COMMIT | MEM_RESERVE, PAGE_READWRITE);
    if (!g_loopbackBase)
        return;   // keep vanilla 16-slot queue, no detour, no flush hook

    Detours::X86::DetourFunction((uintptr_t)T4M::GetAddress("NET_GetLoopPacket"),
                                 (uintptr_t)&T4M_NET_GetLoopPacket_usercall,
                                 Detours::X86Option::USE_JUMP);
    Detours::X86::DetourFunction((uintptr_t)T4M::GetAddress("NET_SendLoopPacket"),
                                 (uintptr_t)&T4M_NET_SendLoopPacket_usercall,
                                 Detours::X86Option::USE_JUMP);
}
