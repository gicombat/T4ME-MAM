#include "StdInc.h"
#include "t4_headers.h"
#include "T4.h"
#include "MemoryMgr.h"
#include <safetyhook.hpp>
#include <cstddef>
#include <cstring>

namespace T4M
{
    // sub_41D360 (BG_RegisterWeapon registrar) is called once per weapon load.
    // At entry, eax = WeaponDef*. Hook applies defaults to fields that the
    // .weapon text parser left at zero (no token authored).
    //
    // Sentinel: iLowReadyInTime == 0 && iLowReadyOutTime == 0 => not authored.
    static void ApplyLowReadyDefaults(WeaponDef* w)
    {
        if (!w) 
            return;

        if (w->iLowReadyInTime == 0 && w->iLowReadyOutTime == 0)
        {
            w->iLowReadyInTime   = 200;
            w->iLowReadyLoopTime = 0;     // 0 = infinite, exit via SetLowReady(0)
            w->iLowReadyOutTime  = 200;
            // offsets/rotations stay at 0 (no pose deviation)
            // anim string pointers stay NULL (engine fallback handled in phase 4)
        }
    }
}

// =============================================================================
// Phase 4 - viewmodel anim tree extension
//
//   Capacity bump:    0x00464A57 byte 25h -> 28h (35 -> 38 slots)
//   Tree extend hook: 0x00464AE0 mid-hook adds slots 0x25/0x26/0x27 from WeaponDef
//   Chooser detour:   call site 0x00469B15 redirects sub_4643A0 -> our wrapper
// =============================================================================

namespace T4M
{
    // @wrapper — Anim_AddTreeSlot (sub_60C8E0) is __usercall: the anim NAME is
    // passed in EDI (ambient), with (tree, slot) on the stack (vanilla pushes
    // slot then tree; caller-cleans). Vanilla sub_464A50 keeps the name live in
    // EDI across the preceding Anim_RegisterByName call, so the slot getter
    // re-resolves the right anim. The cdecl symbol Anim_AddTreeSlot(tree, slot)
    // can't set EDI, so EDI stays stale -> DB_FindXAssetHeader(XANIM, garbage)
    // -> "Could not load xanim <garbage>". This thunk loads EDI = name.
    static void Call_Anim_AddTreeSlot(void* tree, int slot, const char* name)
    {
        static void* fn = (void*)T4M::GetAddress("Anim_AddTreeSlot");  // sub_60C8E0
        __asm
        {
            push    edi             ; preserve callee-saved edi
            push    slot            ; arg_4
            push    tree            ; arg_0
            mov     edi, name       ; ambient name (usercall: edi = name)
            call    fn              ; sub_60C8E0, caller-cleans
            add     esp, 8          ; clean the 2 pushed args
            pop     edi             ; restore edi
        }
    }

    // Mirror sub_464A50's empty-string fallback: empty -> sidleAnim.
    static void RegisterTreeSlot(void* tree, const char* name, const char* fallback, int slot_idx)
    {
        const char* eff = (name && *name) ? name : fallback;
        T4::engine::Anim_RegisterByName(eff, T4::engine::AnimAllocCb);
        Call_Anim_AddTreeSlot(tree, slot_idx, eff);
    }
}

// Chooser hook: replaces the call at 0x00469B15 (the only call site of sub_4643A0).
// Args follow vanilla cdecl: arg_0 = ps-like global ptr, arg_4 = weapon-handle ptr.
extern "C" void __cdecl T4M_ChooserHook_LowReady(playerState_s* ps, void* arg_4)
{
    // T4M vm-anim codes (lowReady 0x20-0x22, reload ext 0x23) play their own tree slot.
    if (T4M::ViewmodelChooser_CustomSlot(ps, arg_4))
        return;

    // Vanilla path: directly call sub_4643A0 (its entry is unpatched, no recursion).
    ((void(__cdecl*)(playerState_s*, void*))T4M::GetAddress("CG_ViewmodelAnim_Chooser"))(ps, arg_4);
}

// =============================================================================
// Single entry-point — installs all lowReady patches (defaults + reload block + anim tree).
// Translation/rotation pose pulls are handled in PatchT4MAM_WeaponState.cpp via the
// T4_Reconstructed::CG_ApplyViewmodelMoveOfs / CG_ApplyViewmodelRotOfs detours.
// =============================================================================

void PatchT4MAM_LowReady()
{
    // Phase 1 - post-load defaults hook on BG_RegisterWeapon entry.
    
    static auto weapon_register_hook = safetyhook::create_mid(T4M::GetAddress("BG_RegisterWeapon_hook"),
        [](SafetyHookContext& ctx) 
    {
            T4M::ApplyLowReadyDefaults((WeaponDef*)ctx.eax);
            T4M::Reload_ApplyWeaponDefDefaults((WeaponDef*)ctx.eax);
        });

    // Phase 3 - the RELOAD_START block (lowReady intent or LOWREADY_* state) now lives at the
    // top of T4M::PM_BeginWeaponReload (PatchT4MAM_Reload.cpp), which detours the same entry.

    // Phase 4 - viewmodel anim tree extension.
    Memory::VP::Patch<uint8_t>(T4M::GetAddress("viewmodelAnimSlotCapacity_imm_site"), T4M::VM_SLOT_RELOAD_START_EMPTY + 1);   // 0x25 -> 0x29 (lowReady 0x25-0x27, reload ext 0x28)
    // Root blend node children 1..0x24 -> 1..0x28: slots past the vanilla root are otherwise
    // orphaned, and playing one leaves the root with no weighted child (bind pose).
    Memory::VP::Patch<uint32_t>(T4M::GetAddress("viewmodelAnimRootChildCount_imm_site"), T4M::VM_SLOT_RELOAD_START_EMPTY);   // mov eax, 24h -> 28h
    // sub_464080 gives the played slot weight 1 and every other slot weight 0 in a loop over
    // slots 1..0x24; a slot past that bound is never weighted in.
    Memory::VP::Patch<uint8_t>(T4M::GetAddress("CG_ViewmodelAnim_SetSlotBlend_slotCount_site"), T4M::VM_SLOT_RELOAD_START_EMPTY + 1);   // cmp esi, 25h -> 29h

    // Relocate dword_8DD5B0 (slot -> WeaponDef-field-offset table) to add 3
    // entries for our slots 0x25/0x26/0x27.
    static DWORD* newSlotOffsetTable = (DWORD*)VirtualAlloc(NULL, 0x100, MEM_COMMIT | MEM_RESERVE, PAGE_READWRITE);
    
    if (newSlotOffsetTable) 
    {
        memcpy(newSlotOffsetTable, (void*)T4M::GetAddress("viewmodelAnimSlotOffsetTable"), 0x25 * sizeof(DWORD));
        // Entries are the WeaponDef TIME field sub_464080 scales playback to (animLength / time);
        // -1 = natural rate. The loop plays at natural rate: iLowReadyLoopTime is the state
        // duration (0 = infinite), which would freeze the anim.
        newSlotOffsetTable[0x25] = offsetof(WeaponDef, iLowReadyInTime);
        newSlotOffsetTable[0x26] = (DWORD)-1;
        newSlotOffsetTable[0x27] = offsetof(WeaponDef, iLowReadyOutTime);
        newSlotOffsetTable[T4M::VM_SLOT_RELOAD_START_EMPTY] = offsetof(WeaponDef, iReloadStartEmptyTime);   // playback-rate time field
        Memory::VP::Patch<DWORD>(T4M::GetAddress("viewmodelAnimSlotOffsetTable_ref_site"), (DWORD)newSlotOffsetTable);
    }
    
    // Tree extend hook: at 0x00464AE0 we add slots 0x25/0x26/0x27 with
    // slowReadyInAnim/LoopAnim/OutAnim or sidleAnim fallback.
    
    static auto tree_extend_hook = safetyhook::create_mid(T4M::GetAddress("viewmodelAnim_treeExtend_site"), [](SafetyHookContext& ctx)
    {
            void* tree = (void*)ctx.ebx;
            const WeaponDef* w = *(const WeaponDef**)(ctx.esp + 0x14);

            if (!tree || !w) 
                return;

            T4M::RegisterTreeSlot(tree, w->slowReadyInAnim,   w->sidleAnim, 0x25);
            T4M::RegisterTreeSlot(tree, w->slowReadyLoopAnim, w->sidleAnim, 0x26);
            T4M::RegisterTreeSlot(tree, w->slowReadyOutAnim,  w->sidleAnim, 0x27);

            const char* startFallback = (w->sreloadStartAnim && *w->sreloadStartAnim) ? w->sreloadStartAnim : w->sidleAnim;
            T4M::RegisterTreeSlot(tree, w->sreloadStartEmptyAnim, startFallback, T4M::VM_SLOT_RELOAD_START_EMPTY);
        });

    // Chooser detour: redirect the unique call site of sub_4643A0 at 0x00469B15.
    
    Detours::X86::DetourFunction(T4M::GetAddress("viewmodelAnimChooser_callsite"), (uintptr_t)&T4M_ChooserHook_LowReady, Detours::X86Option::USE_CALL);
}
