import T4E_items;
#include "StdInc.h"
#include "t4_headers.h"
#include <safetyhook.hpp>

dvar_t* perk_weapRateEnhanced;

inline unsigned int BG_PERK_BITS(int x)
{
	return 1u << x;
}

bool BG_HasPerk(const int* perks, unsigned int perkIndex) 
{
	return (perks[0] & BG_PERK_BITS(perkIndex)) != 0;
}

void PatchT4E_Weapons() 
{

	// Double Tap 2.0- like BO2, implementation from https://github.com/Nukem9/LinkerMod

	perk_weapRateEnhanced = T4::dvar::Dvar_RegisterInt(0,"perk_weapRateEnhanced", 0, 1, DVAR_FLAG_CHEAT, "Double tap will shoot 2x the bullets for every shot");

	static auto DoubleTap20_Bullet_Fire = safetyhook::create_mid(T4M::GetAddress("DoubleTap20_Bullet_Fire_hook"), [](SafetyHookContext& ctx) 
	{

		gentity_s* attacker = (gentity_s*)ctx.edi;

		int& shotCount = *(int*)(ctx.esp + 0x10);

		if (T4M::isZombieMode() && perk_weapRateEnhanced->isEnabled() && attacker->s.eType == T4::engine::ET_PLAYER)
		{
			if (BG_HasPerk(&attacker->client->ps.perks, PERK_SPECIALTY_ROF))
				shotCount *= 2;
		}

		});

	static auto* perks_phdflopper_engine = T4::dvar::Dvar_RegisterBool(false, "perks_phdflopper_engine", 0);

	static auto* perks_phdflopper_engine_enum = T4::dvar::Dvar_RegisterInt(PERK_SPECIALTY_DETECTEXPLOSIVE,"perks_phdflopper_engine_enum",0, PERK_COUNT - 1,0,"Which perk enum to apply PHD engine edits implementation to");

	static auto crashland_test = safetyhook::create_mid(T4M::GetAddress("crashland_test_hook"), [](SafetyHookContext& ctx) 
	{

		playerState_s* client = (playerState_s*)ctx.esi;
		
		if (perks_phdflopper_engine->isEnabled() && BG_HasPerk(&client->perks, perks_phdflopper_engine_enum->current.integer)) {
			ctx.ebx = 0;
		}

		});

	static auto G_Damage_PHD_test = safetyhook::create_mid(T4M::GetAddress("G_Damage_PHD_test_hook"), [](SafetyHookContext& ctx) 
	{

		gclient_s* client = (gclient_s*)ctx.esi;

		T4::engine::meansOfDeath_t mod = *(T4::engine::meansOfDeath_t*)(ctx.esp + 0x3C);

		if (perks_phdflopper_engine->isEnabled() && (mod >= T4::engine::MOD_GRENADE && (mod <= T4::engine::MOD_PROJECTILE_SPLASH || mod == T4::engine::MOD_EXPLOSIVE) ) && BG_HasPerk(&client->ps.perks, perks_phdflopper_engine_enum->current.integer)) {
			ctx.ebx = 0;
		}

		});

	static auto DoubleTap20_Bullet_Imapct = safetyhook::create_mid(T4M::GetAddress("DoubleTap20_Bullet_Impact_hook"), [](SafetyHookContext& ctx) 
	{
		playerState_s* ps = *(playerState_s**)(ctx.ebp + 0x18);

		int& shotCount = *(int*)(ctx.esp + 0x1C);

		if (T4M::isZombieMode() && perk_weapRateEnhanced->isEnabled()) {
			if (BG_HasPerk(&ps->perks, 4))
				shotCount *= 2;
		}

		});

}