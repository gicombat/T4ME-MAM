// ==========================================================
// T4M project
//
// Component: clientdll
// Purpose: Left-hand IK of actors: when the weapon in tag_weapon_right carries tag_ik_hand_left,
//          the animated wrist is moved by the reference -> tag offset (weighted by how firmly
//          the anim holds the gun), the elbow hinge and shoulder solve for it. Called from the
//          DObj pose reconstructions (PatchT4MAM_DObjPose.cpp). Plan: plans/plan_ai_left_hand_ik.md
//          Reference implementation: tools/ik_hand_preview.py (left_hand_ik).
// ==========================================================

#include "StdInc.h"
#include "T4.h"
#include <intrin.h>
#include <cmath>
#include <cstring>
#include <string>
#include <vector>

using namespace T4::engine;

namespace
{
	constexpr int     kModelSlots = 1024;
	constexpr int     kMaxGuns    = 64;
	constexpr unsigned char kEtActor  = 16;   // CG_DoControllers (0x457080): cases 16 / 18 -> actor controller
	constexpr unsigned char kNoParent = 255;  // modelParents[] of a model with no parent bone

	// Wrist of exposed_aim_5 frame 0 in tag_weapon_right space (plan section 3.2).
	const float kRefPos[3]  = { -3.838f, 2.096f, -2.079f };
	const float kRefQuat[4] = { 0.6437f, -0.4216f, -0.2312f, 0.5953f };

	// Tag convention: the wrist is the tag turned -90 deg about its own X (found in game on the Grease
	// Gun tag made in Blender; tools/xmodel_ik_tag.py writes its tags the same way).
	constexpr float kTagRollX = -90.0f;

	struct ModelSlot
	{
		const XModel* model;
		const char*   name;
		int           numBones;
		short         tag;          // tag_ik_hand_left, -1 none
		short         gun;          // g_guns index of a model with the tag
		short         weaponRight;  // tag_weapon_right
		short         shoulder, elbow, wrist;
	};

	struct GunStats
	{
		char           name[64];
		bool           trailingSpace;   // bone named "tag_ik_hand_left " (accepted, to be fixed)
		volatile LONG  applied;
		volatile LONG  weightMilli;
		volatile float maxResidual;
	};

	SRWLOCK   g_lock = SRWLOCK_INIT;
	ModelSlot g_slots[kModelSlots];
	int       g_slotCount;
	GunStats  g_guns[kMaxGuns];
	int       g_gunCount;

	inline bool TestBit(const int* bits, int index)
	{
		return (bits[index >> 5] & (0x80000000u >> (index & 31))) != 0;
	}

	inline void Copy3(float* d, const float* s) { d[0] = s[0]; d[1] = s[1]; d[2] = s[2]; }
	inline void Sub3(const float* a, const float* b, float* o) { o[0] = a[0] - b[0]; o[1] = a[1] - b[1]; o[2] = a[2] - b[2]; }
	inline void Add3(const float* a, const float* b, float* o) { o[0] = a[0] + b[0]; o[1] = a[1] + b[1]; o[2] = a[2] + b[2]; }
	inline float Dot3(const float* a, const float* b) { return a[0] * b[0] + a[1] * b[1] + a[2] * b[2]; }
	inline float Len3(const float* a) { return sqrtf(Dot3(a, a)); }

	inline void Cross3(const float* a, const float* b, float* o)
	{
		o[0] = a[1] * b[2] - a[2] * b[1];
		o[1] = a[2] * b[0] - a[0] * b[2];
		o[2] = a[0] * b[1] - a[1] * b[0];
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

	inline void QuatConj(const float* q, float* o) { o[0] = -q[0]; o[1] = -q[1]; o[2] = -q[2]; o[3] = q[3]; }

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

	void QuatAxisAngle(const float* axis, float angle, float* q)
	{
		const float len = Len3(axis);
		const float s = len > 1e-9f ? sinf(angle * 0.5f) / len : 0.0f;
		q[0] = axis[0] * s; q[1] = axis[1] * s; q[2] = axis[2] * s;
		q[3] = cosf(angle * 0.5f);
	}

	// Shortest rotation taking direction `from` onto direction `to`.
	void Swing(const float* from, const float* to, float* q)
	{
		const float la = Len3(from);
		const float lb = Len3(to);
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
			float axis[3];
			Cross3(a, ref, axis);
			QuatAxisAngle(axis, 3.14159265f, q);
			return;
		}
		Cross3(a, b, q);
		q[3] = 1.0f + d;
		QuatNormalize(q);
	}

	// q^t: same axis, angle scaled by t (slerp from identity).
	void QuatPow(const float* qIn, float t, float* o)
	{
		float q[4] = { qIn[0], qIn[1], qIn[2], qIn[3] };
		QuatNormalize(q);
		if (q[3] < 0.0f)
		{
			q[0] = -q[0]; q[1] = -q[1]; q[2] = -q[2]; q[3] = -q[3];
		}
		const float s = sqrtf(1.0f - (q[3] > 1.0f ? 1.0f : q[3] * q[3]));
		if (s < 1e-7f)
		{
			o[0] = o[1] = o[2] = 0.0f;
			o[3] = 1.0f;
			return;
		}
		const float angle = 2.0f * acosf(q[3] > 1.0f ? 1.0f : q[3]);
		QuatAxisAngle(q, angle * t, o);
	}

	inline float Smooth(float t)
	{
		t = t < 0.0f ? 0.0f : (t > 1.0f ? 1.0f : t);
		return t * t * (3.0f - 2.0f * t);
	}

	inline float DvarFloat(const dvar_t* d, float def)
	{
		return d ? d->current.value : def;
	}

	inline unsigned int HashPtr(const void* p)
	{
		return (unsigned int)(((uintptr_t)p >> 4) * 2654435761u);
	}

	// Bone name equal to `want`, case ignored; trailing spaces accepted and reported.
	bool NameIs(const char* name, const char* want, bool& trailing)
	{
		const size_t n = strlen(want);
		if (_strnicmp(name, want, n))
			return false;
		const char* p = name + n;
		trailing = *p != 0;
		while (*p == ' ')
			++p;
		return *p == 0;
	}

	// Caller holds g_lock (shared or exclusive).
	const ModelSlot* Lookup(const XModel* model)
	{
		const unsigned int h = HashPtr(model);
		for (int i = 0; i < kModelSlots; ++i)
		{
			const ModelSlot& s = g_slots[(h + i) & (kModelSlots - 1)];
			if (!s.model)
				return nullptr;
			if (s.model == model)
				return s.name == model->name && s.numBones == (unsigned char)model->numBones ? &s : nullptr;
		}
		return nullptr;
	}

	// Caller holds g_lock exclusive.
	void Build(const XModel* model)
	{
		if (g_slotCount >= kModelSlots * 3 / 4)
		{
			memset(g_slots, 0, sizeof(g_slots));
			g_slotCount = 0;
		}
		const unsigned int h = HashPtr(model);
		ModelSlot* s = nullptr;
		for (int i = 0; i < kModelSlots && !s; ++i)
		{
			ModelSlot& c = g_slots[(h + i) & (kModelSlots - 1)];
			if (!c.model)
				++g_slotCount;
			if (!c.model || c.model == model)
				s = &c;
		}
		if (!s)
			return;

		s->model = model;
		s->name = model->name;
		s->numBones = (unsigned char)model->numBones;
		s->tag = s->gun = s->weaponRight = s->shoulder = s->elbow = s->wrist = -1;
		bool tagSpace = false;
		for (int i = 0; i < s->numBones; ++i)
		{
			const char* name = T4::engine::SL_ConvertToString(model->boneNames[i], SCRIPTINSTANCE_SERVER);
			if (!name)
				continue;
			bool trailing = false;
			if (NameIs(name, "tag_ik_hand_left", trailing))
			{
				s->tag = (short)i;
				tagSpace = trailing;
			}
			else if (!_stricmp(name, "tag_weapon_right"))
				s->weaponRight = (short)i;
			else if (!_stricmp(name, "j_shoulder_le"))
				s->shoulder = (short)i;
			else if (!_stricmp(name, "j_elbow_le"))
				s->elbow = (short)i;
			else if (!_stricmp(name, "j_wrist_le"))
				s->wrist = (short)i;
		}

		if (s->tag < 0)
			return;
		for (int g = 0; g < g_gunCount; ++g)
		{
			if (!strcmp(g_guns[g].name, model->name ? model->name : "?"))
			{
				s->gun = (short)g;
				return;
			}
		}
		if (g_gunCount < kMaxGuns)
		{
			GunStats& g = g_guns[g_gunCount];
			memset(&g, 0, sizeof(g));
			strncpy_s(g.name, model->name ? model->name : "?", _TRUNCATE);
			g.trailingSpace = tagSpace;
			s->gun = (short)g_gunCount++;
		}
	}

	void EnsureModels(XModel* const* models, int numModels)
	{
		bool missing = false;
		AcquireSRWLockShared(&g_lock);
		for (int m = 0; m < numModels && !missing; ++m)
			missing = models[m] && !Lookup(models[m]);
		ReleaseSRWLockShared(&g_lock);
		if (!missing)
			return;
		AcquireSRWLockExclusive(&g_lock);
		for (int m = 0; m < numModels; ++m)
			if (models[m] && !Lookup(models[m]))
				Build(models[m]);
		ReleaseSRWLockExclusive(&g_lock);
	}

	// World (minus viewOffset) rotation / position of a skel bone.
	void BonePose(const DObjAnimMat& m, float* q, float* t)
	{
		q[0] = m.quat[0]; q[1] = m.quat[1]; q[2] = m.quat[2]; q[3] = m.quat[3];
		QuatNormalize(q);
		Copy3(t, m.trans);
	}

	void WriteBone(DObjAnimMat& m, const float* q, const float* t)
	{
		m.quat[0] = q[0]; m.quat[1] = q[1]; m.quat[2] = q[2]; m.quat[3] = q[3];
		Copy3(m.trans, t);
		m.transWeight = 2.0f / (q[0] * q[0] + q[1] * q[1] + q[2] * q[2] + q[3] * q[3]);
	}
}

namespace T4M
{
	// @new — which bones the IK needs, or false (vanilla single DObjCalcSkel). Under the DObj lock.
	bool HandIK_Prepare(const cpose_t* pose, DObj_s* obj, const int* partBits, const int* before, HandIKJob* job)
	{
		if (!ik_hand_enable || !ik_hand_enable->current.enabled || !pose || pose->eType != kEtActor)
			return false;
		const int numModels = obj->numModels;
		XModel* const* models = obj->models;
		if (numModels < 2 || !models || !models[0])
			return false;
		const unsigned char* parents = reinterpret_cast<const unsigned char*>(models + numModels);

		EnsureModels(models, numModels);

		AcquireSRWLockShared(&g_lock);
		bool ok = false;
		const ModelSlot* body = Lookup(models[0]);
		if (body && body->shoulder >= 0 && body->elbow >= 0 && body->wrist >= 0
			&& TestBit(partBits, body->wrist)
			&& !TestBit(before, body->shoulder) && !TestBit(before, body->elbow) && !TestBit(before, body->wrist))
		{
			// The gun is the model hung on a tag_weapon_right (the body's, or a duplicate's).
			int rightBones[8], numRight = 0, offset = 0;
			for (int m = 0; m < numModels && numRight < 8; ++m)
			{
				const ModelSlot* s = models[m] ? Lookup(models[m]) : nullptr;
				if (s && s->weaponRight >= 0)
					rightBones[numRight++] = offset + s->weaponRight;
				offset += models[m] ? (unsigned char)models[m]->numBones : 0;
			}
			offset = (unsigned char)models[0]->numBones;
			for (int m = 1; m < numModels && !ok; ++m)
			{
				const ModelSlot* s = models[m] ? Lookup(models[m]) : nullptr;
				const int parent = parents[m];
				bool onRight = false;
				for (int r = 0; r < numRight; ++r)
					onRight |= parent == rightBones[r];
				if (s && s->tag >= 0 && parent != kNoParent && onRight && offset + s->tag < obj->numBones)
				{
					job->shoulder = body->shoulder;
					job->elbow = body->elbow;
					job->wrist = body->wrist;
					job->weaponParent = parent;
					job->tag = offset + s->tag;
					job->gun = s->gun;
					ok = true;
				}
				offset += models[m] ? (unsigned char)models[m]->numBones : 0;
			}
		}
		ReleaseSRWLockShared(&g_lock);
		if (!ok)
			return false;

		int tagBits[4];
		DObjGetHierarchyBits(obj, job->wrist, job->pass1);
		DObjGetHierarchyBits(obj, job->tag, tagBits);
		for (int i = 0; i < 4; ++i)
			job->pass1[i] |= tagBits[i];
		return true;
	}

	// @new — moves shoulder, elbow, wrist of the first DObjCalcSkel pass (tools/ik_hand_preview.py
	// left_hand_ik, in quaternions: row matrix M = R(q)^T).
	void HandIK_Solve(DObj_s* obj, const HandIKJob* job)
	{
		DObjAnimMat* mats = obj->skel.mat;
		float qT[4], pT[3], qW[4], pW[3], qS[4], pS[3], qE[4], pE[3], qG[4], pG[3];
		BonePose(mats[job->weaponParent], qT, pT);
		BonePose(mats[job->wrist], qW, pW);
		BonePose(mats[job->shoulder], qS, pS);
		BonePose(mats[job->elbow], qE, pE);
		BonePose(mats[job->tag], qG, pG);

		// Animated wrist and tag in tag_weapon_right space.
		float iqT[4], d[3], qWr[4], pWr[3], qGr[4], pGr[3];
		QuatConj(qT, iqT);
		Sub3(pW, pT, d);
		QuatRotate(iqT, d, pWr);
		QuatMul(iqT, qW, qWr);
		Sub3(pG, pT, d);
		QuatRotate(iqT, d, pGr);
		QuatMul(iqT, qG, qGr);

		// Tag convention, then in-game tuning: its own X, then Y, then Z.
		const float tagRot[3] = { kTagRollX + DvarFloat(ik_hand_tagRotX, 0.0f), DvarFloat(ik_hand_tagRotY, 0.0f), DvarFloat(ik_hand_tagRotZ, 0.0f) };
		for (int a = 0; a < 3; ++a)
		{
			if (tagRot[a] == 0.0f)
				continue;
			const float axis[3] = { a == 0 ? 1.0f : 0.0f, a == 1 ? 1.0f : 0.0f, a == 2 ? 1.0f : 0.0f };
			float r[4], n[4];
			QuatAxisAngle(axis, tagRot[a] * 0.0174532925f, r);
			QuatMul(qGr, r, n);
			memcpy(qGr, n, sizeof(n));
		}

		// Weight: how firmly the anim holds the gun (distance to its axis, position along it).
		const float rFull = DvarFloat(ik_hand_rFull, 5.0f), rZero = DvarFloat(ik_hand_rZero, 6.5f);
		const float xZero = DvarFloat(ik_hand_xZero, -10.0f), xFull = DvarFloat(ik_hand_xFull, -8.5f);
		const float r = sqrtf(pWr[1] * pWr[1] + pWr[2] * pWr[2]);
		const float w = Smooth((rZero - r) / (rZero - rFull)) * Smooth((pWr[0] - xZero) / (xFull - xZero));
		if (w <= 0.0f)
			return;

		// Target = reference -> tag offset applied to the animated wrist, scaled by the weight. How far
		// the anim holds the gun from the reference is kept up to ik_hand_slide only: many anims hold
		// it 5-9 units away, which puts the hand off a short gun.
		float qRef[4] = { kRefQuat[0], kRefQuat[1], kRefQuat[2], kRefQuat[3] };
		QuatNormalize(qRef);
		float iqRef[4], dFull[4], dq[4];
		QuatConj(qRef, iqRef);
		QuatMul(qGr, iqRef, dFull);
		QuatPow(dFull, w, dq);

		float rel[3], rot[3], goalR[3], goalP[3], goalQ[4], tmp[4];
		Sub3(pWr, kRefPos, rel);
		const float slide = DvarFloat(ik_hand_slide, 1.5f);
		const float relLen = Len3(rel);
		if (relLen > slide && relLen > 1e-6f)
		{
			const float k = 1.0f + (slide / relLen - 1.0f) * w;
			rel[0] *= k; rel[1] *= k; rel[2] *= k;
		}
		QuatRotate(dq, rel, rot);
		for (int c = 0; c < 3; ++c)
			goalR[c] = kRefPos[c] + rot[c] + w * (pGr[c] - kRefPos[c]);
		QuatRotate(qT, goalR, rot);
		Add3(pT, rot, goalP);
		QuatMul(dq, qWr, tmp);
		QuatMul(qT, tmp, goalQ);
		QuatNormalize(goalQ);

		// Elbow hinge about its world Z: smallest angle putting the wrist at |goal - shoulder|.
		const float zAxis[3] = { 0.0f, 0.0f, 1.0f };
		float axis[3], fore[3], toGoal[3];
		QuatRotate(qE, zAxis, axis);
		Sub3(pW, pE, fore);
		Sub3(goalP, pS, toGoal);
		const float want = Len3(toGoal);
		const auto reach = [&](float angle)
		{
			float q[4], v[3], p[3], s[3];
			QuatAxisAngle(axis, angle, q);
			QuatRotate(q, fore, v);
			Add3(pE, v, p);
			Sub3(p, pS, s);
			return Len3(s) - want;
		};
		const float step = 0.0174532925f;
		float best = 0.0f, bestAbs = FLT_MAX, closest = 0.0f, closestVal = FLT_MAX;
		bool found = false;
		float prevA = -170.0f * step, prevV = reach(prevA);
		for (int k = -169; k <= 170; ++k)
		{
			const float a = k * step;
			const float v = reach(a);
			if (fabsf(prevV) < closestVal)
			{
				closestVal = fabsf(prevV);
				closest = prevA;
			}
			if (prevV * v <= 0.0f)
			{
				float lo = prevA, hi = a, flo = prevV;
				for (int it = 0; it < 50; ++it)
				{
					const float mid = (lo + hi) * 0.5f;
					const float fm = reach(mid);
					if (flo * fm <= 0.0f)
						hi = mid;
					else
					{
						lo = mid;
						flo = fm;
					}
				}
				const float root = (lo + hi) * 0.5f;
				if (fabsf(root) < bestAbs)
				{
					bestAbs = fabsf(root);
					best = root;
					found = true;
				}
			}
			prevA = a;
			prevV = v;
		}
		if (fabsf(prevV) < closestVal)
			closest = prevA;
		if (!found)
			best = closest;

		// Bend, then turn the whole arm about the shoulder onto the goal.
		float hinge[4], foreBent[3], w1[3], sw[3], qQ[4];
		QuatAxisAngle(axis, best, hinge);
		QuatRotate(hinge, fore, foreBent);
		Add3(pE, foreBent, w1);
		Sub3(w1, pS, sw);
		Swing(sw, toGoal, qQ);

		float qS2[4], qE2[4], pE2[3], pW2[3], v[3];
		QuatMul(qQ, qS, qS2);
		QuatNormalize(qS2);
		QuatMul(hinge, qE, tmp);
		QuatMul(qQ, tmp, qE2);
		QuatNormalize(qE2);
		Sub3(pE, pS, d);
		QuatRotate(qQ, d, v);
		Add3(pS, v, pE2);
		QuatRotate(qQ, sw, v);
		Add3(pS, v, pW2);

		WriteBone(mats[job->shoulder], qS2, pS);
		WriteBone(mats[job->elbow], qE2, pE2);
		WriteBone(mats[job->wrist], goalQ, pW2);

		if (job->gun >= 0)
		{
			GunStats& g = g_guns[job->gun];
			InterlockedIncrement(&g.applied);
			InterlockedExchangeAdd(&g.weightMilli, (LONG)(w * 1000.0f));
			float res[3];
			Sub3(pW2, goalP, res);
			const float resLen = Len3(res);
			if (resLen > g.maxResidual)
				g.maxResidual = resLen;
		}
	}

	// @new — `ik_hand_info`
	void HandIK_Info_f()
	{
		std::vector<std::string> lines;
		char buf[256];
		AcquireSRWLockShared(&g_lock);
		sprintf_s(buf, "  %d model(s) cached, %d with tag_ik_hand_left\n", g_slotCount, g_gunCount);
		lines.emplace_back(buf);
		for (int i = 0; i < g_gunCount; ++i)
		{
			const GunStats& g = g_guns[i];
			const LONG n = g.applied;
			sprintf_s(buf, "  %s:%s %ld IK applied, mean weight %.2f, max residual %.4f units\n", g.name,
				g.trailingSpace ? " (bone name has a trailing space)" : "", (long)n,
				n ? g.weightMilli / 1000.0f / n : 0.0f, (float)g.maxResidual);
			lines.emplace_back(buf);
		}
		ReleaseSRWLockShared(&g_lock);

		T4::engine::Com_Printf(0, "[T4M] ik_hand_enable %d\n", ik_hand_enable && ik_hand_enable->current.enabled ? 1 : 0);
		for (const std::string& line : lines)
			T4::engine::Com_Printf(0, "%s", line.c_str());
	}
}
