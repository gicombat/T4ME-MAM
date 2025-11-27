import T4E_items;
#include "StdInc.h"
#include <safetyhook.hpp>
#include "MemoryMgr.h"

SafetyHookInline UI_KeyEventT;

dvar_t* gpad_lastinput = nullptr;

#define Game game

enum keyNum_t : __int32
{
	K_NONE = 0x0,
	K_FIRSTGAMEPADBUTTON_RANGE_1 = 0x1,
	K_BUTTON_A = 0x1,
	K_BUTTON_B = 0x2,
	K_BUTTON_X = 0x3,
	K_BUTTON_Y = 0x4,
	K_BUTTON_LSHLDR = 0x5,
	K_BUTTON_RSHLDR = 0x6,
	K_LASTGAMEPADBUTTON_RANGE_1 = 0x6,
	K_TAB = 0x9,
	K_ENTER = 0xD,
	K_FIRSTGAMEPADBUTTON_RANGE_2 = 0xE,
	K_BUTTON_START = 0xE,
	K_BUTTON_BACK = 0xF,
	K_BUTTON_LSTICK = 0x10,
	K_BUTTON_RSTICK = 0x11,
	K_BUTTON_LTRIG = 0x12,
	K_BUTTON_RTRIG = 0x13,
	K_DPAD_UP = 0x14,
	K_FIRSTDPAD = 0x14,
	K_DPAD_DOWN = 0x15,
	K_DPAD_LEFT = 0x16,
	K_DPAD_RIGHT = 0x17,
	K_LASTDPAD = 0x17,
	K_LASTGAMEPADBUTTON_RANGE_2 = 0x17,
	K_ESCAPE = 0x1B,
	K_FIRSTGAMEPADBUTTON_RANGE_3 = 0x1C,
	K_APAD_UP = 0x1C,
	K_FIRSTAPAD = 0x1C,
	K_APAD_DOWN = 0x1D,
	K_APAD_LEFT = 0x1E,
	K_APAD_RIGHT = 0x1F,
	K_LASTAPAD = 0x1F,
	K_LASTGAMEPADBUTTON_RANGE_3 = 0x1F,
	K_SPACE = 0x20,
	K_FIRSTBUDDYBUTTON_RANGE = 0x21,
	K_BUTTON_A_ALT = 0x21,
	K_BUTTON_B_ALT = 0x22,
	K_BUTTON_C_ALT = 0x23,
	K_BUTTON_Z_ALT = 0x24,
	K_BUTTON_X_ALT = 0x25,
	K_BUTTON_Y_ALT = 0x26,
	K_BUTTON_PLUS_ALT = 0x27,
	K_BUTTON_MINUS_ALT = 0x28,
	K_DPAD_LEFT_ALT = 0x29,
	K_DPAD_RIGHT_ALT = 0x2A,
	K_LASTBUDDYBUTTON_RANGE = 0x2A,
	K_NUMPAD0 = 0x60,
	K_BACKSPACE = 0x7F,
	K_ASCII_FIRST = 0x80,
	K_ASCII_181 = 0x80,
	K_ASCII_191 = 0x81,
	K_ASCII_223 = 0x82,
	K_ASCII_224 = 0x83,
	K_ASCII_225 = 0x84,
	K_ASCII_228 = 0x85,
	K_ASCII_229 = 0x86,
	K_ASCII_230 = 0x87,
	K_ASCII_231 = 0x88,
	K_ASCII_232 = 0x89,
	K_ASCII_233 = 0x8A,
	K_ASCII_236 = 0x8B,
	K_ASCII_241 = 0x8C,
	K_ASCII_242 = 0x8D,
	K_ASCII_243 = 0x8E,
	K_ASCII_246 = 0x8F,
	K_ASCII_248 = 0x90,
	K_ASCII_249 = 0x91,
	K_ASCII_250 = 0x92,
	K_ASCII_252 = 0x93,
	K_END_ASCII_CHARS = 0x94,
	K_COMMAND = 0x96,
	K_CAPSLOCK = 0x97,
	K_POWER = 0x98,
	K_PAUSE = 0x99,
	K_UPARROW = 0x9A,
	K_DOWNARROW = 0x9B,
	K_LEFTARROW = 0x9C,
	K_RIGHTARROW = 0x9D,
	K_ALT = 0x9E,
	K_CTRL = 0x9F,
	K_SHIFT = 0xA0,
	K_INS = 0xA1,
	K_DEL = 0xA2,
	K_PGDN = 0xA3,
	K_PGUP = 0xA4,
	K_HOME = 0xA5,
	K_END = 0xA6,
	K_F1 = 0xA7,
	K_F2 = 0xA8,
	K_F3 = 0xA9,
	K_F4 = 0xAA,
	K_F5 = 0xAB,
	K_F6 = 0xAC,
	K_F7 = 0xAD,
	K_F8 = 0xAE,
	K_F9 = 0xAF,
	K_F10 = 0xB0,
	K_F11 = 0xB1,
	K_F12 = 0xB2,
	K_F13 = 0xB3,
	K_F14 = 0xB4,
	K_F15 = 0xB5,
	K_KP_HOME = 0xB6,
	K_KP_UPARROW = 0xB7,
	K_KP_PGUP = 0xB8,
	K_KP_LEFTARROW = 0xB9,
	K_KP_5 = 0xBA,
	K_KP_RIGHTARROW = 0xBB,
	K_BUTTON_LSTICK_ALTIMAGE = 0xBC,
	K_KP_END = 0xBC,
	K_BUTTON_RSTICK_ALTIMAGE = 0xBD,
	K_KP_DOWNARROW = 0xBD,
	K_KP_PGDN = 0xBE,
	K_KP_ENTER = 0xBF,
	K_KP_INS = 0xC0,
	K_KP_DEL = 0xC1,
	K_KP_SLASH = 0xC2,
	K_KP_MINUS = 0xC3,
	K_KP_PLUS = 0xC4,
	K_KP_NUMLOCK = 0xC5,
	K_KP_STAR = 0xC6,
	K_KP_EQUALS = 0xC7,
	K_MOUSE1 = 0xC8,
	K_MOUSE2 = 0xC9,
	K_MOUSE3 = 0xCA,
	K_MOUSE4 = 0xCB,
	K_MOUSE5 = 0xCC,
	K_MWHEELDOWN = 0xCD,
	K_MWHEELUP = 0xCE,
	K_AUX1 = 0xCF,
	K_AUX2 = 0xD0,
	K_AUX3 = 0xD1,
	K_AUX4 = 0xD2,
	K_AUX5 = 0xD3,
	K_AUX6 = 0xD4,
	K_AUX7 = 0xD5,
	K_AUX8 = 0xD6,
	K_AUX9 = 0xD7,
	K_AUX10 = 0xD8,
	K_AUX11 = 0xD9,
	K_AUX12 = 0xDA,
	K_AUX13 = 0xDB,
	K_AUX14 = 0xDC,
	K_AUX15 = 0xDD,
	K_AUX16 = 0xDE,
	K_LAST_KEY = 0xDF,
};

bool last_input = false;

void __cdecl UI_KeyEvent(int localClientNum, keyNum_t key, int down) {
	switch (key) {
	case K_DPAD_LEFT:
		key = K_LEFTARROW;
		break;
	case K_DPAD_UP:
		key = K_UPARROW;
		break;
	case K_DPAD_DOWN:
		key = K_DOWNARROW;
		break;
	case K_DPAD_RIGHT:
		key = K_RIGHTARROW;
		break;

	default:
		break;
	}
	//Com_Printf(0, "key is %d down %d\n", key, down);



	if (key == K_UPARROW || key == K_DOWNARROW || key == K_LEFTARROW || key == K_RIGHTARROW) {
		last_input = true;
	}

	return UI_KeyEventT.unsafe_ccall(localClientNum, key, down);
}


typedef void(__cdecl* CL_GamepadButtonEventT)(
	int localClientNum,
	int controllerIndex,
	keyNum_t key,
	int buttonEvent,
	int time,
	int gamePadButton);
CL_GamepadButtonEventT CL_GamepadButtonEvent = nullptr;


void __cdecl CL_GamepadButtonEvent_was(
	int localClientNum,
	int controllerIndex,
	keyNum_t key,
	int buttonEvent,
	int time,
	int gamePadButton) {

	if (buttonEvent) {
		last_input = true;
		gpad_lastinput->current.integer = 2;
		gpad_lastinput->latched.integer = 2;
	}
	CL_GamepadButtonEvent(localClientNum, controllerIndex, key, buttonEvent, time, gamePadButton);
}

SafetyHookInline Display_MouseMoveT;

int __cdecl Display_MouseMoveD(void* a1) {
	if (!last_input)
		return Display_MouseMoveT.unsafe_ccall<int>(a1);
	return 0;
}


dvar_t* gpad_autoaim_enabled;

dvar_t* aim_autoAimRangeScale;

dvar_t* gpad_lockon_enabled;

dvar_t* aim_aimAssistRangeScale;

dvar_t* aim_lockon_pitch_strength;


bool AimAssist_DoBoundsIntersectCenterBox(const float* clipMins, const float* clipMaxs, const float clipHalfWidth, const float clipHalfHeight)
{
	return clipHalfWidth >= clipMins[0] && clipMaxs[0] >= -clipHalfWidth && clipHalfHeight >= clipMins[1] && clipMaxs[1] >= -clipHalfHeight;
}

void AimAssist_ClearAutoAimTarget(Game::AimAssistGlobals* aaGlob)
{
	aaGlob->autoAimTargetEnt = Game::AIM_TARGET_INVALID;
	aaGlob->autoAimActive = 0;
	aaGlob->autoAimPitch = 0.0;
	aaGlob->autoAimPitchTarget = 0.0;
	aaGlob->autoAimYaw = 0.0;
	aaGlob->autoAimYawTarget = 0.0;
	aaGlob->autoAimJustGotTarget = 0;
	aaGlob->autoAimHasRealTarget = 0;
	aaGlob->autoAimPressed = 0;
}

const Game::AimScreenTarget* AimAssist_GetTargetFromEntity(const Game::AimAssistGlobals* aaGlob, const int entIndex)
{

	if (entIndex == Game::AIM_TARGET_INVALID)
	{
		return nullptr;
	}

	for (auto targetIndex = 0; targetIndex < aaGlob->screenTargetCount; targetIndex++)
	{
		const auto* currentTarget = &aaGlob->screenTargets[targetIndex];
		if (currentTarget->entIndex == entIndex)
		{
			return currentTarget;
		}
	}

	return nullptr;
}

bool AimAssist_UpdateAutoAimTarget(Game::AimAssistGlobals* aaGlob)
{
	float targetDir[3]{};
	float targetAngles[3]{};

	const auto screenTarget = AimAssist_GetTargetFromEntity(aaGlob, aaGlob->autoAimTargetEnt);
	if (!screenTarget)
		return false;

	targetDir[0] = screenTarget->aimPos[0] - aaGlob->viewOrigin[0];
	targetDir[1] = screenTarget->aimPos[1] - aaGlob->viewOrigin[1];
	targetDir[2] = screenTarget->aimPos[2] - aaGlob->viewOrigin[2];
	Game::vectoangles(targetDir, targetAngles);
	aaGlob->autoAimPitchTarget = targetAngles[0];
	aaGlob->autoAimYawTarget = targetAngles[1];
	return true;
}


void AimAssist_SetAutoAimTarget(game::AimAssistGlobals* aaGlob, const game::AimScreenTarget* screenTarget)
{
	AimAssist_ClearAutoAimTarget(aaGlob);
	aaGlob->autoAimTargetEnt = screenTarget->entIndex;
	aaGlob->autoAimActive = 1;
	aaGlob->autoAimPitch = aaGlob->viewAngles[0];
	aaGlob->autoAimYaw = aaGlob->viewAngles[1];
	AimAssist_UpdateAutoAimTarget(aaGlob);
}

const game::AimScreenTarget* AimAssist_GetBestTarget(const game::AimAssistGlobals* aaGlob, const float range, const float regionWidth, const float regionHeight)
{
	const auto rangeSqr = range * range;
	for (auto targetIndex = 0; targetIndex < aaGlob->screenTargetCount; targetIndex++)
	{
		const auto* currentTarget = &aaGlob->screenTargets[targetIndex];
		if (currentTarget->distSqr <= rangeSqr && AimAssist_DoBoundsIntersectCenterBox(currentTarget->clipMins, currentTarget->clipMaxs, regionWidth, regionHeight))
		{
			return currentTarget;
		}
	}

	return nullptr;
}

bool AimAssist_IsAutoAimActive(const int localClientNum, const Game::AimInput* input)
{
	//Com_Printf(0, "^3[DEBUG] IsAutoAimActive check - aim_autoaim_enabled: %d, gpad_autoaim_enabled: %d\n",
	//	Dvars::Functions::Dvar_FindVar("aim_autoaim_enabled")->current.enabled,
	//	gpad_autoaim_enabled->current.enabled);

	if (!Dvars::Functions::Dvar_FindVar("aim_autoaim_enabled")->current.enabled || !gpad_autoaim_enabled->current.enabled)
	{
		//Com_Printf(0, "^1[DEBUG] AutoAim disabled by dvar\n");
		return false;
	}



	auto& aaGlob = Game::aaGlobArray[localClientNum];

	if (!(input->buttons & 1 << 11))
	{
		//Com_Printf(0, "^1[DEBUG] ADS button not pressed\n");
		return false;
	}

	if (aaGlob.adsLerp == 0.0f)
	{
		//Com_Printf(0, "^1[DEBUG] adsLerp is 0.0 (not aiming down sights)\n");
		return false;
	}

	//Com_Printf(0, "^2[DEBUG] AutoAim conditions passed! adsLerp: %.2f\n", aaGlob.adsLerp);
	return true;
}

void AimAssist_ApplyAutoAim(const game::AimInput* input, game::AimOutput* output)
{
	assert(input);
	assert(output);


	auto& aaGlob = game::aaGlobArray[input->localClientNum];
	const auto* weaponDef = game::bg_weaponDefs[game::CG_GetPlayerWeapon(input->ps,input->localClientNum)];

	if (!input->ps->weapon)
	{
		//Com_Printf(0, "^1[DEBUG] EXIT: No weapon\n");
		return;
	}

	if ((input->ps->weaponstate >= game::WEAPON_RELOADING && input->ps->weaponstate <= game::WEAPON_RELOAD_END))
	{
		//Com_Printf(0, "^1[DEBUG] EXIT: Weapon reloading (state: %d)\n", input->ps->weaponstate);
		return;
	}

	if (!weaponDef->aimDownSight)
	{
		//Com_Printf(0, "^1[DEBUG] EXIT: Weapon doesn't support ADS\n");
		return;
	}

	//Com_Printf(0, "^3[DEBUG] Passed initial checks, checking if active...\n");
	if (AimAssist_IsAutoAimActive(input->localClientNum, input))
	{
		//Com_Printf(0, "^2[DEBUG] AutoAim IS ACTIVE!\n");
		if (!aaGlob.autoAimPressed)
		{
			//Com_Printf(0, "^3[DEBUG] First activation - screenTargetCount: %d\n", aaGlob.screenTargetCount);
			const auto screenTarget = AimAssist_GetBestTarget(&aaGlob, weaponDef->autoAimRange * aim_autoAimRangeScale->current.value, aaGlob.tweakables.autoAimRegionWidth, aaGlob.tweakables.autoAimRegionHeight);
			if (screenTarget)
			{
				//Com_Printf(0, "^2[DEBUG] TARGET FOUND! EntIndex: %d\n", screenTarget->entIndex);
				AimAssist_SetAutoAimTarget(&aaGlob, screenTarget);
			}
			else
			{
				//Com_Printf(0, "^1[DEBUG] NO TARGET in range\n");
			}
			aaGlob.autoAimPressed = 1;
		}

		if (aaGlob.autoAimActive)
		{
			//Com_Printf(0, "^2[DEBUG] autoAimActive flag = TRUE\n");
			if (AimAssist_UpdateAutoAimTarget(&aaGlob) && aaGlob.adsLerp > 0.0f)
			{
				//Game::Com_Printf(0, "^2auto aim enabled!\n");
				const auto newPitch = Game::DiffTrackAngle(aaGlob.autoAimPitchTarget, aaGlob.autoAimPitch, Dvars::Functions::Dvar_FindVar("aim_autoaim_lerp")->current.value, input->deltaTime);
				const auto newYaw = Game::DiffTrackAngle(aaGlob.autoAimYawTarget, aaGlob.autoAimYaw, Dvars::Functions::Dvar_FindVar("aim_autoaim_lerp")->current.value, input->deltaTime);
				const auto pitchDelta = Game::AngleSubtract(newPitch, aaGlob.autoAimPitch);
				const auto yawDelta = Game::AngleSubtract(newYaw, aaGlob.autoAimYaw);
				aaGlob.autoAimPitch = newPitch;
				aaGlob.autoAimYaw = newYaw;
				output->pitch = output->pitch + pitchDelta;
				output->yaw = output->yaw + yawDelta;
			}
			else
			{
				//Game::Com_Printf(0, "1 jump!\n");
				AimAssist_ClearAutoAimTarget(&aaGlob);
			}
		}
		else
		{
			//Com_Printf(0, "^1[DEBUG] autoAimActive flag = FALSE\n");
		}
	}
	else
	{
		//Com_Printf(0, "^1[DEBUG] AutoAim NOT active\n");
		AimAssist_ClearAutoAimTarget(&aaGlob);
		aaGlob.autoAimPressed = 0;
	
	}
}

bool AimAssist_IsPlayerUsingOffhand(const Game::AimInput* input)
{
	// Check offhand flag
	if ((input->ps->weapFlags & 2) == 0)
	{
		return false;
	}

	// If offhand weapon has no id we are not using one
	if (!input->ps->weapon)
	{
		return false;
	}

	const auto* weaponDef = Game::bg_weaponDefs[input->ps->weapon];

	return weaponDef->offhandClass != Game::OFFHAND_CLASS_NONE;
}

bool AimAssist_IsLockonActive(const int localClientNum, const Game::AimInput* input)
{


	if (!Dvars::Functions::Dvar_FindVar("aim_lockon_enabled")->current.enabled || !gpad_lockon_enabled->current.enabled)
	{
		return false;
	}

	auto& aaGlob = Game::aaGlobArray[localClientNum];
	if (AimAssist_IsPlayerUsingOffhand(input))
	{
		return false;
	}

	if (aaGlob.autoAimActive /*|| aaGlob.autoMeleeActive*/)
	{
		return false;
	}

	return true;
}

float AimAssist_Lerp(const float from, const float to, const float fraction)
{
	return (to - from) * fraction + from;
}

const Game::AimScreenTarget* AimAssist_GetPrevOrBestTarget(const Game::AimAssistGlobals* aaGlob, const float range, const float regionWidth, const float regionHeight,
	const int prevTargetEnt)
{
	const auto screenTarget = AimAssist_GetTargetFromEntity(aaGlob, prevTargetEnt);

	if (screenTarget && (range * range) > screenTarget->distSqr && AimAssist_DoBoundsIntersectCenterBox(screenTarget->clipMins, screenTarget->clipMaxs, regionWidth, regionHeight))
	{
		return screenTarget;
	}

	return AimAssist_GetBestTarget(aaGlob, range, regionWidth, regionHeight);
}

void AimAssist_ApplyLockOn(const Game::AimInput* input, Game::AimOutput* output)
{
	if (gpad_lastinput->current.integer == 2)
		return;
	assert(input);
	assert(output);

	auto& aaGlob = Game::aaGlobArray[input->localClientNum];

	const auto prevTargetEnt = aaGlob.lockOnTargetEnt;
	aaGlob.lockOnTargetEnt = Game::AIM_TARGET_INVALID;

	if (!AimAssist_IsLockonActive(input->localClientNum, input))
	{
		return;
	}

	const auto* weaponDef = Game::bg_weaponDefs[input->ps->weapon];
	if (weaponDef->requireLockonToFire)
	{
		return;
	}

	const auto deflection = Dvars::Functions::Dvar_FindVar("aim_lockon_deflection")->current.value;
	if (deflection > std::fabs(input->pitchAxis) && deflection > std::fabs(input->yawAxis) && deflection > std::fabs(input->rightAxis))
	{
		return;
	}

	if (!input->ps->weapon)
	{
		return;
	}

	const auto aimAssistRange = AimAssist_Lerp(weaponDef->aimAssistRange, weaponDef->aimAssistRangeAds, aaGlob.adsLerp) * aim_aimAssistRangeScale->current.value;
	const auto screenTarget = AimAssist_GetPrevOrBestTarget(&aaGlob, aimAssistRange, aaGlob.tweakables.lockOnRegionWidth, aaGlob.tweakables.lockOnRegionHeight, prevTargetEnt);

	if (screenTarget && screenTarget->distSqr > 0.0f)
	{
		aaGlob.lockOnTargetEnt = screenTarget->entIndex;
		const auto arcLength = std::sqrt(screenTarget->distSqr) * static_cast<float>(3.14159265358979323846);

		const auto pitchTurnRate =
			(screenTarget->velocity[0] * aaGlob.viewAxis[2][0] + screenTarget->velocity[1] * aaGlob.viewAxis[2][1] + screenTarget->velocity[2] * aaGlob.viewAxis[2][2]
				- (input->ps->velocity[0] * aaGlob.viewAxis[2][0] + input->ps->velocity[1] * aaGlob.viewAxis[2][1] + input->ps->velocity[2] * aaGlob.viewAxis[2][2]))
			/ arcLength * 180.0f * aim_lockon_pitch_strength->current.value;

		const auto yawTurnRate =
			(screenTarget->velocity[0] * aaGlob.viewAxis[1][0] + screenTarget->velocity[1] * aaGlob.viewAxis[1][1] + screenTarget->velocity[2] * aaGlob.viewAxis[1][2]
				- (input->ps->velocity[0] * aaGlob.viewAxis[1][0] + input->ps->velocity[1] * aaGlob.viewAxis[1][1] + input->ps->velocity[2] * aaGlob.viewAxis[1][2]))
			/ arcLength * 180.0f * Dvars::Functions::Dvar_FindVar("aim_lockon_strength")->current.value;

		output->pitch -= pitchTurnRate * input->deltaTime;
		output->yaw += yawTurnRate * input->deltaTime;
	}
}



void __cdecl AimAssist_ApplyAutoMelee_4_ApplyAutoAim(game::AimInput* a1, game::AimOutput* a2) {

	AimAssist_ApplyAutoAim(a1, a2);
	cdecl_call<void>(0x4032C0, a1, a2);

}

void PatchT4E_Input() {
	gpad_lastinput = Dvar_RegisterInt(1, "gpad_lastinput", 1, 2, DVAR_FLAG_ROM, "Returns what was last input by player\n1 = Mouse\n2 = Gamepad");

	gpad_autoaim_enabled = Dvar_RegisterBool(true, "gpad_autoaim_enabled", DVAR_FLAG_SAVED | DVAR_FLAG_ARCHIVE);

	gpad_lockon_enabled = Dvar_RegisterBool(true, "gpad_lockon_enabled", DVAR_FLAG_SAVED | DVAR_FLAG_ARCHIVE);

	gpad_lockon_enabled = Dvar_RegisterBool(true, "gpad_lockon_enabled", DVAR_FLAG_SAVED | DVAR_FLAG_ARCHIVE);

	aim_autoAimRangeScale = Dvar_RegisterFloat("aim_autoAimRangeScale", 1.f, 0.f, 2.f,0);

	aim_lockon_pitch_strength = Dvar_RegisterFloat("aim_lockon_pitch_strength", 0.6f, 0.f, 1.f, 0);


	UI_KeyEventT = safetyhook::create_inline(0x5D6A90, UI_KeyEvent);
	Display_MouseMoveT = safetyhook::create_inline(0x5CAA30, Display_MouseMoveD);

	Memory::VP::InterceptCall(0x5FA48E, CL_GamepadButtonEvent, CL_GamepadButtonEvent_was);

	Memory::VP::Nop(0x40372E, 5);

	Memory::VP::InjectHook(0x403735, AimAssist_ApplyAutoMelee_4_ApplyAutoAim);

	static auto CL_MouseEvent = safetyhook::create_mid(0x005FA74B, [](SafetyHookContext& ctx) {

		if (ctx.edi != 0 || ctx.esi != 0) {
			gpad_lastinput->current.integer = 1;
			gpad_lastinput->latched.integer = 1;
			last_input = false;
		}

		});


}