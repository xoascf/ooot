#pragma once
#define Z_BG_BDAN_SWITCH_H

#include "global.h"
#include "ultra64.h"

// BgBdanSwitch.actor.params & 0xFF
enum BgBdanSwitchType
{
	/* 0x00 */ BLUE,
	/* 0x01 */ YELLOW_HEAVY,
	/* 0x02 */ YELLOW,
	/* 0x03 */ YELLOW_TALL_1,
	/* 0x04 */ YELLOW_TALL_2
};

struct BgBdanSwitch;

typedef void (*BgBdanSwitchActionFunc)(struct BgBdanSwitch*, GlobalContext*);

struct BgBdanSwitch
{
	/* 0x0000 */ DynaPolyActor dyna;
	/* 0x0164 */ BgBdanSwitchActionFunc actionFunc;
	/* 0x0168 */ ColliderJntSph collider;
	/* 0x0188 */ ColliderJntSphElement colliderItems[1];
	/* 0x01C8 */ TimerF32 unk_1C8;
	/* 0x01CC */ TimerS16 unk_1CC;
	/* 0x01CE */ char unk_1CE[0x2];
	/* 0x01D0 */ f32 unk_1D0;
	/* 0x01D4 */ f32 unk_1D4;
	/* 0x01D8 */ TimerS16 unk_1D8;
	/* 0x01DA */ TimerS16 unk_1DA;
	/* 0x01DC */ u8 unk_1DC;
	/* 0x01DD */ char unk_1DD[0x3];
};
