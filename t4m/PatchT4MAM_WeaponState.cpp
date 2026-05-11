#include "enums.hpp"
#include "structs.hpp"
#include "xasset.hpp"
#include "clientscript/clientscript_public.hpp"
using namespace T4;
#include "StdInc.h"
#include "T4.h"
#include "MemoryMgr.h"
#include <safetyhook.hpp>


// =====================================================================
// Forward decls + raw-access helpers
// =====================================================================

using pmove_t = ::T4::pmove_s;  // alias to typed struct from cod/structs.hpp

namespace T4_Reconstructed
{
    extern "C"
    {
        // Mono-caller helpers — reconstructed @faithful in this file.
        void PM_Weapon_AnimCmdStateInit(pmove_t* pm);                       // sub_41FF20
        char PM_Weapon_CanForceFire(pmove_t* pm);                           // sub_421A30 — returns bool
        char PM_Weapon_FireRefresh(playerState_s* ps, int arg_0);           // sub_421940 — returns bool; arg_0 is opaque pointer (deref [+0x28])
        void PM_Weapon_DetonateCheck(pmove_t* pm);                          // sub_421A80
        void PM_Weapon_OffhandStateBridge(pmove_t* pm);                     // sub_421B40
        char PM_Weapon_DefaultGate(pmove_t* pm, int callValidation);        // sub_420530 — returns bool
        void PM_Weapon_AnimSyncCleanup(pmove_t* pm, int callValidation);    // sub_421110
        void PM_Weapon_DetonateTransition(playerState_s* ps, int arg_0);    // sub_421BC0
        void PM_Weapon_ReloadStart(pmove_t* pm, int callValidation);        // sub_41F630
        void PM_Weapon_Reloading(pmove_t* pm, int callValidation);          // sub_41F780
        void PM_Weapon_Dropping(pmove_t* pm, int isQuick);                  // sub_41F010

        // Per-state chunk handlers — reconstructed @faithful in this file.
        void PM_Weapon_MeleeFire(playerState_s* ps);      // chunk #1 @ 0x00420CE0
        void PM_Weapon_MeleeCharge(playerState_s* ps);    // chunk #2 @ 0x00420D70
        void PM_Weapon_MeleeInit(playerState_s* ps);      // chunk #3 @ 0x00420E10
        void PM_Weapon_OffhandPrepare(playerState_s* ps); // chunk #4 @ 0x004213D0
        void PM_Weapon_OffhandStart(pmove_t* pm);         // chunk #5 @ 0x00421410
        void PM_Weapon_OffhandHold(pmove_t* pm);          // chunk #6 @ 0x004214E0
        void PM_Weapon_SwimIn(playerState_s* ps);         // chunk #7 @ 0x00421C40
        void PM_Weapon_SwimOut(playerState_s* ps);        // chunk #8 @ 0x00421CD0
        void PM_Weapon_SprintRaise(playerState_s* ps);    // chunk #9 @ 0x00421D80

        // Shared chunks — kept as register-preserving stubs (vanilla call).
        void PM_Weapon_OffhandInit_Stub(playerState_s* ps);   // shared A @ 0x00421300
        void PM_Weapon_Offhand_Stub(playerState_s* ps);       // shared B @ 0x00421630

        // Main entry — full reconstruction.
        void PM_Weapon(pmove_t* pm, int callValidation);
    }
}

namespace T4M
{
    void PM_Weapon_Wrapper();
    void PM_Weapon_LowReady_Start(playerState_s* ps);
    void PM_Weapon_LowReady_End(playerState_s* ps);

    // Phase 2 lowReady helpers (defined in this file, exposed via PatchT4Script.cpp forward decls).
    bool TransitionToReadyOrLowReady(playerState_s* ps);
    void EnterLowReadyStart(playerState_s* ps);
    void EnterLowReadyEnd(playerState_s* ps);
    void SetLowReadyIntent(playerState_s* ps, bool enable);

    // Bitmap-ext helpers (defined in PatchT4MAM_PsBitmapExt.cpp). Read/write
    // the 3 weapon bitmaps at ps+0x7FC/0x80C/0x81C with extension to 512 bits
    // via per-PS sidecar.
    extern "C" bool PsBitTest (playerState_s* ps, int inStructOffset, unsigned idx);
    extern "C" void PsBitSet  (playerState_s* ps, int inStructOffset, unsigned idx);
    extern "C" void PsBitClear(playerState_s* ps, int inStructOffset, unsigned idx);
}

void PatchT4MAM_WeaponState();


// =====================================================================
// Field accessors (raw offsets where t4_headers.h doesn't type the field)
// =====================================================================

static inline int   raw_int  (const void* base, int offset) { return *(const int*)((const char*)base + offset); }
static inline int&  raw_iref (void* base,       int offset) { return *(int*)((char*)base + offset); }
static inline float raw_float(const void* base, int offset) { return *(const float*)((const char*)base + offset); }
static inline unsigned char  raw_byte (const void* base, int offset) { return *((const unsigned char*)base + offset); }
static inline unsigned char& raw_bref (void* base,       int offset) { return *((unsigned char*)base + offset); }

extern T4::WeaponDef** bg_weaponDefs;  // defined in PatchT4Script.cpp,
                                       // pointed to relocated table by SetupBgWeaponDefsTable.

static inline WeaponDef* GetWeaponDef(unsigned int idx)
{
    return bg_weaponDefs[idx];
}


// =====================================================================
// Multi-caller / complex-mono helpers — register-preserving shims.
// All shims save+restore esi/edi/ebx around the vanilla call to honor cdecl.
// =====================================================================

// sub_421FF0 — __usercall(esi=ps, [esp+4]=arg_0); returns bool in al. (multi-caller)
static __declspec(naked) char call_IsWeaponInputBlocked(playerState_s* /*ps*/, int /*arg_0*/)
{
    __asm 
    {
        push    esi
        push    edi
        push    ebx
        mov     esi, [esp+10h]              ; ps (1st arg, +4 + 12 shift = +0x10)
        push    [esp+14h]                    ; arg_0 (2nd arg, +8 + 12 shift = +0x14)
        mov     eax, 0x00421FF0
        call    eax
        add     esp, 4
        pop     ebx
        pop     edi
        pop     esi
        ret
    }
}

// sub_41FF60 — __thiscall(ecx=arg_0, [esp+4]=pm); returns int in eax.
//   Mono-caller (called twice from sub_422160). Kept as shim for now.
static __declspec(naked) int call_PM_Weapon_AnimCmdState_Resync(int /*arg_0*/, pmove_t* /*pm*/)
{
    __asm 
    {
        push    esi
        push    edi
        push    ebx
        mov     ecx, [esp+10h]               ; arg_0 (1st arg)
        push    [esp+14h]                     ; pm (2nd arg)
        mov     eax, 0x0041FF60
        call    eax
        add     esp, 4
        pop     ebx
        pop     edi
        pop     esi
        ret
    }
}

// sub_41FED0 — __thiscall(ecx=ps); returns bool. (multi-caller)
static __declspec(naked) char call_PM_Weapon_HasAnimSync(playerState_s* /*ps*/)
{
    __asm 
    {
        push    esi
        push    edi
        push    ebx
        mov     ecx, [esp+10h]
        mov     eax, 0x0041FED0
        call    eax
        pop     ebx
        pop     edi
        pop     esi
        ret
    }
}

// sub_41DE50 — __usercall(edi=pm, [esp+4]=arg_0); returns void. (multi-caller)
static __declspec(naked) void call_PM_Weapon_DispatchAnimHandler(pmove_t* /*pm*/, int /*arg_0*/)
{
    __asm 
    {
        push    esi
        push    edi
        push    ebx
        mov     edi, [esp+10h]               ; pm
        push    [esp+14h]                     ; arg_0
        mov     eax, 0x0041DE50
        call    eax
        add     esp, 4
        pop     ebx
        pop     edi
        pop     esi
        ret
    }
}

// sub_41E0B0 — cdecl(arg_0=pm, arg_4=callValidation). Vanilla call site:
//   `push ebx; push edi; call sub_41E0B0` → [esp+4]=edi=pm, [esp+8]=ebx=callValidation.
static __declspec(naked) int call_PM_Weapon_TickValidation(pmove_t* /*pm*/, int /*callValidation*/)
{
    __asm 
    {
        push    esi
        push    edi
        push    ebx
        push    [esp+14h]                     ; callValidation (right arg, pushed first)
        push    [esp+14h]                     ; pm (left arg, pushed second)
        mov     eax, 0x0041E0B0
        call    eax
        add     esp, 8
        pop     ebx
        pop     edi
        pop     esi
        ret
    }
}

// sub_41E700 — __usercall(eax=ps, [esp+4]=arg_0); returns int.
static __declspec(naked) int call_PM_Weapon_FinalGate(playerState_s* /*ps*/, int /*arg_0*/)
{
    __asm 
    {
        push    esi
        push    edi
        push    ebx
        mov     eax, [esp+10h]
        push    [esp+14h]
        mov     edx, 0x0041E700
        call    edx
        add     esp, 4
        pop     ebx
        pop     edi
        pop     esi
        ret
    }
}

// sub_421EA0 — __thiscall(ecx=pm); has chunks. Kept as shim.
static __declspec(naked) void call_PM_Weapon_TickSprintMachine(pmove_t* /*pm*/)
{
    __asm 
    {
        push    esi
        push    edi
        push    ebx
        mov     ecx, [esp+10h]
        mov     eax, 0x00421EA0
        call    eax
        pop     ebx
        pop     edi
        pop     esi
        ret
    }
}

// sub_421F20 — __usercall(eax=pm); has chunks.
static __declspec(naked) void call_PM_Weapon_TickIdleMachine(pmove_t* /*pm*/)
{
    __asm 
    {
        push    esi
        push    edi
        push    ebx
        mov     eax, [esp+10h]
        mov     edx, 0x00421F20
        call    edx
        pop     ebx
        pop     edi
        pop     esi
        ret
    }
}

// sub_4217D0 — __usercall(eax=pm); has chunks.
static __declspec(naked) void call_PM_Weapon_TickOffhandMachine(pmove_t* /*pm*/)
{
    __asm 
    {
        push    esi
        push    edi
        push    ebx
        mov     eax, [esp+10h]
        mov     edx, 0x004217D0
        call    edx
        pop     ebx
        pop     edi
        pop     esi
        ret
    }
}

// sub_420260 — cdecl(pm). (multi-caller)
static __declspec(naked) void call_PM_Weapon_TickPickup(pmove_t* /*pm*/)
{
    __asm 
    {
        push    esi
        push    edi
        push    ebx
        push    [esp+10h]
        mov     eax, 0x00420260
        call    eax
        add     esp, 4
        pop     ebx
        pop     edi
        pop     esi
        ret
    }
}

// sub_41F8D0 — __usercall(edx=pm); long function, kept as shim.
static __declspec(naked) void call_PM_Weapon_TickRecoil(pmove_t* /*pm*/)
{
    __asm 
    {
        push    esi
        push    edi
        push    ebx
        mov     edx, [esp+10h]
        mov     eax, 0x0041F8D0
        call    eax
        pop     ebx
        pop     edi
        pop     esi
        ret
    }
}

// sub_420A60 — cdecl(ps, callValidation); has chunks.
//   Vanilla: `push ebx; push ebp; call sub_420A60` → [esp+4]=ebp=ps, [esp+8]=ebx=callValidation.
static __declspec(naked) void call_PM_Weapon_ProcessAttackInput(playerState_s* /*ps*/, int /*callValidation*/)
{
    __asm 
    {
        push    esi
        push    edi
        push    ebx
        push    [esp+14h]                     ; callValidation (right arg, pushed first)
        push    [esp+14h]                     ; ps (left arg, pushed second)
        mov     eax, 0x00420A60
        call    eax
        add     esp, 8
        pop     ebx
        pop     edi
        pop     esi
        ret
    }
}

// sub_420530 — fallback shim (not used; real reconstruction below).
// sub_4225D0 — multi-caller (called from MELEE_END / OFFHAND_END / SPRINT_DROP / BREAKING_DOWN)
//   __usercall(eax=ps).
static __declspec(naked) void call_PM_Weapon_FinalizeStateExit(playerState_s* /*ps*/)
{
    __asm 
    {
        push    esi
        push    edi
        push    ebx
        mov     eax, [esp+10h]
        mov     edx, 0x004225D0
        call    edx
        pop     ebx
        pop     edi
        pop     esi
        ret
    }
}

// sub_412BF0 — multi-caller event/anim trigger. __usercall(eax=ps, ecx=animId).
static __declspec(naked) void call_Anim_TriggerEvent(playerState_s* /*ps*/, int /*animId*/)
{
    __asm 
    {
        push    esi
        push    edi
        push    ebx
        mov     eax, [esp+10h]
        mov     ecx, [esp+14h]
        mov     edx, 0x00412BF0
        call    edx
        pop     ebx
        pop     edi
        pop     esi
        ret
    }
}

// sub_420CC0 — __usercall(eax=pm); returns int. (multi-caller)
static __declspec(naked) int call_PM_Weapon_IsLocked(pmove_t* /*pm*/)
{
    __asm 
    {
        push    esi
        push    edi
        push    ebx
        mov     eax, [esp+10h]
        mov     edx, 0x00420CC0
        call    edx
        pop     ebx
        pop     edi
        pop     esi
        ret
    }
}

// sub_41F010 (DROPPING handler) is reconstructed in C++ as
// T4_Reconstructed::PM_Weapon_Dropping (defined below). Direct call from
// the dispatcher — no shim, no SafetyHook detour.

// Helpers called from PM_Weapon_Reloading / PM_Weapon_ReloadStart reconstructions.

// sub_41F4E0 — __usercall(eax = ps). Some weapon-state refresh.
static __declspec(naked) void call_PM_Weapon_RefreshReload(playerState_s* /*ps*/)
{
    __asm 
    {
        push    esi
        push    edi
        push    ebx
        mov     eax, [esp+10h]
        mov     edx, 0x0041F4E0
        call    edx
        pop     ebx
        pop     edi
        pop     esi
        ret
    }
}

// sub_41F410 — __usercall(edi = ps); returns int in eax.
static __declspec(naked) int call_PM_Weapon_CheckFastReload(playerState_s* /*ps*/)
{
    __asm 
    {
        push    esi
        push    edi
        push    ebx
        mov     edi, [esp+10h]
        mov     eax, 0x0041F410
        call    eax
        pop     ebx
        pop     edi
        pop     esi
        ret
    }
}

// sub_41D420 — __usercall(eax = ps, [esp+4] = arg_0). Anim event submit.
static __declspec(naked) void call_PM_Weapon_SubmitAnimEvent(playerState_s* /*ps*/, int /*arg_0*/)
{
    __asm 
    {
        push    esi
        push    edi
        push    ebx
        mov     eax, [esp+10h]
        push    [esp+14h]
        mov     edx, 0x0041D420
        call    edx
        add     esp, 4
        pop     ebx
        pop     edi
        pop     esi
        ret
    }
}

// chunk_41E940 — shared chunk (sub_41EA30 + sub_41F630 + sub_41F780).
//   __usercall(eax = ps). Falls into reload-finalize logic + sub_41E860 call.
static __declspec(naked) void call_PM_Weapon_ReloadFinalize(playerState_s* /*ps*/)
{
    __asm 
    {
        push    esi
        push    edi
        push    ebx
        mov     eax, [esp+10h]
        mov     edx, 0x0041E940
        call    edx
        pop     ebx
        pop     edi
        pop     esi
        ret
    }
}

// External helpers called from chunks/handlers:
//   sub_406D00 (event lookup), sub_406E70 (event queue), sub_7AA7A8 (rand),
//   sub_41E5E0 (anim helper), sub_41D440, sub_420500,
//   sub_419F20, sub_420E70.
// These are wrapped per call site via inline asm to avoid declaring more shims.


// =====================================================================
// Reconstructed mono-caller helpers (@faithful)
// =====================================================================

// @faithful — sub_41FF20 (entry 0x0041FF20). __thiscall(ecx=pm).
//   Sets ps->[+0x10] |= 0x400 if anim cmd state qualifies.
extern "C" void T4_Reconstructed::PM_Weapon_AnimCmdStateInit(pmove_t* pm)
{
    auto* ps = *(playerState_s**)pm;
    const WeaponDef* w = GetWeaponDef(ps->weapon);
    int v = w->fireType;

    if (v == 0 || v == 1) 
        return;

    if ((pm->cmd.buttons  & 1) == 0) 
        return;

    if ((pm->oldcmd.buttons & 1) != 0) 
        return;

    raw_iref(ps, 0x10) |= 0x400;
}

// @faithful — sub_421A30 (entry 0x00421A30). __usercall(edi=pm); returns bool.
//   Force-fire check for melee/RPG weapons.
//   Mono-caller from PM_Weapon (our reconstruction) → plain cdecl.
extern "C" char T4_Reconstructed::PM_Weapon_CanForceFire(pmove_t* pm)
{
    auto* ps = *(playerState_s**)pm;

    if (ps->weapon == 0) 
        return 0;

    const WeaponDef* w = GetWeaponDef(ps->weapon);
    int weaponClass = w->weapType;

    if (weaponClass != 1 && weaponClass != 6) 
        return 0;

    if (w->holdButtonToThrow != 0) 
        return 0;

    // sub_420500 is __usercall(eax=weaponIdx); returns bit mask in eax.
    int mask;
    int weaponIdx = ps->weapon;
    __asm 
    {
        push    esi
        push    edi
        push    ebx
        mov     eax, weaponIdx
        mov     edx, 0x00420500
        call    edx
        mov     mask, eax
        pop     ebx
        pop     edi
        pop     esi
    }

    if ((pm->cmd.buttons & mask) == 0) 
        return 0;

    ps->weaponDelay = 1;
    return 1;
}

// @faithful — sub_421940 (entry 0x00421940). __usercall(eax=ps, [esp+4]=pm); returns bool.
//   Fire-refresh / weapon-time decrement check.
//   Mono-caller from PM_Weapon → plain cdecl.
extern "C" char T4_Reconstructed::PM_Weapon_FireRefresh(playerState_s* ps, int arg_0)
{
    int edx_state = raw_int(ps, 0x10) & 2;
    int weaponIdx;
    if (edx_state != 0) 
    {
        weaponIdx = raw_int(ps, 0xFC);
    } 
    else 
    {
        weaponIdx = ps->weapon;
        if (weaponIdx == 0) 
            return 0;
    }

    const WeaponDef* w = GetWeaponDef(weaponIdx);
    int weaponClass = w->weapType;
    if (weaponClass != 1 && weaponClass != 6) 
        return 0;

    // loc_421974 — bail if v48 <= 0 (timer not active)
    int v48 = raw_int(ps, 0x48);
    if (v48 <= 0) 
        return 0;                 // jle loc_4219B7 → return 0

    // v48 > 0: maybe decrement
    bool skip_decrement = (raw_byte(ps, 0x8E8) & 1) != 0 || w->bCookOffHold == 0;

    if (!skip_decrement) 
    {
        // Vanilla: mov ecx, [esp+4+arg_0]; sub eax, [ecx+28h]
        // arg_0 is an opaque caller-context pointer; deref [+0x28] as int.
        int delta = *(const int*)((const char*)(uintptr_t)arg_0 + 0x28);
        raw_iref(ps, 0x48) = v48 - delta;   // v48 may now be <= 0
    }

    // loc_421997 — state transition if conditions met
    bool to_4219BB = (raw_int(ps, 0x944) < 3) || (edx_state == 0) || (ps->weaponstate != 0x13);

    if (!to_4219BB) 
    {
        ps->weaponstate = (weaponstate_t)0x12;
        return 0;                            // fall through to loc_4219B7
    }

    // loc_4219BB — post-decrement check
    if (raw_int(ps, 0x48) > 0) 
        return 0;     // still pending → bail

    // Timer just expired: queue notify 0x56 with low-16-bits of [ps+0xFC] (offhand idx).
    // Vanilla unconditionally loads eax = [edi+0FCh] at loc_4219BB regardless of edx_state.
    int offhandIdx = raw_int(ps, 0xFC);
    int head = raw_int(ps, 0xD0) & 3;
    raw_iref(ps, 0x48) = -1;
    raw_iref(ps, 0xD4 + head * 4) = 0x56;
    int head2 = raw_int(ps, 0xD0) & 3;
    raw_iref(ps, 0xE4 + head2 * 4) = offhandIdx & 0xFFFF;     // movzx ecx, ax in vanilla
    raw_iref(ps, 0xD0) = (raw_int(ps, 0xD0) + 1) & 0xFF;

    if (raw_int(ps, 0x4C) != 0x3FF || (raw_byte(ps, 0x8E8) & 1) != 0) 
        return 1;  // loc_421A2C: mov al, 1; pop edi; retn

    // sub_41E5E0 is __usercall(edi=ps, esi=cap, [esp+4]=offhandIdx).
    __asm 
    {
        push    esi
        push    edi
        push    ebx
        push    offhandIdx                    ; arg_0 = ps[+0xFC]
        mov     edi, ps
        mov     esi, 1
        mov     eax, 0x0041E5E0
        call    eax
        add     esp, 4
        pop     ebx
        pop     edi
        pop     esi
    }

    return 1;
}

// @faithful — sub_421A80 (entry 0x00421A80). __usercall(esi=pm).
//   If weapon supports detonation AND state is non-busy, transition to DETONATING.
//   Mono-caller from PM_Weapon → plain cdecl.
extern "C" void T4_Reconstructed::PM_Weapon_DetonateCheck(pmove_t* pm)
{
    auto* ps = *(playerState_s**)pm;

    if (ps->weapon == 0) 
        return;

    const WeaponDef* w = GetWeaponDef(ps->weapon);
    int weaponClass = w->weapType;

    if (weaponClass != 1 && weaponClass != 6) 
        return;

    if (w->hasDetonator == 0) 
        return;

    int state = ps->weaponstate;
    // bail list (states that block transition to DETONATING):
    //   0x16, 7, 9, 0xB, 0xA, 8, 5, 6, 0xD, 0xE, 0xF, 1, 2, 3, 4
    //   AND any state in [0x10, 0x15]
    if (state == 0x16 || state == 7 || state == 9 || state == 0xB ||
        state == 0xA  || state == 8 || state == 5 || state == 6 ||
        state == 0xD  || state == 0xE || state == 0xF ||
        state == 1    || state == 2 || state == 3 || state == 4) 
        return;

    if (state >= 0x10 && state <= 0x15) 
        return;

    // loc_421B13
    if ((pm->cmd.buttons & 1) == 0) 
        return;

    ps->weaponstate = (weaponstate_t)0x16;     // WEAPON_DETONATING
    ps->weaponTime = w->iDetonateTime;
    ps->weaponDelay = w->iDetonateDelay;

    // sub_41D420 is __usercall(eax=ps, [esp+4]=new_vm_state).
    __asm 
    {
        push    esi
        push    edi
        push    ebx
        push    1Bh
        mov     eax, ps
        mov     edx, 0x0041D420
        call    edx
        add     esp, 4
        pop     ebx
        pop     edi
        pop     esi
    }
}

// @faithful — sub_421B40 (entry 0x00421B40). __usercall(edi=pm).
//   Bridges from OFFHAND_PREPARE to OFFHAND_END (via shared chunk @ 0x421630)
//   or finalizes auto-fire chain.
//   Mono-caller from PM_Weapon → plain cdecl.
extern "C" void T4_Reconstructed::PM_Weapon_OffhandStateBridge(pmove_t* pm)
{
    auto* ps = *(playerState_s**)pm;
    int state = ps->weaponstate;

    if (state == 0x11) 
    {                      // WEAPON_OFFHAND_PREPARE
        const WeaponDef* w = GetWeaponDef(raw_int(ps, 0xFC));
        if (w->holdButtonToThrow != 0 && (pm->cmd.buttons & 0xC000) == 0) 
        {
            // jmp shared chunk @ 0x00421630 (with eax = ps)
            T4_Reconstructed::PM_Weapon_Offhand_Stub(ps);
            return;
        }
        // fall through to loc_421BB7 (no-op end)
        return;
    }

    // loc_421B73
    if (ps->weapon == 0) 
        return;

    const WeaponDef* w = GetWeaponDef(ps->weapon);
    int weaponClass = w->weapType;

    if (weaponClass != 1 && weaponClass != 6) 
        return;

    // loc_421B94
    if (state != 5) 
        return;                   // not WEAPON_FIRING

    if (w->holdButtonToThrow == 0) 
        return;

    if ((pm->cmd.buttons & 1) != 0) 
        return;

    // call sub_4225D0(eax = ps)
    call_PM_Weapon_FinalizeStateExit(ps);
    // sub_41D420 is __usercall(eax=ps, [esp+4]=new_vm_state).
    __asm 
    {
        push    esi
        push    edi
        push    ebx
        push    1
        mov     eax, ps
        mov     edx, 0x0041D420
        call    edx
        add     esp, 4
        pop     ebx
        pop     edi
        pop     esi
    }
}

// @faithful — sub_420530 (entry 0x00420530). __usercall(esi=pm, [esp+4]=arg_0); returns bool.
//   Mono-caller from PM_Weapon → plain cdecl.
extern "C" char T4_Reconstructed::PM_Weapon_DefaultGate(pmove_t* pm, int arg_0)
{
    auto* ps = *(playerState_s**)pm;
    const WeaponDef* w = GetWeaponDef(ps->weapon);
    int weaponClass = w->weapType;

    int mask;
    if ((weaponClass == 1 || weaponClass == 6) && w->hasDetonator != 0) 
    {
        mask = 0x400000;
    } 
    else 
    {
        mask = 1;
    }

    int bl = ((pm->cmd.buttons & mask) != 0) ? 1 : 0;

    if (w->freezeMovementWhenFiring != 0 && raw_int(ps, 0x88) == 0x3FF) 
    {
        bl = 0;
    }

    if (w->canUseInVehicle != 0) 
    {
        // dword_1F552A4 holds a dvar_t* pointer; deref then check [+0x10] (dvar bool field).
        unsigned char* dvar_ptr = *(unsigned char**)0x01F552A4;
        if (dvar_ptr[0x10] != 0) 
        {
            bl = 0;
        }
    }

    char al;
    if (arg_0 != 0) 
    {
        al = 1;
    } 
    else 
    {
        al = call_PM_Weapon_HasAnimSync(ps);
        if (al == 0) 
        {
            // fall through with al = 0
        } 
        else 
        {
            al = 1;
        }
    }

    if (bl != 0 || al != 0) 
        return 1;

    if (ps->weaponstate == 5) 
    {
        // sub_41D440 is __usercall(ecx=ps, edx=new_vm_anim_state).
        __asm 
        {
            push    esi
            push    edi
            push    ebx
            mov     ecx, ps
            xor     edx, edx
            mov     eax, 0x0041D440
            call    eax
            pop     ebx
            pop     edi
            pop     esi
        }
    }

    ps->weaponstate = (weaponstate_t)0;
    return 0;
}

// @faithful — sub_421110 (entry 0x00421110). __thiscall(ecx=pm, [esp+4]=arg_0).
//   Anim sync cleanup after sub_41FED0 returned false.
//   Mono-caller from PM_Weapon → plain cdecl.
extern "C" void T4_Reconstructed::PM_Weapon_AnimSyncCleanup(pmove_t* pm, int arg_0)
{
    auto* ps = *(playerState_s**)pm;

    if (raw_int(ps, 0x944) >= 3)
        return;

    if ((raw_int(ps, 0x0C) & 0x8000000) != 0) 
        return;

    const WeaponDef* w = GetWeaponDef(ps->weapon);

    int state = ps->weaponstate;

    if (state == 0xD || state == 0xE || state == 0xF)
        return;

    if (state >= 0x10 && state <= 0x15) 
        return;

    // loc_421169
    if ((pm->cmd.buttons & 4) != 0) 
    {
        if (call_PM_Weapon_IsLocked(pm) != 0) 
            return;
    }

    // loc_42117E
    if (w->iMeleeDamage == 0) 
        return;

    if (arg_0 != 0)
        return;

    if ((raw_byte(ps, 0x14) & 0x20) != 0) 
        return;

    if (ps->weaponDelay != 0) 
    {
        if (state != 7 && state != 9 && state != 0xB && state != 0xA && state != 8) 
            return;
    }

    // loc_4211B9
    if ((pm->cmd.buttons  & 4) == 0) 
        return;

    if ((pm->oldcmd.buttons & 4) != 0) 
        return;

    float wpf = ps->fWeaponPosFrac;
    bool slowAnim = (wpf <= *(float*)0x008B7B30);  // ds:String2 — float 0.0 sentinel

    if (!slowAnim && w->overlayReticle != 0) 
        return;

    // loc_4211DF — get string pointer at w+0x6C
    const char* s = *(const char**)((char*)w + 0x6C);

    if (s == nullptr || s[0] == 0) 
        return;

    if (state > 0 && state <= 4) 
        return;

    // loc_4211F6: call sub_419F20(__thiscall ecx=pm); call sub_420E70(cdecl arg_0=ps).
    // sub_420E70 reads [ps+0xC] = pm_flags, [ps+0x104] = weapon — transitions to MELEE_CHARGE.
    __asm 
    {
        push    esi
        push    edi
        push    ebx
        mov     ecx, pm
        mov     eax, 0x00419F20
        call    eax
        push    [ps]                          ; arg_0 = ps (NOT pm)
        mov     eax, 0x00420E70
        call    eax
        add     esp, 4
        pop     ebx
        pop     edi
        pop     esi
    }
}

// @faithful — sub_421BC0 (entry 0x00421BC0). __usercall(eax=ps, [esp+arg_0]).
//   DETONATING handler: queue notify or reset weapon state.
//   Mono-caller from PM_Weapon → plain cdecl.
extern "C" void T4_Reconstructed::PM_Weapon_DetonateTransition(playerState_s* ps, int arg_0)
{
    if (arg_0 == 0 || ps->weapon == 0) 
    {
        // loc_421C0A — finalize (no-op end)
        raw_iref(ps, 0x10) &= 0xFFFFFFFD;
        raw_iref(ps, 0x0C) &= 0xFFFFFDFF;
        ps->weaponTime = 0;
        ps->weaponDelay = 0;

        if (T4M::TransitionToReadyOrLowReady(ps)) 
            return;

        ps->weaponstate = (weaponstate_t)0;
        if (ps->pm_type < 8) 
        {
            int v = raw_int(ps, 0x910);
            raw_iref(ps, 0x910) = (~v) & 0x200;
        }
        return;
    }

    // Queue notify entry 0x57
    int head = raw_int(ps, 0xD0) & 3;
    raw_iref(ps, 0xD4 + head * 4) = 0x57;
    int head2 = raw_int(ps, 0xD0) & 3;
    raw_iref(ps, 0xE4 + head2 * 4) = 0;
    raw_iref(ps, 0xD0) = (raw_int(ps, 0xD0) + 1) & 0xFF;
}


// =====================================================================
// Reconstructed weapon-drop handler — sub_41F010 (vanilla VA 0x0041F010).
// Invoked from T4_Reconstructed::PM_Weapon dispatcher when ps.weaponstate
// is WEAPON_DROPPING / WEAPON_DROPPING_QUICK. Mono-caller, no SafetyHook
// detour, no shim — direct C++ call from the dispatcher.
//
// Vanilla notes :
//   - `[pm+0x18]` = `pm->cmd.weapon` is a usercmd byte field. Read as byte
//     to match vanilla. Idx ≥ 256 truncates here (intrinsic to wire format) ;
//     the high byte is reconstructed from cmd.weapon_high (Phase 1a).
//   - The function chunk at vanilla loc_41EF40 is inlined as a tail block.
//   - Bitmaps at ps+0x7FC and ps+0x80C are accessed via T4M::PsBitTest/Set
//     which extends them from 128 to 512 bits via per-PS sidecar (cf.
//     PatchT4MAM_PsBitmapExt.cpp). Idx >= 128 stored in the sidecar.
// =====================================================================

// sub_41EBB0 — __usercall(eax = ps, [esp+4] = arg_0 idx, [esp+8] = arg_4 byte).
static __declspec(naked) void call_PM_Weapon_FireImpulse(
    playerState_s* /*ps*/, int /*arg_0*/, int /*arg_4*/)
{
    __asm
    {
        mov     eax, [esp+4]
        push    [esp+12]
        push    [esp+12]
        mov     ecx, 0x0041EBB0
        call    ecx
        add     esp, 8
        retn
    }
}

// sub_41EEF0 — __usercall(eax=ps, edx=var_10, xmm0=xmm0_val, [esp+4]=ebp_state,
// [esp+8]=var_C). Writes ps[+0x40] = edx, ps[+0x914] = xmm0.
static __declspec(naked) void call_PM_Weapon_FinalizeSwap(
    playerState_s* /*ps*/, int /*var_10_ptr*/, float /*xmm0_val*/,
    int /*ebp_state*/, int /*var_C*/)
{
    __asm
    {
        mov     eax, [esp+4]
        mov     edx, [esp+8]
        movss   xmm0, dword ptr [esp+12]
        push    [esp+20]
        push    [esp+20]
        mov     ecx, 0x0041EEF0
        call    ecx
        add     esp, 8
        retn
    }
}

// sub_41E6A0 — __usercall(ecx = ps), returns int.
static __declspec(naked) int call_PM_Weapon_TestState(playerState_s* /*ps*/)
{
    __asm
    {
        mov     ecx, [esp+4]
        mov     eax, 0x0041E6A0
        jmp     eax
    }
}

// sub_412BF0 — __usercall(eax = ps, ecx = arg).
static __declspec(naked) void call_PM_Weapon_QueueState(
    playerState_s* /*ps*/, int /*ecx_arg*/)
{
    __asm
    {
        mov     eax, [esp+4]
        mov     ecx, [esp+8]
        mov     edx, 0x00412BF0
        call    edx
        retn
    }
}

// sub_406D00 — __usercall(eax = struct DWORD_46DE3B0+0x1958C, ecx = ps[+0xF8]),
// returns ptr (eax).
static __declspec(naked) int call_PM_Weapon_LookupSwapStruct(
    int /*eax_struct*/, int /*ecx_arg*/)
{
    __asm
    {
        mov     eax, [esp+4]
        mov     ecx, [esp+8]
        mov     edx, 0x00406D00
        jmp     edx
    }
}

// sub_406E70 — __usercall(eax = 1, [esp+4]=ps, [esp+8]=eptr, [esp+C]=0, [esp+10]=0).
static __declspec(naked) void call_PM_Weapon_TriggerSwapEvent(
    int /*eax_in*/, void* /*ps*/, void* /*eptr*/, int /*a8*/, int /*aC*/)
{
    __asm
    {
        mov     eax, [esp+4]
        push    [esp+20]
        push    [esp+20]
        push    [esp+20]
        push    [esp+20]
        mov     ecx, 0x00406E70
        call    ecx
        add     esp, 16
        retn
    }
}

// sub_7AA7A8 — vanilla rand-ish; __cdecl(), returns int.
typedef int (__cdecl* call_PM_Weapon_RandTick_t)();
static const call_PM_Weapon_RandTick_t call_PM_Weapon_RandTick =
    (call_PM_Weapon_RandTick_t)0x007AA7A8;

// sub_41D6A0 — __cdecl(int).
typedef void (__cdecl* call_PM_Weapon_DispatchPickup_t)(int);
static const call_PM_Weapon_DispatchPickup_t call_PM_Weapon_DispatchPickup =
    (call_PM_Weapon_DispatchPickup_t)0x0041D6A0;

// Vanilla globals referenced by sub_41F010.
#define DWORD_8F0BE8     (*(void**)0x008F0BE8)        // SP/MP gate
#define DWORD_8E0C7C     ((uint32_t*)0x008E0C7C)      // weapon-class table
#define DWORD_8F71D4     (*(uint32_t*)0x008F71D4)
#define DWORD_46DE3BC    (*(uint32_t*)0x046DE3BC)     // max valid weapon idx
#define DWORD_46DE3B0    (*(uint32_t*)0x046DE3B0)     // base for per-weapon flag arrays
#define DWORD_1F552C0    (*(void**)0x01F552C0)
#define FLT_83DA58       (*(float*)0x0083DA58)
#define FLT_8AF224       (*(float*)0x008AF224)

// @faithful — full reconstruction of sub_41F010 + the loc_41EF40 tail chunk.
//
// Vanilla ABI : __usercall(ecx = pmove_t*, [esp+4] = arg_0 byte). Vanilla
// caller (sub_422160+0x2A5) does `cmp eax, 4; setz cl; push ecx; mov ecx, edi;
// call sub_41F010` — pushed dword has cl set (= isQuick) plus stack garbage
// in upper 3 bytes. Clean ABI here (we own the caller) : (pmove_t*, int isQuick).
extern "C" void T4_Reconstructed::PM_Weapon_Dropping(pmove_t* pm, int isQuick)
{
    auto sscr = (uint8_t*)pm;
    auto ps   = (uint8_t*)pm->ps;

    // ---- entry-time gate (sub_41F010+0..+2F) ----
    bool special_path_skip = false;
    {
        auto gate = (uint8_t*)DWORD_8F0BE8;
        if (gate && gate[0x10] != 0 && (ps[0x0C] & 4)) {
            uint32_t v = *(uint32_t*)(ps + 0x8B0);
            uint32_t edx = v * 3;
            if (DWORD_8E0C7C[edx] != 0xA) {
                special_path_skip = true;          // jump to loc_41F094
            }
        }
    }

    uint32_t edx;                                  // candidate new weapon idx

    if (special_path_skip || (ps[0x0C] & 8)) {
        // loc_41F094 : edx = 0
        edx = 0;
    } else {
        // @modified : vanilla lisait `movzx edx, byte ptr [ecx+18h]` (= cmd.weapon
        // byte) → tronquait idx ≥ 256. T4M étend via `cmd.weapon_high` à offset
        // +0x1B (= ex-padding, rempli par CL_FinishMove midhook + wire codec).
        // Combine low + high pour reconstruire l'idx jusqu'à 16 bits (0..0xFFFF).
        // Voir PatchT4MAM_CmdWeaponExt.cpp + plan_usercmd_weapon_word_extension.md.
        const uint32_t weaponLow  = *(uint8_t*)(sscr + 0x18);   // cmd.weapon
        const uint32_t weaponHigh = *(uint8_t*)(sscr + 0x1B);   // cmd.weapon_high (T4M)
        edx = weaponLow | (weaponHigh << 8);

        // Bitmap test 1 : ps_s ownedWeapons at +0x7FC. For idx >= 128 the
        // T4M sidecar bitmap (per-PS unordered_map) covers 128..511.
        bool owned = T4M::PsBitTest(pm->ps, 0x7FC, edx);

        if (!owned
            || (ps[0x10] & 0x80)
            || (((*(uint32_t*)(ps + 0xCC) & 0x4000) != 0)
                && *(uint32_t*)((uint8_t*)bg_weaponDefs[edx] + 0x5F0) == 0)) {
            edx = 0;
        } else {
            // Range check: edx < dword_46DE3BC + 1 ?
            if (edx >= DWORD_46DE3BC + 1) edx = 0;
        }
    }

    // loc_41F096 : refined bitmap re-test via unified helper.
    if (!T4M::PsBitTest(pm->ps, 0x7FC, edx)) {
        edx = 0;
    }

    // loc_41F0B2 : write the new ps.weapon. @faithful — vanilla `mov ecx,
    // [esi+104h]; movzx eax, dl; mov [esi+104h], eax`. Since edx is already
    // byte-sized (from the byte read at +0x18 above), the movzx is a no-op
    // here ; we just store the dword value.
    uint32_t old_idx = *(uint32_t*)(ps + 0x104);
    *(uint32_t*)(ps + 0x104) = edx;

    T4::WeaponDef* new_def = bg_weaponDefs[edx];
    T4::WeaponDef* old_def = bg_weaponDefs[old_idx];
    auto new_def_b = (uint8_t*)new_def;
    auto old_def_b = (uint8_t*)old_def;

    // Cl flag at loc_41F0F4 area.
    bool cl_flag = (*(uint32_t*)(new_def_b + 0x140) != DWORD_8F71D4)
                && (*(uint32_t*)(new_def_b + 0x5F0) == 0);

    // Special branch : new_def[+0x144] == 1 AND DWORD_1F552C0[+0x10] != 0
    if (*(uint32_t*)(new_def_b + 0x144) == 1) {
        auto edi = (uint8_t*)DWORD_1F552C0;
        if (edi && edi[0x10] != 0) {
            cl_flag = false;
            goto L_loc_41F148;
        }
    }

    // loc_41F109 — alt-firemode swap detection.
    if (cl_flag && old_def_b && *(uint32_t*)(old_def_b + 0x154) == 3) {
        uint32_t alt_idx = *(uint32_t*)(old_def_b + 0x63C);
        if (edx != alt_idx) {
            // Swap ps.weapon to old's alt-firemode idx, clear weaponstate,
            // call sub_41EBB0 with the requested new idx, return DIRECTLY
            // (vanilla loc_41F141 path — never touches the chunk).
            *(uint32_t*)(ps + 0x104) = alt_idx;
            *(uint32_t*)(ps + 0x108) = 0;
            call_PM_Weapon_FireImpulse((playerState_s*)ps, (int)edx, 0);
            return;
        }
    }

L_loc_41F148:
    // loc_41F148 — same-weapon vs different-weapon split.
    if (old_idx == edx) {
        *(uint32_t*)(ps + 0x108) = 0;
        if (*(uint32_t*)(ps + 4) >= 8) {
            return;                          // vanilla : jge loc_41F141; pop; retn
        }
        *(uint32_t*)(ps + 0x910) = ~*(uint32_t*)(ps + 0x910) & 0x200;
        return;                              // vanilla : pop; retn
    }

    // loc_41F179 — different weapon : full state-machine transition.
    {
        bool wasUnset     = !T4M::PsBitTest(pm->ps, 0x80C, edx);
        uint32_t edi_flag = wasUnset ? 1 : 0;
        T4M::PsBitSet(pm->ps, 0x80C, edx);

        int   arg_flag   = isQuick;          // [esp+arg_0] in vanilla : initial = caller's setz cl
        int   var_C      = 0;
        void* var_10_ptr = nullptr;
        int   ebp_state  = 0;
        float xmm0_val   = FLT_8AF224;       // var_4 in vanilla : default (loc_41F23E)

        if (old_idx == 0) {
            var_C = 0;
            if (*(uint32_t*)(new_def_b + 0x144) == 2) {
                arg_flag = 1;
            }
        } else {
            // loc_41F21B — old_idx != 0 : detect alt-firemode swap.
            if (edx == 0) {
                var_C = 0;
            } else {
                auto old_def_for_alt = (uint8_t*)bg_weaponDefs[old_idx];
                if (old_def_for_alt
                    && edx == *(uint32_t*)(old_def_for_alt + 0x63C)) {
                    var_C = 1;
                } else {
                    var_C = 0;
                }
            }
        }

        // loc_41F1D0 — common path.
        if (*(uint32_t*)(old_def_b + 0x5F0) != 0
         || *(uint32_t*)(new_def_b + 0x5F0) != 0) {
            arg_flag = 1;
        }

        // loc_41F1E9 — if var_C set, alt-firemode swap → loc_41F323.
        if (var_C != 0) {
            var_10_ptr   = (void*)*(uint32_t*)(new_def_b + 0x490);
            float ps_914 = *(float*)(ps + 0x914);
            xmm0_val     = (FLT_83DA58 > ps_914) ? FLT_83DA58 : ps_914;
            ebp_state    = 0x12;
            goto L_loc_41F323;
        }

        // loc_41F23E — var_C == 0, set xmm0 default and pick var_10/ebp_state.
        xmm0_val = FLT_8AF224;
        if (*(uint32_t*)(new_def_b + 0x5F4) != 0) {
            var_10_ptr = nullptr;
            goto L_loc_41F2AE;
        }

        // loc_41F25A
        if (call_PM_Weapon_TestState((playerState_s*)ps) != 0) {
            var_10_ptr = (void*)*(uint32_t*)(new_def_b + 0x4A0);
            ebp_state  = 0x16;
            goto L_loc_41F2AE;
        }

        // loc_41F272
        if (edi_flag != 0) {
            var_10_ptr = (void*)*(uint32_t*)(new_def_b + 0x49C);
            ebp_state  = 0x0C;
            goto L_loc_41F2AE;
        }

        // loc_41F287
        if (arg_flag != 0) {
            var_10_ptr = (void*)*(uint32_t*)(new_def_b + 0x498);
            ebp_state  = 0x14;
            goto L_loc_41F2AE;
        }

        // loc_41F29F / loc_41F2AA — default.
        var_10_ptr = (void*)*(uint32_t*)(new_def_b + 0x488);
        ebp_state  = 0x0B;

    L_loc_41F2AE:
        if (old_idx != 0) {
            int sw_arg = (edi_flag != 0) ? 0x18 : 0x17;
            call_PM_Weapon_QueueState((playerState_s*)ps, sw_arg);

            if (*(uint32_t*)(ps + 4) < 8) {
                uint32_t struct_va = DWORD_46DE3B0 + 0x1958C;
                if (*(uint32_t*)struct_va != 0) {
                    int   sub_res = call_PM_Weapon_LookupSwapStruct(
                                        (int)struct_va, *(int*)(ps + 0xF8));
                    auto  edi_ptr = (uint8_t*)(uintptr_t)sub_res;
                    if (edi_ptr && *(uint32_t*)(edi_ptr + 0x34) != 0) {
                        uint32_t rnd  = (uint32_t)call_PM_Weapon_RandTick();
                        uint32_t mod  = rnd % *(uint32_t*)(edi_ptr + 0x34);
                        void*    eptr = edi_ptr + (mod * 5) * 4 + 0x38;
                        if (eptr) {
                            call_PM_Weapon_TriggerSwapEvent(1, ps, eptr, 0, 0);
                        }
                    }
                }
            }
        }

    L_loc_41F323:
        if (*(uint32_t*)(new_def_b + 0x5F4) != 0) {
            *(uint32_t*)(ps + 0x40)  = (uint32_t)(uintptr_t)var_10_ptr;
            *(uint32_t*)(ps + 0x108) = (uint32_t)ebp_state;
            *(float*)   (ps + 0x914) = xmm0_val;
            uint32_t flags0C = *(uint32_t*)(ps + 0x0C);
            if (flags0C & 1) {
                *(uint32_t*)(ps + 0x0C) = flags0C | 0x200;
            }
            // loc_41F350
            if (*(uint32_t*)(ps + 4) < 8) {
                *(uint32_t*)(ps + 0x910) = ~*(uint32_t*)(ps + 0x910) & 0x200;
            }
        } else {
            // loc_41F36C
            call_PM_Weapon_FinalizeSwap((playerState_s*)ps,
                                        (int)(uintptr_t)var_10_ptr, xmm0_val,
                                        ebp_state, var_C);
        }

        // loc_41F380 — per-weapon flag arrays at DWORD_46DE3B0 + (f8 * 0x594) + 0x87EA8/B0.
        {
            uint32_t f8       = *(uint32_t*)(ps + 0xF8);
            uint32_t edi_base = DWORD_46DE3B0;

            // Array A keyed by new_def[+0x140]
            {
                uint32_t base = edi_base + f8 * 0x594;
                uint32_t k    = *(uint32_t*)(new_def_b + 0x140);
                *(uint32_t*)(base + 0x87EA8) = 0;
                *(uint32_t*)(base + 0x87EAC) = 0;
                uint32_t bw  = k >> 5;
                uint32_t bb  = 1u << (k & 0x1F);
                *(uint32_t*)(base + bw * 4 + 0x87EA8) |= bb;
            }

            // Array B keyed by new_def[+0x148]
            {
                uint32_t base = edi_base + f8 * 0x594;
                uint32_t k    = *(uint32_t*)(new_def_b + 0x148);
                *(uint32_t*)(base + 0x87EB0) = 0;
                *(uint32_t*)(base + 0x87EB4) = 0;
                uint32_t bw  = k >> 5;
                uint32_t bb  = 1u << (k & 0x1F);
                *(uint32_t*)(base + bw * 4 + 0x87EB0) |= bb;
            }
        }
        // fall through to chunk
    }

L_chunk_41EF40:
    // ---- inlined loc_41EF40 chunk (vanilla : eax=old_idx, ecx=ps) ----
    if (old_idx == 0) return;
    {
        if (!T4M::PsBitTest(pm->ps, 0x7FC, old_idx)) return;

        auto odb = (uint8_t*)bg_weaponDefs[old_idx];
        if (!odb || *(uint32_t*)(odb + 0x5EC) == 0) return;

        uint32_t off3FC = *(uint32_t*)(odb + 0x3FC);
        if (*(uint32_t*)(ps + off3FC * 4 + 0x5FC) != 0) return;

        uint32_t off3F4 = *(uint32_t*)(odb + 0x3F4);
        if (*(uint32_t*)(ps + off3F4 * 4 + 0x17C) != 0) return;

        if (*(uint32_t*)(odb + 0x6AC) != 0) return;

        call_PM_Weapon_DispatchPickup(0);
    }
}

#undef DWORD_8F0BE8
#undef DWORD_8E0C7C
#undef DWORD_8F71D4
#undef DWORD_46DE3BC
#undef DWORD_46DE3B0
#undef DWORD_1F552C0
#undef FLT_83DA58
#undef FLT_8AF224


// =====================================================================
// Reconstructed reload handlers — sub_41F630 (RELOAD_START) and
// sub_41F780 (RELOADING). Both cdecl(pm, callValidation). They share the
// chunk @ 0x0041E940 (call_PM_Weapon_ReloadFinalize) for the "fast-reload" branch.
// =====================================================================

// @faithful — sub_41F630 (entry 0x0041F630). RELOAD_START / RELOAD_START_INTERUPT.
extern "C" void T4_Reconstructed::PM_Weapon_ReloadStart(pmove_t* pm, int callValidation)
{
    auto* ps = *(playerState_s**)pm;
    const WeaponDef* w = GetWeaponDef(ps->weapon);

    if (callValidation != 0) 
    {
        call_PM_Weapon_RefreshReload(ps);
    }

    // loc_41F654
    if (ps->weaponTime != 0) 
        return;

    // Force INTERUPT state if weapon supports interruptible reload AND attack pressed
    if (w->bSegmentedReload != 0 && (pm->cmd.buttons & 1) != 0) 
    {
        ps->weaponstate = (weaponstate_t)0xA;       // RELOAD_START_INTERUPT
    }

    // loc_41F678
    if (ps->weaponstate == 0xA) 
    {
        const WeaponDef* w2 = GetWeaponDef(ps->weapon);
        int idx = w2->iClipIndex;
        if (raw_int(ps, 0x5FC + idx * 4) != 0) 
        {
            goto path_clear_bit;                     // jmp loc_41F6B0
        }
    }

    // loc_41F69D — try fast-reload chunk
    if (call_PM_Weapon_CheckFastReload(ps) != 0) 
    {
        call_PM_Weapon_ReloadFinalize(ps);                       // tail-call shared chunk
        return;
    }

path_clear_bit:
    // loc_41F6B0 — clear weapon-pickup bit in [+0x81C + (weapon>>5)*4]
    {
        unsigned int wIdx = (unsigned int)ps->weapon;
        raw_iref(ps, 0x81C + (wIdx >> 5) * 4) &= ~(1u << (wIdx & 0x1F));
    }

    if (w->iReloadEndTime != 0) 
    {
        // loc_41F6FF: state = RELOAD_END (0xB), submit anim event 0x10, queue notify 0x14
        ps->weaponstate = (weaponstate_t)0xB;
        if (ps->pm_type < 8) 
        {
            int v = raw_int(ps, 0x910);
            raw_iref(ps, 0x910) = ((~v) & 0x200) | 0x10;
        }
        ps->weaponTime = w->iReloadEndTime;
        int head = raw_int(ps, 0xD0) & 3;
        raw_iref(ps, 0xD4 + head * 4) = 0x14;
        int head2 = raw_int(ps, 0xD0) & 3;
        raw_iref(ps, 0xE4 + head2 * 4) = 0;
        raw_iref(ps, 0xD0) = (raw_int(ps, 0xD0) + 1) & 0xFF;
        return;
    }

    // loc_41F749: state = READY (0)
    if (T4M::TransitionToReadyOrLowReady(ps)) 
        return;

    ps->weaponstate = (weaponstate_t)0;

    if (ps->pm_type >= 8) 
        return;

    int v = raw_int(ps, 0x910);
    raw_iref(ps, 0x910) = (~v) & 0x200;
}

// @faithful — sub_41F780 (entry 0x0041F780). RELOADING / RELOADING_INTERUPT.
extern "C" void T4_Reconstructed::PM_Weapon_Reloading(pmove_t* pm, int callValidation)
{
    auto* ps = *(playerState_s**)pm;
    const WeaponDef* w = GetWeaponDef(ps->weapon);

    if (callValidation != 0) 
    {
        call_PM_Weapon_RefreshReload(ps);

        if (ps->weaponTime != 0) 
            return;          // bail to loc_41F8BE
    }

    // loc_41F7AE
    if (ps->weaponTime != 0) 
        return;

    // Force INTERUPT state if weapon supports interruptible reload AND attack pressed
    if (w->bSegmentedReload != 0 && (pm->cmd.buttons & 1) != 0) 
        ps->weaponstate = (weaponstate_t)8;          // RELOADING_INTERUPT

    // loc_41F7D2 — clear weapon-pickup bit
    {
        unsigned int wIdx = (unsigned int)ps->weapon;
        raw_iref(ps, 0x81C + (wIdx >> 5) * 4) &= ~(1u << (wIdx & 0x1F));
    }

    if (w->bSegmentedReload == 0) 
    {
        // loc_41F89B: state = READY

        if (T4M::TransitionToReadyOrLowReady(ps)) 
            return;

        ps->weaponstate = (weaponstate_t)0;

        if (ps->pm_type >= 8) 
            return;

        int v = raw_int(ps, 0x910);
        raw_iref(ps, 0x910) = (~v) & 0x200;
        return;
    }

    if (ps->weaponstate != 8) 
    {
        if (call_PM_Weapon_CheckFastReload(ps) != 0) 
        {
            call_PM_Weapon_ReloadFinalize(ps);                   // tail-call shared chunk
            return;
        }
    }

    // loc_41F81A
    if (w->iReloadEndTime != 0) 
    {
        ps->weaponstate = (weaponstate_t)0xB;        // RELOAD_END
        call_PM_Weapon_SubmitAnimEvent(ps, 0x10);
        ps->weaponTime = w->iReloadEndTime;
        int head = raw_int(ps, 0xD0) & 3;
        raw_iref(ps, 0xD4 + head * 4) = 0x14;
        int head2 = raw_int(ps, 0xD0) & 3;
        raw_iref(ps, 0xE4 + head2 * 4) = 0;
        raw_iref(ps, 0xD0) = (raw_int(ps, 0xD0) + 1) & 0xFF;
        return;
    }

    // loc_41F883: state = READY, submit anim event 0
    if (T4M::TransitionToReadyOrLowReady(ps)) 
        return;

    ps->weaponstate = (weaponstate_t)0;
    call_PM_Weapon_SubmitAnimEvent(ps, 0);
}


// =====================================================================
// Reconstructed chunks (@faithful)
// Each chunk is reachable only from PM_Weapon's switch via tail-call from
// the dispatch label. The dispatch label sets up a register (eax = ps, or
// eax/ecx = pm); we receive the value as a normal cdecl arg.
// =====================================================================

// @faithful — chunk @ 0x00420CE0 — MELEE_FIRE → MELEE_END transition.
extern "C" void T4_Reconstructed::PM_Weapon_MeleeFire(playerState_s* ps)
{
    const WeaponDef* w = GetWeaponDef(ps->weapon);

    if (w->bayonet != 0) 
    {
        ps->weaponstate = (weaponstate_t)0xF;
        ps->weaponTime = 0x12C;
        ps->weaponDelay = 0;
        return;
    }

    if (w->knifeModel == 0) 
    {
        // Path C: tail-call sub_4225D0
        call_PM_Weapon_FinalizeStateExit(ps);
        return;
    }

    ps->weaponstate = (weaponstate_t)0xF;
    ps->weaponTime = w->quickRaiseTime;
    ps->weaponDelay = 0;

    if (raw_int(ps, 0x944) < 3 && ps->pm_type < 8) 
    {
        int v = raw_int(ps, 0x910);
        raw_iref(ps, 0x910) = ((~v) & 0x200) | 0x14;
    }

    int v0C = raw_int(ps, 0x0C);
    if ((v0C & 1) != 0) 
    {
        raw_iref(ps, 0x0C) = v0C | 0x200;
    }
}

// @faithful — chunk @ 0x00420D70 — MELEE_CHARGE → MELEE_INIT transition.
extern "C" void T4_Reconstructed::PM_Weapon_MeleeCharge(playerState_s* ps)
{
    const WeaponDef* w = GetWeaponDef(ps->weapon);
    ps->weaponTime = w->meleeChargeTime;
    ps->weaponDelay = w->meleeChargeDelay;

    if (ps->pm_type < 8) 
    {
        int v = raw_int(ps, 0x910);
        raw_iref(ps, 0x910) = ((~v) & 0x200) | 9;
    }

    // Queue notify entry 0x20 with parm 0
    int head = raw_int(ps, 0xD0) & 3;
    ps->weaponstate = (weaponstate_t)0xD;
    raw_iref(ps, 0xD4 + head * 4) = 0x20;
    int head2 = raw_int(ps, 0xD0) & 3;
    raw_iref(ps, 0xE4 + head2 * 4) = 0;
    raw_iref(ps, 0xD0) = (raw_int(ps, 0xD0) + 1) & 0xFF;

    int v0C = raw_int(ps, 0x0C);
    if ((v0C & 1) != 0) 
    {
        raw_iref(ps, 0x0C) = v0C | 0x200;
    }
}

// @faithful — chunk @ 0x00420E10 — MELEE_INIT → MELEE_FIRE transition.
extern "C" void T4_Reconstructed::PM_Weapon_MeleeInit(playerState_s* ps)
{
    int head = raw_int(ps, 0xD0) & 3;
    ps->weaponstate = (weaponstate_t)0xE;
    raw_iref(ps, 0xD4 + head * 4) = 0x21;
    int head2 = raw_int(ps, 0xD0) & 3;
    raw_iref(ps, 0xE4 + head2 * 4) = 0;
    raw_iref(ps, 0xD0) = (raw_int(ps, 0xD0) + 1) & 0xFF;

    int v0C = raw_int(ps, 0x0C);
    if ((v0C & 1) != 0) 
    {
        raw_iref(ps, 0x0C) = v0C | 0x200;
    }
}

// @faithful — chunk @ 0x004213D0 — OFFHAND_PREPARE → OFFHAND_START transition.
extern "C" void T4_Reconstructed::PM_Weapon_OffhandPrepare(playerState_s* ps)
{
    raw_iref(ps, 0x10) |= 2;
    ps->weaponstate = (weaponstate_t)0x13;
    ps->weaponTime = 0;
    ps->weaponDelay = 0;

    if (raw_int(ps, 0x4C) != 0x3FF) 
        return;

    const WeaponDef* w = GetWeaponDef(raw_int(ps, 0xFC));
    raw_iref(ps, 0x48) = w->fuseTime;
}

// @faithful — chunk @ 0x00421410 — OFFHAND_START → OFFHAND_HOLD transition.
//   Special case: if w->[+0x6B8] == 0 AND pmove_t.[+0x40] & 0xC000 AND pmove_t.[+0x8] & 0xC000
//   → set fireTime=1 and do NOT transition.
extern "C" void T4_Reconstructed::PM_Weapon_OffhandStart(pmove_t* pm)
{
    auto* ps = *(playerState_s**)pm;
    const WeaponDef* w = GetWeaponDef(raw_int(ps, 0xFC));   // offHandIndex

    if (w->holdButtonToThrow == 0
        && (pm->oldcmd.buttons & 0xC000) != 0
        && (pm->cmd.buttons  & 0xC000) != 0) 
    {
        // Inhibit transition: set fireTime=1 and exit
        ps->weaponDelay = 1;
        return;
    }

    // Standard transition path
    ps->weaponstate = (weaponstate_t)0x12;
    ps->weaponTime = w->iFireTime;
    raw_iref(ps, 0x10) |= 2;
    ps->weaponDelay = w->iFireDelay;

    if (ps->pm_type >= 8)
        return;

    int v = raw_int(ps, 0x910);
    raw_iref(ps, 0x910) = ((~v) & 0x200) | 2;

    if (ps->pm_type >= 8)
        return;

    // Vanilla: mov eax, dword_46DE3B0; add eax, 18B78h; cmp [eax], 0; jz skip
    // dword_46DE3B0 holds a pointer; we deref then add 0x18B78 to get an event-list struct.
    char* clientBase = *(char**)0x046DE3B0;
    int* eventListBase = (int*)(clientBase + 0x18B78);
    if (*eventListBase != 0) 
    {
        int offhand_x_F8 = raw_int(ps, 0xF8);

        __asm 
        {
            push    esi
            push    edi
            push    ebx

            ; sub_406D00 is __usercall(eax = list_ptr, ecx = idx, [esp+4] = consumed).
            ; Use EDX as the call target so EAX is preserved as the input struct.
            push    0                          ; dummy stack arg (vanilla pushes edi=random)
            mov     eax, eventListBase         ; eax = events list
            mov     ecx, offhand_x_F8          ; ecx = ps[+0xF8] (offhand-related index)
            mov     edx, 0x00406D00
            call    edx
            add     esp, 4                     ; clean dummy
            test    eax, eax
            jz      done
            mov     edi, eax                   ; edi = eventInfo
            mov     ecx, [edi + 34h]
            test    ecx, ecx
            jz      done

            mov     edx, 0x007AA7A8
            call    edx
            cdq
            idiv    dword ptr [edi + 34h]
            lea     edx, [edx + edx*4]
            lea     eax, [edi + edx*4 + 38h]
            test    eax, eax
            jz      done

            push    1
            push    0
            push    eax
            push    [ps]
            mov     eax, 1
            mov     edx, 0x00406E70
            call    edx
            add     esp, 10h

done:
            pop     ebx
            pop     edi
            pop     esi
        }
    }
}

// @faithful — chunk @ 0x004214E0 — OFFHAND_HOLD → SWIM_IN or OFFHAND.
extern "C" void T4_Reconstructed::PM_Weapon_OffhandHold(pmove_t* pm)
{
    auto* ps = *(playerState_s**)pm;
    int head = raw_int(ps, 0xD0) & 3;
    int offhand = raw_int(ps, 0xFC) & 0xFFFF;

    if (raw_int(ps, 0x944) >= 3) 
    {
        // Path A: queue 0x55 + transition to SWIM_IN (0x1D)
        raw_iref(ps, 0xD4 + head * 4) = 0x55;
        int head2 = raw_int(ps, 0xD0) & 3;
        raw_iref(ps, 0xE4 + head2 * 4) = offhand;
        raw_iref(ps, 0xD0) = (raw_int(ps, 0xD0) + 1) & 0xFF;
        raw_iref(ps, 0x10) = (raw_int(ps, 0x10) & 0xFFFFFFFD) | 0x2000;
        raw_iref(ps, 0x0C) &= 0xFFFFFDFF;
        ps->weaponstate = (weaponstate_t)0x1D;
        return;
    }

    // Path B/C: queue 0x27 first
    raw_iref(ps, 0xD4 + head * 4) = 0x27;
    int head2b = raw_int(ps, 0xD0) & 3;
    raw_iref(ps, 0xE4 + head2b * 4) = offhand;
    raw_iref(ps, 0xD0) = (raw_int(ps, 0xD0) + 1) & 0xFF;

    if (raw_int(ps, 0x4C) != 0x3FF) 
    {
        // loc_421618: state = OFFHAND
        raw_iref(ps, 0x10) |= 2;
        ps->weaponstate = (weaponstate_t)0x14;
        return;
    }

    // [+0x4C] == 0x3FF
    const WeaponDef* w = GetWeaponDef(raw_int(ps, 0xFC));
    int wOff_3FC = w->iClipIndex;
    int wOff_3F4 = w->iAmmoIndex;
    int sum = raw_int(ps, 0x5FC + wOff_3FC * 4) + raw_int(ps, 0x17C + wOff_3F4 * 4);

    // Vanilla: cmp sum, 0; jnz loc_4215FF (sum!=0 → call sub_41E5E0 path).
    // Fall-through (sum==0) → queue 0x0F.
    if (sum == 0) 
    {
        // Path: queue 0x0F + state = OFFHAND
        int head3 = raw_int(ps, 0xD0) & 3;
        raw_iref(ps, 0xD4 + head3 * 4) = 0x0F;
        int head4 = raw_int(ps, 0xD0) & 3;
        raw_iref(ps, 0xE4 + head4 * 4) = sum;
        raw_iref(ps, 0xD0) = (raw_int(ps, 0xD0) + 1) & 0xFF;
        raw_iref(ps, 0x10) |= 2;
        ps->weaponstate = (weaponstate_t)0x14;
        return;
    }

    // sum != 0 (loc_4215FF)
    if ((raw_byte(ps, 0x8E8) & 1) == 0) 
    {
        // sub_41E5E0 is __usercall(edi=ps, esi=cap, [esp+4]=offhandIdx).
        int offhandIdx = raw_int(ps, 0xFC);
        __asm {
            push    esi
            push    edi
            push    ebx
            push    offhandIdx                 ; arg_0 = ps[+0xFC]
            mov     edi, ps
            mov     esi, 1
            mov     eax, 0x0041E5E0
            call    eax
            add     esp, 4
            pop     ebx
            pop     edi
            pop     esi
        }
    }

    raw_iref(ps, 0x10) |= 2;
    ps->weaponstate = (weaponstate_t)0x14;
}

// @faithful — chunk @ 0x00421C40 — SWIM_IN tick: anim flag toggle.
extern "C" void T4_Reconstructed::PM_Weapon_SwimIn(playerState_s* ps)
{
    int head = raw_int(ps, 0xD0) & 3;
    raw_iref(ps, 0xD4 + head * 4) = 3;
    int head2 = raw_int(ps, 0xD0) & 3;
    raw_iref(ps, 0xE4 + head2 * 4) = 0;
    raw_iref(ps, 0xD0) = (raw_int(ps, 0xD0) + 1) & 0xFF;

    if ((raw_int(ps, 0x10) & 0x2000) != 0) 
    {
        if (ps->pm_type >= 8) 
            return;

        int v = raw_int(ps, 0x910);
        raw_iref(ps, 0x910) = ((~v) & 0x200) | 0xA;

        return;
    }

    int v910 = raw_int(ps, 0x910);

    if ((v910 & 0xFFFFFDFF) == 0xA) 
        return;

    if (ps->pm_type >= 8) 
        return;

    raw_iref(ps, 0x910) = ((~v910) & 0x200) | 0xA;
}

// @faithful — chunk @ 0x00421CD0 — SWIM_OUT tick: sound notify code 1.
extern "C" void T4_Reconstructed::PM_Weapon_SwimOut(playerState_s* ps)
{
    int head = raw_int(ps, 0xD0) & 3;
    raw_iref(ps, 0xD4 + head * 4) = 1;
    int head2 = raw_int(ps, 0xD0) & 3;
    raw_iref(ps, 0xE4 + head2 * 4) = 0;
    raw_iref(ps, 0xD0) = (raw_int(ps, 0xD0) + 1) & 0xFF;

    if (ps->pm_type >= 8) 
        return;

    int v = raw_int(ps, 0x910);
    raw_iref(ps, 0x910) = ((~v) & 0x200) | 0xB;
}

// =====================================================================
// T4M lowReady helpers (BO1-style stance)
// Bit `eFlags & 0x400` = persistent intent ("user wants low-ready").
// Decoupled from blocking: the actual block is via weaponstate dispatch.
// =====================================================================

void T4M::EnterLowReadyStart(playerState_s* ps)
{
    const WeaponDef* w = GetWeaponDef(ps->weapon);
    ps->weaponstate = WEAPON_LOWREADY_START;
    ps->weaponTime = w->iLowReadyInTime;
    ps->weaponDelay = 0;
    if (ps->pm_type < 8) 
    {
        int v = raw_int(ps, 0x910);
        raw_iref(ps, 0x910) = ((~v) & 0x200) | 0x20;
    }
}

void T4M::EnterLowReadyEnd(playerState_s* ps)
{
    const WeaponDef* w = GetWeaponDef(ps->weapon);
    ps->weaponstate = WEAPON_LOWREADY_END;
    ps->weaponTime = w->iLowReadyOutTime;
    ps->weaponDelay = 0;
    if (ps->pm_type < 8) 
    {
        int v = raw_int(ps, 0x910);
        raw_iref(ps, 0x910) = ((~v) & 0x200) | 0x22;
    }
}

// Exit-point helper: observes intent bit. Returns true if transitioned to LOWREADY_START
// (caller should early-return to skip the vanilla READY post-processing).
// Returns false if intent bit not set; caller proceeds with vanilla READY transition.
bool T4M::TransitionToReadyOrLowReady(playerState_s* ps)
{
    if ((ps->eFlags & 0x400) == 0) 
        return false;

    EnterLowReadyStart(ps);
    return true;
}

// Called from GScr_SetLowReady (script intent change).
// Force immediate transition for ALL non-LowReady states. Rationale: the
// SECTION 0 prologue catch only fires when PM_Weapon detour intercepts the
// right ps. In SP/zombies the rendering scratch ps (dword_351DF50) gets
// copied from cg.snap.ps each frame; if the server's ps doesn't transition,
// the scratch is repeatedly reset to READY. By writing weaponstate directly
// on the server-side ps in setLowReady, the snap reflects LOWREADY_START
// immediately and the cycle is broken.
void T4M::SetLowReadyIntent(playerState_s* ps, bool enable)
{
    if (enable) 
    {
        ps->eFlags |= 0x400;
        // Force immediate transition unless already in a LowReady state.
        if (ps->weaponstate < WEAPON_LOWREADY_START || ps->weaponstate > WEAPON_LOWREADY_END) 
        {
            EnterLowReadyStart(ps);
        }

    } 
    else 
    {
        ps->eFlags &= ~0x400;
        if (ps->weaponstate == WEAPON_LOWREADY_LOOP) 
        {
            EnterLowReadyEnd(ps);
        }
        // For START/END states: let timer expire naturally (handlers observe bit).
    }
}

// =====================================================================
// LOWREADY state tick handlers — dispatched from PM_Weapon main switch.
// =====================================================================

// State LOWREADY_START tick — fires when iLowReadyInTime timer expires.
// Vanilla pattern (cf. SprintRaise): set next state + reset timer + vm-anim-state.
void T4M::PM_Weapon_LowReady_Start(playerState_s* ps)
{
    const WeaponDef* w = GetWeaponDef(ps->weapon);
    ps->weaponstate = WEAPON_LOWREADY_LOOP;
    ps->weaponTime = w->iLowReadyLoopTime;   // 0 = infinite (LOOP case is no-op)
    ps->weaponDelay = 0;
    if (ps->pm_type < 8) 
    {
        int v = raw_int(ps, 0x910);
        raw_iref(ps, 0x910) = ((~v) & 0x200) | 0x21;
    }
}

// State LOWREADY_END tick — fires when iLowReadyOutTime timer expires.
// If intent re-set during END: jump back to START (avoid stale READY).
// Else: full exit to READY.
void T4M::PM_Weapon_LowReady_End(playerState_s* ps)
{
    if ((ps->eFlags & 0x400) != 0) 
    {
        EnterLowReadyStart(ps);
        return;
    }
    ps->weaponstate = WEAPON_READY;
    ps->weaponTime = 0;
    ps->weaponDelay = 0;
    if (ps->pm_type < 8) 
    {
        int v = raw_int(ps, 0x910);
        raw_iref(ps, 0x910) = (~v) & 0x200;
    }
}

// @faithful — chunk @ 0x00421D80 — SPRINT_RAISE → SPRINT_LOOP transition.
extern "C" void T4_Reconstructed::PM_Weapon_SprintRaise(playerState_s* ps)
{
    ps->weaponstate = (weaponstate_t)0x18;
    ps->weaponTime = 0;
    ps->weaponDelay = 0;

    if (ps->pm_type >= 8) 
        return;

    int v = raw_int(ps, 0x910);
    raw_iref(ps, 0x910) = ((~v) & 0x200) | 0x18;
}

// =====================================================================
// Shared chunks — kept as register-preserving stubs (vanilla call).
// =====================================================================

extern "C" __declspec(naked) void T4_Reconstructed::PM_Weapon_OffhandInit_Stub(playerState_s* /*ps*/)
{
    __asm 
    {
        push    esi
        push    edi
        push    ebx
        mov     eax, [esp+10h]
        mov     edx, 0x00421300
        call    edx
        pop     ebx
        pop     edi
        pop     esi
        ret
    }
}

extern "C" __declspec(naked) void T4_Reconstructed::PM_Weapon_Offhand_Stub(playerState_s* /*ps*/)
{
    __asm 
    {
        push    esi
        push    edi
        push    ebx
        mov     eax, [esp+10h]
        mov     edx, 0x00421630
        call    edx
        pop     ebx
        pop     edi
        pop     esi
        ret
    }
}


// =====================================================================
// Main reconstruction — sub_422160 (PM_Weapon)  @modified
// =====================================================================

extern "C" void T4_Reconstructed::PM_Weapon(pmove_t* pm, int callValidation)
{
    auto* ps = *(playerState_s**)pm;

    // SECTION 0: Prologue early-out
    if (ps->pm_type >= 8)
    {
        ps->weapon = 0;
        return;
    }

    // T4M: block ADS + grenades while lowReady intent is set. Mapping confirmed:
    //   BUTTON_ADS              = 0x00000800 (bit 11)
    //   BUTTON_TOGGLEADS_THROW  = 0x00080000 (bit 19) — combo ADS+throw bind
    //   BUTTON_FRAG             = 0x00004000 (bit 14) — frag grenade
    //   BUTTON_OFFHAND_SMOKE    = 0x00008000 (bit 15) — smoke / special grenade
    //   BUTTON_OFFHAND_THROW    = 0x00400000 (bit 22) — release of charged offhand
    //   (BUTTON_MELEE = 0x4 KEPT — melee remains usable in lowReady)
    //   (BUTTON_ATTACK = 0x1 KEPT — fire/click left intact, blocking handled by state machine)
    if ((ps->eFlags & 0x400) != 0)
    {
        const int kClearMask = 0x800 | 0x4000 | 0x8000 | 0x80000 | 0x400000;
        pm->cmd.buttons = (T4::button_mask)(pm->cmd.buttons & ~kClearMask);
    }

    if ((ps->eFlags & 0x400) != 0 
            && (ps->weaponstate == WEAPON_READY
            || ps->weaponstate == WEAPON_LOWREADY_START
            || ps->weaponstate == WEAPON_LOWREADY_END))
    {
        ps->weaponstate = WEAPON_LOWREADY_LOOP;
        ps->weaponTime = 0;
        ps->weaponDelay = 0;

        if (ps->pm_type < 8) 
        {
            int v = raw_int(ps, 0x910);
            raw_iref(ps, 0x910) = ((~v) & 0x200) | 0x21;
        }
    }

    // SECTION 1: First validation cluster
    if (call_IsWeaponInputBlocked(ps, callValidation) != 0) 
        return;

    if (raw_int(ps, 0x0C)  & 0x400)
        return;

    if (ps->eFlags & 0x300) 
        return;

    // SECTION 2: Swim transitions (when [+0x944] < 3)
    if (raw_int(ps, 0x944) < 3)
    {
        int state = ps->weaponstate;
        if (state == 0x1D) 
        {
            ps->weaponstate = WEAPON_SWIM_OUT;
        } 
        else if (state == 0x1E) 
        {
            if (!T4M::TransitionToReadyOrLowReady(ps))
                ps->weaponstate = WEAPON_READY;
        }
        goto dispatch_setup;
    }

    // SECTION 3: Validation cluster #2
    {
        int prev = call_PM_Weapon_AnimCmdState_Resync(callValidation, pm);
        if (call_PM_Weapon_HasAnimSync(ps) == 0)
        {
            T4_Reconstructed::PM_Weapon_AnimSyncCleanup(pm, prev);
        }

        int state = ps->weaponstate;
        bool needs_swim_check = false;

        if (state == 0x1D)
        {
            needs_swim_check = true;
        }
        else if (state >= 0x10 && state < 0x15)
        {
            needs_swim_check = true;
        }
        else if (state >= 0x0D && state < 0x0F)
        {
            needs_swim_check = true;
        }
        else if (state >= WEAPON_LOWREADY_START && state <= WEAPON_LOWREADY_END)
        {
            // T4M: LOWREADY states bypass the force-SWIM_IN else branch.
            // Without this, SECTION 3 forces SWIM_IN every frame causing visual cycling.
            needs_swim_check = true;
        }
        else
        {
            // loc_42222B — force SWIM_IN
            raw_iref(ps, 0x10) |= 0x2000;
            ps->weaponstate = WEAPON_SWIM_IN;
            goto dispatch_setup;
        }

        if (needs_swim_check) 
        {
            if (state != 0x1D) 
                goto dispatch_setup;

            int v = raw_int(ps, 0x10);

            if (v & 0x2000) 
            {
                raw_iref(ps, 0x10) = v & ~0x2000;
                goto dispatch_setup;
            }

            if (state == 0x1D) 
                return;
        }
    }

dispatch_setup:
    // SECTION 4: Validation + anim sync + heavy block
    call_PM_Weapon_DispatchAnimHandler(pm, callValidation);
    call_PM_Weapon_TickValidation(pm, callValidation);
    // Vanilla `push ebx; mov eax, ebp; call sub_421940` — ebx = callValidation
    // (BEFORE the SECTION 4 overwrite that turns ebx into animSync).
    if (T4_Reconstructed::PM_Weapon_FireRefresh(ps, callValidation) != 0) 
        return;

    int animSync;
    {
        T4_Reconstructed::PM_Weapon_AnimCmdStateInit(pm);
        int prev = call_PM_Weapon_AnimCmdState_Resync(callValidation, pm);
        animSync = prev;
        if (call_PM_Weapon_HasAnimSync(ps) == 0) 
        {
            call_PM_Weapon_TickSprintMachine(pm);
            call_PM_Weapon_TickIdleMachine(pm);
            call_PM_Weapon_TickOffhandMachine(pm);
            call_PM_Weapon_TickPickup(pm);
            call_PM_Weapon_TickRecoil(pm);
            T4_Reconstructed::PM_Weapon_AnimSyncCleanup(pm, animSync);
            T4_Reconstructed::PM_Weapon_DetonateCheck(pm);
            T4_Reconstructed::PM_Weapon_OffhandStateBridge(pm);
        }
    }

    if (call_PM_Weapon_FinalGate(ps, animSync) != 0)
        return;

    // [ps+0x914] update
    bool do_float_store = false;
    bool first_block = (ps->pm_flags & 1) != 0
        && pm->cmd.forward == 0
        && pm->cmd.right == 0;

    if (first_block) 
    {
        if (ps->fWeaponPosFrac != 0.0f) 
            do_float_store = true;
    }
    if (!do_float_store) 
    {
        int s = ps->weaponstate;
        if (s == 0xD || s == 0xE || s == 0xF)
            do_float_store = true;
    }
    if (do_float_store) 
    {
        raw_iref(ps, 0x914) = *(int*)0x008AF224;
    }

    // Bail if not anim-sync and any timer pending
    if (animSync == 0) 
    {
        if (ps->weaponTime != 0) 
            return;

        if (ps->weaponDelay != 0) 
            return;
    }

    // SECTION 5: Main switch dispatch
    int state = ps->weaponstate;
    int v = raw_int(ps, 0x910);

    switch (state) 
    {
        case WEAPON_RAISING:
        case WEAPON_RAISING_ALTSWITCH:
            if (ps->pm_type >= 8)
                break;

            if (T4M::TransitionToReadyOrLowReady(ps)) 
                break;

            ps->weaponstate = WEAPON_READY;
            raw_iref(ps, 0x910) = (~v) & 0x200;
            break;
        case WEAPON_DROPPING:
        case WEAPON_DROPPING_QUICK:
            T4_Reconstructed::PM_Weapon_Dropping(pm, (ps->weaponstate == WEAPON_DROPPING_QUICK) ? 1 : 0);
            break;
        case WEAPON_RELOADING:
        case WEAPON_RELOADING_INTERUPT:
            T4_Reconstructed::PM_Weapon_Reloading(pm, animSync);
            break;
        case WEAPON_RELOAD_START:
        case WEAPON_RELOAD_START_INTERUPT:
            T4_Reconstructed::PM_Weapon_ReloadStart(pm, animSync);
            break;
        case WEAPON_RELOAD_END:
            if (T4M::TransitionToReadyOrLowReady(ps)) 
                break;

            ps->weaponstate = WEAPON_READY;
            if (ps->pm_type >= 8)
                break;

            raw_iref(ps, 0x910) = (~v) & 0x200;
            break;
        case WEAPON_MELEE_CHARGE:
            T4_Reconstructed::PM_Weapon_MeleeCharge(ps);
            break;
        case WEAPON_MELEE_INIT:
            T4_Reconstructed::PM_Weapon_MeleeInit(ps);
            break;
        case WEAPON_MELEE_FIRE:
            T4_Reconstructed::PM_Weapon_MeleeFire(ps);
            break;
        case WEAPON_MELEE_END:
        case WEAPON_OFFHAND_END:
        case WEAPON_SPRINT_DROP:
            call_PM_Weapon_FinalizeStateExit(ps);
            break;
        case WEAPON_OFFHAND_INIT:
            T4_Reconstructed::PM_Weapon_OffhandInit_Stub(ps);
            break;
        case WEAPON_OFFHAND_PREPARE:
            T4_Reconstructed::PM_Weapon_OffhandPrepare(ps);
            break;
        case WEAPON_OFFHAND_HOLD:
            T4_Reconstructed::PM_Weapon_OffhandHold(pm);
            break;
        case WEAPON_OFFHAND_START:
            T4_Reconstructed::PM_Weapon_OffhandStart(pm);
            break;
        case WEAPON_OFFHAND:
            T4_Reconstructed::PM_Weapon_Offhand_Stub(ps);
            break;
        case WEAPON_DETONATING:
            T4_Reconstructed::PM_Weapon_DetonateTransition(ps, animSync);
            break;
        case WEAPON_SPRINT_RAISE:
            T4_Reconstructed::PM_Weapon_SprintRaise(ps);
            break;
        case WEAPON_SPRINT_LOOP:
        case WEAPON_DEPLOYED:
            break;
        case WEAPON_DEPLOYING:
            call_Anim_TriggerEvent(ps, 0x23);
            break;
        case WEAPON_BREAKING_DOWN:
            call_PM_Weapon_FinalizeStateExit(ps);
            call_Anim_TriggerEvent(ps, 0x25);
            break;
        case WEAPON_SWIM_IN:
            T4_Reconstructed::PM_Weapon_SwimIn(ps);
            break;
        case WEAPON_SWIM_OUT:
            T4_Reconstructed::PM_Weapon_SwimOut(ps);
            break;
        case WEAPON_LOWREADY_START:
            T4M::PM_Weapon_LowReady_Start(ps);
            break;
        case WEAPON_LOWREADY_LOOP:
            break;
        case WEAPON_LOWREADY_END:
            T4M::PM_Weapon_LowReady_End(ps);
            break;
        case WEAPON_FIRING:
        case WEAPON_RECHAMBERING:
        case WEAPON_READY:
        default:
            // def_422367 — fire input dispatch.
            // Vanilla uses ebx (= animSync after the SECTION 4 overwrite) as arg_0 here,
            // NOT callValidation. ebx was reassigned to call_PM_Weapon_AnimCmdState_Resync's
            // return value just before this dispatch.
            if (ps->weapon == 0) 
                return;

            if (T4_Reconstructed::PM_Weapon_DefaultGate(pm, animSync) == 0) 
                return;

            if (call_PM_Weapon_IsLocked(pm) != 0) 
                return;

            if (animSync != 0) 
            {
                if (T4_Reconstructed::PM_Weapon_CanForceFire(pm) != 0) 
                    return;
            }

            if ((raw_int(ps, 0x0C) & 0x800) != 0)
                return;

            if (ps->weaponstate == WEAPON_DEPLOYING)
                return;

            call_PM_Weapon_ProcessAttackInput(ps, animSync);
            break;
    }
}


// =====================================================================
// __usercall → __cdecl wrapper for sub_422160 detour.
// Vanilla convention: eax = pmove_t*, [esp+4] = arg_0.
// =====================================================================

__declspec(naked) void T4M::PM_Weapon_Wrapper()
{
    __asm 
    {
        push    [esp+4]                 ; arg_0
        push    eax                     ; pmove_t*
        call    T4_Reconstructed::PM_Weapon
        add     esp, 8
        retn
    }
}


// =====================================================================
// === Viewmodel pose reconstructions (translation + rotation) =========
// =====================================================================
// Reconstruction of sub_465FF0 (CG_ApplyViewmodelMoveOfs) and
// sub_4664E0 (CG_ApplyViewmodelRotOfs). Each adds a LOWREADY pull branch
// parallel to the SPRINT pull, using existing WeaponDef fields.
// All other behavior is replicated byte-for-byte from vanilla.
// See analysis/sub_465FF0_RE.md and analysis/sub_4664E0_RE.md.
// Merged 2026-05-06: was PatchT4MAM_ViewmodelMove + PatchT4MAM_ViewmodelRot.
// =====================================================================

namespace T4_Reconstructed
{
    extern "C"
    {
        // @faithful + LowReady branch — sub_465FF0
        void CG_ApplyViewmodelMoveOfs(void* cg, float* outOfs);
        // @faithful + LowReady branch — sub_4664E0
        void CG_ApplyViewmodelRotOfs(void* cg, float* outRot);
    }
}

namespace T4M
{
    void CG_ApplyViewmodelMoveOfs_Wrapper();  // naked __usercall(edx=cg) → cdecl
    void CG_ApplyViewmodelRotOfs_Wrapper();   // naked __usercall(ecx=cg) → cdecl
}

// === Vanilla globals (read-only) ===
// ADDR_bg_WeaponDefs removed: bg_weaponDefs is relocated by SetupBgWeaponDefsTable
// to a heap address. Use the runtime pointer `::bg_weaponDefs` instead.
static const uintptr_t ADDR_cg_gun_move_f        = 0x03688A88;
static const uintptr_t ADDR_cg_gun_move_r        = 0x03688A70;
static const uintptr_t ADDR_cg_gun_move_u        = 0x0368EB98;
static const uintptr_t ADDR_cg_gun_ofs_f         = 0x0368EBB4;
static const uintptr_t ADDR_cg_gun_ofs_r         = 0x03688A38;
static const uintptr_t ADDR_cg_gun_ofs_u         = 0x034660B4;
static const uintptr_t ADDR_cg_gun_minspeed      = 0x034660B0;
static const uintptr_t ADDR_cg_gun_move_rate     = 0x03466564;
static const uintptr_t ADDR_cg_gun_rot_p         = 0x03688AC4;
static const uintptr_t ADDR_cg_gun_rot_y         = 0x03688A20;
static const uintptr_t ADDR_cg_gun_rot_r         = 0x03466118;
static const uintptr_t ADDR_cg_gun_rot_minspeed  = 0x03688B48;
static const uintptr_t ADDR_cg_gun_rot_rate      = 0x03466060;

// dvar.value lives at *(float*)(dvar_ptr + 0x10).
static inline float DvarF(uintptr_t dvarSlotAddr)
{
    const char* dvarPtr = *(const char* const*)dvarSlotAddr;
    return *(const float*)(dvarPtr + 0x10);
}

// === cg_t / WeaponDef raw accessors (offset-based, byte-precise to vanilla asm) ===
static inline int   CgI(const void* cg, ptrdiff_t o) { return *(const int  *)((const char*)cg + o); }
static inline float CgF(const void* cg, ptrdiff_t o) { return *(const float*)((const char*)cg + o); }
static inline int   WdI(const WeaponDef* w, ptrdiff_t o) { return *(const int*)((const char*)w + o); }

// =====================================================================
// CG_ApplyViewmodelMoveOfs — reconstruction of sub_465FF0 (translation)
// =====================================================================
//
// Quirks preserved byte-for-byte:
//   - STAND fall-through: shared addss with PRONE for cg_gun_minspeed
//   - Re-read of stanceBits at the duck-test (eax was clobbered)
//   - No saturate-lower of t (t<0 not clamped — vanilla doesn't either)
//   - clamp-to-zero via `xmm0 = -0.0f - minSpeed; if (xmm0 < 0) keep else minSpeed=0`
//   - sprint asymmetric raise/drop formulas (kept identical)
//   - epsilon-negative force-clamp magnitude during smoothing
//   - ADS >= 0.5 path: NO write to outOfs (not even zero)
extern "C" void T4_Reconstructed::CG_ApplyViewmodelMoveOfs(void* cg, float* outOfs)
{
    int weaponIdx = (CgI(cg, 0xAACA8) & 0x02)
                  ? CgI(cg, 0xAAD94)
                  : CgI(cg, 0xAAD9C);
    const WeaponDef* wd = bg_weaponDefs[weaponIdx];

    int stanceBits = CgI(cg, 0xACE1C);
    bool prone = (stanceBits & 0x08) != 0;
    bool duck  = !prone && ((stanceBits & 0x04) != 0);

    float minSpeed;
    if (prone)      
        minSpeed = wd->fProneMoveMinSpeed;
    else if (duck)  
        minSpeed = wd->fDuckedMoveMinSpeed;
    else            
        minSpeed = wd->fStandMoveMinSpeed;

    minSpeed += DvarF(ADDR_cg_gun_minspeed);

    if (-minSpeed >= 0.0f) 
        minSpeed = 0.0f;

    float curSpeed = CgF(cg, 0xB8378);
    bool moving = (curSpeed > minSpeed);

    int weaponState = CgI(cg, 0xAADA0);
    int weaponType  = CgI(cg, 0xAADC8);
    bool stateIs7   = (weaponState == 7);
    bool isType0B   = (weaponType == 0x0B);
    bool isType28   = (weaponType == 0x28);
    bool sprintRange   = (weaponState >= 0x17 && weaponState <= 0x19);
    bool lowReadyRange = (weaponState >= 0x1F && weaponState <= 0x21);

    float smTargetF, smTargetR, smTargetU;

    if (!moving || stateIs7) 
    {
        smTargetF = 0.0f;
        smTargetR = 0.0f;
        smTargetU = 0.0f;
    } 
    else 
    {
        float maxSpeed = (float)CgI(cg, 0xAAD10);
        float t = (curSpeed - minSpeed) / (maxSpeed - minSpeed);

        if (t - 1.0f >= 0.0f) 
            t = 1.0f;

        float ofsF, ofsR, ofsU;
        if (prone) 
        {
            ofsF = wd->vProneMove[0]; ofsR = wd->vProneMove[1]; ofsU = wd->vProneMove[2];
        } 
        else 
        {
            int sb = CgI(cg, 0xACE1C);
            if (sb & 0x04) 
            {
                ofsF = wd->vDuckedMove[0]; ofsR = wd->vDuckedMove[1]; ofsU = wd->vDuckedMove[2];
            } 
            else 
            {
                ofsF = wd->vStandMove[0];  ofsR = wd->vStandMove[1];  ofsU = wd->vStandMove[2];
            }
        }

        float gmf = DvarF(ADDR_cg_gun_move_f);
        float gmr = DvarF(ADDR_cg_gun_move_r);
        float gmu = DvarF(ADDR_cg_gun_move_u);
        smTargetF = ofsF * t + gmf * t;
        smTargetR = ofsR * t + gmr * t;
        smTargetU = ofsU * t + gmu * t;
    }

    if (isType28) 
    {
        float gof = DvarF(ADDR_cg_gun_ofs_f);
        float gor = DvarF(ADDR_cg_gun_ofs_r);
        float gou = DvarF(ADDR_cg_gun_ofs_u);
        smTargetF = gof + (wd->vDuckedOfs[0] + smTargetF);
        smTargetR = gor + (wd->vDuckedOfs[1] + smTargetR);
        smTargetU = gou + (wd->vDuckedOfs[2] + smTargetU);
    } 
    else if (isType0B) 
    {
        float gof = DvarF(ADDR_cg_gun_ofs_f);
        float gor = DvarF(ADDR_cg_gun_ofs_r);
        float gou = DvarF(ADDR_cg_gun_ofs_u);
        smTargetF = gof + (wd->vProneOfs[0] + smTargetF);
        smTargetR = gor + (wd->vProneOfs[1] + smTargetR);
        smTargetU = gou + (wd->vProneOfs[2] + smTargetU);
    }

    float* smBuf = (float*)((char*)cg + 0xAD03C);
    const float kMs2S   =  0.001f;
    const float kEpsPos =  1.0e-4f;
    const float kEpsNeg = -1.0e-4f;
    float gunMoveRate = DvarF(ADDR_cg_gun_move_rate);
    float dt_ms = (float)CgI(cg, 0xAAC78);
    bool useProneRate = (CgF(cg, 0xAADCC) == 11.0f);

    float targets[3] = { smTargetF, smTargetR, smTargetU };
    for (int i = 0; i < 3; ++i) 
    {
        float cur = smBuf[i];
        float tgt = targets[i];

        if (cur == tgt) 
            continue;

        float rate = useProneRate ? wd->fPosProneMoveRate : wd->fPosMoveRate;
        rate += gunMoveRate;

        float delta = tgt - cur;
        float move  = rate * delta * dt_ms * kMs2S;

        if (tgt > cur) 
        {
            float eps = dt_ms * kEpsPos;
            if (eps > move) move = eps;
            cur += move;
            if (cur > tgt) cur = tgt;
        } 
        else 
        {
            float eps = dt_ms * kEpsNeg;
            if (move > eps) move = eps;
            cur += move;
            if (tgt > cur) cur = tgt;
        }
        smBuf[i] = cur;
    }

    if (sprintRange) 
    {
        float t_sprint;
        if (weaponState == 0x17) 
        {
            float dur     = (float)WdI(wd, 0x4A8);
            float counter = (float)CgI(cg, 0xAACD8);   // vanilla: cvtsi2ss → int, NOT float
            t_sprint = 1.0f - (counter / dur);
        } 
        else if (weaponState == 0x18) 
        {
            t_sprint = 1.0f;
        } 
        else 
        {
            float dur     = (float)WdI(wd, 0x4B0);
            float counter = (float)CgI(cg, 0xAACD8);   // vanilla: cvtsi2ss → int
            t_sprint = counter / dur;
        }

        float gmf = DvarF(ADDR_cg_gun_move_f);
        float gmr = DvarF(ADDR_cg_gun_move_r);
        float gmu = DvarF(ADDR_cg_gun_move_u);
        float tgtSF = gmf * t_sprint + wd->sprintOfs[0] * t_sprint;
        float tgtSR = gmr * t_sprint + wd->sprintOfs[1] * t_sprint;
        float tgtSU = gmu * t_sprint + wd->sprintOfs[2] * t_sprint;

        smBuf[0] = (tgtSF - smBuf[0]) * t_sprint + smBuf[0];
        smBuf[1] = (tgtSR - smBuf[1]) * t_sprint + smBuf[1];
        smBuf[2] = (tgtSU - smBuf[2]) * t_sprint + smBuf[2];
    }
    // T4M LOWREADY pose pull (states 0x1F..0x21) — client-side ramp tween.
    {
        static int   s_inLR       = 0;
        static DWORD s_tweenStart = 0;
        static int   s_tweenDur   = 1;

        if (lowReadyRange) 
        {
            if (!s_inLR) 
            {
                s_tweenStart = GetTickCount();
                s_tweenDur   = (wd->iLowReadyInTime > 0) ? wd->iLowReadyInTime : 1;
                s_inLR = 1;
            }
        } 
        else 
        {
            s_inLR = 0;
        }

        if (lowReadyRange) 
        {
            DWORD elapsed = GetTickCount() - s_tweenStart;
            float t_lr = (float)elapsed / (float)s_tweenDur;
            if (t_lr < 0.0f) t_lr = 0.0f;
            if (t_lr > 1.0f) t_lr = 1.0f;

            float lrTgtF = wd->lowReadyOfsF * t_lr;
            float lrTgtR = wd->lowReadyOfsR * t_lr;
            float lrTgtU = wd->lowReadyOfsU * t_lr;

            smBuf[0] = (lrTgtF - smBuf[0]) * t_lr + smBuf[0];
            smBuf[1] = (lrTgtR - smBuf[1]) * t_lr + smBuf[1];
            smBuf[2] = (lrTgtU - smBuf[2]) * t_lr + smBuf[2];
        }
    }

    float adsFrac = CgF(cg, 0xAADA8);
    if (adsFrac == 0.0f) 
    {
        outOfs[0] += smBuf[0];
        outOfs[1] += smBuf[1];
        outOfs[2] += smBuf[2];
    } 
    else if (adsFrac < 0.5f) 
    {
        float scale = 1.0f - 2.0f * adsFrac;
        outOfs[0] += smBuf[0] * scale;
        outOfs[1] += smBuf[1] * scale;
        outOfs[2] += smBuf[2] * scale;
    }
}

// =====================================================================
// CG_ApplyViewmodelRotOfs — reconstruction of sub_4664E0 (rotation)
// =====================================================================
//
// Quirks preserved byte-for-byte:
//   - cg arrives in ECX (not EDX as sub_465FF0). Handled by wrapper.
//   - Saturate-LOWER of t at 0 (sub_465FF0 doesn't have this)
//   - No -minSpeed clamp (vs sub_465FF0)
//   - No weapon-type-specific additif (no 0x28/0x0B branches)
//   - ADS PRÉ-smoothing scale: smTarget *= (1 - ads) if ads != 0
//   - smBuf at cg+0xAD048 (rotation buffer, distinct from translation +0xAD03C)
//   - Rates inverted vs sub_465FF0: ==11.0 → fPosProneRotRate, !=11.0 → fPosRotRate
//   - Sprint pull: target = sprintRot * t_sprint (NO add of cg_gun_rot_*)
extern "C" void T4_Reconstructed::CG_ApplyViewmodelRotOfs(void* cg, float* outRot)
{
    int weaponIdx = (CgI(cg, 0xAACA8) & 0x02)
                  ? CgI(cg, 0xAAD94)
                  : CgI(cg, 0xAAD9C);
    const WeaponDef* wd = bg_weaponDefs[weaponIdx];

    int stanceBits = CgI(cg, 0xACE1C);
    bool prone = (stanceBits & 0x08) != 0;
    bool duck  = !prone && ((stanceBits & 0x04) != 0);

    float minSpeed;
    if (prone)      
        minSpeed = wd->fProneRotMinSpeed;
    else if (duck)  
        minSpeed = wd->fDuckedRotMinSpeed;
    else            
        minSpeed = wd->fStandRotMinSpeed;

    minSpeed += DvarF(ADDR_cg_gun_rot_minspeed);

    float curSpeed = CgF(cg, 0xB8378);
    bool moving    = (curSpeed > minSpeed);
    bool stateNot7 = (CgI(cg, 0xAADA0) != 7);

    float smTargetP, smTargetY, smTargetR;

    if (moving && stateNot7) 
    {
        float maxSpeed = (float)CgI(cg, 0xAAD10);
        float t        = (curSpeed - minSpeed) / (maxSpeed - minSpeed);

        if (-t >= 0.0f) 
            t = 0.0f;

        if (t - 1.0f >= 0.0f)
            t = 1.0f;

        float pitchOfs, yawOfs, rollOfs;
        if (prone) 
        {
            pitchOfs = wd->vProneRot[0]; yawOfs = wd->vProneRot[1]; rollOfs = wd->vProneRot[2];
        } 
        else if (duck) 
        {
            pitchOfs = wd->vDuckedRot[0]; yawOfs = wd->vDuckedRot[1]; rollOfs = wd->vDuckedRot[2];
        } 
        else 
        {
            pitchOfs = wd->vStandRot[0];  yawOfs = wd->vStandRot[1];  rollOfs = wd->vStandRot[2];
        }

        float grp = DvarF(ADDR_cg_gun_rot_p);
        float gry = DvarF(ADDR_cg_gun_rot_y);
        float grr = DvarF(ADDR_cg_gun_rot_r);
        smTargetP = pitchOfs * t + grp * t;
        smTargetY = yawOfs   * t + gry * t;
        smTargetR = rollOfs  * t + grr * t;
    } 
    else 
    {
        smTargetP = 0.0f;
        smTargetY = 0.0f;
        smTargetR = 0.0f;
    }

    // ADS PRÉ-smoothing scale
    float adsFrac = CgF(cg, 0xAADA8);
    if (adsFrac != 0.0f) 
    {
        float k = 1.0f - adsFrac;
        smTargetP *= k;
        smTargetY *= k;
        smTargetR *= k;
    }

    float* smBuf = (float*)((char*)cg + 0xAD048);
    const float kMs2S   =  0.001f;
    const float kEpsPos =  1.0e-4f;
    const float kEpsNeg = -1.0e-4f;
    float gunRotRate = DvarF(ADDR_cg_gun_rot_rate);
    float dt_ms      = (float)CgI(cg, 0xAAC78);
    bool useProneRate = (CgF(cg, 0xAADCC) == 11.0f);

    float targets[3] = { smTargetP, smTargetY, smTargetR };
    for (int i = 0; i < 3; ++i) 
    {
        float cur = smBuf[i];
        float tgt = targets[i];
        if (cur == tgt) 
            continue;

        float rate = useProneRate ? wd->fPosProneRotRate : wd->fPosRotRate;
        rate += gunRotRate;

        float delta = tgt - cur;
        float move  = rate * delta * dt_ms * kMs2S;

        if (tgt > cur) 
        {
            float eps = dt_ms * kEpsPos;

            if (eps > move) 
                move = eps;

            cur += move;

            if (cur > tgt) 
                cur = tgt;
        } 
        else 
        {
            float eps = dt_ms * kEpsNeg;

            if (move > eps) 
                move = eps;

            cur += move;

            if (tgt > cur) 
                cur = tgt;
        }
        smBuf[i] = cur;
    }

    int weaponState = CgI(cg, 0xAADA0);
    if (weaponState >= 0x17 && weaponState <= 0x19) 
    {
        float t_sprint;
        if (weaponState == 0x17) 
        {
            float dur     = (float)WdI(wd, 0x4A8);
            float counter = (float)CgI(cg, 0xAACD8);
            t_sprint = 1.0f - (counter / dur);
        } 
        else if (weaponState == 0x18) 
        {
            t_sprint = 1.0f;
        } 
        else 
        {
            float dur     = (float)WdI(wd, 0x4B0);
            float counter = (float)CgI(cg, 0xAACD8);
            t_sprint = counter / dur;
        }

        float tgtSP = wd->sprintRot[0] * t_sprint;
        float tgtSY = wd->sprintRot[1] * t_sprint;
        float tgtSR = wd->sprintRot[2] * t_sprint;

        smBuf[0] = (tgtSP - smBuf[0]) * t_sprint + smBuf[0];
        smBuf[1] = (tgtSY - smBuf[1]) * t_sprint + smBuf[1];
        smBuf[2] = (tgtSR - smBuf[2]) * t_sprint + smBuf[2];
    }
    // T4M LOWREADY pose pull (states 0x1F..0x21) — client-side ramp tween.
    {
        bool lowReadyRange = (weaponState >= 0x1F && weaponState <= 0x21);
        static int   s_inLR       = 0;
        static DWORD s_tweenStart = 0;
        static int   s_tweenDur   = 1;

        if (lowReadyRange) 
        {
            if (!s_inLR) 
            {
                s_tweenStart = GetTickCount();
                s_tweenDur   = (wd->iLowReadyInTime > 0) ? wd->iLowReadyInTime : 1;
                s_inLR = 1;
            }
        } 
        else 
        {
            s_inLR = 0;
        }

        if (lowReadyRange) 
        {
            DWORD elapsed = GetTickCount() - s_tweenStart;
            float t_lr = (float)elapsed / (float)s_tweenDur;

            if (t_lr < 0.0f) 
                t_lr = 0.0f;

            if (t_lr > 1.0f) 
                t_lr = 1.0f;

            float lrTgtP = wd->lowReadyRotP * t_lr;
            float lrTgtY = wd->lowReadyRotY * t_lr;
            float lrTgtR = wd->lowReadyRotR * t_lr;

            smBuf[0] = (lrTgtP - smBuf[0]) * t_lr + smBuf[0];
            smBuf[1] = (lrTgtY - smBuf[1]) * t_lr + smBuf[1];
            smBuf[2] = (lrTgtR - smBuf[2]) * t_lr + smBuf[2];
        }
    }

    // ADS POST-scale
    float adsFrac2 = CgF(cg, 0xAADA8);
    if (adsFrac2 == 0.0f) 
    {
        outRot[0] += smBuf[0];
        outRot[1] += smBuf[1];
        outRot[2] += smBuf[2];
    } 
    else if (adsFrac2 < 0.5f) 
    {
        float scale = 1.0f - 2.0f * adsFrac2;
        outRot[0] += smBuf[0] * scale;
        outRot[1] += smBuf[1] * scale;
        outRot[2] += smBuf[2] * scale;
    }
}

// === Naked wrappers ===
//
// Move : __usercall(edx=cg, [esp+0]=outOfs) → cdecl. Vanilla preserves edx
// implicitly; we preserve explicitly to be safe (caller may rely on it).
__declspec(naked) void T4M::CG_ApplyViewmodelMoveOfs_Wrapper()
{
    __asm 
    {
        push    ebp
        push    edi
        push    esi
        push    ebx
        push    edx                                          // PRESERVE edx
        push    [esp+0x18]                                   // outOfs
        push    [esp+0x4]                                    // cg
        call    T4_Reconstructed::CG_ApplyViewmodelMoveOfs
        add     esp, 8
        pop     edx                                          // RESTORE edx
        pop     ebx
        pop     esi
        pop     edi
        pop     ebp
        retn
    }
}

// Rot : __usercall(ecx=cg, [esp+0]=outRot) → cdecl. Caller (sub_4668D0) reads
// [ecx+0xAD02C] right after the call — ECX preservation is mandatory.
__declspec(naked) void T4M::CG_ApplyViewmodelRotOfs_Wrapper()
{
    __asm 
    {
        push    ebp
        push    edi
        push    esi
        push    ebx
        push    ecx                                          // PRESERVE ecx
        push    [esp+0x18]                                   // outRot
        push    [esp+0x4]                                    // cg
        call    T4_Reconstructed::CG_ApplyViewmodelRotOfs
        add     esp, 8
        pop     ecx                                          // RESTORE ecx
        pop     ebx
        pop     esi
        pop     edi
        pop     ebp
        retn
    }
}

// =====================================================================
// PM_CheckAds entry hook (sub_41DD10) — block ADS while lowReady intent is set.
// =====================================================================
// Convention: __usercall(edi=pmove_t* pm, eax=pml_t* pml).
// Vanilla flow: pre-clears [ps+0xC] &= ~0x10 at entry, then sets it back to
// |= 0x10 (or |= 0x210) if all ADS conditions are met. To block ADS we fake
// "ADS not held" by clearing the relevant bits in pmove_t before vanilla reads
// them — the pre-clear remains, the conditions fail, [ps+0xC] stays cleared.
//
// Bits cleared in pm-side:
//   pm+0x08 (cmd.buttons)    & ~0x800 — current frame ADS button
//   pm+0x40 (oldcmd.buttons) & ~0x800 — previous frame mirror used for edge-detect
// And pre-emptively in ps-side:
//   ps+0x0C & ~0x210 — ADS-allowed flags (the values vanilla writes when ADS allowed).

static void InstallPmCheckAdsHook()
{
    static auto pm_checkads_hook = safetyhook::create_mid(0x0041DD10,
        [](SafetyHookContext& ctx) 
        {
            auto* pm  = (char*)ctx.edi;
            auto* ps  = *(playerState_s**)pm;

            if ((ps->eFlags & 0x400) == 0)
                return;

            *(int*)(pm + 0x08)               &= ~0x800;   // cmd.buttons
            *(int*)(pm + 0x40)               &= ~0x800;   // oldcmd.buttons mirror
            *(int*)((char*)ps + 0x0C)        &= ~0x210;   // ADS-allowed result bits
        });
}

// =====================================================================
// Installer — PM_Weapon + viewmodel pose detours + PM_CheckAds hook.
// =====================================================================

void PatchT4MAM_WeaponState()
{
    Detours::X86::DetourFunction((uintptr_t)0x00422160, (uintptr_t)&T4M::PM_Weapon_Wrapper, Detours::X86Option::USE_JUMP);

    // sub_465FF0 — viewmodel translation (lowReady pull added)
    Detours::X86::DetourFunction((uintptr_t)0x00465FF0, (uintptr_t)&T4M::CG_ApplyViewmodelMoveOfs_Wrapper, Detours::X86Option::USE_JUMP);

    // sub_4664E0 — viewmodel rotation (lowReady pull added)
    Detours::X86::DetourFunction((uintptr_t)0x004664E0, (uintptr_t)&T4M::CG_ApplyViewmodelRotOfs_Wrapper, Detours::X86Option::USE_JUMP);

    // sub_41DD10 (PM_CheckAds) — block ADS while lowReady intent is set
    InstallPmCheckAdsHook();
}
