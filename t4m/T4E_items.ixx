#include <Stdinc.hpp>
export module T4E_items;

export enum perks_e
{
    PERK_SPECIALTY_GPSJAMMER = 0,
    PERK_SPECIALTY_RECONNAISSANCE,
    PERK_SPECIALTY_BULLETACCURACY,
    PERK_SPECIALTY_FASTRELOAD,
    PERK_SPECIALTY_ROF,
    PERK_SPECIALTY_HOLDBREATH,
    PERK_SPECIALTY_BULLETPENETRATION,
    PERK_SPECIALTY_GRENADEPULLDEATH,
    PERK_SPECIALTY_PISTOLDEATH,
    PERK_SPECIALTY_QUIETER,
    PERK_SPECIALTY_LONGERSPRINT,
    PERK_SPECIALTY_DETECTEXPLOSIVE,
    PERK_SPECIALTY_EXPLOSIVEDAMAGE,
    PERK_SPECIALTY_EXPOSEENEMY,
    PERK_SPECIALTY_BULLETDAMAGE,
    PERK_SPECIALTY_EXTRAAMMO,
    PERK_SPECIALTY_TWOPRIMARIES,
    PERK_SPECIALTY_ARMORVEST,
    PERK_SPECIALTY_FRAGGRENADE,
    PERK_SPECIALTY_SPECIALGRENADE,
    PERK_SPECIALTY_FLAKJACKET,
    PERK_SPECIALTY_PIN_BACK,
    PERK_SPECIALTY_GREASED_BARRINGS,
    PERK_SPECIALTY_WATER_COOLED,
    PERK_SPECIALTY_SHADES,
    PERK_SPECIALTY_GAS_MASK,
    PERK_SPECIALTY_FIREPROOF,
    PERK_SPECIALTY_ORDINANCE,
    PERK_SPECIALTY_BOOST,
    PERK_SPECIALTY_LEADFOOT,
    PERK_SPECIALTY_QUICKREVIVE,
    PERK_SPECIALTY_ALTMELEE,

    PERK_COUNT
};

export namespace game {

	WeaponDef** bg_weaponDefs = reinterpret_cast<WeaponDef**>(0x008F6770);
	AimAssistGlobals* aaGlobArray = reinterpret_cast<AimAssistGlobals*>(0x008E8690);

    float DiffTrackAngle(float target, float current, float rate, float deltaTime)
    {
        float diff = target - current;

        while (diff > 180.0f)
        {
            target -= 360.0f;
            diff = target - current;
        }

        while (diff < -180.0f)
        {
            target += 360.0f;
            diff = target - current;
        }

        float maxMove = diff * rate * deltaTime;
        if (std::fabs(diff) > 0.001f && std::fabs(diff) >= std::fabs(maxMove))
        {
            target = current + maxMove;
        }

        float normalized = target - std::floor((target / 360.0f) + 0.5f) * 360.0f;

        return normalized;
    }

    float AngleSubtract(float a1, float a2)
    {
        float diff = a1 - a2;

        float normalized = diff - std::floor((diff / 360.0f) + 0.5f) * 360.0f;

        return normalized;
    }

export int __cdecl CG_GetPlayerWeapon(playerState_s* a1, int a2) {
	return cdecl_call<int>(0x46BF90, a1, a2);
}

void vectoangles(const float* vec, float* angles)
{
    float x = vec[0];
    float y = vec[1];
    float z = vec[2];

    if (y == 0.0f && x == 0.0f)
    {
        // Pointing straight up or down
        angles[0] = (z > 0.0f) ? 270.0f : 90.0f;  // pitch
        angles[1] = 0.0f;   // yaw
        angles[2] = 0.0f;   // roll
    }
    else
    {
        // Calculate yaw
        float yaw = atan2f(y, x) * 57.295776f;  // rad to deg
        if (yaw < 0.0f)
            yaw += 360.0f;

        // Calculate pitch
        float forward = sqrtf(x * x + y * y);
        float pitch = atan2f(z, forward) * -57.295776f;
        if (pitch < 0.0f)
            pitch += 360.0f;

        angles[0] = pitch;
        angles[1] = yaw;
        angles[2] = 0.0f;
    }
}

game::dvar_s* __cdecl Dvar_RegisterVariant(
    const char* name,
    game::dvarType32_t type,
    game::DvarFlags32 flags,
    game::DvarValue dval,
    game::DvarLimits dom,
    const char* desc) {
    return cdecl_call<game::dvar_s*>(0x5EED90, name, type, flags, dval, dom, desc);
}

}