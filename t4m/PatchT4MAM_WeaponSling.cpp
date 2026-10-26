// ==========================================================
// T4M project
//
// Component: clientdll
// Purpose: Simulated weapon sling. Bones j_sling_0..N of any model in a DObj are
//          driven by a Verlet chain pinned at both ends, written into the skel
//          right after DObjCalcSkel. Plan: plans/plan_weapon_sling_physics.md
// ==========================================================

#include "StdInc.h"
#include "T4.h"
#include <cfloat>
#include <climits>
#include <cmath>
#include <cstring>
#include <string>
#include <vector>

using namespace T4::engine;

namespace
{
	constexpr int   kMaxSlingBones  = 8;
	constexpr int   kMinSlingBones  = 3;
	constexpr float kGridCell        = 0.25f;
	constexpr int   kGridMaxCells    = 1 << 18;
	constexpr float kSegmentProbe    = 0.4f;
	constexpr float kFriction        = 0.6f;   // Coulomb: sliding slowed by at most kFriction x the push
	constexpr float kDeepPen         = 0.3f;   // deeper than this, leave on the side the point came from...
	constexpr float kHistoryJump     = 2.0f;   // ...unless it moved more than this since (fast spin: history meaningless)
	constexpr float kPinClear        = 1.0f;   // no collision this close to an attachment: the swivel holds the sling
	constexpr int   kModelSlots     = 2048;
	constexpr int   kMaxRigs        = 128;
	constexpr int   kMaxBodies      = 128;
	constexpr int   kStateSlots     = 256;
	constexpr int   kMaxSubsteps    = 4;
	constexpr float kStepMs         = 1000.0f / 60.0f;
	constexpr int   kResetDtMs      = 250;
	constexpr DWORD kStaleMs        = 2000;
	constexpr float kSlingThickness = 0.3f;

	const char* const kBodyBones[] = {
		"j_mainroot", "j_spinelower", "j_spine4", "j_neck", "j_head",
		"j_shoulder_le", "j_elbow_le", "j_shoulder_ri", "j_elbow_ri",
		"j_hip_le", "j_knee_le", "j_ankle_le",
		"j_hip_ri", "j_knee_ri", "j_ankle_ri",
	};
	constexpr int kNumBodyBones = sizeof(kBodyBones) / sizeof(kBodyBones[0]);

	// Free chains, pinned at the root only: root (or root_0), root_1, root_2... (helmet straps). A helmet
	// melded onto a body (attach without tag) must use root_0: every WaW body has the plain root, and a
	// melded bone of the same name is recopied from the body at each DObjCalcSkel.
	const char* const kDangleRoots[] = { "j_helmetstrap_le", "j_helmetstrap_ri" };
	constexpr int kNumDangleRoots = sizeof(kDangleRoots) / sizeof(kDangleRoots[0]);

	struct CapsuleDef { int a, b; float radius; };

	// Indices into kBodyBones. Radii = median distance of the skin to the bone segment on
	// char_usa_marine_body1_1 (gear included), neck and head on char_usa_marine_sullivan_pel1; upper chest
	// trimmed to 5.6 so helmet straps start on the collar, not in it. No forearm or
	// hand: they hold the gun between its sling swivels, the sling got trapped (looped) around them.
	const CapsuleDef kCapsules[] = {
		{ 0, 1, 6.9f }, { 1, 2, 7.6f }, { 2, 5, 5.6f }, { 2, 7, 5.6f }, { 2, 3, 4.0f }, { 3, 4, 4.0f },
		{ 5, 6, 3.2f }, { 7, 8, 3.2f },
		{ 9, 10, 5.0f }, { 10, 11, 3.4f },
		{ 12, 13, 5.0f }, { 13, 14, 3.4f },
	};
	constexpr int kNumCapsules = sizeof(kCapsules) / sizeof(kCapsules[0]);

	struct Capsule { float a[3], b[3], radius; };

	struct SlingRig
	{
		const XModel*  model;
		char           name[64];
		char           anchorName[32];
		const char*    reject;
		int            count;
		bool           freeEnd;                 // pinned at the root only (helmet strap)
		int            anchor;
		int            bone[kMaxSlingBones];
		float          localQuat[kMaxSlingBones][4];
		float          localTrans[kMaxSlingBones][3];
		float          restLen[kMaxSlingBones - 1];
		// Weapon collision from the LOD0 render geometry, in g_grids[rig index]; collReject says why none.
		const char*    collReject;
		int            collTris;
		int            collCells;
		int            collCarved;
		int            collRestHits;
		volatile LONG  checks;
		volatile float maxPosErr;
		volatile float maxAngErr;
	};

	struct BodyBones
	{
		int bone[kNumBodyBones];
	};

	struct ModelSlot
	{
		const XModel* model;
		const char*   name;
		int           numBones;
		short         rig;                      // first rig in g_rigs
		short         rigCount;
		short         body;
	};

	struct SlingState
	{
		const DObj_s* obj;
		const XModel* model;
		int           modelIndex;
		DWORD         touchMs;
		int           skelStamp;
		int           cgTime;
		float         hPrev;
		bool          valid;
		float         pos[kMaxSlingBones][3];
		float         prev[kMaxSlingBones][3];
		float         rigidPrev[kMaxSlingBones][3];
		bool          history;                  // anchorPrev* / prevCaps of the last step
		float         anchorPrevQ[4];
		float         anchorPrevT[3];
		Capsule       prevCaps[kNumCapsules];
		int           prevNumCaps;              // -1: no capsule history
	};

	// Weapon seen from its side (anchor x, z), one cell per kGridCell: y extent of the weapon there,
	// grown in-plane by kSlingThickness, and the nearest empty cell for in-plane exits.
	struct SlingGrid
	{
		float              x0, z0;
		int                nx, nz;
		std::vector<float> yMin, yMax;   // empty cell: yMin > yMax
		std::vector<int>   nearFree;     // nearest empty cell, -1 if none
	};

	// Model cache: shared while processing, exclusive to build or clear.
	SRWLOCK    g_modelLock = SRWLOCK_INIT;
	ModelSlot  g_modelSlots[kModelSlots];
	int        g_modelCount;
	SlingRig   g_rigs[kMaxRigs];
	SlingGrid  g_grids[kMaxRigs];
	int        g_rigCount;
	BodyBones  g_bodies[kMaxBodies];
	int        g_bodyCount;

	// Claim/lookup only; a state itself is touched under its DObj lock.
	SRWLOCK    g_stateLock = SRWLOCK_INIT;
	SlingState g_states[kStateSlots];

	inline bool TestBit(const int* bits, int index)
	{
		return (bits[index >> 5] & (0x80000000u >> (index & 31))) != 0;
	}

	inline void Copy3(float* d, const float* s) { d[0] = s[0]; d[1] = s[1]; d[2] = s[2]; }
	inline void Sub3(const float* a, const float* b, float* o) { o[0] = a[0] - b[0]; o[1] = a[1] - b[1]; o[2] = a[2] - b[2]; }
	inline void Add3(const float* a, const float* b, float* o) { o[0] = a[0] + b[0]; o[1] = a[1] + b[1]; o[2] = a[2] + b[2]; }
	inline float Dot3(const float* a, const float* b) { return a[0] * b[0] + a[1] * b[1] + a[2] * b[2]; }
	inline float DistSq3(const float* a, const float* b) { float d[3]; Sub3(a, b, d); return Dot3(d, d); }

	inline void Cross3(const float* a, const float* b, float* o)
	{
		o[0] = a[1] * b[2] - a[2] * b[1];
		o[1] = a[2] * b[0] - a[0] * b[2];
		o[2] = a[0] * b[1] - a[1] * b[0];
	}

	inline bool Finite3(const float* v)
	{
		return std::isfinite(v[0]) && std::isfinite(v[1]) && std::isfinite(v[2]);
	}

	// Hamilton product a*b, xyzw — same as the engine's QuatMultiplyEquals(a, b).
	inline void QuatMul(const float* a, const float* b, float* o)
	{
		const float x = a[3] * b[0] + a[0] * b[3] + a[1] * b[2] - a[2] * b[1];
		const float y = a[3] * b[1] - a[0] * b[2] + a[1] * b[3] + a[2] * b[0];
		const float z = a[3] * b[2] + a[0] * b[1] - a[1] * b[0] + a[2] * b[3];
		const float w = a[3] * b[3] - a[0] * b[0] - a[1] * b[1] - a[2] * b[2];
		o[0] = x; o[1] = y; o[2] = z; o[3] = w;
	}

	inline void QuatNormalize(float* q)
	{
		const float lenSq = q[0] * q[0] + q[1] * q[1] + q[2] * q[2] + q[3] * q[3];
		if (lenSq < 1e-12f)
		{
			q[0] = q[1] = q[2] = 0.0f;
			q[3] = 1.0f;
			return;
		}
		const float inv = 1.0f / sqrtf(lenSq);
		q[0] *= inv; q[1] *= inv; q[2] *= inv; q[3] *= inv;
	}

	// q unit.
	inline void QuatRotate(const float* q, const float* v, float* o)
	{
		float t[3], c[3];
		Cross3(q, v, t);
		t[0] *= 2.0f; t[1] *= 2.0f; t[2] *= 2.0f;
		Cross3(q, t, c);
		o[0] = v[0] + q[3] * t[0] + c[0];
		o[1] = v[1] + q[3] * t[1] + c[1];
		o[2] = v[2] + q[3] * t[2] + c[2];
	}

	// Shortest rotation taking direction `from` onto direction `to`.
	void Swing(const float* from, const float* to, float* q)
	{
		const float la = sqrtf(Dot3(from, from));
		const float lb = sqrtf(Dot3(to, to));
		if (la < 1e-6f || lb < 1e-6f)
		{
			q[0] = q[1] = q[2] = 0.0f;
			q[3] = 1.0f;
			return;
		}
		const float a[3] = { from[0] / la, from[1] / la, from[2] / la };
		const float b[3] = { to[0] / lb, to[1] / lb, to[2] / lb };
		const float d = Dot3(a, b);
		if (d < -0.9999f)
		{
			const float ref[3] = { fabsf(a[0]) < 0.9f ? 1.0f : 0.0f, fabsf(a[0]) < 0.9f ? 0.0f : 1.0f, 0.0f };
			Cross3(a, ref, q);
			q[3] = 0.0f;
			QuatNormalize(q);
			return;
		}
		Cross3(a, b, q);
		q[3] = 1.0f + d;
		QuatNormalize(q);
	}

	inline unsigned int HashPtr(const void* p)
	{
		return (unsigned int)(((uintptr_t)p >> 4) * 2654435761u);
	}

	// Caller holds g_modelLock (shared or exclusive).
	ModelSlot* LookupModel(const XModel* model)
	{
		const unsigned int h = HashPtr(model);
		for (int i = 0; i < kModelSlots; ++i)
		{
			ModelSlot& s = g_modelSlots[(h + i) & (kModelSlots - 1)];
			if (!s.model)
				return nullptr;
			if (s.model == model)
				return &s;
		}
		return nullptr;
	}

	inline bool SlotIsCurrent(const ModelSlot* s, const XModel* model)
	{
		return s && model && s->name == model->name && s->numBones == (unsigned char)model->numBones;
	}

	// LOD0 render geometry in anchor space; a triangle with no j_sling_ bone is the weapon. Layout
	// from Load_XSurface (sub_47DB00); verts0 are model-space bind positions (rigid placement =
	// anim * inverse(base) in sub_71DDB0). Returns why the geometry is unusable, or nullptr.
	const char* CollectGeometry(const XModel* model, const SlingRig& rig, std::vector<float>& tris)
	{
		const int numBones = (unsigned char)model->numBones;
		const XModelLodInfo& lod = model->lodInfo[0];
		if (!model->surfs || !model->baseMat || !lod.numsurfs
			|| lod.surfIndex + lod.numsurfs > (unsigned char)model->numsurfs)
			return "no LOD0 surfaces";

		bool slingBone[256] = {};
		for (int k = 0; k < rig.count; ++k)
			slingBone[rig.bone[k]] = true;

		const DObjAnimMat& base = model->baseMat[rig.anchor];
		float iq[4] = { -base.quat[0], -base.quat[1], -base.quat[2], base.quat[3] };
		QuatNormalize(iq);

		// Loose bounds: only catches garbage.
		float lo[3], hi[3];
		for (int c = 0; c < 3; ++c)
		{
			const float pad = 1.0f + 0.25f * (model->maxs[c] - model->mins[c]);
			lo[c] = model->mins[c] - pad;
			hi[c] = model->maxs[c] + pad;
		}

		float wxMin = FLT_MAX, wxMax = -FLT_MAX;
		std::vector<unsigned char> isSling;

		for (int s = 0; s < lod.numsurfs; ++s)
		{
			const XSurface& surf = model->surfs[lod.surfIndex + s];
			const int nv = surf.vertCount;
			const int nt = surf.triCount;
			if (!surf.verts0 || !surf.triIndices || !nv || !nt)
				return "empty LOD0 surface";

			isSling.assign(nv, 0);
			if (!surf.deformed)
			{
				if (!surf.vertListCount || !surf.vertList)
					return "rigid surface without vertex list";
				int first = 0;
				for (unsigned int i = 0; i < surf.vertListCount; ++i)
				{
					const XRigidVertList& list = surf.vertList[i];
					const int bone = list.boneOffset >> 6;
					if ((list.boneOffset & 63) || bone >= numBones || first + list.vertCount > nv)
						return "inconsistent rigid vertex list";
					if (slingBone[bone])
						memset(&isSling[first], 1, list.vertCount);
					first += list.vertCount;
				}
				if (first != nv)
					return "rigid vertex lists do not cover the surface";
			}
			else
			{
				// Vertices sorted by weight count; per vertex: bone, then (bone, weight) per extra weight.
				const unsigned short* blend = surf.vertInfo.vertsBlend;
				int v = 0;
				for (int w = 0; w < 4; ++w)
				{
					const int count = (unsigned short)surf.vertInfo.vertCount[w];
					if (count && !blend)
						return "deformed surface without blend data";
					for (int i = 0; i < count; ++i, ++v, blend += 1 + 2 * w)
					{
						if (v >= nv)
							return "blend counts exceed the vertex count";
						for (int j = 0; j <= w; ++j)
						{
							const unsigned short off = blend[j ? 2 * j - 1 : 0];
							if ((off & 63) || (off >> 6) >= numBones)
								return "inconsistent blend bone";
							if (slingBone[off >> 6])
								isSling[v] = 1;
						}
					}
				}
				if (v != nv)
					return "blend counts do not cover the surface";
			}

			for (int v = 0; v < nv; ++v)
			{
				const float* p = surf.verts0[v].xyz;
				if (!Finite3(p) || p[0] < lo[0] || p[0] > hi[0] || p[1] < lo[1] || p[1] > hi[1] || p[2] < lo[2] || p[2] > hi[2])
					return "vertex outside the model bounds";
			}

			for (int t = 0; t < nt; ++t)
			{
				const unsigned short* idx = surf.triIndices + 3 * t;
				if (idx[0] >= nv || idx[1] >= nv || idx[2] >= nv)
					return "triangle index out of range";
				if (isSling[idx[0]] || isSling[idx[1]] || isSling[idx[2]])
					continue;
				for (int c = 0; c < 3; ++c)
				{
					const float* p = surf.verts0[idx[c]].xyz;
					wxMin = p[0] < wxMin ? p[0] : wxMin;
					wxMax = p[0] > wxMax ? p[0] : wxMax;
					float d[3], a[3];
					Sub3(p, base.trans, d);
					QuatRotate(iq, d, a);
					tris.insert(tris.end(), a, a + 3);
				}
			}
		}

		if (tris.empty())
			return "no weapon triangle in LOD0";
		if (wxMax - wxMin < 0.5f * (model->maxs[0] - model->mins[0]))
			return "LOD0 vertices not loaded";
		return nullptr;
	}

	inline bool GridEmpty(const SlingGrid& g, int c)
	{
		return g.yMin[c] > g.yMax[c];
	}

	// y extent at (fx, fz) in cell units: bilinear over the 4 surrounding cell centres, an empty or
	// out-of-grid neighbour taking cell c's own extent. Keeps the push continuous across cells.
	void GridYRange(const SlingGrid& g, float fx, float fz, int c, float& y0, float& y1)
	{
		const float gx = fx - 0.5f;
		const float gz = fz - 0.5f;
		const int ix = (int)floorf(gx);
		const int iz = (int)floorf(gz);
		const float tx = gx - ix;
		const float tz = gz - iz;
		const float w[4] = { (1 - tx) * (1 - tz), tx * (1 - tz), (1 - tx) * tz, tx * tz };
		y0 = y1 = 0.0f;
		for (int k = 0; k < 4; ++k)
		{
			const int cx = ix + (k & 1);
			const int cz = iz + (k >> 1);
			int n = c;
			if (cx >= 0 && cz >= 0 && cx < g.nx && cz < g.nz && !GridEmpty(g, cz * g.nx + cx))
				n = cz * g.nx + cx;
			y0 += g.yMin[n] * w[k];
			y1 += g.yMax[n] * w[k];
		}
	}

	// First empty cell on the way from p back to prevP, in-plane: where the point came in.
	bool GridEntry(const SlingGrid& g, const float* p, const float* prevP, float& tx, float& tz)
	{
		const float dx = prevP[0] - p[0];
		const float dz = prevP[2] - p[2];
		const float len = sqrtf(dx * dx + dz * dz);
		if (len < 1e-4f)
			return false;
		int n = (int)ceilf(len / (kGridCell * 0.5f));
		n = n > 64 ? 64 : n;
		for (int i = 1; i <= n; ++i)
		{
			const float x = p[0] + dx * i / n;
			const float z = p[2] + dz * i / n;
			const int ix = (int)floorf((x - g.x0) / kGridCell);
			const int iz = (int)floorf((z - g.z0) / kGridCell);
			if (ix < 0 || iz < 0 || ix >= g.nx || iz >= g.nz || GridEmpty(g, iz * g.nx + ix))
			{
				tx = x;
				tz = z;
				return true;
			}
		}
		return false;
	}

	// Anchor-space point inside the weapon (grown by kSlingThickness)? push = shortest way out: along
	// y to a face, or in-plane to the edge of the nearest empty cell. Deeper than kDeepPen, the
	// shortest way may be the far side of a thin part: leave on the side of prevP (where the point
	// was before this step, anchor space of that step) instead.
	bool GridPush(const SlingGrid& g, const float* p, const float* prevP, float* push)
	{
		const float fx = (p[0] - g.x0) / kGridCell;
		const float fz = (p[2] - g.z0) / kGridCell;
		const int ix = (int)floorf(fx);
		const int iz = (int)floorf(fz);
		if (ix < 0 || iz < 0 || ix >= g.nx || iz >= g.nz)
			return false;
		const int c = iz * g.nx + ix;
		if (GridEmpty(g, c))
			return false;
		float y0, y1;
		GridYRange(g, fx, fz, c, y0, y1);
		y0 -= kSlingThickness;
		y1 += kSlingThickness;
		if (p[1] <= y0 || p[1] >= y1)
			return false;

		const float d0 = p[1] - y0;
		const float d1 = y1 - p[1];
		float dIn = FLT_MAX, tx = p[0], tz = p[2];
		const int f = g.nearFree[c];
		if (f >= 0)
		{
			const float h = kGridCell * 0.5f - 0.005f;
			const float cx = g.x0 + ((f % g.nx) + 0.5f) * kGridCell;
			const float cz = g.z0 + ((f / g.nx) + 0.5f) * kGridCell;
			tx = p[0] < cx - h ? cx - h : (p[0] > cx + h ? cx + h : p[0]);
			tz = p[2] < cz - h ? cz - h : (p[2] > cz + h ? cz + h : p[2]);
			dIn = sqrtf((tx - p[0]) * (tx - p[0]) + (tz - p[2]) * (tz - p[2]));
		}
		int exit = d0 <= d1 && d0 <= dIn ? 0 : (d1 <= dIn ? 1 : 2);
		const float depth = exit == 0 ? d0 : (exit == 1 ? d1 : dIn);
		if (depth > kDeepPen && prevP && DistSq3(p, prevP) < kHistoryJump * kHistoryJump)
		{
			if (prevP[1] >= y1)
				exit = 1;
			else if (prevP[1] <= y0)
				exit = 0;
			else if (GridEntry(g, p, prevP, tx, tz))
				exit = 2;
		}
		push[0] = push[1] = push[2] = 0.0f;
		if (exit == 0)
			push[1] = -d0;
		else if (exit == 1)
			push[1] = d1;
		else
		{
			push[0] = tx - p[0];
			push[2] = tz - p[2];
		}
		return true;
	}

	// Moves a face of p's cell and of its interpolation cells so that p is out of them; `all` also
	// moves the cells p is not inside of on its own. Returns the number of cells changed.
	int CarveAround(SlingGrid& g, const float* p, bool all)
	{
		const float fx = (p[0] - g.x0) / kGridCell;
		const float fz = (p[2] - g.z0) / kGridCell;
		const int ix = (int)floorf(fx);
		const int iz = (int)floorf(fz);
		if (ix < 0 || iz < 0 || ix >= g.nx || iz >= g.nz)
			return 0;
		const int gx = (int)floorf(fx - 0.5f);
		const int gz = (int)floorf(fz - 0.5f);
		const int around[5][2] = { { ix, iz }, { gx, gz }, { gx + 1, gz }, { gx, gz + 1 }, { gx + 1, gz + 1 } };
		int moved = 0;
		for (int a = 0; a < 5; ++a)
		{
			const int cx = around[a][0];
			const int cz = around[a][1];
			if (cx < 0 || cz < 0 || cx >= g.nx || cz >= g.nz)
				continue;
			const int k = cz * g.nx + cx;
			if (GridEmpty(g, k))
				continue;
			const bool above = p[1] >= (g.yMin[k] + g.yMax[k]) * 0.5f;
			if ((above && p[1] >= g.yMax[k] + kSlingThickness) || (!above && p[1] <= g.yMin[k] - kSlingThickness))
				continue;
			if (!all && (p[1] <= g.yMin[k] - kSlingThickness || p[1] >= g.yMax[k] + kSlingThickness))
				continue;
			++moved;
			if (above)
				g.yMax[k] = p[1] - kSlingThickness - 0.01f;
			else
				g.yMin[k] = p[1] + kSlingThickness + 0.01f;
		}
		return moved;
	}

	// Side grid of the weapon for sling collision, once per model. The sling at rest legitimately
	// touches the weapon at its attachments: those cells get their faces moved so it starts outside.
	void BuildCollision(SlingRig& rig, const XModel* model, SlingGrid& g)
	{
		g = SlingGrid();
		std::vector<float> tris;
		rig.collReject = CollectGeometry(model, rig, tris);
		if (rig.collReject)
			return;
		rig.collTris = (int)(tris.size() / 9);

		float xMin = FLT_MAX, xMax = -FLT_MAX, zMin = FLT_MAX, zMax = -FLT_MAX;
		for (size_t i = 0; i < tris.size(); i += 3)
		{
			xMin = tris[i] < xMin ? tris[i] : xMin;
			xMax = tris[i] > xMax ? tris[i] : xMax;
			zMin = tris[i + 2] < zMin ? tris[i + 2] : zMin;
			zMax = tris[i + 2] > zMax ? tris[i + 2] : zMax;
		}
		g.x0 = xMin - 1.0f;
		g.z0 = zMin - 1.0f;
		g.nx = (int)((xMax + 1.0f - g.x0) / kGridCell) + 1;
		g.nz = (int)((zMax + 1.0f - g.z0) / kGridCell) + 1;
		if (g.nx * g.nz > kGridMaxCells)
		{
			rig.collReject = "weapon too large for the collision grid";
			g = SlingGrid();
			return;
		}
		const int cells = g.nx * g.nz;

		// Raw extent: triangles sampled finer than half a cell.
		std::vector<float> rawMin(cells, FLT_MAX), rawMax(cells, -FLT_MAX);
		for (size_t i = 0; i < tris.size(); i += 9)
		{
			const float* a = &tris[i];
			const float* b = a + 3;
			const float* c = a + 6;
			float e2 = DistSq3(a, b);
			e2 = DistSq3(b, c) > e2 ? DistSq3(b, c) : e2;
			e2 = DistSq3(c, a) > e2 ? DistSq3(c, a) : e2;
			const float e = sqrtf(e2);
			int n = (int)ceilf(e / (kGridCell * 0.5f));
			n = n < 1 ? 1 : (n > 400 ? 400 : n);
			for (int u = 0; u <= n; ++u)
			{
				for (int v = 0; u + v <= n; ++v)
				{
					const float fu = (float)u / n;
					const float fv = (float)v / n;
					const float x = a[0] + (b[0] - a[0]) * fu + (c[0] - a[0]) * fv;
					const float y = a[1] + (b[1] - a[1]) * fu + (c[1] - a[1]) * fv;
					const float z = a[2] + (b[2] - a[2]) * fu + (c[2] - a[2]) * fv;
					const int ix = (int)floorf((x - g.x0) / kGridCell);
					const int iz = (int)floorf((z - g.z0) / kGridCell);
					if (ix < 0 || iz < 0 || ix >= g.nx || iz >= g.nz)
						continue;
					const int k = iz * g.nx + ix;
					rawMin[k] = y < rawMin[k] ? y : rawMin[k];
					rawMax[k] = y > rawMax[k] ? y : rawMax[k];
				}
			}
		}

		// Grow in-plane by the sling thickness.
		g.yMin.assign(cells, FLT_MAX);
		g.yMax.assign(cells, -FLT_MAX);
		const int r = (int)ceilf(kSlingThickness / kGridCell);
		const float reach = (kSlingThickness + 0.71f * kGridCell) * (kSlingThickness + 0.71f * kGridCell);
		for (int iz = 0; iz < g.nz; ++iz)
		{
			for (int ix = 0; ix < g.nx; ++ix)
			{
				const int k = iz * g.nx + ix;
				if (rawMin[k] > rawMax[k])
					continue;
				for (int dz = -r; dz <= r; ++dz)
				{
					for (int dx = -r; dx <= r; ++dx)
					{
						const int cx = ix + dx;
						const int cz = iz + dz;
						if (cx < 0 || cz < 0 || cx >= g.nx || cz >= g.nz || (dx * dx + dz * dz) * kGridCell * kGridCell > reach)
							continue;
						const int n = cz * g.nx + cx;
						g.yMin[n] = rawMin[k] < g.yMin[n] ? rawMin[k] : g.yMin[n];
						g.yMax[n] = rawMax[k] > g.yMax[n] ? rawMax[k] : g.yMax[n];
					}
				}
			}
		}

		// Rest chain in anchor space, sampled every 0.1.
		float rest[kMaxSlingBones][3];
		float q[4] = { 0.0f, 0.0f, 0.0f, 1.0f }, t[3] = { 0.0f, 0.0f, 0.0f };
		for (int k = 0; k < rig.count; ++k)
		{
			float d[3], nq[4];
			QuatRotate(q, rig.localTrans[k], d);
			Add3(t, d, t);
			QuatMul(q, rig.localQuat[k], nq);
			QuatNormalize(nq);
			memcpy(q, nq, sizeof(q));
			Copy3(rest[k], t);
		}
		std::vector<float> restPts;
		for (int k = 0; k + 1 < rig.count; ++k)
		{
			const int m = (int)(rig.restLen[k] / 0.1f) + 1;
			for (int j = 0; j <= m; ++j)
			{
				float pt[3];
				for (int c = 0; c < 3; ++c)
					pt[c] = rest[k][c] + (rest[k + 1][c] - rest[k][c]) * j / m;
				if (DistSq3(pt, rest[0]) >= kPinClear * kPinClear && DistSq3(pt, rest[rig.count - 1]) >= kPinClear * kPinClear)
					restPts.insert(restPts.end(), pt, pt + 3);
			}
		}

		// Move the faces of the cells the rest sling sits in (own cell + interpolation cells), again while
		// a rest point is still inside: an emptied cell changes its neighbours' interpolation.
		g.nearFree.assign(cells, -1);
		float push[3];
		for (int pass = 0; pass < 4; ++pass)
		{
			int moved = 0;
			for (size_t i = 0; i < restPts.size(); i += 3)
			{
				const float* p = &restPts[i];
				if (pass && !GridPush(g, p, nullptr, push))
					continue;
				moved += CarveAround(g, p, pass > 0);
			}
			rig.collCarved += moved;
			if (!moved)
				break;
		}

		// Nearest empty cell of every occupied one (ring search, squared cell distance).
		for (int iz = 0; iz < g.nz; ++iz)
		{
			for (int ix = 0; ix < g.nx; ++ix)
			{
				const int k = iz * g.nx + ix;
				if (GridEmpty(g, k))
					continue;
				++rig.collCells;
				int best = -1, bestD = INT_MAX;
				for (int rad = 1; rad < 64 && bestD > rad * rad; ++rad)
				{
					for (int dz = -rad; dz <= rad; ++dz)
					{
						const int step = (dz == -rad || dz == rad) ? 1 : 2 * rad;
						for (int dx = -rad; dx <= rad; dx += step)
						{
							const int cx = ix + dx;
							const int cz = iz + dz;
							const bool outside = cx < 0 || cz < 0 || cx >= g.nx || cz >= g.nz;
							if (!outside && !GridEmpty(g, cz * g.nx + cx))
								continue;
							const int d = dx * dx + dz * dz;
							if (d < bestD && !outside)
							{
								bestD = d;
								best = cz * g.nx + cx;
							}
						}
					}
				}
				g.nearFree[k] = best;
			}
		}

		for (size_t i = 0; i < restPts.size(); i += 3)
			if (GridPush(g, &restPts[i], nullptr, push))
				++rig.collRestHits;
	}

	// Digits after a prefix; false when empty, non-numeric or above 99.
	bool ParseIndex(const char* p, int& k)
	{
		k = 0;
		if (!*p)
			return false;
		for (; *p; ++p)
		{
			if (*p < '0' || *p > '9' || k > 99)
				return false;
			k = k * 10 + (*p - '0');
		}
		return true;
	}

	void BuildRig(SlingRig& rig, const XModel* model, const int* sling, int found, int maxK, const char* reject, bool freeEnd)
	{
		const int numRoot = (unsigned char)model->numRootBones;

		memset(&rig, 0, sizeof(rig));
		rig.model = model;
		strncpy_s(rig.name, model->name ? model->name : "?", _TRUNCATE);
		rig.count = maxK + 1;
		rig.freeEnd = freeEnd;
		rig.anchor = -1;

		if (!reject && found != rig.count)
			reject = "gap in the chain numbering";
		if (!reject && rig.count < (freeEnd ? 2 : kMinSlingBones))
			reject = freeEnd ? "fewer than 2 strap bones" : "fewer than 3 sling bones";

		for (int k = 0; !reject && k < rig.count; ++k)
		{
			const int li = sling[k];
			if (li < numRoot)
			{
				reject = "a sling bone is a root bone";
				break;
			}
			const int parent = li - (unsigned char)model->parentList[li - numRoot];
			if (k == 0)
			{
				for (int j = 0; j < rig.count; ++j)
					if (sling[j] == parent)
						reject = "j_sling_0 is parented to a sling bone";
				rig.anchor = parent;
			}
			else if (parent != sling[k - 1])
			{
				reject = "not a j_sling_0 -> j_sling_1 -> ... chain";
				break;
			}

			rig.bone[k] = li;
			const short* q = model->quats + 4 * (li - numRoot);
			for (int c = 0; c < 4; ++c)
				rig.localQuat[k][c] = q[c] * (1.0f / 32767.0f);
			QuatNormalize(rig.localQuat[k]);
			Copy3(rig.localTrans[k], model->trans + 3 * (li - numRoot));
		}

		for (int k = 0; !reject && k + 1 < rig.count; ++k)
		{
			rig.restLen[k] = sqrtf(Dot3(rig.localTrans[k + 1], rig.localTrans[k + 1]));
			if (rig.restLen[k] < 0.01f)
				reject = "zero-length sling segment";
		}

		if (rig.anchor >= 0)
		{
			const char* an = T4::engine::SL_ConvertToString(model->boneNames[rig.anchor], SCRIPTINSTANCE_SERVER);
			strncpy_s(rig.anchorName, an ? an : "?", _TRUNCATE);
		}
		rig.reject = reject;
	}

	void ScanModel(const XModel* model, short& rigOut, short& rigCount, short& bodyOut)
	{
		const int numBones = (unsigned char)model->numBones;
		int sling[kMaxSlingBones];
		int body[kNumBodyBones];
		for (int k = 0; k < kMaxSlingBones; ++k) sling[k] = -1;
		for (int b = 0; b < kNumBodyBones; ++b) body[b] = -1;

		int dangle[kNumDangleRoots][kMaxSlingBones];
		int dangleFound[kNumDangleRoots] = {};
		int dangleMax[kNumDangleRoots];
		const char* dangleReject[kNumDangleRoots] = {};
		for (int r = 0; r < kNumDangleRoots; ++r)
		{
			dangleMax[r] = -1;
			for (int k = 0; k < kMaxSlingBones; ++k) dangle[r][k] = -1;
		}

		int found = 0, maxK = -1, bodyFound = 0;
		const char* reject = nullptr;

		for (int i = 0; i < numBones; ++i)
		{
			const char* name = T4::engine::SL_ConvertToString(model->boneNames[i], SCRIPTINSTANCE_SERVER);
			if (!name)
				continue;

			int k;
			if (!_strnicmp(name, "j_sling_", 8))
			{
				if (!ParseIndex(name + 8, k))   reject = "j_sling_ bone without a numeric index";
				else if (k >= kMaxSlingBones) reject = "more than 8 sling bones";
				else if (sling[k] >= 0)     reject = "duplicate j_sling_ bone";
				else
				{
					sling[k] = i;
					++found;
					if (k > maxK)
						maxK = k;
				}
				continue;
			}

			bool isDangle = false;
			for (int r = 0; r < kNumDangleRoots && !isDangle; ++r)
			{
				const size_t len = strlen(kDangleRoots[r]);
				if (_strnicmp(name, kDangleRoots[r], len))
					continue;
				if (!name[len])
					k = 0;
				else if (name[len] != '_' || !ParseIndex(name + len + 1, k))
					continue;
				isDangle = true;
				if (k >= kMaxSlingBones)      dangleReject[r] = "more than 8 strap bones";
				else if (dangle[r][k] >= 0)   dangleReject[r] = "duplicate strap bone";
				else
				{
					dangle[r][k] = i;
					++dangleFound[r];
					if (k > dangleMax[r])
						dangleMax[r] = k;
				}
			}
			if (isDangle)
				continue;

			for (int b = 0; b < kNumBodyBones; ++b)
			{
				if (body[b] < 0 && !_stricmp(name, kBodyBones[b]))
				{
					body[b] = i;
					++bodyFound;
					break;
				}
			}
		}

		rigOut = (short)g_rigCount;
		rigCount = 0;
		if ((found || reject) && g_rigCount < kMaxRigs)
		{
			SlingRig& rig = g_rigs[g_rigCount];
			BuildRig(rig, model, sling, found, maxK, reject, false);
			if (!rig.reject)
				BuildCollision(rig, model, g_grids[g_rigCount]);
			++g_rigCount;
			++rigCount;
		}
		// Every WaW rig has the strap roots; only a root with a _1 makes a chain.
		for (int r = 0; r < kNumDangleRoots && g_rigCount < kMaxRigs; ++r)
		{
			if (dangleMax[r] < 1 && !dangleReject[r])
				continue;
			SlingRig& rig = g_rigs[g_rigCount];
			BuildRig(rig, model, dangle[r], dangleFound[r], dangleMax[r], dangleReject[r], true);
			rig.collReject = "free chain, body capsules only";
			g_grids[g_rigCount] = SlingGrid();
			++g_rigCount;
			++rigCount;
		}
		if (!rigCount)
			rigOut = -1;

		bodyOut = -1;
		if (body[1] >= 0 && body[2] >= 0 && g_bodyCount < kMaxBodies)
		{
			memcpy(g_bodies[g_bodyCount].bone, body, sizeof(body));
			bodyOut = (short)g_bodyCount++;
		}
	}

	void ClearModelCache()
	{
		memset(g_modelSlots, 0, sizeof(g_modelSlots));
		g_modelCount = 0;
		for (int i = 0; i < g_rigCount; ++i)
			g_grids[i] = SlingGrid();
		g_rigCount = 0;
		g_bodyCount = 0;
	}

	// Caller holds g_modelLock exclusive.
	void EnsureModel(const XModel* model)
	{
		if (!model)
			return;
		ModelSlot* s = LookupModel(model);
		if (SlotIsCurrent(s, model))
			return;

		if (!s)
		{
			if (g_modelCount >= kModelSlots * 3 / 4)
				ClearModelCache();
			const unsigned int h = HashPtr(model);
			for (int i = 0; i < kModelSlots; ++i)
			{
				ModelSlot& c = g_modelSlots[(h + i) & (kModelSlots - 1)];
				if (!c.model)
				{
					s = &c;
					++g_modelCount;
					break;
				}
			}
			if (!s)
				return;
		}

		s->model = model;
		s->name = model->name;
		s->numBones = (unsigned char)model->numBones;
		ScanModel(model, s->rig, s->rigCount, s->body);
	}

	SlingState* AcquireState(const DObj_s* obj, int modelIndex)
	{
		const DWORD now = GetTickCount();
		AcquireSRWLockExclusive(&g_stateLock);

		const unsigned int h = HashPtr(obj) + (unsigned int)modelIndex;
		SlingState* freeSlot = nullptr;
		for (int i = 0; i < kStateSlots; ++i)
		{
			SlingState& s = g_states[(h + i) & (kStateSlots - 1)];
			if (s.obj == obj && s.modelIndex == modelIndex)
			{
				s.touchMs = now;
				ReleaseSRWLockExclusive(&g_stateLock);
				return &s;
			}
			if (!freeSlot && (!s.obj || now - s.touchMs > kStaleMs))
				freeSlot = &s;
		}

		if (freeSlot)
		{
			freeSlot->obj = obj;
			freeSlot->modelIndex = modelIndex;
			freeSlot->model = nullptr;
			freeSlot->valid = false;
			freeSlot->touchMs = now;
		}
		ReleaseSRWLockExclusive(&g_stateLock);
		return freeSlot;
	}

	int BuildCapsules(const DObj_s* obj, const BodyBones* body, const float* viewOffset, Capsule* out)
	{
		if (!body)
			return 0;
		const DObjAnimMat* mats = obj->skel.mat;
		const int* skelBits = obj->skel.partBits.skel;
		int n = 0;
		for (int c = 0; c < kNumCapsules; ++c)
		{
			const int ia = body->bone[kCapsules[c].a];
			const int ib = body->bone[kCapsules[c].b];
			if (ia < 0 || ib < 0 || !TestBit(skelBits, ia) || !TestBit(skelBits, ib))
				continue;
			Add3(mats[ia].trans, viewOffset, out[n].a);
			Add3(mats[ib].trans, viewOffset, out[n].b);
			out[n].radius = kCapsules[c].radius + kSlingThickness;
			++n;
		}
		return n;
	}

	void ClosestOnSegment(const float* a, const float* b, const float* p, float* out)
	{
		float ab[3], ap[3];
		Sub3(b, a, ab);
		Sub3(p, a, ap);
		const float len = Dot3(ab, ab);
		float t = len > 1e-6f ? Dot3(ap, ab) / len : 0.0f;
		t = t < 0.0f ? 0.0f : (t > 1.0f ? 1.0f : t);
		for (int c = 0; c < 3; ++c)
			out[c] = a[c] + ab[c] * t;
	}

	// Parameter on p1q1 of the closest points of segments p1q1 and p2q2 (Ericson, RTCD 5.1.9).
	float ClosestSegSeg(const float* p1, const float* q1, const float* p2, const float* q2)
	{
		float d1[3], d2[3], r[3];
		Sub3(q1, p1, d1);
		Sub3(q2, p2, d2);
		Sub3(p1, p2, r);
		const float a = Dot3(d1, d1);
		const float e = Dot3(d2, d2);
		if (a <= 1e-8f)
			return 0.0f;
		const float c = Dot3(d1, r);
		const auto clamp01 = [](float x) { return x < 0.0f ? 0.0f : (x > 1.0f ? 1.0f : x); };
		if (e <= 1e-8f)
			return clamp01(-c / a);
		const float f = Dot3(d2, r);
		const float b = Dot3(d1, d2);
		const float denom = a * e - b * b;
		float s = denom > 1e-8f ? clamp01((b * f - c * e) / denom) : 0.0f;
		const float t = (b * s + f) / e;
		if (t < 0.0f)
			s = clamp01(-c / a);
		else if (t > 1.0f)
			s = clamp01((b - c) / a);
		return s;
	}

	// p out of capsule c. Deeper than kDeepPen, out on the side prevP was on relative to the capsule's
	// previous pose cp: an arm sweeping through the sling must not leave it wrapped around.
	bool CapsulePush(const Capsule& c, const Capsule* cp, const float* p, const float* prevP, float* push)
	{
		float closest[3], d[3];
		ClosestOnSegment(c.a, c.b, p, closest);
		Sub3(p, closest, d);
		const float distSq = Dot3(d, d);
		if (distSq >= c.radius * c.radius)
			return false;
		const float dist = sqrtf(distSq);
		float n[3];
		bool haveN = false;
		if (c.radius - dist > kDeepPen && cp && prevP && DistSq3(p, prevP) < kHistoryJump * kHistoryJump)
		{
			float pc[3], pd[3];
			ClosestOnSegment(cp->a, cp->b, prevP, pc);
			Sub3(prevP, pc, pd);
			const float pdSq = Dot3(pd, pd);
			if (pdSq >= cp->radius * cp->radius)
			{
				const float inv = 1.0f / sqrtf(pdSq);
				n[0] = pd[0] * inv; n[1] = pd[1] * inv; n[2] = pd[2] * inv;
				haveN = true;
			}
		}
		if (!haveN)
		{
			if (dist < 1e-4f)
				return false;
			n[0] = d[0] / dist; n[1] = d[1] / dist; n[2] = d[2] / dist;
		}
		for (int i = 0; i < 3; ++i)
			push[i] = closest[i] + n[i] * c.radius - p[i];
		return true;
	}

	// Segments against the carrier's capsules: the closest point of each segment to the capsule axis,
	// pushed out and split over the free ends. prevW = positions before this step (world).
	void ApplyCapsules(SlingState& st, const SlingRig& rig, const Capsule* caps, int numCaps, const Capsule* prevCaps,
		const float (*prevW)[3], float (*acc)[3])
	{
		const int last = rig.count - 1;
		for (int c = 0; c < numCaps; ++c)
		{
			for (int k = 0; k < last; ++k)
			{
				const float t = ClosestSegSeg(st.pos[k], st.pos[k + 1], caps[c].a, caps[c].b);
				if ((k == 0 && t * rig.restLen[k] < kPinClear) || (k + 1 == last && !rig.freeEnd && (1.0f - t) * rig.restLen[k] < kPinClear))
					continue;
				float p[3], pp[3], push[3];
				for (int i = 0; i < 3; ++i)
				{
					p[i] = st.pos[k][i] + (st.pos[k + 1][i] - st.pos[k][i]) * t;
					pp[i] = prevW ? prevW[k][i] + (prevW[k + 1][i] - prevW[k][i]) * t : 0.0f;
				}
				if (!CapsulePush(caps[c], prevCaps ? &prevCaps[c] : nullptr, p, prevW ? pp : nullptr, push))
					continue;
				const float wa = k > 0 ? 1.0f - t : 0.0f;
				const float wb = k + 1 < last || rig.freeEnd ? t : 0.0f;
				const float den = wa * wa + wb * wb;
				if (den < 1e-6f)
					continue;
				for (int i = 0; i < 3; ++i)
				{
					st.pos[k][i] += push[i] * wa / den;
					st.pos[k + 1][i] += push[i] * wb / den;
					acc[k][i] += push[i] * wa / den;
					acc[k + 1][i] += push[i] * wb / den;
				}
			}
		}
	}

	// Keeps the simulated bones and the segments (probed every kSegmentProbe) out of the weapon; a
	// segment push is split over its free ends. Pushes are summed in acc for the contact response.
	// aq/at = anchor, world; prevL = positions before this step in the anchor space of that step.
	void ApplyCollision(SlingState& st, const SlingRig& rig, const SlingGrid& grid, const float* aq, const float* at,
		const float (*prevL)[3], float (*acc)[3])
	{
		const int last = rig.count - 1;
		const float iq[4] = { -aq[0], -aq[1], -aq[2], aq[3] };
		float d[3], l[3], pl[3], pw[3];

		for (int k = 1; k < last; ++k)
		{
			Sub3(st.pos[k], at, d);
			QuatRotate(iq, d, l);
			if (!GridPush(grid, l, prevL ? prevL[k] : nullptr, pl))
				continue;
			QuatRotate(aq, pl, pw);
			Add3(st.pos[k], pw, st.pos[k]);
			Add3(acc[k], pw, acc[k]);
		}

		for (int k = 0; k < last; ++k)
		{
			const int m = (int)ceilf(rig.restLen[k] / kSegmentProbe);
			for (int j = 1; j < m; ++j)
			{
				const float t = (float)j / m;
				if ((k == 0 && t * rig.restLen[k] < kPinClear) || (k + 1 == last && !rig.freeEnd && (1.0f - t) * rig.restLen[k] < kPinClear))
					continue;
				float p[3], pp[3];
				for (int c = 0; c < 3; ++c)
				{
					p[c] = st.pos[k][c] + (st.pos[k + 1][c] - st.pos[k][c]) * t;
					pp[c] = prevL ? prevL[k][c] + (prevL[k + 1][c] - prevL[k][c]) * t : 0.0f;
				}
				Sub3(p, at, d);
				QuatRotate(iq, d, l);
				if (!GridPush(grid, l, prevL ? pp : nullptr, pl))
					continue;
				QuatRotate(aq, pl, pw);
				const float wa = k > 0 ? 1.0f - t : 0.0f;
				const float wb = k + 1 < last || rig.freeEnd ? t : 0.0f;
				const float den = wa * wa + wb * wb;
				if (den < 1e-6f)
					continue;
				for (int c = 0; c < 3; ++c)
				{
					st.pos[k][c] += pw[c] * wa / den;
					st.pos[k + 1][c] += pw[c] * wb / den;
					acc[k][c] += pw[c] * wa / den;
					acc[k + 1][c] += pw[c] * wb / den;
				}
			}
		}
	}

	// A push must not turn into a bounce: drop the velocity along it. Sliding is slowed by at most
	// kFriction x the push (Coulomb, the same at any frame rate), so the sling still slides off a slope.
	void ContactResponse(SlingState& st, const SlingRig& rig, const float (*acc)[3])
	{
		for (int k = 1; k < (rig.freeEnd ? rig.count : rig.count - 1); ++k)
		{
			const float len = sqrtf(Dot3(acc[k], acc[k]));
			if (len < 1e-6f)
				continue;
			const float n[3] = { acc[k][0] / len, acc[k][1] / len, acc[k][2] / len };
			float v[3], vt[3];
			Sub3(st.pos[k], st.prev[k], v);
			const float vn = Dot3(v, n);
			for (int c = 0; c < 3; ++c)
				vt[c] = v[c] - vn * n[c];
			const float vtLen = sqrtf(Dot3(vt, vt));
			const float keepT = vtLen > 1e-6f && vtLen > kFriction * len ? 1.0f - kFriction * len / vtLen : 0.0f;
			for (int c = 0; c < 3; ++c)
				st.prev[k][c] = st.pos[k][c] - vt[c] * keepT;
		}
	}

	void SetHistory(SlingState& st, const float* aq, const float* at, const Capsule* caps, int numCaps)
	{
		memcpy(st.anchorPrevQ, aq, sizeof(st.anchorPrevQ));
		Copy3(st.anchorPrevT, at);
		st.prevNumCaps = caps ? numCaps : -1;
		if (caps && numCaps > 0)
			memcpy(st.prevCaps, caps, sizeof(Capsule) * numCaps);
		st.history = true;
	}


	void ResetState(SlingState& st, const SlingRig& rig, const float (*rw)[3])
	{
		for (int k = 0; k < rig.count; ++k)
		{
			Copy3(st.pos[k], rw[k]);
			Copy3(st.prev[k], rw[k]);
			Copy3(st.rigidPrev[k], rw[k]);
		}
		st.hPrev = 0.0f;
		st.history = false;
		st.valid = true;
	}

	void StepState(SlingState& st, const SlingRig& rig, const float (*rw)[3], int dtMs, const Capsule* caps, int numCaps,
		const SlingGrid* grid, const float* anchorQ, const float* anchorT)
	{
		const int last = rig.count - 1;
		const int freeTo = rig.freeEnd ? rig.count : last;
		int sub = 0;
		if (dtMs > 0)
		{
			sub = (int)ceilf(dtMs / kStepMs);
			sub = sub < 1 ? 1 : (sub > kMaxSubsteps ? kMaxSubsteps : sub);
		}
		const float h = sub ? (dtMs * 0.001f) / sub : 0.0f;
		// Damping and stiffness are tuned per 1/60 s step; rescale them so the sling behaves the
		// same at any frame rate (per-frame values made it rigid at 250 fps).
		const float ratio = h / (kStepMs * 0.001f);
		const float drop = -(sling_gravity ? sling_gravity->current.value : 800.0f) * h * h;
		const float keep = powf(1.0f - (sling_damping ? sling_damping->current.value : 0.04f), ratio);
		float stiff = (sling_stiffness ? sling_stiffness->current.value : 0.025f) * ratio * ratio;
		stiff = stiff > 1.0f ? 1.0f : stiff;
		float vscale = st.hPrev > 0.0f && h > 0.0f ? h / st.hPrev : 1.0f;
		vscale = vscale < 0.25f ? 0.25f : (vscale > 4.0f ? 4.0f : vscale);
		int iters = sling_iterations ? sling_iterations->current.integer : 4;
		iters = iters < 1 ? 1 : (iters > 16 ? 16 : iters);

		const int passes = sub ? sub : 1;
		for (int s = 1; s <= passes; ++s)
		{
			const float alpha = (float)s / passes;
			float target[kMaxSlingBones][3];
			for (int k = 0; k < rig.count; ++k)
				for (int c = 0; c < 3; ++c)
					target[k][c] = st.rigidPrev[k][c] + (rw[k][c] - st.rigidPrev[k][c]) * alpha;

			Copy3(st.prev[0], st.pos[0]);
			Copy3(st.pos[0], target[0]);
			if (!rig.freeEnd)
			{
				Copy3(st.prev[last], st.pos[last]);
				Copy3(st.pos[last], target[last]);
			}

			if (sub)
			{
				for (int k = 1; k < freeTo; ++k)
				{
					float* p = st.pos[k];
					float v[3];
					Sub3(p, st.prev[k], v);
					Copy3(st.prev[k], p);
					p[0] += v[0] * keep * vscale;
					p[1] += v[1] * keep * vscale;
					p[2] += v[2] * keep * vscale + drop;
					p[0] += (target[k][0] - p[0]) * stiff;
					p[1] += (target[k][1] - p[1]) * stiff;
					p[2] += (target[k][2] - p[2]) * stiff;
				}
			}

			// Where each bone was before this step, relative to the weapon of that step.
			const bool hist = st.history;
			float prevL[kMaxSlingBones][3];
			if (hist && grid)
			{
				const float iq[4] = { -st.anchorPrevQ[0], -st.anchorPrevQ[1], -st.anchorPrevQ[2], st.anchorPrevQ[3] };
				for (int k = 0; k < rig.count; ++k)
				{
					float d[3];
					Sub3(st.prev[k], st.anchorPrevT, d);
					QuatRotate(iq, d, prevL[k]);
				}
			}
			const Capsule* prevCaps = hist && st.prevNumCaps == numCaps ? st.prevCaps : nullptr;

			float acc[kMaxSlingBones][3] = {};
			for (int it = 0; it < iters; ++it)
			{
				for (int k = 0; k < last; ++k)
				{
					float* a = st.pos[k];
					float* b = st.pos[k + 1];
					float d[3];
					Sub3(b, a, d);
					const float len = sqrtf(Dot3(d, d));
					const float wa = k == 0 ? 0.0f : 1.0f;
					const float wb = k + 1 == last && !rig.freeEnd ? 0.0f : 1.0f;
					if (len < 1e-5f || wa + wb == 0.0f)
						continue;
					const float corr = (len - rig.restLen[k]) / (len * (wa + wb));
					for (int c = 0; c < 3; ++c)
					{
						a[c] += d[c] * corr * wa;
						b[c] -= d[c] * corr * wb;
					}
				}
				// Weapon last: the sling may end up in a sleeve, never through the gun.
				if (numCaps)
					ApplyCapsules(st, rig, caps, numCaps, prevCaps, hist ? (const float (*)[3])st.prev : nullptr, acc);
				if (grid)
					ApplyCollision(st, rig, *grid, anchorQ, anchorT, hist ? (const float (*)[3])prevL : nullptr, acc);
			}
			ContactResponse(st, rig, acc);
			vscale = 1.0f;
		}

		if (sub)
			st.hPrev = h;
		for (int k = 0; k < rig.count; ++k)
			Copy3(st.rigidPrev[k], rw[k]);
	}

	void CheckAgainstEngine(SlingRig& rig, const DObjAnimMat* mats, int boneOffset, const int* newBits,
		const float (*rq)[4], const float (*rt)[3])
	{
		for (int k = 0; k < rig.count; ++k)
		{
			const int b = boneOffset + rig.bone[k];
			if (!TestBit(newBits, b))
				break;
			float eq[4] = { mats[b].quat[0], mats[b].quat[1], mats[b].quat[2], mats[b].quat[3] };
			QuatNormalize(eq);
			float dot = fabsf(eq[0] * rq[k][0] + eq[1] * rq[k][1] + eq[2] * rq[k][2] + eq[3] * rq[k][3]);
			dot = dot > 1.0f ? 1.0f : dot;
			const float ang = 2.0f * acosf(dot) * 57.29578f;
			const float pos = sqrtf(DistSq3(mats[b].trans, rt[k]));
			if (pos > rig.maxPosErr) rig.maxPosErr = pos;
			if (ang > rig.maxAngErr) rig.maxAngErr = ang;
			InterlockedIncrement(&rig.checks);
		}
	}

	// stateKey = model index | rig of that model << 8.
	void ProcessRig(SlingRig& rig, DObj_s* obj, int stateKey, int boneOffset, const int* newBits, const BodyBones* body)
	{
		DObjAnimMat* mats = obj->skel.mat;
		const int* skelBits = obj->skel.partBits.skel;
		const int last = rig.count - 1;

		bool anyNew = false;
		for (int k = 0; k < rig.count && !anyNew; ++k)
			anyNew = TestBit(newBits, boneOffset + rig.bone[k]);
		const int anchor = boneOffset + rig.anchor;
		if (!anyNew || !TestBit(skelBits, anchor))
			return;

		// Rigid chain, composed like CalcSkelNonRootBones from the engine-computed anchor.
		float rq[kMaxSlingBones][4], rt[kMaxSlingBones][3];
		float pq[4] = { mats[anchor].quat[0], mats[anchor].quat[1], mats[anchor].quat[2], mats[anchor].quat[3] };
		float pt[3];
		QuatNormalize(pq);
		Copy3(pt, mats[anchor].trans);
		float anchorQ[4], anchorT[3];
		memcpy(anchorQ, pq, sizeof(anchorQ));
		for (int k = 0; k < rig.count; ++k)
		{
			float t[3];
			QuatMul(pq, rig.localQuat[k], rq[k]);
			QuatNormalize(rq[k]);
			QuatRotate(pq, rig.localTrans[k], t);
			Add3(pt, t, rt[k]);
			memcpy(pq, rq[k], sizeof(pq));
			Copy3(pt, rt[k]);
		}

		CheckAgainstEngine(rig, mats, boneOffset, newBits, rq, rt);

		const float* viewOffset = cgArray->refdef.viewOffset;
		float rw[kMaxSlingBones][3];
		for (int k = 0; k < rig.count; ++k)
			Add3(rt[k], viewOffset, rw[k]);
		Add3(mats[anchor].trans, viewOffset, anchorT);

		SlingState* st = AcquireState(obj, stateKey);
		if (!st)
			return;

		const int now = cgArray->time;
		const int stamp = obj->skel.timeStamp;
		const int dt = now - st->cgTime;
		const float resetDist = sling_resetDist ? sling_resetDist->current.value : 48.0f;

		if (!st->valid || st->model != rig.model || dt < 0 || dt > kResetDtMs
			|| DistSq3(rw[0], st->rigidPrev[0]) > resetDist * resetDist)
		{
			ResetState(*st, rig, rw);
			SetHistory(*st, anchorQ, anchorT, nullptr, 0);
			st->cgTime = now;
		}
		else if (st->skelStamp != stamp)
		{
			Capsule caps[kNumCapsules];
			const bool collide = sling_collide && sling_collide->current.enabled;
			const int numCaps = collide ? BuildCapsules(obj, body, viewOffset, caps) : 0;
			const SlingGrid* grid = collide && !rig.collReject ? &g_grids[&rig - g_rigs] : nullptr;
			StepState(*st, rig, rw, dt, caps, numCaps, grid, anchorQ, anchorT);
			st->cgTime = now;
			SetHistory(*st, anchorQ, anchorT, caps, numCaps);

			for (int k = 0; k < rig.count; ++k)
			{
				if (!Finite3(st->pos[k]) || !Finite3(st->prev[k]))
				{
					ResetState(*st, rig, rw);
					SetHistory(*st, anchorQ, anchorT, nullptr, 0);
					break;
				}
			}
		}
		st->model = rig.model;
		st->skelStamp = stamp;

		for (int k = 0; k < rig.count; ++k)
		{
			const int b = boneOffset + rig.bone[k];
			if (!TestBit(skelBits, b))
				continue;
			const int seg = k < last ? k : last - 1;
			float from[3], to[3], sw[4], q[4];
			Sub3(rt[seg + 1], rt[seg], from);
			Sub3(st->pos[seg + 1], st->pos[seg], to);
			Swing(from, to, sw);
			QuatMul(sw, rq[k], q);
			QuatNormalize(q);

			DObjAnimMat& m = mats[b];
			m.quat[0] = q[0]; m.quat[1] = q[1]; m.quat[2] = q[2]; m.quat[3] = q[3];
			Sub3(st->pos[k], viewOffset, m.trans);
			m.transWeight = 2.0f / (q[0] * q[0] + q[1] * q[1] + q[2] * q[2] + q[3] * q[3]);
		}
	}


}

namespace T4M
{
	// @new — under the DObj lock, after the last DObjCalcSkel of CG_DObjCalcPose / CG_DObjCalcBone
	// (PatchT4MAM_DObjPose.cpp). `before` = skel bits before the first one.
	void Sling_PostSkel(DObj_s* obj, const int* before)
	{
		if (!sling_enable || !sling_enable->current.enabled)
			return;

		int newBits[4];
		int any = 0;
		for (int i = 0; i < 4; ++i)
		{
			newBits[i] = obj->skel.partBits.skel[i] & ~before[i];
			any |= newBits[i];
		}
		const int numModels = obj->numModels;
		if (!any || !numModels || !obj->models)
			return;

		XModel** models = obj->models;

		bool missing = false;
		AcquireSRWLockShared(&g_modelLock);
		for (int m = 0; m < numModels && models[m] && !missing; ++m)
			missing = !SlotIsCurrent(LookupModel(models[m]), models[m]);
		ReleaseSRWLockShared(&g_modelLock);

		if (missing)
		{
			AcquireSRWLockExclusive(&g_modelLock);
			for (int m = 0; m < numModels; ++m)
				EnsureModel(models[m]);
			ReleaseSRWLockExclusive(&g_modelLock);
		}

		AcquireSRWLockShared(&g_modelLock);
		const ModelSlot* bodySlot = LookupModel(models[0]);
		const BodyBones* body = SlotIsCurrent(bodySlot, models[0]) && bodySlot->body >= 0 ? &g_bodies[bodySlot->body] : nullptr;

		int boneOffset = 0;
		for (int m = 0; m < numModels; ++m)
		{
			const XModel* model = models[m];
			if (!model)
				break;
			const ModelSlot* s = LookupModel(model);
			// Model 0 only for a character (helmet straps): a weapon alone in its DObj (dropped,
			// script_model) has no world collision, it keeps the modeled pose.
			if ((m > 0 || body) && SlotIsCurrent(s, model))
				for (int r = 0; r < s->rigCount; ++r)
					if (!g_rigs[s->rig + r].reject)
						ProcessRig(g_rigs[s->rig + r], obj, m | (r << 8), boneOffset, newBits, body);
			boneOffset += (unsigned char)model->numBones;
		}
		ReleaseSRWLockShared(&g_modelLock);
	}

	// @new — `sling_info`
	void Sling_Info_f()
	{
		std::vector<std::string> lines;
		char buf[256];

		AcquireSRWLockShared(&g_modelLock);
		sprintf_s(buf, "  %d model(s) cached, %d with j_sling_ bones\n", g_modelCount, g_rigCount);
		lines.emplace_back(buf);
		for (int i = 0; i < g_rigCount; ++i)
		{
			const SlingRig& r = g_rigs[i];
			if (r.reject)
			{
				sprintf_s(buf, "  %s: REJECTED (%s)\n", r.name, r.reject);
				lines.emplace_back(buf);
				continue;
			}
			float length = 0.0f;
			for (int k = 0; k + 1 < r.count; ++k)
				length += r.restLen[k];
			sprintf_s(buf, "  %s: %d bones on %s%s, length %.2f, rigid check %ld samples, max %.4f units / %.3f deg\n",
				r.name, r.count, r.anchorName, r.freeEnd ? " (free end)" : "", length, (long)r.checks, (float)r.maxPosErr, (float)r.maxAngErr);
			lines.emplace_back(buf);
			if (r.collReject)
				sprintf_s(buf, "    weapon collision: none (%s)\n", r.collReject);
			else
				sprintf_s(buf, "    weapon collision: %d LOD0 triangles, %d cells of %.2f, %d carved for the sling at rest, %d rest points still inside\n",
					r.collTris, r.collCells, kGridCell, r.collCarved, r.collRestHits);
			lines.emplace_back(buf);
		}

		const DWORD now = GetTickCount();
		int active = 0;
		AcquireSRWLockExclusive(&g_stateLock);
		for (int i = 0; i < kStateSlots; ++i)
		{
			const SlingState& s = g_states[i];
			if (!s.obj || now - s.touchMs > kStaleMs)
				continue;
			++active;
			const char* name = "?";
			for (int r = 0; r < g_rigCount; ++r)
				if (g_rigs[r].model == s.model)
					name = g_rigs[r].name;
			sprintf_s(buf, "  active: entity %d, model %d (%s), dobj %d bones, %d body capsules\n",
				(int)s.obj->entnum - 1, s.modelIndex, name, (int)s.obj->numBones, s.prevNumCaps);
			lines.emplace_back(buf);
		}
		ReleaseSRWLockExclusive(&g_stateLock);
		ReleaseSRWLockShared(&g_modelLock);

		T4::engine::Com_Printf(0, "[T4M] sling_enable %d\n", sling_enable && sling_enable->current.enabled ? 1 : 0);
		for (const std::string& line : lines)
			T4::engine::Com_Printf(0, "%s", line.c_str());
		T4::engine::Com_Printf(0, "  %d active sling(s)\n", active);
	}
}
