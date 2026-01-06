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
        float* AmmoRocketsPos = (float*)0x8E4570;
        drawclipammo_scale[0] = 4.f * scale;
        ClipAmmoShortMagazine_scale[0] = 40.f * scale;
        ClipAmmoShotgunShells_scale[0] = 20.f * scale;
        DrawClipAmmoRockets_scale[0] = 64.f * scale;
        AmmoRocketsPos[0] = 72.f * scale;



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


dvar_t* cg_scoreboardTextOffset_player_0;

dvar_t* cg_scoreboardTextOffset_player_1;

dvar_t* cg_scoreboardTextOffset_player_2;

dvar_t* cg_scoreboardTextOffset_player_3;

inline float cg_scoreboardTextOffset_og_multiplier() {
    dvar_t* scoreboard = *(dvar_t**)(0x0368FC14);

    return scoreboard->current.value / scoreboard->defaulta.value;
}

float get_cg_scoreboardTextOffset_per_player(int client) {
    dvar_t* which = cg_scoreboardTextOffset_player_0;
    switch (client) {
    case 0:
        which = cg_scoreboardTextOffset_player_0;
        break;
    case 1:
        which = cg_scoreboardTextOffset_player_1;
        break;
    case 2:
        which = cg_scoreboardTextOffset_player_2;
        break;
    case 3:
        which = cg_scoreboardTextOffset_player_3;
        break;
    }
    return which->current.value * cg_scoreboardTextOffset_og_multiplier();

}

int __cdecl UI_TextWidth(const char* text, int maxChars, game::Font_s* font, float scale);

dvar_t* cg_drawAmmoClipOffset{};

dvar_t* cg_drawAmmoDivider{};

dvar_t* cg_drawAmmoReserveOffset;

dvar_t* cg_drawAmmoMod;

const char* cg_drawAmmoModStrings[] = { "off","offset only","better anchor & offsets",NULL };

void CG_DrawPlayerAmmoValueClip(SafetyHookContext& ctx, bool lowclip) {
    float* x = (float*)ctx.esp;
    float* scale = (float*)(ctx.esp + 0x8);
    game::Font_s* current_font = (game::Font_s*)ctx.ebp;
    game::rectDef_s* rect = (game::rectDef_s*)ctx.esi;
    const char* current_text = lowclip ? (const char*)ctx.edx : (const char*)ctx.eax;

    //// Skip leading spaces
    //while (*current_text == ' ') {
    //    current_text++;
    //}
    if (cg_drawAmmoMod->current.integer == 2) {
        // Calculate separator position
        float separator_pos = ((rect->w - (float)UI_TextWidth((const char*)&cg_drawAmmoDivider->current.integer, 0, current_font, *scale)) * 0.5f) + rect->x - 5.0f;

        // Right-align clip to separator with a gap
        float clip_width = (float)UI_TextWidth(current_text, 0, current_font, *scale);
        *x = separator_pos - clip_width - cg_drawAmmoClipOffset->current.value; // Adjust the 3.0f for desired gap
    }
    else if (cg_drawAmmoMod->current.integer == 1) {
        *x += cg_drawAmmoClipOffset->current.value;
    }
}

void CG_DrawPlayerAmmoValueClip_lowclip(SafetyHookContext& ctx) {

    CG_DrawPlayerAmmoValueClip(ctx, true);

}

void CG_DrawPlayerAmmoValueClip_normal(SafetyHookContext& ctx) {

    CG_DrawPlayerAmmoValueClip(ctx, false);

}

void PatchT4E_UI() {

    cg_scoreboardTextOffset_player_0 = Dvar_RegisterFloat("cg_scoreboardTextOffset_player_0", 0.64f, 0, FLT_MAX, 0);
    cg_scoreboardTextOffset_player_1 = Dvar_RegisterFloat("cg_scoreboardTextOffset_player_1", 0.64f, 0, FLT_MAX, 0);
    cg_scoreboardTextOffset_player_2 = Dvar_RegisterFloat("cg_scoreboardTextOffset_player_2", 0.64f, 0, FLT_MAX, 0);
    cg_scoreboardTextOffset_player_3 = Dvar_RegisterFloat("cg_scoreboardTextOffset_player_3", 0.64f, 0, FLT_MAX, 0);

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

    static auto cg_scoreboard_w = Dvar_RegisterFloat("cg_scoreboard_w", 100.f, 1.f, FLT_MAX,DVAR_FLAG_ARCHIVE);

    static auto cg_scoreboard_h = Dvar_RegisterFloat("cg_scoreboard_h", 21.f, 1.f, FLT_MAX, DVAR_FLAG_ARCHIVE);

    static auto cg_scoreboard_textscale = Dvar_RegisterFloat("cg_scoreboard_textscale", 1.f, 0.1f, FLT_MAX, DVAR_FLAG_ARCHIVE);

    static auto cg_scoreboard_zombie_console_hud = Dvar_RegisterBool(false, "cg_scoreboard_zombie_console_hud", DVAR_FLAG_ARCHIVE);

    static auto playerSpectatingHide = Dvar_RegisterBool(false, "playerSpectatingHide", 0);

    static auto ForJB = safetyhook::create_mid(0x00668A87, [](SafetyHookContext& ctx) {

        if (!isZombieMode())
            return;

        float w_scale = cg_scoreboard_w->current.value / cg_scoreboard_w->defaulta.value;

        float h_scale = cg_scoreboard_h->current.value / cg_scoreboard_h->defaulta.value;

        if (cg_scoreboard_zombie_console_hud->isEnabled()) {
            w_scale = 1.f;
            h_scale = 1.28571428571;
        }

        float& w = *(float*)(ctx.esp + 0x70);

        float& h = ctx.xmm0.f32[0];

        w *= w_scale;

        h *= h_scale;

        });


    static auto forJB_textScale = safetyhook::create_mid(0x668800, [](SafetyHookContext& ctx) {
        if (!isZombieMode())
            return;
        float& text_scale = *(float*)(ctx.esp + 0x18);

        int client = *(int*)(ctx.esp + 0x14);

        //Com_Printf(0, "rendering client %d\n", client);

        float muliplier = cg_scoreboard_zombie_console_hud->isEnabled() ? 1.5f : cg_scoreboard_textscale->current.value;


        text_scale *= muliplier;

        });
    static int current_zombie_client_text = 0;
    static auto saveClientNumber = safetyhook::create_mid(0x668CC0, [](SafetyHookContext& ctx) {
        current_zombie_client_text = ctx.ecx;
        });

    Memory::VP::Nop(0x6689F8, 5);

    static auto per_player_text_offset = safetyhook::create_mid(0x6689F8, [](SafetyHookContext& ctx) {
        ctx.xmm0.f32[0] *= get_cg_scoreboardTextOffset_per_player(current_zombie_client_text);
        });

    static auto hide_spectate_text = safetyhook::create_mid(0x438706, [](SafetyHookContext& ctx) {
        if (playerSpectatingHide->current.boolean)
            ctx.eip = 0x43873C;
        });

    cg_drawAmmoDivider = Dvar_RegisterInt('|', "cg_drawAmmoDivider", 0, CHAR_MAX, 0);

    Memory::VP::Patch<void*>(0x0044F3CC + 1, &cg_drawAmmoDivider->current.integer);
    Memory::VP::Patch<void*>(0x44F418 + 1, &cg_drawAmmoDivider->current.integer);

    cg_drawAmmoClipOffset = Dvar_RegisterFloat("cg_drawAmmoClipOffset", 0.f, -FLT_MAX, FLT_MAX, DVAR_FLAG_ARCHIVE);

    cg_drawAmmoReserveOffset = Dvar_RegisterFloat("cg_drawAmmoReserveOffset", 0.f, -FLT_MAX, FLT_MAX, DVAR_FLAG_ARCHIVE);

    cg_drawAmmoMod = Dvar_RegisterEnum(cg_drawAmmoModStrings, 2, "cg_drawAmmoMod", DVAR_FLAG_ARCHIVE," ");

    static auto cg_draw_ammo_clip = safetyhook::create_mid(0x0044F2FA, CG_DrawPlayerAmmoValueClip_normal);


    static auto cg_draw_ammo_clip1 = safetyhook::create_mid(0x44F33E, CG_DrawPlayerAmmoValueClip_lowclip);


    static auto cg_draw_ammo_reserve = safetyhook::create_mid(0x0044F3AA, [](SafetyHookContext& ctx) {
        float* x = (float*)ctx.esp;
        float* scale = (float*)(ctx.esp + 0x8);
        game::Font_s* current_font = (game::Font_s*)ctx.ebp;
        game::rectDef_s* rect = (game::rectDef_s*)ctx.esi;
        const char* current_text = (const char*)(ctx.esp + 0x154);

        if (cg_drawAmmoMod->current.integer == 2) {
            float separator_pos = ((rect->w - (float)UI_TextWidth((const char*)&cg_drawAmmoDivider->current.integer, 0, current_font, *scale)) * 0.5f) + rect->x - 5.0f;
            float separator_width = (float)UI_TextWidth((const char*)&cg_drawAmmoDivider->current.integer, 0, current_font, *scale);

            int space_count = 0;
            while (current_text[space_count] == ' ') {
                space_count++;
            }

            float space_offset = space_count * UI_TextWidth(" ", 0, current_font, *scale);

            *x = separator_pos + separator_width + cg_drawAmmoReserveOffset->current.value - space_offset;
        }
        else if (cg_drawAmmoMod->current.integer == 1) {
            *x += cg_drawAmmoReserveOffset->current.value;
        }
        });

}