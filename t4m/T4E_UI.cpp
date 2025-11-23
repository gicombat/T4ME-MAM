#include "StdInc.h"
#include "T4.h"
#include "MemoryMgr.h"

#include "safetyhook.hpp"

//dvar_t* r_addcmd_x;
//dvar_t* r_addcmd_y;
//dvar_t* r_addcmd_width;
//dvar_t* r_addcmd_height;
//dvar_t* r_addcmd_s0;
//dvar_t* r_addcmd_t0;
//dvar_t* r_addcmd_s1;
//dvar_t* r_addcmd_t1;
dvar_t* r_hud_scale_fix;

inline bool is_hud_scale_fix() {
    return r_hud_scale_fix->current.boolean;
}

float hud_x_fix(float original) {



    auto Height = (float)*(int*)0x03BED834;

    if (Height <= 720.f || !is_hud_scale_fix())
        return 0.f;

    float scale = Height / 720.0f;
    float result = original * scale;
    return result - original;

}


float drawclipammo_scale[2] = { 4.f,4.f };

float ClipAmmoShortMagazine_scale[2] = { 40.f,4.f };

float ClipAmmoShotgunShells_scale[2] = { 20.f,4.f };

float DrawClipAmmoRockets_scale[2] = { 64.f,16.f };

void __cdecl R_AddCmdDrawStretchPic_HUD_fix(
    float x,
    float y,
    float width,
    float height,
    float s0,
    float t0,
    float s1,
    float t1,
    float* color,
    void* material) {

    auto Width = (float)*(int*)0x03BED830;
    auto Height = (float)*(int*)0x03BED834;
    float scale = 1.f;
    if (is_hud_scale_fix()) {
        scale = Height / 720.0f;

        width *= scale;
        height *= scale;

    }


        float* TEST_bullet_wh_3 = (float*)0x008E4580;
        drawclipammo_scale[0] = 4.f * scale;
        ClipAmmoShortMagazine_scale[0] = 40.f * scale;
        ClipAmmoShotgunShells_scale[0] = 20.f * scale;
        DrawClipAmmoRockets_scale[0] = 64.f * scale;


        TEST_bullet_wh_3[0] = 8.f * scale;
        TEST_bullet_wh_3[1] = (-2.f) * scale;


        

    


    //x += r_addcmd_x->current.value;
    //y += r_addcmd_y->current.value;
    //width *= r_addcmd_width->current.value;
    //height *= r_addcmd_width->current.value;
    //s0 *= r_addcmd_s0->current.value;
    //t0 *= r_addcmd_t0->current.value;
    //s1 *= r_addcmd_s1->current.value;
    //t1 *= r_addcmd_t1->current.value;

    ((void(__cdecl*)(float x,
        float y,
        float width,
        float height,
        float s0,
        float t0,
        float s1,
        float t1,
        float* color,
        void* material))0x006F58E0)(x, y, width, height, s0, t0, s1, t1, color, material);
}



void __cdecl R_AddCmdDrawStretchPic_HUD_fix_DrawClipAmmoShortMagazine(
    float x,
    float y,
    float width,
    float height,
    float s0,
    float t0,
    float s1,
    float t1,
    float* color,
    void* material) {


    x = x - hud_x_fix(32.f);

    R_AddCmdDrawStretchPic_HUD_fix(x, y, width, height, s0, t0, s1, t1, color, material);
}

void __cdecl R_AddCmdDrawStretchPic_HUD_fix_DrawClipAmmoMagazine(
    float x,
    float y,
    float width,
    float height,
    float s0,
    float t0,
    float s1,
    float t1,
    float* color,
    void* material) {


    x = x - hud_x_fix(4.f);

    R_AddCmdDrawStretchPic_HUD_fix(x, y, width, height, s0, t0, s1, t1, color, material);
}

void __cdecl R_AddCmdDrawStretchPic_HUD_fix_DrawClipAmmoShotgunShells(
    float x,
    float y,
    float width,
    float height,
    float s0,
    float t0,
    float s1,
    float t1,
    float* color,
    void* material) {


    x = x - hud_x_fix(16.f);

    R_AddCmdDrawStretchPic_HUD_fix(x, y, width, height, s0, t0, s1, t1, color, material);
}


void __cdecl R_AddCmdDrawStretchPic_HUD_fix_ClipAmmoBeltfed(
    float x,
    float y,
    float width,
    float height,
    float s0,
    float t0,
    float s1,
    float t1,
    float* color,
    void* material) {


    x = x - hud_x_fix(1.f);

    y = y - hud_x_fix(-2.f);

    R_AddCmdDrawStretchPic_HUD_fix(x, y, width, height, s0, t0, s1, t1, color, material);
}

void __cdecl R_AddCmdDrawStretchPic_HUD_fix_ClipAmmoRockets(
    float x,
    float y,
    float width,
    float height,
    float s0,
    float t0,
    float s1,
    float t1,
    float* color,
    void* material) {


    x = x - hud_x_fix(64.f);

    R_AddCmdDrawStretchPic_HUD_fix(x, y, width, height, s0, t0, s1, t1, color, material);
}

void PatchT4E_UI() {

    Memory::VP::InjectHook(0x0042B814, R_AddCmdDrawStretchPic_HUD_fix_DrawClipAmmoMagazine);
    Memory::VP::InjectHook(0x0042B954, R_AddCmdDrawStretchPic_HUD_fix_DrawClipAmmoShortMagazine);
    Memory::VP::InjectHook(0x0042BACA, R_AddCmdDrawStretchPic_HUD_fix_DrawClipAmmoShotgunShells);
    Memory::VP::InjectHook(0x0042BC10, R_AddCmdDrawStretchPic_HUD_fix_ClipAmmoRockets);
    Memory::VP::InjectHook(0x0042BE13, R_AddCmdDrawStretchPic_HUD_fix_ClipAmmoBeltfed);

    r_hud_scale_fix = Dvar_RegisterBool(true, "r_hud_scale_fix", DVAR_FLAG_ARCHIVE);
    //r_addcmd_x = Dvar_RegisterFloat("r_addcmd_x", 0.f, -10000.f, FLT_MAX, 0);
    //r_addcmd_y = Dvar_RegisterFloat("r_addcmd_y", 0.f, -10000.f, FLT_MAX, 0);
    //r_addcmd_width = Dvar_RegisterFloat("r_addcmd_width", 1.f, 0.f, FLT_MAX, 0);
    //r_addcmd_height = Dvar_RegisterFloat("r_addcmd_height", 1.f, 0.f, FLT_MAX, 0);
    //r_addcmd_s0 = Dvar_RegisterFloat("r_addcmd_s0", 1.f, 0.f, FLT_MAX, 0);
    //r_addcmd_t0 = Dvar_RegisterFloat("r_addcmd_t0", 1.f, 0.f, FLT_MAX, 0);
    //r_addcmd_s1 = Dvar_RegisterFloat("r_addcmd_s1", 1.f, 0.f, FLT_MAX, 0);
    //r_addcmd_t1 = Dvar_RegisterFloat("r_addcmd_t1", 1.f, 0.f, FLT_MAX, 0);

    //static auto r_drawclipammo_x = Dvar_RegisterFloat("r_drawclipammo_x", 4.f, FLT_MIN, FLT_MAX, 0); 
    //static auto r_ClipAmmoShortMagazine_x = Dvar_RegisterFloat("r_ClipAmmoShortMagazine_x", 40.f, FLT_MIN, FLT_MAX, 0);
    //static auto r_ClipAmmoShotgunShells_x = Dvar_RegisterFloat("r_ClipAmmoShotgunShells_x", 20.f, FLT_MIN, FLT_MAX, 0);
    //static auto r_drawclipammo_y = Dvar_RegisterFloat("r_drawclipammo_y", 4.f, FLT_MIN, FLT_MAX, 0);
    Memory::VP::Patch<void*>((0x0042B81F + 4), drawclipammo_scale);

    Memory::VP::Patch<void*>((0x0042B95F + 4), ClipAmmoShortMagazine_scale);

    Memory::VP::Patch<void*>((0x0042BAD5 + 4), ClipAmmoShotgunShells_scale);

    //Memory::VP::Patch<void*>((0x0042BB3A + 4), DrawClipAmmoRockets_scale);

    //static auto r_DrawClipAmmoMag_HUDFix1 = safetyhook::create_mid(0x0042B735, [](SafetyHookContext& ctx) {

    //    if (!is_hud_scale_fix())
    //        return;


    //    float* bulletX = (float*)(ctx.esp + 0x24);
    //    *bulletX = *bulletX - hud_x_fix(4.f);


    //    });


    //static auto r_DrawClipAmmoShortMagazine_HUDFix1 = safetyhook::create_mid(0x042B871, [](SafetyHookContext& ctx) {

    //    if (!is_hud_scale_fix())
    //        return;


    //    float* bulletX = (float*)(ctx.esp + 0x24);
    //    *bulletX = *bulletX - hud_x_fix(32.f);


    //    });

    //Memory::VP::Patch<void*>((0x0042B717 + 4), &r_drawclipammo_x->current.value);
    //static auto base_hack = safetyhook::create_mid(0x0042BE50, [](SafetyHookContext& ctx) {
    //    float* base = (float*)ctx.edi;

    //    base[0] += r_drawclipammo_x->current.value;
    //    base[1] += r_drawclipammo_y->current.value;
    //    });

}