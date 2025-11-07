#include "StdInc.h"
#include "MemoryMgr.h"
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




void PatchT4E_Render() {

	r_buf_skinnedCacheVb = Dvar_RegisterInt(120, "r_buf_skinnedCacheVb", 60, 512, DVAR_FLAG_ARCHIVE);
	r_buf_tempSkin = Dvar_RegisterInt(120, "r_buf_tempSkin", 60, 512, DVAR_FLAG_ARCHIVE);
	r_buf_dynamicVertexBuffer = Dvar_RegisterInt(3, "r_buf_dynamicVertexBuffer", 1, 16, DVAR_FLAG_ARCHIVE);
	r_buf_dynamicIndexBuffer = Dvar_RegisterInt(4, "r_buf_dynamicIndexBuffer", 2, 16, DVAR_FLAG_ARCHIVE);
	r_buf_preTessIndexBuffer = Dvar_RegisterInt(4, "r_buf_preTessIndexBuffer", 2, 16, DVAR_FLAG_ARCHIVE);

	r_increase_render_buffers = Dvar_RegisterBool(true, "r_increase_render_buffers", DVAR_FLAG_ARCHIVE, "increasing rendering buffers");

	InterceptCall(0x6D594D, R_CreateDynamicBuffers, R_CreateDynamicBuffers_hook);


}