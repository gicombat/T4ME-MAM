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
    // Mirror sub_464A50's empty-string fallback: empty -> sidleAnim.
    static void RegisterTreeSlot(void* tree, const char* name, const char* fallback, int slot_idx)
    {
        const char* eff = (name && *name) ? name : fallback;
        T4::engine::Anim_RegisterByName(eff, T4::engine::AnimAllocCb);
        T4::engine::Anim_AddTreeSlot(tree, slot_idx);
    }
}

// Chooser hook: replaces the call at 0x00469B15 (the only call site of sub_4643A0).
// Args follow vanilla cdecl: arg_0 = ps-like global ptr, arg_4 = weapon-handle ptr.
extern "C" void __cdecl T4M_ChooserHook_LowReady(playerState_s* ps, void* arg_4)
{
    int vm_state_raw = *(int*)((char*)ps + 0x910);
    int vm_state     = vm_state_raw & ~0x200;

    if (vm_state >= 0x20 && vm_state <= 0x22) 
    {
        int weapon_idx = (*(int*)((char*)ps + 0x10) & 2)
            ? *(int*)((char*)ps + 0xFC)        // offhand
            : *(int*)((char*)ps + 0x104);      // main

        // Tree handle stored by sub_464BF0 at dword_3463C40 + weapon_idx*0x48 + 0x30.
        void* tree = *(void**)((char*)T4M::GetAddress("viewmodelAnimTreeHandles") + weapon_idx * 0x48 + 0x30);
        
        DWORD func = T4M::GetAddress("CG_ViewmodelAnim_SetSlotBlend");

        if (tree) 
        {
            int   slot  = 0x25 + (vm_state - 0x20);
            float blend = 0.0f;
            // sub_464080 __usercall(eax=slot, ecx=weapon_idx, [esp+4]=tree, [esp+8]=blend)
            __asm 
            {
                push    blend
                push    tree
                mov     eax, slot
                mov     ecx, weapon_idx
                mov     edx, func
                call    edx
                add     esp, 8
            }
        }
        return; // suppress vanilla chooser
    }

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
        });

    // Phase 3 - block RELOAD_START while lowReady intent is set (or already in any LOWREADY_* state).
    // sub_41EA30 entry: esi = ps (vanilla __usercall). Redirect to 0x0041EB9C (raw retn after the
    // function's own pop ebp) since push ebp hasn't executed yet at the hook point.
    
    static auto reload_block_hook = safetyhook::create_mid(T4M::GetAddress("CG_ReloadStartCheck_hook"), [](SafetyHookContext& ctx)
    {
            auto* ps = (playerState_s*)ctx.esi;
            if ((ps->eFlags & 0x400) != 0
                || ps->weaponstate == T4::engine::WEAPON_LOWREADY_START
                || ps->weaponstate == T4::engine::WEAPON_LOWREADY_LOOP
                || ps->weaponstate == T4::engine::WEAPON_LOWREADY_END)
            {
                ctx.eip = T4M::GetAddress("CG_ReloadStartCheck_ret");   // retn only (no pop ebp pairing needed)
            }
        });

    // Phase 4 - viewmodel anim tree extension.
    Memory::VP::Patch<uint8_t>(T4M::GetAddress("viewmodelAnimSlotCapacity_imm_site"), 0x28);   // capacity 0x25 -> 0x28

    // Relocate dword_8DD5B0 (slot -> WeaponDef-field-offset table) to add 3
    // entries for our slots 0x25/0x26/0x27.
    static DWORD* newSlotOffsetTable = (DWORD*)VirtualAlloc(NULL, 0x100, MEM_COMMIT | MEM_RESERVE, PAGE_READWRITE);
    
    if (newSlotOffsetTable) 
    {
        memcpy(newSlotOffsetTable, (void*)T4M::GetAddress("viewmodelAnimSlotOffsetTable"), 0x25 * sizeof(DWORD));
        newSlotOffsetTable[0x25] = offsetof(WeaponDef, slowReadyInAnim);
        newSlotOffsetTable[0x26] = offsetof(WeaponDef, slowReadyLoopAnim);
        newSlotOffsetTable[0x27] = offsetof(WeaponDef, slowReadyOutAnim);
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
        });

    // Chooser detour: redirect the unique call site of sub_4643A0 at 0x00469B15.
    
    Detours::X86::DetourFunction(T4M::GetAddress("viewmodelAnimChooser_callsite"), (uintptr_t)&T4M_ChooserHook_LowReady, Detours::X86Option::USE_CALL);
}
