#include "StdInc.h"
#include "t4_headers.h"
#include "T4.h"
#include "MemoryMgr.h"
#include <safetyhook.hpp>


// =====================================================================
// Forward decls + raw-access helpers
// =====================================================================

struct pmove_t;  // opaque

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

        // Per-state chunk handlers — reconstructed @faithful in this file.
        void PM_Weapon_Tick_MeleeFire(playerState_s* ps);      // chunk #1 @ 0x00420CE0
        void PM_Weapon_Tick_MeleeCharge(playerState_s* ps);    // chunk #2 @ 0x00420D70
        void PM_Weapon_Tick_MeleeInit(playerState_s* ps);      // chunk #3 @ 0x00420E10
        void PM_Weapon_Tick_OffhandPrepare(playerState_s* ps); // chunk #4 @ 0x004213D0
        void PM_Weapon_Tick_OffhandStart(pmove_t* pm);         // chunk #5 @ 0x00421410
        void PM_Weapon_Tick_OffhandHold(pmove_t* pm);          // chunk #6 @ 0x004214E0
        void PM_Weapon_Tick_SwimIn(playerState_s* ps);         // chunk #7 @ 0x00421C40
        void PM_Weapon_Tick_SwimOut(playerState_s* ps);        // chunk #8 @ 0x00421CD0
        void PM_Weapon_Tick_SprintRaise(playerState_s* ps);    // chunk #9 @ 0x00421D80

        // Shared chunks — kept as register-preserving stubs (vanilla call).
        void PM_Weapon_Tick_OffhandInit_Stub(playerState_s* ps);   // shared A @ 0x00421300
        void PM_Weapon_Tick_Offhand_Stub(playerState_s* ps);       // shared B @ 0x00421630

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

static inline WeaponDef* GetWeaponDef(unsigned int idx)
{
    return ((WeaponDef**)T4M::GetAddress("bg_weaponDefs"))[idx];
}


// =====================================================================
// Multi-caller / complex-mono helpers — register-preserving shims.
// All shims save+restore esi/edi/ebx around the vanilla call to honor cdecl.
// =====================================================================

// sub_421FF0 — __usercall(esi=ps, [esp+4]=arg_0); returns bool in al. (multi-caller)
// Vanilla call targets used by the naked shims below — resolved at runtime
// (variant-aware) in PatchT4MAM_WeaponState(); naked bodies do `call p_X`.


static __declspec(naked) char call_IsWeaponInputBlocked(playerState_s* /*ps*/, int /*arg_0*/)
{
    __asm 
    {
        push    esi
        push    edi
        push    ebx
        mov     esi, [esp+10h]              ; ps (1st arg, +4 + 12 shift = +0x10)
        push    [esp+14h]                    ; arg_0 (2nd arg, +8 + 12 shift = +0x14)
        call T4::engine::p_IsWeaponInputBlocked
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
        call T4::engine::p_PM_Weapon_AnimCmdState_Resync
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
        call T4::engine::p_PM_Weapon_HasAnimSync
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
        call T4::engine::p_PM_Weapon_DispatchAnimHandler
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
        call T4::engine::p_PM_Weapon_TickValidation
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
        call T4::engine::p_PM_Weapon_FinalGate
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
        call T4::engine::p_PM_Weapon_TickSprintMachine
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
        call T4::engine::p_PM_Weapon_TickIdleMachine
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
        call T4::engine::p_PM_Weapon_TickOffhandMachine
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
        call T4::engine::p_PM_Weapon_TickPickup
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
        call T4::engine::p_PM_Weapon_TickRecoil
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
        call T4::engine::p_PM_Weapon_ProcessAttackInput
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
        call T4::engine::p_PM_Weapon_FinalizeStateExit
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
        call T4::engine::p_Anim_TriggerEvent
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
        call T4::engine::p_PM_Weapon_IsLocked
        pop     ebx
        pop     edi
        pop     esi
        ret
    }
}

// sub_41F010 — DROPPING handler. __thiscall(ecx=pm, [esp+4]=isQuick). Kept as shim.
static __declspec(naked) void call_PM_Weapon_TickDrop(pmove_t* /*pm*/, int /*isQuick*/)
{
    __asm 
    {
        push    esi
        push    edi
        push    ebx
        mov     ecx, [esp+10h]
        push    [esp+14h]
        call T4::engine::p_PM_Weapon_TickDrop
        add     esp, 4
        pop     ebx
        pop     edi
        pop     esi
        ret
    }
}

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
        call T4::engine::p_PM_Weapon_RefreshReload
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
        call T4::engine::p_PM_Weapon_CheckFastReload
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
        call T4::engine::p_PM_Weapon_SubmitAnimEvent
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
        call T4::engine::p_PM_Weapon_ReloadFinalize
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
    int v = raw_int(w, 0x158);

    if (v == 0 || v == 1) 
        return;

    if ((raw_byte(pm, 0x8)  & 1) == 0) 
        return;

    if ((raw_byte(pm, 0x40) & 1) != 0) 
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
    int weaponClass = raw_int(w, 0x144);

    if (weaponClass != 1 && weaponClass != 6) 
        return 0;

    if (raw_int(w, 0x6B8) != 0) 
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
        call T4::engine::p_PM_Weapon_GetInputMask
        mov     mask, eax
        pop     ebx
        pop     edi
        pop     esi
    }

    if ((raw_int(pm, 0x8) & mask) == 0) 
        return 0;

    raw_iref(ps, 0x44) = 1;
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
    int weaponClass = raw_int(w, 0x144);
    if (weaponClass != 1 && weaponClass != 6) 
        return 0;

    // loc_421974 — bail if v48 <= 0 (timer not active)
    int v48 = raw_int(ps, 0x48);
    if (v48 <= 0) 
        return 0;                 // jle loc_4219B7 → return 0

    // v48 > 0: maybe decrement
    bool skip_decrement = (raw_byte(ps, 0x8E8) & 1) != 0 || raw_int(w, 0x5E8) == 0;

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
        call T4::engine::p_PM_Weapon_OffhandSelect
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
    int weaponClass = raw_int(w, 0x144);

    if (weaponClass != 1 && weaponClass != 6) 
        return;

    if (raw_int(w, 0x6AC) == 0) 
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
    if ((raw_byte(pm, 0x8) & 1) == 0) 
        return;

    ps->weaponstate = (weaponstate_t)0x16;     // WEAPON_DETONATING
    raw_iref(ps, 0x40) = raw_int(w, 0x458);
    raw_iref(ps, 0x44) = raw_int(w, 0x444);

    // sub_41D420 is __usercall(eax=ps, [esp+4]=new_vm_state).
    __asm 
    {
        push    esi
        push    edi
        push    ebx
        push    1Bh
        mov     eax, ps
        call T4::engine::p_PM_Weapon_SubmitAnimEvent
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
        if (raw_int(w, 0x6B8) != 0 && (raw_int(pm, 0x8) & 0xC000) == 0) 
        {
            // jmp shared chunk @ 0x00421630 (with eax = ps)
            T4_Reconstructed::PM_Weapon_Tick_Offhand_Stub(ps);
            return;
        }
        // fall through to loc_421BB7 (no-op end)
        return;
    }

    // loc_421B73
    if (ps->weapon == 0) 
        return;

    const WeaponDef* w = GetWeaponDef(ps->weapon);
    int weaponClass = raw_int(w, 0x144);

    if (weaponClass != 1 && weaponClass != 6) 
        return;

    // loc_421B94
    if (state != 5) 
        return;                   // not WEAPON_FIRING

    if (raw_int(w, 0x6B8) == 0) 
        return;

    if ((raw_byte(pm, 0x8) & 1) != 0) 
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
        call T4::engine::p_PM_Weapon_SubmitAnimEvent
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
    int weaponClass = raw_int(w, 0x144);

    int mask;
    if ((weaponClass == 1 || weaponClass == 6) && raw_int(w, 0x6AC) != 0) 
    {
        mask = 0x400000;
    } 
    else 
    {
        mask = 1;
    }

    int bl = ((raw_int(pm, 0x8) & mask) != 0) ? 1 : 0;

    if (raw_int(w, 0x6BC) != 0 && raw_int(ps, 0x88) == 0x3FF) 
    {
        bl = 0;
    }

    if (raw_int(w, 0x5F0) != 0) 
    {
        // dword_1F552A4 holds a dvar_t* pointer; deref then check [+0x10] (dvar bool field).
        unsigned char* dvar_ptr = *(unsigned char**)T4M::GetAddress("dvar_g_testForDpadItem");
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
            call T4::engine::p_PM_Weapon_SetVmAnimState
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
    if ((raw_byte(pm, 0x8) & 4) != 0) 
    {
        if (call_PM_Weapon_IsLocked(pm) != 0) 
            return;
    }

    // loc_42117E
    if (raw_int(w, 0x430) == 0) 
        return;

    if (arg_0 != 0)
        return;

    if ((raw_byte(ps, 0x14) & 0x20) != 0) 
        return;

    if (raw_int(ps, 0x44) != 0) 
    {
        if (state != 7 && state != 9 && state != 0xB && state != 0xA && state != 8) 
            return;
    }

    // loc_4211B9
    if ((raw_byte(pm, 0x8)  & 4) == 0) 
        return;

    if ((raw_byte(pm, 0x40) & 4) != 0) 
        return;

    float wpf = raw_float(ps, 0x110);
    bool slowAnim = (wpf <= *(float*)T4M::GetAddress("flt_zero_8B7B30"));  // ds:String2 — float 0.0 sentinel

    if (!slowAnim && raw_int(w, 0x524) != 0) 
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
        call T4::engine::p_PM_Weapon_AdsBlendReset
        push    [ps]                          ; arg_0 = ps (NOT pm)
        call T4::engine::p_PM_Weapon_FireSwitchEvent
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
        raw_iref(ps, 0x40) = 0;
        raw_iref(ps, 0x44) = 0;

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
    if (raw_int(ps, 0x40) != 0) 
        return;

    // Force INTERUPT state if weapon supports interruptible reload AND attack pressed
    if (raw_int(w, 0x628) != 0 && (raw_byte(pm, 0x8) & 1) != 0) 
    {
        ps->weaponstate = (weaponstate_t)0xA;       // RELOAD_START_INTERUPT
    }

    // loc_41F678
    if (ps->weaponstate == 0xA) 
    {
        const WeaponDef* w2 = GetWeaponDef(ps->weapon);
        int idx = raw_int(w2, 0x3FC);
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

    if (raw_int(w, 0x480) != 0) 
    {
        // loc_41F6FF: state = RELOAD_END (0xB), submit anim event 0x10, queue notify 0x14
        ps->weaponstate = (weaponstate_t)0xB;
        if (ps->pm_type < 8) 
        {
            int v = raw_int(ps, 0x910);
            raw_iref(ps, 0x910) = ((~v) & 0x200) | 0x10;
        }
        raw_iref(ps, 0x40) = raw_int(w, 0x480);
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

        if (raw_int(ps, 0x40) != 0) 
            return;          // bail to loc_41F8BE
    }

    // loc_41F7AE
    if (raw_int(ps, 0x40) != 0) 
        return;

    // Force INTERUPT state if weapon supports interruptible reload AND attack pressed
    if (raw_int(w, 0x628) != 0 && (raw_byte(pm, 0x8) & 1) != 0) 
        ps->weaponstate = (weaponstate_t)8;          // RELOADING_INTERUPT

    // loc_41F7D2 — clear weapon-pickup bit
    {
        unsigned int wIdx = (unsigned int)ps->weapon;
        raw_iref(ps, 0x81C + (wIdx >> 5) * 4) &= ~(1u << (wIdx & 0x1F));
    }

    if (raw_int(w, 0x628) == 0) 
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
    if (raw_int(w, 0x480) != 0) 
    {
        ps->weaponstate = (weaponstate_t)0xB;        // RELOAD_END
        call_PM_Weapon_SubmitAnimEvent(ps, 0x10);
        raw_iref(ps, 0x40) = raw_int(w, 0x480);
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
extern "C" void T4_Reconstructed::PM_Weapon_Tick_MeleeFire(playerState_s* ps)
{
    const WeaponDef* w = GetWeaponDef(ps->weapon);

    if (raw_int(w, 0x60C) != 0) 
    {
        ps->weaponstate = (weaponstate_t)0xF;
        raw_iref(ps, 0x40) = 0x12C;
        raw_iref(ps, 0x44) = 0;
        return;
    }

    if (raw_int(w, 0x3CC) == 0) 
    {
        // Path C: tail-call sub_4225D0
        call_PM_Weapon_FinalizeStateExit(ps);
        return;
    }

    ps->weaponstate = (weaponstate_t)0xF;
    raw_iref(ps, 0x40) = raw_int(w, 0x498);
    raw_iref(ps, 0x44) = 0;

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
extern "C" void T4_Reconstructed::PM_Weapon_Tick_MeleeCharge(playerState_s* ps)
{
    const WeaponDef* w = GetWeaponDef(ps->weapon);
    raw_iref(ps, 0x40) = raw_int(w, 0x460);
    raw_iref(ps, 0x44) = raw_int(w, 0x440);

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
extern "C" void T4_Reconstructed::PM_Weapon_Tick_MeleeInit(playerState_s* ps)
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
extern "C" void T4_Reconstructed::PM_Weapon_Tick_OffhandPrepare(playerState_s* ps)
{
    raw_iref(ps, 0x10) |= 2;
    ps->weaponstate = (weaponstate_t)0x13;
    raw_iref(ps, 0x40) = 0;
    raw_iref(ps, 0x44) = 0;

    if (raw_int(ps, 0x4C) != 0x3FF) 
        return;

    const WeaponDef* w = GetWeaponDef(raw_int(ps, 0xFC));
    raw_iref(ps, 0x48) = raw_int(w, 0x4D4);
}

// @faithful — chunk @ 0x00421410 — OFFHAND_START → OFFHAND_HOLD transition.
//   Special case: if w->[+0x6B8] == 0 AND pmove_t.[+0x40] & 0xC000 AND pmove_t.[+0x8] & 0xC000
//   → set fireTime=1 and do NOT transition.
extern "C" void T4_Reconstructed::PM_Weapon_Tick_OffhandStart(pmove_t* pm)
{
    auto* ps = *(playerState_s**)pm;
    const WeaponDef* w = GetWeaponDef(raw_int(ps, 0xFC));   // offHandIndex

    if (raw_int(w, 0x6B8) == 0
        && (raw_int(pm, 0x40) & 0xC000) != 0
        && (raw_int(pm, 0x8)  & 0xC000) != 0) 
    {
        // Inhibit transition: set fireTime=1 and exit
        raw_iref(ps, 0x44) = 1;
        return;
    }

    // Standard transition path
    ps->weaponstate = (weaponstate_t)0x12;
    raw_iref(ps, 0x40) = raw_int(w, 0x448);
    raw_iref(ps, 0x10) |= 2;
    raw_iref(ps, 0x44) = raw_int(w, 0x438);

    if (ps->pm_type >= 8)
        return;

    int v = raw_int(ps, 0x910);
    raw_iref(ps, 0x910) = ((~v) & 0x200) | 2;

    if (ps->pm_type >= 8)
        return;

    // Vanilla: mov eax, dword_46DE3B0; add eax, 18B78h; cmp [eax], 0; jz skip
    // dword_46DE3B0 holds a pointer; we deref then add 0x18B78 to get an event-list struct.
    char* clientBase = *(char**)T4M::GetAddress("clientStatePtr");
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
            call T4::engine::p_EventList_GetEntry
            add     esp, 4                     ; clean dummy
            test    eax, eax
            jz      done
            mov     edi, eax                   ; edi = eventInfo
            mov     ecx, [edi + 34h]
            test    ecx, ecx
            jz      done

            call T4::engine::p_rand
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
            call T4::engine::p_WeaponEvent_ApplyToPS
            add     esp, 10h

done:
            pop     ebx
            pop     edi
            pop     esi
        }
    }
}

// @faithful — chunk @ 0x004214E0 — OFFHAND_HOLD → SWIM_IN or OFFHAND.
extern "C" void T4_Reconstructed::PM_Weapon_Tick_OffhandHold(pmove_t* pm)
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
    int wOff_3FC = raw_int(w, 0x3FC);
    int wOff_3F4 = raw_int(w, 0x3F4);
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
            call T4::engine::p_PM_Weapon_OffhandSelect
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
extern "C" void T4_Reconstructed::PM_Weapon_Tick_SwimIn(playerState_s* ps)
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
extern "C" void T4_Reconstructed::PM_Weapon_Tick_SwimOut(playerState_s* ps)
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
    ps->weaponstate = T4::engine::WEAPON_LOWREADY_START;
    raw_iref(ps, 0x40) = w->iLowReadyInTime;
    raw_iref(ps, 0x44) = 0;
    if (ps->pm_type < 8) 
    {
        int v = raw_int(ps, 0x910);
        raw_iref(ps, 0x910) = ((~v) & 0x200) | 0x20;
    }
}

void T4M::EnterLowReadyEnd(playerState_s* ps)
{
    const WeaponDef* w = GetWeaponDef(ps->weapon);
    ps->weaponstate = T4::engine::WEAPON_LOWREADY_END;
    raw_iref(ps, 0x40) = w->iLowReadyOutTime;
    raw_iref(ps, 0x44) = 0;
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
        if (ps->weaponstate < T4::engine::WEAPON_LOWREADY_START || ps->weaponstate > T4::engine::WEAPON_LOWREADY_END)
        {
            EnterLowReadyStart(ps);
        }

    } 
    else 
    {
        ps->eFlags &= ~0x400;
        if (ps->weaponstate == T4::engine::WEAPON_LOWREADY_LOOP)
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
    ps->weaponstate = T4::engine::WEAPON_LOWREADY_LOOP;
    raw_iref(ps, 0x40) = w->iLowReadyLoopTime;   // 0 = infinite (LOOP case is no-op)
    raw_iref(ps, 0x44) = 0;
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
    ps->weaponstate = T4::engine::WEAPON_READY;
    raw_iref(ps, 0x40) = 0;
    raw_iref(ps, 0x44) = 0;
    if (ps->pm_type < 8) 
    {
        int v = raw_int(ps, 0x910);
        raw_iref(ps, 0x910) = (~v) & 0x200;
    }
}

// @faithful — chunk @ 0x00421D80 — SPRINT_RAISE → SPRINT_LOOP transition.
extern "C" void T4_Reconstructed::PM_Weapon_Tick_SprintRaise(playerState_s* ps)
{
    ps->weaponstate = (weaponstate_t)0x18;
    raw_iref(ps, 0x40) = 0;
    raw_iref(ps, 0x44) = 0;

    if (ps->pm_type >= 8) 
        return;

    int v = raw_int(ps, 0x910);
    raw_iref(ps, 0x910) = ((~v) & 0x200) | 0x18;
}

// =====================================================================
// Shared chunks — kept as register-preserving stubs (vanilla call).
// =====================================================================

extern "C" __declspec(naked) void T4_Reconstructed::PM_Weapon_Tick_OffhandInit_Stub(playerState_s* /*ps*/)
{
    __asm 
    {
        push    esi
        push    edi
        push    ebx
        mov     eax, [esp+10h]
        call T4::engine::p_PM_Weapon_Tick_OffhandInit_Stub
        pop     ebx
        pop     edi
        pop     esi
        ret
    }
}

extern "C" __declspec(naked) void T4_Reconstructed::PM_Weapon_Tick_Offhand_Stub(playerState_s* /*ps*/)
{
    __asm 
    {
        push    esi
        push    edi
        push    ebx
        mov     eax, [esp+10h]
        call T4::engine::p_PM_Weapon_Tick_Offhand_Stub
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
        raw_iref(pm, 0x8) &= ~kClearMask;
    }

    if ((ps->eFlags & 0x400) != 0 
            && (ps->weaponstate == T4::engine::WEAPON_READY
            || ps->weaponstate == T4::engine::WEAPON_LOWREADY_START
            || ps->weaponstate == T4::engine::WEAPON_LOWREADY_END))
    {
        ps->weaponstate = T4::engine::WEAPON_LOWREADY_LOOP;
        raw_iref(ps, 0x40) = 0;
        raw_iref(ps, 0x44) = 0;

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

    if (raw_int(ps, 0xCC) & 0x300) 
        return;

    // SECTION 2: Swim transitions (when [+0x944] < 3)
    if (raw_int(ps, 0x944) < 3)
    {
        int state = ps->weaponstate;
        if (state == 0x1D) 
        {
            ps->weaponstate = T4::engine::WEAPON_SWIM_OUT;
        } 
        else if (state == 0x1E) 
        {
            if (!T4M::TransitionToReadyOrLowReady(ps))
                ps->weaponstate = T4::engine::WEAPON_READY;
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
        else if (state >= T4::engine::WEAPON_LOWREADY_START && state <= T4::engine::WEAPON_LOWREADY_END)
        {
            // T4M: LOWREADY states bypass the force-SWIM_IN else branch.
            // Without this, SECTION 3 forces SWIM_IN every frame causing visual cycling.
            needs_swim_check = true;
        }
        else
        {
            // loc_42222B — force SWIM_IN
            raw_iref(ps, 0x10) |= 0x2000;
            ps->weaponstate = T4::engine::WEAPON_SWIM_IN;
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
        && raw_byte(pm, 0x1A) == 0
        && raw_byte(pm, 0x1B) == 0;

    if (first_block) 
    {
        if (raw_float(ps, 0x110) != 0.0f) 
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
        raw_iref(ps, 0x914) = *(int*)T4M::GetAddress("flt_255");
    }

    // Bail if not anim-sync and any timer pending
    if (animSync == 0) 
    {
        if (raw_int(ps, 0x40) != 0) 
            return;

        if (raw_int(ps, 0x44) != 0) 
            return;
    }

    // SECTION 5: Main switch dispatch
    int state = ps->weaponstate;
    int v = raw_int(ps, 0x910);

    switch (state) 
    {
        case T4::engine::WEAPON_RAISING:
        case T4::engine::WEAPON_RAISING_ALTSWITCH:
            if (ps->pm_type >= 8)
                break;

            if (T4M::TransitionToReadyOrLowReady(ps)) 
                break;

            ps->weaponstate = T4::engine::WEAPON_READY;
            raw_iref(ps, 0x910) = (~v) & 0x200;
            break;
        case T4::engine::WEAPON_DROPPING:
            call_PM_Weapon_TickDrop(pm, 0);
            break;
        case T4::engine::WEAPON_DROPPING_QUICK:
            call_PM_Weapon_TickDrop(pm, 1);
            break;
        case T4::engine::WEAPON_RELOADING:
        case T4::engine::WEAPON_RELOADING_INTERUPT:
            // Vanilla `push ebx; push edi; call sub_41F780` — ebx = animSync (after overwrite).
            T4_Reconstructed::PM_Weapon_Reloading(pm, animSync);
            break;
        case T4::engine::WEAPON_RELOAD_START:
        case T4::engine::WEAPON_RELOAD_START_INTERUPT:
            T4_Reconstructed::PM_Weapon_ReloadStart(pm, animSync);
            break;
        case T4::engine::WEAPON_RELOAD_END:
            if (T4M::TransitionToReadyOrLowReady(ps)) 
                break;

            ps->weaponstate = T4::engine::WEAPON_READY;
            if (ps->pm_type >= 8)
                break;

            raw_iref(ps, 0x910) = (~v) & 0x200;
            break;
        case T4::engine::WEAPON_MELEE_CHARGE:
            T4_Reconstructed::PM_Weapon_Tick_MeleeCharge(ps);
            break;
        case T4::engine::WEAPON_MELEE_INIT:
            T4_Reconstructed::PM_Weapon_Tick_MeleeInit(ps);
            break;
        case T4::engine::WEAPON_MELEE_FIRE:
            T4_Reconstructed::PM_Weapon_Tick_MeleeFire(ps);
            break;
        case T4::engine::WEAPON_MELEE_END:
        case T4::engine::WEAPON_OFFHAND_END:
        case T4::engine::WEAPON_SPRINT_DROP:
            call_PM_Weapon_FinalizeStateExit(ps);
            break;
        case T4::engine::WEAPON_OFFHAND_INIT:
            T4_Reconstructed::PM_Weapon_Tick_OffhandInit_Stub(ps);
            break;
        case T4::engine::WEAPON_OFFHAND_PREPARE:
            T4_Reconstructed::PM_Weapon_Tick_OffhandPrepare(ps);
            break;
        case T4::engine::WEAPON_OFFHAND_HOLD:
            T4_Reconstructed::PM_Weapon_Tick_OffhandHold(pm);
            break;
        case T4::engine::WEAPON_OFFHAND_START:
            T4_Reconstructed::PM_Weapon_Tick_OffhandStart(pm);
            break;
        case T4::engine::WEAPON_OFFHAND:
            T4_Reconstructed::PM_Weapon_Tick_Offhand_Stub(ps);
            break;
        case T4::engine::WEAPON_DETONATING:
            T4_Reconstructed::PM_Weapon_DetonateTransition(ps, animSync);
            break;
        case T4::engine::WEAPON_SPRINT_RAISE:
            T4_Reconstructed::PM_Weapon_Tick_SprintRaise(ps);
            break;
        case T4::engine::WEAPON_SPRINT_LOOP:
        case T4::engine::WEAPON_DEPLOYED:
            break;
        case T4::engine::WEAPON_DEPLOYING:
            call_Anim_TriggerEvent(ps, 0x23);
            break;
        case T4::engine::WEAPON_BREAKING_DOWN:
            call_PM_Weapon_FinalizeStateExit(ps);
            call_Anim_TriggerEvent(ps, 0x25);
            break;
        case T4::engine::WEAPON_SWIM_IN:
            T4_Reconstructed::PM_Weapon_Tick_SwimIn(ps);
            break;
        case T4::engine::WEAPON_SWIM_OUT:
            T4_Reconstructed::PM_Weapon_Tick_SwimOut(ps);
            break;
        case T4::engine::WEAPON_LOWREADY_START:
            T4M::PM_Weapon_LowReady_Start(ps);
            break;
        case T4::engine::WEAPON_LOWREADY_LOOP:
            break;
        case T4::engine::WEAPON_LOWREADY_END:
            T4M::PM_Weapon_LowReady_End(ps);
            break;
        case T4::engine::WEAPON_FIRING:
        case T4::engine::WEAPON_RECHAMBERING:
        case T4::engine::WEAPON_READY:
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

            if (ps->weaponstate == T4::engine::WEAPON_DEPLOYING)
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
    const WeaponDef* wd = ((const WeaponDef* const*)T4M::GetAddress("bg_weaponDefs"))[weaponIdx];

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

    minSpeed += DvarF(T4M::GetAddress("cg_gun_minspeed"));

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

        float gmf = DvarF(T4M::GetAddress("cg_gun_move_f"));
        float gmr = DvarF(T4M::GetAddress("cg_gun_move_r"));
        float gmu = DvarF(T4M::GetAddress("cg_gun_move_u"));
        smTargetF = ofsF * t + gmf * t;
        smTargetR = ofsR * t + gmr * t;
        smTargetU = ofsU * t + gmu * t;
    }

    if (isType28) 
    {
        float gof = DvarF(T4M::GetAddress("cg_gun_ofs_f"));
        float gor = DvarF(T4M::GetAddress("cg_gun_ofs_r"));
        float gou = DvarF(T4M::GetAddress("cg_gun_ofs_u"));
        smTargetF = gof + (wd->vDuckedOfs[0] + smTargetF);
        smTargetR = gor + (wd->vDuckedOfs[1] + smTargetR);
        smTargetU = gou + (wd->vDuckedOfs[2] + smTargetU);
    } 
    else if (isType0B) 
    {
        float gof = DvarF(T4M::GetAddress("cg_gun_ofs_f"));
        float gor = DvarF(T4M::GetAddress("cg_gun_ofs_r"));
        float gou = DvarF(T4M::GetAddress("cg_gun_ofs_u"));
        smTargetF = gof + (wd->vProneOfs[0] + smTargetF);
        smTargetR = gor + (wd->vProneOfs[1] + smTargetR);
        smTargetU = gou + (wd->vProneOfs[2] + smTargetU);
    }

    float* smBuf = (float*)((char*)cg + 0xAD03C);
    const float kMs2S   =  0.001f;
    const float kEpsPos =  1.0e-4f;
    const float kEpsNeg = -1.0e-4f;
    float gunMoveRate = DvarF(T4M::GetAddress("cg_gun_move_rate"));
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

        float gmf = DvarF(T4M::GetAddress("cg_gun_move_f"));
        float gmr = DvarF(T4M::GetAddress("cg_gun_move_r"));
        float gmu = DvarF(T4M::GetAddress("cg_gun_move_u"));
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
    const WeaponDef* wd = ((const WeaponDef* const*)T4M::GetAddress("bg_weaponDefs"))[weaponIdx];

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

    minSpeed += DvarF(T4M::GetAddress("cg_gun_rot_minspeed"));

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

        float grp = DvarF(T4M::GetAddress("cg_gun_rot_p"));
        float gry = DvarF(T4M::GetAddress("cg_gun_rot_y"));
        float grr = DvarF(T4M::GetAddress("cg_gun_rot_r"));
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
    float gunRotRate = DvarF(T4M::GetAddress("cg_gun_rot_rate"));
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
    static auto pm_checkads_hook = safetyhook::create_mid(T4M::GetAddress("PM_CheckAds"),
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
    Detours::X86::DetourFunction((uintptr_t)T4M::GetAddress("PM_Weapon"), (uintptr_t)&T4M::PM_Weapon_Wrapper, Detours::X86Option::USE_JUMP);

    // sub_465FF0 — viewmodel translation (lowReady pull added)
    Detours::X86::DetourFunction((uintptr_t)T4M::GetAddress("CG_ApplyViewmodelMoveOfs"), (uintptr_t)&T4M::CG_ApplyViewmodelMoveOfs_Wrapper, Detours::X86Option::USE_JUMP);

    // sub_4664E0 — viewmodel rotation (lowReady pull added)
    Detours::X86::DetourFunction((uintptr_t)T4M::GetAddress("CG_ApplyViewmodelRotOfs"), (uintptr_t)&T4M::CG_ApplyViewmodelRotOfs_Wrapper, Detours::X86Option::USE_JUMP);

    // sub_41DD10 (PM_CheckAds) — block ADS while lowReady intent is set
    InstallPmCheckAdsHook();
}
