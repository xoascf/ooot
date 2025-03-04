﻿#define INTERNAL_SRC_CODE_Z_LIB_C
#include "global.h"
#include "framerate.h"
#include "ichain.h"
#include "regs.h"
#include "z64actor.h"
#include "z64audio.h"
#include "z64game.h"
#include "z64math.h"
#include "def/audio_bank.h"
#include "def/coss.h"
#include "def/random.h"
#include "def/sins.h"
#include "def/sys_math_atan.h"
#include "def/z_lib.h"

void Lib_MemSet(u8* dest, size_t size, u8 val)
{
	u32 i;

	// clang-format off
    for (i = 0; i < size; i++) { *dest++ = val; }
	// clang-format on
}

f32 Math_CosS(s16 angle)
{
	return coss(angle) * SHT_MINV;
}

f32 Math_SinS(s16 angle)
{
	return sins(angle) * SHT_MINV;
}

f32 Math_AccurateCosS(s16 angle)
{
	return cosf(DEG_TO_RAD((f32)(angle & 0xFFFC) / SHT_MAX) * 180.0f);
}
f32 Math_AccurateSinS(s16 angle)
{
	return sinf(DEG_TO_RAD((f32)(angle & 0xFFFC) / SHT_MAX) * 180.0f);
}
/**
 * Changes pValue by step (scaled by the update rate) towards target, setting it equal when the target is reached.
 * Returns true when target is reached, false otherwise.
 */
template <class T> static inline s32 _Math_ScaledStepToS(T* pValue, s16 target, const FStep& _step)
{
	float step = _step.value();
	if(step != 0)
	{
		f32 updateScale = FRAMERATE_ANIM_SCALER;

		if((s16)(*pValue - target) > 0)
		{
			step = -step;
		}

		*pValue = *pValue + (s16)(step * updateScale);

		if(((s16)(*pValue - target) * step) >= 0)
		{
			*pValue = target;
			return true;
		}
	}
	else if(target == *pValue)
	{
		return true;
	}

	return false;
}

s32 Math_ScaledStepToS(s16* pValue, s16 target, const FStep& _step)
{
	return _Math_ScaledStepToS(pValue, target, _step);
}

s32 Math_ScaledStepToS(Rotation* pValue, s16 target, const FStep& _step)
{
	return _Math_ScaledStepToS(pValue, target, _step);
}

/**
 * Changes pValue by step towards target, setting it equal when the target is reached.
 * Returns true when target is reached, false otherwise.
 */
template <class T> static inline s32 _Math_StepToS(T* pValue, s16 target, const Step& _step)
{
	float step = _step.value();
	if(step != 0)
	{
		if(target < *pValue)
		{
			step = -step;
		}

		*pValue = *pValue + step;

		if(((*pValue - target) * step) >= 0)
		{
			*pValue = target;
			return true;
		}
	}
	else if(target == *pValue)
	{
		return true;
	}

	return false;
}

s32 Math_StepToS(s16* pValue, s16 target, const Step& _step)
{
	return _Math_StepToS(pValue, target, _step);
}

s32 Math_StepToS(Rotation* pValue, s16 target, const FStep& _step)
{
	return _Math_StepToS(pValue, target, _step);
}

/**
 * Changes pValue by step towards target, setting it equal when the target is reached.
 * Returns true when target is reached, false otherwise.
 */
template<class T>
static inline s32 _Math_StepToF(T* pValue, f32 target, const Step& _step)
{
	float step = _step.value();
	if(step != 0.0f)
	{
		if(target < *pValue)
		{
			step = -step;
		}

		*pValue += step;

		if(((*pValue - target) * step) >= 0)
		{
			*pValue = target;
			return true;
		}
	}
	else if(target == *pValue)
	{
		return true;
	}

	return false;
}

s32 Math_StepToF(f32* pValue, f32 target, const Step& _step)
{
	return _Math_StepToF(pValue, target, _step);
}

s32 Math_StepToF(CounterF* pValue, f32 target, const Step& _step)
{
	return _Math_StepToF(pValue, target, _step);
}

f32 Math_NormalizeAngleF(f32 angle)
{
	while(angle < 0.0f)
	{
		angle += 360.0f;
	}

	while(angle >= 360.0f)
	{
		angle -= 360.0f;
	}

	return angle;
}

f32 Math_AngleF(Vec2f v)
{
	if(v.x == 0)
	{
		return (v.y > 0) ? 90 : (v.y == 0) ? 0 : 270;
	}
	else if(v.y == 0)
	{
		return (v.x >= 0) ? 0 : 180;
	}

	int ret = RADTODEG(atan2f(v.y, v.x));

	if(v.x < 0 && v.y < 0)
	{
		ret = 180 + ret;
	}
	else if(v.x < 0)
	{
		ret = 180 + ret;
	}
	else if(v.y < 0)
	{
		ret = 270 + (90 + ret);
	}
	return ret;
}

f32 Math_AngleDiffF(f32 a, f32 b)
{
	const float angleDelta = Math_NormalizeAngleF(a - b);

	if(angleDelta > 180.0f)
	{
		return 360.0f - angleDelta;
	}

	return angleDelta;
}

s32 Math_StepRotationToF(f32* pValue, f32 target, const Step& _step)
{
	float step = _step.value();
	if(step != 0.0f)
	{
		const float angleDelta = target - *pValue;

		if(angleDelta > 180.0f) // value > target
		{
			step = -step;

			if(target < *pValue)
			{
				step = -step;
			}
		}
		else if(angleDelta < -180.0f) // target > value
		{
			step = step;
		}
		else
		{
			if(target < *pValue)
			{
				step = -step;
			}
		}

		*pValue = Math_NormalizeAngleF(*pValue + step);

		if(fabsf(*pValue - target) < fabs(step))
		{
			*pValue = target;
			return true;
		}
	}
	else if(target == *pValue)
	{
		return true;
	}

	return false;
}

/**
 *  Changes pValue by step. If pvalue reaches limit angle or its opposite, sets it equal to limit angle.
 * Returns true when limit angle or its opposite is reached, false otherwise.
 */
s32 Math_StepUntilAngleS(s16* pValue, s16 limit, const Step& step)
{
	s16 orig = *pValue;

	*pValue += step.value();

	if(((s16)(*pValue - limit) * (s16)(orig - limit)) <= 0)
	{
		*pValue = limit;
		return true;
	}

	return false;
}

/**
 * Changes pValue by step. If pvalue reaches limit, sets it equal to limit.
 * Returns true when limit is reached, false otherwise.
 */
template <class T> static inline s32 _Math_StepUntilS(T* pValue, s16 limit, const Step& step)
{
	s16 orig = *pValue;

	*pValue = *pValue + step.value();

	if(((*pValue - limit) * (orig - limit)) <= 0)
	{
		*pValue = limit;
		return true;
	}

	return false;
}

s32 Math_StepUntilS(s16* pValue, s16 limit, const Step& step)
{
	return _Math_StepUntilS(pValue, limit, step);
}

s32 Math_StepUntilS(Rotation* pValue, s16 limit, const FStep& step)
{
	return _Math_StepUntilS(pValue, limit, step);
}

/**
 * Changes pValue by step towards target angle, setting it equal when the target is reached.
 * Returns true when target is reached, false otherwise.
 */
template <class T> static inline s32 _Math_StepToAngleS(T* pValue, s16 target, const Step& _step)
{
	float step = _step.value();
	s32 diff = target - *pValue;

	if(diff < 0)
	{
		step = -step;
	}

	if(diff >= 0x8000)
	{
		step = -step;
		diff = -0xFFFF - -diff;
	}
	else if(diff <= -0x8000)
	{
		diff += 0xFFFF;
		step = -step;
	}

	if(step != 0)
	{
		*pValue += step;

		if((diff * step) <= 0)
		{
			*pValue = target;
			return true;
		}
	}
	else if(target == *pValue)
	{
		return true;
	}

	return false;
}

s32 Math_StepToAngleS(s16* pValue, s16 target, const Step& _step)
{
	return _Math_StepToAngleS(pValue, target, _step);
}

s32 Math_StepToAngleS(Rotation* pValue, s16 target, const FStep& _step)
{
	return _Math_StepToAngleS(pValue, target, _step);
}

/**
 * Changes pValue by step. If pvalue reaches limit, sets it equal to limit.
 * Returns true when limit is reached, false otherwise.
 */
s32 Math_StepUntilF(f32* pValue, f32 limit, const Step& step)
{
	f32 orig = *pValue;

	*pValue += step.value();

	if(((*pValue - limit) * (orig - limit)) <= 0)
	{
		*pValue = limit;
		return true;
	}

	return false;
}

/**
 * Changes pValue toward target by incrStep if pValue is smaller and by decrStep if it is greater, setting it equal when
 * target is reached. Returns true when target is reached, false otherwise.
 */
s32 Math_AsymStepToF(f32* pValue, f32 target, const Step& incrStep, const Step& decrStep)
{
	f32 step = (target >= *pValue) ? incrStep.value() : decrStep.value();

	if(step != 0.0f)
	{
		if(target < *pValue)
		{
			step = -step;
		}

		*pValue += step;

		if(((*pValue - target) * step) >= 0)
		{
			*pValue = target;
			return 1;
		}
	}
	else if(target == *pValue)
	{
		return 1;
	}

	return 0;
}

void func_80077D10(f32* magnitude, s16* arg1, Input* input)
{
	f32 relX = input->rel.stick_x;
	f32 relY = input->rel.stick_y;

	*magnitude = sqrtf(SQ(relX) + SQ(relY));
	*magnitude = (60.0f < *magnitude) ? 60.0f : *magnitude;

	*arg1 = Math_Atan2S(relY, -relX);
}

s16 Rand_S16Offset(s16 base, s16 range)
{
	return (s16)(Rand_ZeroOne() * range) + base;
}

s16 Rand_S16OffsetStride(s16 base, s16 stride, s16 range)
{
	return (s16)(Rand_ZeroOne() * range) * stride + base;
}

void Math_Vec3f_Copy(Vec3f* dest, Vec3f* src)
{
	dest->x = src->x;
	dest->y = src->y;
	dest->z = src->z;
}

void Math_VecPos_Copy(VecPos* dest, Vec3f* src)
{
	dest->x = src->x;
	dest->y = src->y;
	dest->z = src->z;
}

void Math_Vec3s_ToVec3f(Vec3f* dest, Vec3s* src)
{
	dest->x = src->x;
	dest->y = src->y;
	dest->z = src->z;
}

void Math_Vec3f_Sum(Vec3f* a, Vec3f* b, Vec3f* dest)
{
	dest->x = a->x + b->x;
	dest->y = a->y + b->y;
	dest->z = a->z + b->z;
}

void Math_Vec3f_Diff(Vec3f* a, Vec3f* b, Vec3f* dest)
{
	dest->x = a->x - b->x;
	dest->y = a->y - b->y;
	dest->z = a->z - b->z;
}

void Math_Vec3s_DiffToVec3f(Vec3f* dest, Vec3s* a, Vec3s* b)
{
	dest->x = a->x - b->x;
	dest->y = a->y - b->y;
	dest->z = a->z - b->z;
}

void Math_Vec3f_Scale(Vec3f* vec, f32 scaleF)
{
	vec->x *= scaleF;
	vec->y *= scaleF;
	vec->z *= scaleF;
}

f32 Math_Vec3f_DistXYZ(Vec3f* a, Vec3f* b)
{
	f32 dx = b->x - a->x;
	f32 dy = b->y - a->y;
	f32 dz = b->z - a->z;

	return sqrtf(SQ(dx) + SQ(dy) + SQ(dz));
}

f32 Math_Vec3f_DistXYZAndStoreDiff(Vec3f* a, Vec3f* b, Vec3f* dest)
{
	dest->x = b->x - a->x;
	dest->y = b->y - a->y;
	dest->z = b->z - a->z;

	return sqrtf(SQ(dest->x) + SQ(dest->y) + SQ(dest->z));
}

f32 Math_Vec3f_DistXZ(Vec3f* a, Vec3f* b)
{
	f32 dx = b->x - a->x;
	f32 dz = b->z - a->z;

	return sqrtf(SQ(dx) + SQ(dz));
}

f32 Math_Vec3f_DiffY(Vec3f* a, Vec3f* b)
{
	return b->y - a->y;
}

s16 Math_Vec3f_Yaw(Vec3f* a, Vec3f* b)
{
	f32 dx = b->x - a->x;
	f32 dz = b->z - a->z;

	return Math_Atan2S(dz, dx);
}

s16 Math_Vec3f_Pitch(Vec3f* a, Vec3f* b)
{
	return Math_Atan2S(Math_Vec3f_DistXZ(a, b), a->y - b->y);
}

void IChain_Apply_u8(u8* ptr, InitChainEntry* ichain);
void IChain_Apply_s8(u8* ptr, InitChainEntry* ichain);
void IChain_Apply_u16(u8* ptr, InitChainEntry* ichain);
void IChain_Apply_s16(u8* ptr, InitChainEntry* ichain);
void IChain_Apply_u32(u8* ptr, InitChainEntry* ichain);
void IChain_Apply_s32(u8* ptr, InitChainEntry* ichain);
void IChain_Apply_f32(u8* ptr, InitChainEntry* ichain);
void IChain_Apply_f32div1000(u8* ptr, InitChainEntry* ichain);
void IChain_Apply_Vec3f(u8* ptr, InitChainEntry* ichain);
void IChain_Apply_Vec3fdiv1000(u8* ptr, InitChainEntry* ichain);
void IChain_Apply_Vec3s(u8* ptr, InitChainEntry* ichain);

void (*sInitChainHandlers[])(u8* ptr, InitChainEntry* ichain) = {
    IChain_Apply_u8, IChain_Apply_s8, IChain_Apply_u16, IChain_Apply_s16, IChain_Apply_u32, IChain_Apply_s32, IChain_Apply_f32, IChain_Apply_f32div1000, IChain_Apply_Vec3f, IChain_Apply_Vec3fdiv1000, IChain_Apply_Vec3s,
};

void Actor_ProcessInitChain(Actor* actor, InitChainEntry* ichain)
{
	do
	{
		sInitChainHandlers[ichain->type]((u8*)actor, ichain);
	} while((ichain++)->cont);
}

void IChain_Apply_u8(u8* ptr, InitChainEntry* ichain)
{
	*(u8*)(ptr + ichain->offset) = ichain->value;
}

void IChain_Apply_s8(u8* ptr, InitChainEntry* ichain)
{
	*(s8*)(ptr + ichain->offset) = ichain->value;
}

void IChain_Apply_u16(u8* ptr, InitChainEntry* ichain)
{
	*(u16*)(ptr + ichain->offset) = ichain->value;
}

void IChain_Apply_s16(u8* ptr, InitChainEntry* ichain)
{
	*(s16*)(ptr + ichain->offset) = ichain->value;
}

void IChain_Apply_u32(u8* ptr, InitChainEntry* ichain)
{
	*(u32*)(ptr + ichain->offset) = ichain->value;
}

void IChain_Apply_s32(u8* ptr, InitChainEntry* ichain)
{
	*(s32*)(ptr + ichain->offset) = ichain->value;
}

void IChain_Apply_f32(u8* ptr, InitChainEntry* ichain)
{
	*(f32*)(ptr + ichain->offset) = ichain->value;
}

void IChain_Apply_f32div1000(u8* ptr, InitChainEntry* ichain)
{
	*(f32*)(ptr + ichain->offset) = ichain->value / 1000.0f;
}

void IChain_Apply_Vec3f(u8* ptr, InitChainEntry* ichain)
{
	Vec3f* vec = (Vec3f*)(ptr + ichain->offset);
	f32 val = ichain->value;

	vec->z = val;
	vec->y = val;
	vec->x = val;
}

void IChain_Apply_Vec3fdiv1000(u8* ptr, InitChainEntry* ichain)
{
	Vec3f* vec = (Vec3f*)(ptr + ichain->offset);
	f32 val;

	osSyncPrintf("pp=%x data=%f\n", vec, ichain->value / 1000.0f);
	val = ichain->value / 1000.0f;

	vec->z = val;
	vec->y = val;
	vec->x = val;
}

void IChain_Apply_Vec3s(u8* ptr, InitChainEntry* ichain)
{
	Vec3s* vec = (Vec3s*)(ptr + ichain->offset);
	s16 val = ichain->value;

	vec->z = val;
	vec->y = val;
	vec->x = val;
}

/**
 * Changes pValue by step towards target. If this step is more than fraction of the remaining distance, step by that
 * instead, with a minimum step of minStep. Returns remaining distance to target.
 */
template<class T>
static inline f32 _Math_SmoothStepToF(T* pValue, f32 target, f32 fraction, const Step& _step, const Step& _minStep)
{
	float step = _step.value();
	float minStep = _minStep.value();
	if(*pValue != target)
	{
		if(fraction != 1.0f)
			fraction = fraction * FRAMERATE_SCALER;
		f32 stepSize = (target - *pValue) * fraction;

		if((stepSize >= minStep) || (stepSize <= -minStep))
		{
			if(stepSize > step)
			{
				stepSize = step;
			}

			if(stepSize < -step)
			{
				stepSize = -step;
			}

			*pValue += stepSize;
		}
		else
		{
			if(stepSize < minStep)
			{
				*pValue += minStep;
				stepSize = minStep;

				if(target < *pValue)
				{
					*pValue = target;
				}
			}
			if(stepSize > -minStep)
			{
				*pValue += -minStep;

				if(*pValue < target)
				{
					*pValue = target;
				}
			}
		}
	}

	return fabsf(target - *pValue);
}

f32 Math_SmoothStepToF(f32* pValue, f32 target, f32 fraction, const Step& _step, const Step& _minStep)
{
	return _Math_SmoothStepToF(pValue, target, fraction, _step, _minStep);
}

f32 Math_SmoothStepToF(Rotation* pValue, f32 target, f32 fraction, const FStep& _step, const FStep& _minStep)
{
	return _Math_SmoothStepToF(pValue, target, fraction, _step, _minStep);
}

f32 Math_SmoothStepToF(CounterF* pValue, f32 target, f32 fraction, const FStep& _step, const FStep& _minStep)
{
	return _Math_SmoothStepToF(pValue, target, fraction, _step, _minStep);
}

/**
 * Changes pValue by step towards target. If step is more than fraction of the remaining distance, step by that instead.
 */
template<class T>
static inline void _Math_ApproachF(T* pValue, f32 target, f32 fraction, const Step& _step)
{
	float step = _step.value();
	if(*pValue != target)
	{
		f32 stepSize = (target - *pValue) * fraction;

		if(stepSize > step)
		{
			stepSize = step;
		}
		else if(stepSize < -step)
		{
			stepSize = -step;
		}

		*pValue += stepSize;
	}
}

void Math_ApproachF(f32* pValue, f32 target, f32 fraction, const Step& _step)
{
	_Math_ApproachF(pValue, target, fraction, _step);
}

void Math_ApproachF(CounterF* pValue, f32 target, f32 fraction, const Step& _step)
{
	_Math_ApproachF(pValue, target, fraction, _step);
}

void Math_ApproachF(Rotation* pValue, f32 target, f32 fraction, const Step& _step)
{
	_Math_ApproachF(pValue, target, fraction, _step);
}

/**
 * Changes pValue by step towards zero. If step is more than fraction of the remaining distance, step by that instead.
 */
void Math_ApproachZeroF(f32* pValue, f32 fraction, const Step& _step)
{
	float step = _step.value();
	f32 stepSize = *pValue * fraction;

	if(stepSize > step)
	{
		stepSize = step;
	}
	else if(stepSize < -step)
	{
		stepSize = -step;
	}

	*pValue -= stepSize;
}

f32 Math_ApproachZeroF(f32 pValue, f32 fraction, const Step& step)
{
	Math_ApproachZeroF(&pValue, fraction, step);
	return pValue;
}

void Math_ApproachZeroF(CounterF* pValue, f32 fraction, const Step& _step)
{
	float step = _step.value();
	f32 stepSize = *pValue * fraction;

	if(stepSize > step)
	{
		stepSize = step;
	}
	else if(stepSize < -step)
	{
		stepSize = -step;
	}

	*pValue -= stepSize;
}

/**
 * Changes pValue by step towards target angle in degrees. If this step is more than fraction of the remaining distance,
 * step by that instead, with a minimum step of minStep. Returns the value of the step taken.
 */
f32 Math_SmoothStepToDegF(f32* pValue, f32 target, f32 fraction, const Step& _step, const Step& _minStep)
{
	float step = _step.value();
	float minStep = _minStep.value();
	f32 stepSize = 0.0f;
	f32 diff = target - *pValue;

	if(*pValue != target)
	{
		if(diff > 180.0f)
		{
			diff = -(360.0f - diff);
		}
		else if(diff < -180.0f)
		{
			diff = 360.0f + diff;
		}

		stepSize = diff * fraction;

		if((stepSize >= minStep) || (stepSize <= -minStep))
		{
			if(stepSize > step)
			{
				stepSize = step;
			}

			if(stepSize < -step)
			{
				stepSize = -step;
			}

			*pValue += stepSize;
		}
		else
		{
			if(stepSize < minStep)
			{
				stepSize = minStep;
				*pValue += stepSize;
				if(*pValue > target)
				{
					*pValue = target;
				}
			}
			if(stepSize > -minStep)
			{
				stepSize = -minStep;
				*pValue += stepSize;
				if(*pValue < target)
				{
					*pValue = target;
				}
			}
		}
	}

	if(*pValue >= 360.0f)
	{
		*pValue -= 360.0f;
	}

	if(*pValue < 0.0f)
	{
		*pValue += 360.0f;
	}

	return stepSize;
}

/**
 * Changes pValue by step towards target. If this step is more than 1/scale of the remaining distance, step by that
 * instead, with a minimum step of minStep. Returns remaining distance to target.
 */
template <class T> static inline s16 _Math_SmoothStepToS(T* pValue, s16 target, s16 scale, const Step& _step, const Step& _minStep)
{
	float step = _step.value();
	float minStep = _minStep.value();
	s16 stepSize = 0;
	s16 diff = target - *pValue;

	if(*pValue != target)
	{
		stepSize = diff / scale;

		if((stepSize > minStep) || (stepSize < -minStep))
		{
			if(stepSize > step)
			{
				stepSize = step;
			}

			if(stepSize < -step)
			{
				stepSize = -step;
			}

			*pValue = *pValue + stepSize;
		}
		else
		{
			if(diff >= 0)
			{
				*pValue = *pValue + minStep;

				if((s16)(target - *pValue) <= 0)
				{
					*pValue = target;
				}
			}
			else
			{
				*pValue = *pValue - minStep;

				if((s16)(target - *pValue) >= 0)
				{
					*pValue = target;
				}
			}
		}
	}

	return diff;
}

s16 Math_SmoothStepToS(s16* pValue, s16 target, s16 scale, const Step& _step, const Step& _minStep)
{
	return _Math_SmoothStepToS(pValue, target, scale, _step, _minStep);
}

s16 Math_SmoothStepToS(Rotation* pValue, s16 target, s16 scale, const FStep& _step, const FStep& _minStep)
{
	return _Math_SmoothStepToS(pValue, target, scale, _step, _minStep);
}

/**
 * Changes pValue by step towards target. If step is more than 1/scale of the remaining distance, step by that instead.
 */
template <class T> static inline void _Math_ApproachS(T* pValue, s16 target, s16 scale, const Step& _maxStep)
{
	float maxStep = _maxStep.value();
	s16 diff = target - *pValue;

	diff /= scale;

	if(diff > maxStep)
	{
		*pValue = *pValue + maxStep;
	}
	else if(diff < -maxStep)
	{
		*pValue = *pValue - maxStep;
	}
	else
	{
		*pValue = *pValue + diff;
	}
}

void Math_ApproachS(s16* pValue, s16 target, s16 scale, const Step& _maxStep)
{
	_Math_ApproachS(pValue, target, scale, _maxStep);
}

void Math_ApproachS(Rotation* pValue, s16 target, s16 scale, const FStep& _maxStep)
{
	_Math_ApproachS(pValue, target, scale, _maxStep);
}

void Color_RGBA8_Copy(Color_RGBA8* dst, Color_RGBA8* src)
{
	dst->r = src->r;
	dst->g = src->g;
	dst->b = src->b;
	dst->a = src->a;
}

void Common_PlaySfx(u16 sfxId)
{
	Audio_PlaySoundGeneral(sfxId, &gAudioDefaultPos, 4, &D_801333E0, &D_801333E0, &gReverbAdd2);
}

void Common_PlaySfx2(u16 sfxId)
{
	Audio_PlaySoundGeneral(sfxId, &gAudioDefaultPos, 4, &D_801333E0, &D_801333E0, &gReverbAdd2);
}

void Common_PlaySfxAtPos(Vec3f* pos, u16 sfxId)
{
	Audio_PlaySoundGeneral(sfxId, pos, 4, &D_801333E0, &D_801333E0, &gReverbAdd2);
}
