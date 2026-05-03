#include "t4_headers.h"
#include "StdInc.h"
#include "T4.h"
#include "MemoryMgr.h"


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
    return ((WeaponDef**)0x008F6770)[idx];
}


// =====================================================================
// Multi-caller / complex-mono helpers — register-preserving shims.
// All shims save+restore esi/edi/ebx around the vanilla call to honor cdecl.
// =====================================================================

// sub_421FF0 — __usercall(esi=ps, [esp+4]=arg_0); returns bool in al. (multi-caller)
static __declspec(naked) char call_IsWeaponInputBlocked(playerState_s* /*ps*/, int /*arg_0*/)
{
    __asm {
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
    __asm {
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
    __asm {
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
    __asm {
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
    __asm {
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
    __asm {
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
    __asm {
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
    __asm {
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
    __asm {
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
    __asm {
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
    __asm {
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
    __asm {
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
    __asm {
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
    __asm {
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
    __asm {
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

// sub_41F010 — DROPPING handler. __thiscall(ecx=pm, [esp+4]=isQuick). Kept as shim.
static __declspec(naked) void call_PM_Weapon_TickDrop(pmove_t* /*pm*/, int /*isQuick*/)
{
    __asm {
        push    esi
        push    edi
        push    ebx
        mov     ecx, [esp+10h]
        push    [esp+14h]
        mov     eax, 0x0041F010
        call    eax
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
    __asm {
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
    __asm {
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
    __asm {
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
    __asm {
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
    int v = raw_int(w, 0x158);
    if (v == 0 || v == 1) return;
    if ((raw_byte(pm, 0x8)  & 1) == 0) return;
    if ((raw_byte(pm, 0x40) & 1) != 0) return;
    raw_iref(ps, 0x10) |= 0x400;
}

// @faithful — sub_421A30 (entry 0x00421A30). __usercall(edi=pm); returns bool.
//   Force-fire check for melee/RPG weapons.
//   Mono-caller from PM_Weapon (our reconstruction) → plain cdecl.
extern "C" char T4_Reconstructed::PM_Weapon_CanForceFire(pmove_t* pm)
{
    auto* ps = *(playerState_s**)pm;
    if (ps->weapon == 0) return 0;
    const WeaponDef* w = GetWeaponDef(ps->weapon);
    int weaponClass = raw_int(w, 0x144);
    if (weaponClass != 1 && weaponClass != 6) return 0;
    if (raw_int(w, 0x6B8) != 0) return 0;

    // sub_420500 is __usercall(eax=weaponIdx); returns bit mask in eax.
    int mask;
    int weaponIdx = ps->weapon;
    __asm {
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

    if ((raw_int(pm, 0x8) & mask) == 0) return 0;

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
    if (edx_state != 0) {
        weaponIdx = raw_int(ps, 0xFC);
    } else {
        weaponIdx = ps->weapon;
        if (weaponIdx == 0) {
            return 0;
        }
    }

    const WeaponDef* w = GetWeaponDef(weaponIdx);
    int weaponClass = raw_int(w, 0x144);
    if (weaponClass != 1 && weaponClass != 6) {
        return 0;
    }

    // loc_421974 — bail if v48 <= 0 (timer not active)
    int v48 = raw_int(ps, 0x48);
    if (v48 <= 0) return 0;                 // jle loc_4219B7 → return 0

    // v48 > 0: maybe decrement
    bool skip_decrement = (raw_byte(ps, 0x8E8) & 1) != 0
                       || raw_int(w, 0x5E8) == 0;
    if (!skip_decrement) {
        // Vanilla: mov ecx, [esp+4+arg_0]; sub eax, [ecx+28h]
        // arg_0 is an opaque caller-context pointer; deref [+0x28] as int.
        int delta = *(const int*)((const char*)(uintptr_t)arg_0 + 0x28);
        raw_iref(ps, 0x48) = v48 - delta;   // v48 may now be <= 0
    }

    // loc_421997 — state transition if conditions met
    bool to_4219BB = (raw_int(ps, 0x944) < 3)
                  || (edx_state == 0)
                  || (ps->weaponstate != 0x13);
    if (!to_4219BB) {
        ps->weaponstate = (weaponstate_t)0x12;
        return 0;                            // fall through to loc_4219B7
    }

    // loc_4219BB — post-decrement check
    if (raw_int(ps, 0x48) > 0) return 0;     // still pending → bail

    // Timer just expired: queue notify 0x56 with low-16-bits of [ps+0xFC] (offhand idx).
    // Vanilla unconditionally loads eax = [edi+0FCh] at loc_4219BB regardless of edx_state.
    int offhandIdx = raw_int(ps, 0xFC);
    int head = raw_int(ps, 0xD0) & 3;
    raw_iref(ps, 0x48) = -1;
    raw_iref(ps, 0xD4 + head * 4) = 0x56;
    int head2 = raw_int(ps, 0xD0) & 3;
    raw_iref(ps, 0xE4 + head2 * 4) = offhandIdx & 0xFFFF;     // movzx ecx, ax in vanilla
    raw_iref(ps, 0xD0) = (raw_int(ps, 0xD0) + 1) & 0xFF;

    if (raw_int(ps, 0x4C) != 0x3FF || (raw_byte(ps, 0x8E8) & 1) != 0) {
        return 1;  // loc_421A2C: mov al, 1; pop edi; retn
    }

    // sub_41E5E0 is __usercall(edi=ps, esi=cap, [esp+4]=offhandIdx).
    __asm {
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
    if (ps->weapon == 0) return;
    const WeaponDef* w = GetWeaponDef(ps->weapon);
    int weaponClass = raw_int(w, 0x144);
    if (weaponClass != 1 && weaponClass != 6) return;
    if (raw_int(w, 0x6AC) == 0) return;

    int state = ps->weaponstate;
    // bail list (states that block transition to DETONATING):
    //   0x16, 7, 9, 0xB, 0xA, 8, 5, 6, 0xD, 0xE, 0xF, 1, 2, 3, 4
    //   AND any state in [0x10, 0x15]
    if (state == 0x16 || state == 7 || state == 9 || state == 0xB ||
        state == 0xA  || state == 8 || state == 5 || state == 6 ||
        state == 0xD  || state == 0xE || state == 0xF ||
        state == 1    || state == 2 || state == 3 || state == 4) return;
    if (state >= 0x10 && state <= 0x15) return;

    // loc_421B13
    if ((raw_byte(pm, 0x8) & 1) == 0) return;

    ps->weaponstate = (weaponstate_t)0x16;     // WEAPON_DETONATING
    raw_iref(ps, 0x40) = raw_int(w, 0x458);
    raw_iref(ps, 0x44) = raw_int(w, 0x444);

    // sub_41D420 is __usercall(eax=ps, [esp+4]=new_vm_state).
    __asm {
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

    if (state == 0x11) {                      // WEAPON_OFFHAND_PREPARE
        const WeaponDef* w = GetWeaponDef(raw_int(ps, 0xFC));
        if (raw_int(w, 0x6B8) != 0 && (raw_int(pm, 0x8) & 0xC000) == 0) {
            // jmp shared chunk @ 0x00421630 (with eax = ps)
            T4_Reconstructed::PM_Weapon_Tick_Offhand_Stub(ps);
            return;
        }
        // fall through to loc_421BB7 (no-op end)
        return;
    }

    // loc_421B73
    if (ps->weapon == 0) return;
    const WeaponDef* w = GetWeaponDef(ps->weapon);
    int weaponClass = raw_int(w, 0x144);
    if (weaponClass != 1 && weaponClass != 6) return;

    // loc_421B94
    if (state != 5) return;                   // not WEAPON_FIRING
    if (raw_int(w, 0x6B8) == 0) return;
    if ((raw_byte(pm, 0x8) & 1) != 0) return;

    // call sub_4225D0(eax = ps)
    call_PM_Weapon_FinalizeStateExit(ps);
    // sub_41D420 is __usercall(eax=ps, [esp+4]=new_vm_state).
    __asm {
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
    int weaponClass = raw_int(w, 0x144);

    int mask;
    if ((weaponClass == 1 || weaponClass == 6) && raw_int(w, 0x6AC) != 0) {
        mask = 0x400000;
    } else {
        mask = 1;
    }

    int bl = ((raw_int(pm, 0x8) & mask) != 0) ? 1 : 0;

    if (raw_int(w, 0x6BC) != 0 && raw_int(ps, 0x88) == 0x3FF) {
        bl = 0;
    }

    if (raw_int(w, 0x5F0) != 0) {
        // dword_1F552A4 holds a dvar_t* pointer; deref then check [+0x10] (dvar bool field).
        unsigned char* dvar_ptr = *(unsigned char**)0x01F552A4;
        if (dvar_ptr[0x10] != 0) {
            bl = 0;
        }
    }

    char al;
    if (arg_0 != 0) {
        al = 1;
    } else {
        al = call_PM_Weapon_HasAnimSync(ps);
        if (al == 0) {
            // fall through with al = 0
        } else {
            al = 1;
        }
    }

    if (bl != 0 || al != 0) return 1;

    if (ps->weaponstate == 5) {
        // sub_41D440 is __usercall(ecx=ps, edx=new_vm_anim_state).
        __asm {
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
    if (raw_int(ps, 0x944) >= 3) return;
    if ((raw_int(ps, 0x0C) & 0x8000000) != 0) return;
    const WeaponDef* w = GetWeaponDef(ps->weapon);

    int state = ps->weaponstate;
    if (state == 0xD || state == 0xE || state == 0xF) return;
    if (state >= 0x10 && state <= 0x15) return;

    // loc_421169
    if ((raw_byte(pm, 0x8) & 4) != 0) {
        if (call_PM_Weapon_IsLocked(pm) != 0) return;
    }

    // loc_42117E
    if (raw_int(w, 0x430) == 0) return;
    if (arg_0 != 0) return;
    if ((raw_byte(ps, 0x14) & 0x20) != 0) return;

    if (raw_int(ps, 0x44) != 0) {
        if (state != 7 && state != 9 && state != 0xB && state != 0xA && state != 8) return;
    }

    // loc_4211B9
    if ((raw_byte(pm, 0x8)  & 4) == 0) return;
    if ((raw_byte(pm, 0x40) & 4) != 0) return;

    float wpf = raw_float(ps, 0x110);
    bool slowAnim = (wpf <= *(float*)0x008B7B30);  // ds:String2 — float 0.0 sentinel
    if (!slowAnim && raw_int(w, 0x524) != 0) return;

    // loc_4211DF — get string pointer at w+0x6C
    const char* s = *(const char**)((char*)w + 0x6C);
    if (s == nullptr || s[0] == 0) return;

    if (state > 0 && state <= 4) return;

    // loc_4211F6: call sub_419F20(__thiscall ecx=pm); call sub_420E70(cdecl arg_0=ps).
    // sub_420E70 reads [ps+0xC] = pm_flags, [ps+0x104] = weapon — transitions to MELEE_CHARGE.
    __asm {
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
    if (arg_0 == 0 || ps->weapon == 0) {
        // loc_421C0A — finalize (no-op end)
        raw_iref(ps, 0x10) &= 0xFFFFFFFD;
        raw_iref(ps, 0x0C) &= 0xFFFFFDFF;
        raw_iref(ps, 0x40) = 0;
        raw_iref(ps, 0x44) = 0;
        ps->weaponstate = (weaponstate_t)0;

        if (ps->pm_type < 8) {
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

    if (callValidation != 0) {
        call_PM_Weapon_RefreshReload(ps);
    }

    // loc_41F654
    if (raw_int(ps, 0x40) != 0) return;

    // Force INTERUPT state if weapon supports interruptible reload AND attack pressed
    if (raw_int(w, 0x628) != 0 && (raw_byte(pm, 0x8) & 1) != 0) {
        ps->weaponstate = (weaponstate_t)0xA;       // RELOAD_START_INTERUPT
    }

    // loc_41F678
    if (ps->weaponstate == 0xA) {
        const WeaponDef* w2 = GetWeaponDef(ps->weapon);
        int idx = raw_int(w2, 0x3FC);
        if (raw_int(ps, 0x5FC + idx * 4) != 0) {
            goto path_clear_bit;                     // jmp loc_41F6B0
        }
    }

    // loc_41F69D — try fast-reload chunk
    if (call_PM_Weapon_CheckFastReload(ps) != 0) {
        call_PM_Weapon_ReloadFinalize(ps);                       // tail-call shared chunk
        return;
    }

path_clear_bit:
    // loc_41F6B0 — clear weapon-pickup bit in [+0x81C + (weapon>>5)*4]
    {
        unsigned int wIdx = (unsigned int)ps->weapon;
        raw_iref(ps, 0x81C + (wIdx >> 5) * 4) &= ~(1u << (wIdx & 0x1F));
    }

    if (raw_int(w, 0x480) != 0) {
        // loc_41F6FF: state = RELOAD_END (0xB), submit anim event 0x10, queue notify 0x14
        ps->weaponstate = (weaponstate_t)0xB;
        if (ps->pm_type < 8) {
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
    ps->weaponstate = (weaponstate_t)0;
    if (ps->pm_type >= 8) return;
    int v = raw_int(ps, 0x910);
    raw_iref(ps, 0x910) = (~v) & 0x200;
}

// @faithful — sub_41F780 (entry 0x0041F780). RELOADING / RELOADING_INTERUPT.
extern "C" void T4_Reconstructed::PM_Weapon_Reloading(pmove_t* pm, int callValidation)
{
    auto* ps = *(playerState_s**)pm;
    const WeaponDef* w = GetWeaponDef(ps->weapon);

    if (callValidation != 0) {
        call_PM_Weapon_RefreshReload(ps);
        if (raw_int(ps, 0x40) != 0) return;          // bail to loc_41F8BE
    }

    // loc_41F7AE
    if (raw_int(ps, 0x40) != 0) return;

    // Force INTERUPT state if weapon supports interruptible reload AND attack pressed
    if (raw_int(w, 0x628) != 0 && (raw_byte(pm, 0x8) & 1) != 0) {
        ps->weaponstate = (weaponstate_t)8;          // RELOADING_INTERUPT
    }

    // loc_41F7D2 — clear weapon-pickup bit
    {
        unsigned int wIdx = (unsigned int)ps->weapon;
        raw_iref(ps, 0x81C + (wIdx >> 5) * 4) &= ~(1u << (wIdx & 0x1F));
    }

    if (raw_int(w, 0x628) == 0) {
        // loc_41F89B: state = READY
        ps->weaponstate = (weaponstate_t)0;
        if (ps->pm_type >= 8) return;
        int v = raw_int(ps, 0x910);
        raw_iref(ps, 0x910) = (~v) & 0x200;
        return;
    }

    if (ps->weaponstate != 8) {
        if (call_PM_Weapon_CheckFastReload(ps) != 0) {
            call_PM_Weapon_ReloadFinalize(ps);                   // tail-call shared chunk
            return;
        }
    }

    // loc_41F81A
    if (raw_int(w, 0x480) != 0) {
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

    if (ps->pm_type < 8) {
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
    if ((v0C & 1) != 0) {
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
    if ((v0C & 1) != 0) {
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

    if (raw_int(ps, 0x4C) != 0x3FF) return;

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
        && (raw_int(pm, 0x8)  & 0xC000) != 0) {
        // Inhibit transition: set fireTime=1 and exit
        raw_iref(ps, 0x44) = 1;
        return;
    }

    // Standard transition path
    ps->weaponstate = (weaponstate_t)0x12;
    raw_iref(ps, 0x40) = raw_int(w, 0x448);
    raw_iref(ps, 0x10) |= 2;
    raw_iref(ps, 0x44) = raw_int(w, 0x438);

    if (ps->pm_type >= 8) return;

    int v = raw_int(ps, 0x910);
    raw_iref(ps, 0x910) = ((~v) & 0x200) | 2;

    if (ps->pm_type >= 8) return;

    // Vanilla: mov eax, dword_46DE3B0; add eax, 18B78h; cmp [eax], 0; jz skip
    // dword_46DE3B0 holds a pointer; we deref then add 0x18B78 to get an event-list struct.
    char* clientBase = *(char**)0x046DE3B0;
    int* eventListBase = (int*)(clientBase + 0x18B78);
    if (*eventListBase != 0) {
        int offhand_x_F8 = raw_int(ps, 0xF8);

        __asm {
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
extern "C" void T4_Reconstructed::PM_Weapon_Tick_OffhandHold(pmove_t* pm)
{
    auto* ps = *(playerState_s**)pm;
    int head = raw_int(ps, 0xD0) & 3;
    int offhand = raw_int(ps, 0xFC) & 0xFFFF;

    if (raw_int(ps, 0x944) >= 3) {
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

    if (raw_int(ps, 0x4C) != 0x3FF) {
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
    if (sum == 0) {
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
    if ((raw_byte(ps, 0x8E8) & 1) == 0) {
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
extern "C" void T4_Reconstructed::PM_Weapon_Tick_SwimIn(playerState_s* ps)
{
    int head = raw_int(ps, 0xD0) & 3;
    raw_iref(ps, 0xD4 + head * 4) = 3;
    int head2 = raw_int(ps, 0xD0) & 3;
    raw_iref(ps, 0xE4 + head2 * 4) = 0;
    raw_iref(ps, 0xD0) = (raw_int(ps, 0xD0) + 1) & 0xFF;

    if ((raw_int(ps, 0x10) & 0x2000) != 0) {
        if (ps->pm_type >= 8) return;
        int v = raw_int(ps, 0x910);
        raw_iref(ps, 0x910) = ((~v) & 0x200) | 0xA;
        return;
    }

    int v910 = raw_int(ps, 0x910);
    if ((v910 & 0xFFFFFDFF) == 0xA) return;
    if (ps->pm_type >= 8) return;
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

    if (ps->pm_type >= 8) return;
    int v = raw_int(ps, 0x910);
    raw_iref(ps, 0x910) = ((~v) & 0x200) | 0xB;
}

// @faithful — chunk @ 0x00421CD0 — SWIM_OUT tick: sound notify code 1.
void T4M::PM_Weapon_LowReady_Start(playerState_s* ps)
{

}

// @faithful — chunk @ 0x00421CD0 — SWIM_OUT tick: sound notify code 1.
void T4M::PM_Weapon_LowReady_End(playerState_s* ps)
{

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
    __asm {
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

extern "C" __declspec(naked) void T4_Reconstructed::PM_Weapon_Tick_Offhand_Stub(playerState_s* /*ps*/)
{
    __asm {
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
        if (state == 0x1D) {
            ps->weaponstate = WEAPON_SWIM_OUT;
        } else if (state == 0x1E) {
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
        else 
        {
            // loc_42222B — force SWIM_IN
            raw_iref(ps, 0x10) |= 0x2000;
            ps->weaponstate = WEAPON_SWIM_IN;
            goto dispatch_setup;
        }

        if (needs_swim_check) {
            if (state != 0x1D) goto dispatch_setup;
            int v = raw_int(ps, 0x10);
            if (v & 0x2000) {
                raw_iref(ps, 0x10) = v & ~0x2000;
                goto dispatch_setup;
            }
            if (state == 0x1D) return;
        }
    }

dispatch_setup:
    // SECTION 4: Validation + anim sync + heavy block
    call_PM_Weapon_DispatchAnimHandler(pm, callValidation);
    call_PM_Weapon_TickValidation(pm, callValidation);
    // Vanilla `push ebx; mov eax, ebp; call sub_421940` — ebx = callValidation
    // (BEFORE the SECTION 4 overwrite that turns ebx into animSync).
    if (T4_Reconstructed::PM_Weapon_FireRefresh(ps, callValidation) != 0) return;

    int animSync;
    {
        T4_Reconstructed::PM_Weapon_AnimCmdStateInit(pm);
        int prev = call_PM_Weapon_AnimCmdState_Resync(callValidation, pm);
        animSync = prev;
        if (call_PM_Weapon_HasAnimSync(ps) == 0) {
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
        raw_iref(ps, 0x914) = *(int*)0x008AF224;
    }

    // Bail if not anim-sync and any timer pending
    if (animSync == 0) 
    {
        if (raw_int(ps, 0x40) != 0) return;
        if (raw_int(ps, 0x44) != 0) return;
    }

    // SECTION 5: Main switch dispatch
    int state = ps->weaponstate;

    switch (state) 
    {
        case WEAPON_RAISING:
        case WEAPON_RAISING_ALTSWITCH: 
        {
            if (ps->pm_type >= 8) 
                return;

            ps->weaponstate = WEAPON_READY;
            int v = raw_int(ps, 0x910);
            raw_iref(ps, 0x910) = (~v) & 0x200;
            return;
        }
        case WEAPON_DROPPING:
        case WEAPON_DROPPING_QUICK: 
        {
            int isQuick = (state == WEAPON_DROPPING_QUICK) ? 1 : 0;
            call_PM_Weapon_TickDrop(pm, isQuick);
            return;
        }
        case WEAPON_RELOADING:
        case WEAPON_RELOADING_INTERUPT:
            // Vanilla `push ebx; push edi; call sub_41F780` — ebx = animSync (after overwrite).
            T4_Reconstructed::PM_Weapon_Reloading(pm, animSync);
            return;
        case WEAPON_RELOAD_START:
        case WEAPON_RELOAD_START_INTERUPT:
            T4_Reconstructed::PM_Weapon_ReloadStart(pm, animSync);
            return;
        case WEAPON_RELOAD_END: 
        {
            ps->weaponstate = WEAPON_READY;
            if (ps->pm_type >= 8) 
                return;

            int v = raw_int(ps, 0x910);
            raw_iref(ps, 0x910) = (~v) & 0x200;
            return;
        }
        case WEAPON_MELEE_CHARGE:
            T4_Reconstructed::PM_Weapon_Tick_MeleeCharge(ps);
            return;
        case WEAPON_MELEE_INIT:
            T4_Reconstructed::PM_Weapon_Tick_MeleeInit(ps);
            return;
        case WEAPON_MELEE_FIRE:
            T4_Reconstructed::PM_Weapon_Tick_MeleeFire(ps);
            return;
        case WEAPON_MELEE_END:
        case WEAPON_OFFHAND_END:
        case WEAPON_SPRINT_DROP:
            call_PM_Weapon_FinalizeStateExit(ps);
            return;
        case WEAPON_OFFHAND_INIT:
            T4_Reconstructed::PM_Weapon_Tick_OffhandInit_Stub(ps);
            return;
        case WEAPON_OFFHAND_PREPARE:
            T4_Reconstructed::PM_Weapon_Tick_OffhandPrepare(ps);
            return;
        case WEAPON_OFFHAND_HOLD:
            T4_Reconstructed::PM_Weapon_Tick_OffhandHold(pm);
            return;
        case WEAPON_OFFHAND_START:
            T4_Reconstructed::PM_Weapon_Tick_OffhandStart(pm);
            return;
        case WEAPON_OFFHAND:
            T4_Reconstructed::PM_Weapon_Tick_Offhand_Stub(ps);
            return;
        case WEAPON_DETONATING:
            T4_Reconstructed::PM_Weapon_DetonateTransition(ps, animSync);
            return;
        case WEAPON_SPRINT_RAISE:
            T4_Reconstructed::PM_Weapon_Tick_SprintRaise(ps);
            return;
        case WEAPON_SPRINT_LOOP:
        case WEAPON_DEPLOYED:
            return;
        case WEAPON_DEPLOYING:
            call_Anim_TriggerEvent(ps, 0x23);
            return;
        case WEAPON_BREAKING_DOWN:
            call_PM_Weapon_FinalizeStateExit(ps);
            call_Anim_TriggerEvent(ps, 0x25);
            return;
        case WEAPON_SWIM_IN:
            T4_Reconstructed::PM_Weapon_Tick_SwimIn(ps);
            return;
        case WEAPON_SWIM_OUT:
            T4_Reconstructed::PM_Weapon_Tick_SwimOut(ps);
            return;
        case WEAPON_LOWREADY_START:
            T4M::PM_Weapon_LowReady_Start(ps);
            return;
        case WEAPON_LOWREADY_LOOP:
            return;
        case WEAPON_LOWREADY_END:
            T4M::PM_Weapon_LowReady_End(ps);
            return;
        case WEAPON_FIRING:
        case WEAPON_RECHAMBERING:
        case WEAPON_READY:
        default:
        {
            // def_422367 — fire input dispatch.
            // Vanilla uses ebx (= animSync after the SECTION 4 overwrite) as arg_0 here,
            // NOT callValidation. ebx was reassigned to call_PM_Weapon_AnimCmdState_Resync's
            // return value just before this dispatch.
            if (ps->weapon == 0) return;
            if (T4_Reconstructed::PM_Weapon_DefaultGate(pm, animSync) == 0) return;
            if (call_PM_Weapon_IsLocked(pm) != 0) return;

            if (animSync != 0) {
                if (T4_Reconstructed::PM_Weapon_CanForceFire(pm) != 0) return;
            }

            if ((raw_int(ps, 0x0C) & 0x800) != 0) return;
            if (ps->weaponstate == WEAPON_DEPLOYING) return;

            call_PM_Weapon_ProcessAttackInput(ps, animSync);
            return;
        }
    }
}


// =====================================================================
// __usercall → __cdecl wrapper for sub_422160 detour.
// Vanilla convention: eax = pmove_t*, [esp+4] = arg_0.
// =====================================================================

__declspec(naked) void T4M::PM_Weapon_Wrapper()
{
    __asm {
        push    [esp+4]                 ; arg_0
        push    eax                     ; pmove_t*
        call    T4_Reconstructed::PM_Weapon
        add     esp, 8
        retn
    }
}


// =====================================================================
// Installer — DETOUR DISABLED. Uncomment after build & validate.
// =====================================================================

void PatchT4MAM_WeaponState()
{
     Detours::X86::DetourFunction((uintptr_t)0x00422160,
                                  (uintptr_t)&T4M::PM_Weapon_Wrapper,
                                  Detours::X86Option::USE_JUMP);
}
