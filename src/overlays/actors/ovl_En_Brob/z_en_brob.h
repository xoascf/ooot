#pragma once
#define Z_EN_BROB_H

#include "global.h"
#include "ultra64.h"

struct EnBrob;

typedef void (*EnBrobActionFunc)(struct EnBrob* pthis, GlobalContext* globalCtx);

struct EnBrob
{
	/* 0x0000 */ DynaPolyActor dyna;
	/* 0x0164 */ SkelAnime skelAnime;
	/* 0x01A8 */ EnBrobActionFunc actionFunc;
	/* 0x01AC */ Timer timer;
	/* 0x01AE */ TimerS16 transitionTimer;
	/* 0x01B0 */ Vec3s jointTable[10];
	/* 0x01EC */ Vec3s morphTable[10];
	/* 0x0228 */ ColliderCylinder colliders[2];
};
