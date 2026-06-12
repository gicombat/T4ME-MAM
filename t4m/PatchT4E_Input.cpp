import T4E_items;
#include "StdInc.h"
#include <safetyhook.hpp>
#include "MemoryMgr.h"

SafetyHookInline UI_KeyEventT;

dvar_t* gpad_lastinput = nullptr;

#define Game T4::engine
#define Dvar T4::dvar

bool last_input = false;

void __cdecl UI_KeyEvent(int localClientNum, Game::keyNum_t key, int down)
{
	switch (key)
	{
		case Game::K_DPAD_LEFT:
			key = Game::K_LEFTARROW;
			break;
		case Game::K_DPAD_UP:
			key = Game::K_UPARROW;
			break;
		case Game::K_DPAD_DOWN:
			key = Game::K_DOWNARROW;
			break;
		case Game::K_DPAD_RIGHT:
			key = Game::K_RIGHTARROW;
			break;

		default:
			break;
	}
	//Com_Printf(0, "key is %d down %d\n", key, down);



	if (key == Game::K_UPARROW || key == Game::K_DOWNARROW || key == Game::K_LEFTARROW || key == Game::K_RIGHTARROW)
	{
		last_input = true;
	}

	return UI_KeyEventT.unsafe_ccall(localClientNum, key, down);
}


typedef void(__cdecl* CL_GamepadButtonEventT)(
	int localClientNum,
	int controllerIndex,
	Game::keyNum_t key,
	int buttonEvent,
	int time,
	int gamePadButton);
CL_GamepadButtonEventT CL_GamepadButtonEvent = nullptr;


void __cdecl CL_GamepadButtonEvent_was(
	int localClientNum,
	int controllerIndex,
	Game::keyNum_t key,
	int buttonEvent,
	int time,
	int gamePadButton) 
{

	if (buttonEvent) 
	{
		last_input = true;
		gpad_lastinput->current.integer = 2;
		gpad_lastinput->latched.integer = 2;
	}
	CL_GamepadButtonEvent(localClientNum, controllerIndex, key, buttonEvent, time, gamePadButton);
}

SafetyHookInline Display_MouseMoveT;

int __cdecl Display_MouseMoveD(void* a1) 
{
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

void AimAssist_ClearAutoAimTarget(const Game::AimInput* input) {

	auto& aaGlob = Game::aaGlobArray[input->localClientNum];
	AimAssist_ClearAutoAimTarget(&aaGlob);
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


void AimAssist_SetAutoAimTarget(Game::AimAssistGlobals* aaGlob, const Game::AimScreenTarget* screenTarget)
{
	AimAssist_ClearAutoAimTarget(aaGlob);
	aaGlob->autoAimTargetEnt = screenTarget->entIndex;
	aaGlob->autoAimActive = 1;
	aaGlob->autoAimPitch = aaGlob->viewAngles[0];
	aaGlob->autoAimYaw = aaGlob->viewAngles[1];
	AimAssist_UpdateAutoAimTarget(aaGlob);
}

const Game::AimScreenTarget* AimAssist_GetBestTarget(const Game::AimAssistGlobals* aaGlob, const float range, const float regionWidth, const float regionHeight)
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

void AimAssist_ApplyAutoAim(const Game::AimInput* input, Game::AimOutput* output)
{
	assert(input);
	assert(output);

	auto& aaGlob = Game::aaGlobArray[input->localClientNum];
	const auto* weaponDef = Game::bg_weaponDefs[Game::CG_GetPlayerWeapon(input->ps, input->localClientNum)];

	if (!input->ps->weapon)
	{
		return;
	}

	if ((input->ps->weaponstate >= Game::WEAPON_RELOADING && input->ps->weaponstate <= Game::WEAPON_RELOAD_END))
	{
		return;
	}

	if (!weaponDef->aimDownSight)
	{
		return;
	}

	if (AimAssist_IsAutoAimActive(input->localClientNum, input))
	{
		if (!aaGlob.autoAimPressed)
		{
			if (input->ps->weaponstate != Game::WEAPON_RELOADING
				&& input->ps->weaponstate != Game::WEAPON_RELOADING_INTERUPT
				&& input->ps->weaponstate != Game::WEAPON_RELOAD_START
				&& input->ps->weaponstate != Game::WEAPON_RELOAD_START_INTERUPT
				&& input->ps->weaponstate != Game::WEAPON_RELOAD_END)
			{
				if (weaponDef->aimDownSight)
				{
					const auto screenTarget = AimAssist_GetBestTarget(&aaGlob, weaponDef->autoAimRange * aim_autoAimRangeScale->current.value, aaGlob.tweakables.autoAimRegionWidth, aaGlob.tweakables.autoAimRegionHeight);
					if (screenTarget)
					{
						AimAssist_ClearAutoAimTarget(&aaGlob);
						aaGlob.autoAimTargetEnt = screenTarget->entIndex;
						aaGlob.autoAimActive = 1;
						aaGlob.autoAimPitch = aaGlob.viewAngles[0];
						aaGlob.autoAimYaw = aaGlob.viewAngles[1];
						aaGlob.autoAimHasRealTarget = 1;
						AimAssist_UpdateAutoAimTarget(&aaGlob);
						aaGlob.autoAimJustGotTarget = 1;
					}
				}
			}
			aaGlob.autoAimPressed = 1;
		}

		if (aaGlob.autoAimActive)
		{
			if ((input->ps->eFlags & 0x300) == 0
				&& aaGlob.adsLerp < 1.0f
				&& !AimAssist_UpdateAutoAimTarget(&aaGlob)
				&& !aaGlob.autoAimJustGotTarget)
			{
				AimAssist_ClearAutoAimTarget(&aaGlob);
				return;
			}

			// Apply tracking
			const auto newPitch = Game::DiffTrackAngle(aaGlob.autoAimPitchTarget, aaGlob.autoAimPitch, Dvars::Functions::Dvar_FindVar("aim_autoaim_lerp")->current.value, input->deltaTime);
			const auto newYaw = Game::DiffTrackAngle(aaGlob.autoAimYawTarget, aaGlob.autoAimYaw, Dvars::Functions::Dvar_FindVar("aim_autoaim_lerp")->current.value, input->deltaTime);
			const auto pitchDelta = Game::AngleSubtract(newPitch, aaGlob.autoAimPitch);
			const auto yawDelta = Game::AngleSubtract(newYaw, aaGlob.autoAimYaw);

			aaGlob.autoAimPitch = newPitch;
			aaGlob.autoAimYaw = newYaw;
			output->pitch += pitchDelta;
			output->yaw += yawDelta;

			aaGlob.autoAimJustGotTarget = 0;

			if (fabs(pitchDelta) < 0.001f && fabs(yawDelta) <= 0.001f)
			{
				if (input->ps->fWeaponPosFrac == 0.0f)
				{
					AimAssist_ClearAutoAimTarget(&aaGlob);
				}
			}
		}
	}
	else
	{
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
	if (gpad_lastinput->current.integer == 1)
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

void __cdecl AimAssist_ApplyAutoMelee_4_ApplyAutoAim(Game::AimInput* a1, Game::AimOutput* a2) 
{
	if (gpad_lastinput->current.integer != 1) 
	{
		AimAssist_ApplyAutoAim(a1, a2);
	}
	else 
	{
		AimAssist_ClearAutoAimTarget(a1);
	}
	cdecl_call<void>(T4M::GetAddress("AimAssist_ApplyAutoMelee"), a1, a2);
}

void PatchT4E_Input() 
{
	gpad_lastinput = Dvar::Dvar_RegisterInt(1, "gpad_lastinput", 1, 2, DVAR_FLAG_ROM, "Returns what was last input by player\n1 = Mouse\n2 = Gamepad");

	gpad_autoaim_enabled = Dvar::Dvar_RegisterBool(true, "gpad_autoaim_enabled", DVAR_FLAG_SAVED | DVAR_FLAG_ARCHIVE);

	gpad_lockon_enabled = Dvar::Dvar_RegisterBool(true, "gpad_lockon_enabled", DVAR_FLAG_SAVED | DVAR_FLAG_ARCHIVE);


	aim_autoAimRangeScale = Dvar::Dvar_RegisterFloat("aim_autoAimRangeScale", 1.f, 0.f, 2.f, 0);

	aim_lockon_pitch_strength = Dvar::Dvar_RegisterFloat("aim_lockon_pitch_strength", 0.6f, 0.f, 1.f, 0);

	UI_KeyEventT = safetyhook::create_inline(T4M::GetAddress("UI_KeyEvent"), UI_KeyEvent);
	Display_MouseMoveT = safetyhook::create_inline(T4M::GetAddress("Display_MouseMove"), Display_MouseMoveD);

	Memory::VP::InterceptCall(T4M::GetAddress("CL_GamepadButtonEvent_callsite"), CL_GamepadButtonEvent, CL_GamepadButtonEvent_was);

	Memory::VP::Nop(T4M::GetAddress("applyAutoMelee_call_nop"), 5);

	Memory::VP::InjectHook(T4M::GetAddress("applyAutoMelee_inject"), AimAssist_ApplyAutoMelee_4_ApplyAutoAim);

	static auto CL_MouseEvent = safetyhook::create_mid(T4M::GetAddress("CL_MouseEvent_hook"), [](SafetyHookContext& ctx) 
	{
		if (ctx.edi != 0 || ctx.esi != 0) {
			gpad_lastinput->current.integer = 1;
			gpad_lastinput->latched.integer = 1;
			last_input = false;
		}
	});
}