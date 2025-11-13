#include "t4_headers.h"
#include "StdInc.h"
#include "MemoryMgr.h"
#include <safetyhook.hpp>

typedef void*(__cdecl* R_CreateDynamicBuffersT)();
R_CreateDynamicBuffersT R_CreateDynamicBuffers = nullptr;

constexpr uint32_t MB_SIZE = 104857;

using namespace Memory::VP;

dvar_t* r_increase_render_buffers;

dvar_t* r_buf_skinnedCacheVb = nullptr;

dvar_t* r_buf_tempSkin = nullptr;
dvar_t* r_buf_dynamicVertexBuffer = nullptr;
dvar_t* r_buf_dynamicIndexBuffer = nullptr;
dvar_t* r_buf_preTessIndexBuffer = nullptr;

// Raises limits of several buffers, reference taken from iw3xo-dev by xoxor4d https://github.com/xoxor4d/iw3xo-dev
void* __cdecl R_CreateDynamicBuffers_hook() {
	if (r_increase_render_buffers && r_increase_render_buffers->current.boolean) {
		if (r_buf_dynamicVertexBuffer) {
			// Default 1 MB
			Patch<uint32_t>((0x0070EC1A + 3), r_buf_dynamicVertexBuffer->current.integer * MB_SIZE);
			Patch<uint32_t>((0x0070EC42), r_buf_dynamicVertexBuffer->current.integer * MB_SIZE);
			Patch<uint32_t>((0x70ED2C + 1), r_buf_dynamicVertexBuffer->current.integer * MB_SIZE);
		}

		if (r_buf_skinnedCacheVb) {
			// default 60 MB?
			Patch<uint32_t>((0x0070EC77 + 1), r_buf_skinnedCacheVb->current.integer * MB_SIZE);
			Patch<uint32_t>((0x0070EC42), r_buf_skinnedCacheVb->current.integer * MB_SIZE);
			Patch<uint32_t>((0x70ED2C + 1), r_buf_skinnedCacheVb->current.integer * MB_SIZE);

			Patch<uint32_t>((0x71DD7C + 2), ((uint32_t)((float)(r_buf_skinnedCacheVb->current.integer * MB_SIZE) / 1.44444444f))); // ok this idk f why
			Patch<uint32_t>((0x71DD95 + 2), r_buf_skinnedCacheVb->current.integer * MB_SIZE);
		}

		if (r_buf_tempSkin) {
			// default 60 MB?
			Patch<uint32_t>((0x6F52A7 + 1), r_buf_tempSkin->current.integer * MB_SIZE);

		}

		if (r_buf_dynamicIndexBuffer) {
			// default 2 MB
			Patch<uint32_t>((0x70ECF8 + 3), (r_buf_dynamicIndexBuffer->current.integer * MB_SIZE) / 2); // idk why
			Patch<uint32_t>((0x70EDA6 + 1), r_buf_dynamicIndexBuffer->current.integer * MB_SIZE);
			Patch<uint32_t>((0x70EE1E + 1), r_buf_dynamicIndexBuffer->current.integer * MB_SIZE);
		}

		if (r_buf_preTessIndexBuffer) {
			// default 2 MB
			Patch<uint32_t>((0x0070EDEA + 3), (r_buf_preTessIndexBuffer->current.integer * MB_SIZE) / 2); // idk why
			Patch<uint32_t>((0x70EE7D + 1), r_buf_preTessIndexBuffer->current.integer * MB_SIZE);
		}
	}
	return R_CreateDynamicBuffers();
}

dvar_t* cg_fov_default;

dvar_t* cg_gun_fovcomp_x;

dvar_t* cg_gun_fovcomp_y;

dvar_t* cg_gun_fovcomp_z;

dvar_t* cg_fovCompMax;

dvar_t* cg_fovComp_enable;

void __cdecl CG_CalculateWeaponMovement_Debug(const cg_s* cgameGlob, float* origin)
{
	float v2;
	float v3;
	float v4;
	float v5;
	float v6;
	float v7;
	float fovcomp_y;
	float fovcomp_z;
	float fovCoeff;

	dvar_t* cg_fov = *(dvar_t**)0x0368EB70;

	dvar_t* cg_gun_x = *(dvar_t**)0x034660EC;

	dvar_t* cg_gun_y = *(dvar_t**)0x0339B758;

	dvar_t* cg_gun_z = *(dvar_t**)0x03466074;

	v6 = (float)(cg_fov->current.value - cg_fov_default->current.value)
		* (float)(1.0 / (float)(cg_fovCompMax->current.value - cg_fov_default->current.value));
	if ((float)(v6 - 1.0) < 0.0)
		v7 = (float)(cg_fov->current.value - cg_fov_default->current.value)
		* (float)(1.0 / (float)(cg_fovCompMax->current.value - cg_fov_default->current.value));
	else
		v7 = 1.f;
	if ((float)(0.0 - v6) < 0.0)
		v2 = v7;
	else
		v2 = 0.f;
	fovCoeff = (float)(1.0 - cgameGlob->predictedPlayerState.fWeaponPosFrac) * v2;
	fovcomp_y = cg_gun_fovcomp_y->current.value * fovCoeff;
	fovcomp_z = cg_gun_fovcomp_z->current.value * fovCoeff;
	v5 = cg_gun_x->current.value + (float)(cg_gun_fovcomp_x->current.value * fovCoeff);
	*origin = (float)(v5 * cgameGlob->viewModelAxis[0][0]) + *origin;
	origin[1] = (float)(v5 * cgameGlob->viewModelAxis[0][1]) + origin[1];
	origin[2] = (float)(v5 * cgameGlob->viewModelAxis[0][2]) + origin[2];
	v4 = cg_gun_y->current.value + fovcomp_y;
	*origin = (float)(v4 * cgameGlob->viewModelAxis[1][0]) + *origin;
	origin[1] = (float)(v4 * cgameGlob->viewModelAxis[1][1]) + origin[1];
	origin[2] = (float)(v4 * cgameGlob->viewModelAxis[1][2]) + origin[2];
	v3 = cg_gun_z->current.value + fovcomp_z;
	*origin = (float)(v3 * cgameGlob->viewModelAxis[2][0]) + *origin;
	origin[1] = (float)(v3 * cgameGlob->viewModelAxis[2][1]) + origin[1];
	origin[2] = (float)(v3 * cgameGlob->viewModelAxis[2][2]) + origin[2];
}



void PatchT4E_Render() {

	static auto fovcomp_backport = safetyhook::create_mid(0x00469CD6, [](SafetyHookContext& ctx) {
		if (cg_fovComp_enable && cg_fovComp_enable->isEnabled()) {
			float* origin = (float*)(ctx.esp + 0x28);

			CG_CalculateWeaponMovement_Debug((cg_s*)(0x034732B8), origin);
			ctx.eip = 0x00469CE8;
		}
		});

	cg_fovComp_enable = Dvar_RegisterBool(false, "cg_fovComp_enable", DVAR_FLAG_ARCHIVE, "Enables backported fovComp behaviour from Black Ops 1");

	cg_fov_default = Dvar_RegisterFloat("cg_fov_default", 65.f, 10.f, 160.f, DVAR_FLAG_ARCHIVE, "User default field of view angle in degrees");

	cg_fovCompMax = Dvar_RegisterFloat(
		"cg_fovCompMax",
		85.0,
		1.0,
		160.0,
		0,
		"The maximum field of view to compensate for gun placement");

	cg_gun_fovcomp_x = Dvar_RegisterFloat(
		"cg_gun_fovcomp_x",
		-2.0,
		FLT_MIN,
		FLT_MAX,
		0,
		"x position FOV offset compensation of the viewmodel");
	cg_gun_fovcomp_y = Dvar_RegisterFloat(
		"cg_gun_fovcomp_y",
		0.0,
		FLT_MIN,
		FLT_MAX,
		0,
		"y position FOV offset compensation of the viewmodel");
	cg_gun_fovcomp_z = Dvar_RegisterFloat(
		"cg_gun_fovcomp_z",
		0.0,
		FLT_MIN,
		FLT_MAX,
		0,
		"z position FOV offset compensation of the viewmodel");

	r_buf_skinnedCacheVb = Dvar_RegisterInt(120, "r_buf_skinnedCacheVb", 60, 512, DVAR_FLAG_ARCHIVE);
	r_buf_tempSkin = Dvar_RegisterInt(120, "r_buf_tempSkin", 60, 512, DVAR_FLAG_ARCHIVE);
	r_buf_dynamicVertexBuffer = Dvar_RegisterInt(3, "r_buf_dynamicVertexBuffer", 1, 16, DVAR_FLAG_ARCHIVE);
	r_buf_dynamicIndexBuffer = Dvar_RegisterInt(4, "r_buf_dynamicIndexBuffer", 2, 16, DVAR_FLAG_ARCHIVE);
	r_buf_preTessIndexBuffer = Dvar_RegisterInt(4, "r_buf_preTessIndexBuffer", 2, 16, DVAR_FLAG_ARCHIVE);

	r_increase_render_buffers = Dvar_RegisterBool(true, "r_increase_render_buffers", DVAR_FLAG_ARCHIVE, "increasing rendering buffers");

	InterceptCall(0x6D594D, R_CreateDynamicBuffers, R_CreateDynamicBuffers_hook);


}