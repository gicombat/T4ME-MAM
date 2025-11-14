#include "t4_headers.h"
#include "StdInc.h"
#include <safetyhook.hpp>

dvar_t* perk_weapRateEnhanced;

inline unsigned int BG_PERK_BITS(int x)
{
	return 1u << x;
}

bool BG_HasPerk(const int* perks, unsigned int perkIndex) {
	return (perks[0] & BG_PERK_BITS(perkIndex)) != 0;
}

void PatchT4E_Weapons() {

	// Double Tap 2.0- like BO2, implementation from https://github.com/Nukem9/LinkerMod

	perk_weapRateEnhanced = Dvar_RegisterInt(0,"perk_weapRateEnhanced", 0, 1, DVAR_FLAG_CHEAT, "Double tap will shoot 2x the bullets for every shot");

	static auto DoubleTap20_Bullet_Fire = safetyhook::create_mid(0x004E6868, [](SafetyHookContext& ctx) {

		gentity_s* attacker = (gentity_s*)ctx.edi;

		int& shotCount = *(int*)(ctx.esp + 0x10);

		if (isZombieMode() && perk_weapRateEnhanced->isEnabled() && attacker->s.eType == ET_PLAYER) {
			if (BG_HasPerk(&attacker->client->ps.perks, 4))
				shotCount *= 2;
		}

		});


	static auto DoubleTap20_Bullet_Imapct = safetyhook::create_mid(0x00469557, [](SafetyHookContext& ctx) {

		playerState_s* ps = *(playerState_s**)(ctx.ebp + 0x18);

		int& shotCount = *(int*)(ctx.esp + 0x1C);

		if (isZombieMode() && perk_weapRateEnhanced->isEnabled()) {
			if (BG_HasPerk(&ps->perks, 4))
				shotCount *= 2;
		}

		});

}