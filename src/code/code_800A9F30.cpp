#define INTERNAL_SRC_CODE_CODE_800A9F30_C
#include "global.h"
#include "padmgr.h"
#include <math.h>
#include "def/code_800A9F30.h"
#include "def/code_800D2E30.h"
#include "def/padmgr.h"

UnkRumbleStruct g_Rumble;

void Rumble_Init(PadMgr* a, s32 b) {
    FindRumblePack(&g_Rumble);
    PadMgr_RumbleSet(a, g_Rumble.rumbleEnable);
}

//distance to player (unused), strength, time, decay?
void Rumble_Shake2(f32 a, u8 b, u8 c, u8 d) {//Used for bosses and fishing
    printf("Rumble Rumble ... V2 %d   %d   %d\n", b, c, d);

    if (g_Rumble.onVibrate)
        g_Rumble.onVibrate(b, c, d);

    s32 temp1;
    s32 temp2;

    if (1000000.0f < a) {
        temp1 = 1000;
    } else {
        temp1 = sqrtf(a);
    }

    if ((temp1 < 1000) && (b != 0) && (d != 0)) {
        temp2 = b - (temp1 * 255) / 1000;
        if (temp2 > 0) {
            g_Rumble.unk_10A = temp2;
            g_Rumble.unk_10B = c;
            g_Rumble.unk_10C = d;
        }
    }
}
#include <stdio.h>
//distance to player, strength, time, decay?
void Rumble_Shake(f32 a, u8 b, u8 c, u8 d) {
    printf("Rumble Rumble ...  V1 %d   %d   %d\n", b, c, d);

    if (g_Rumble.onVibrate)
        g_Rumble.onVibrate(b, c, d);

    s32 temp1;
    s32 temp2;
    s32 i;

    if (1000000.0f < a) {
        temp1 = 1000;
    } else {
        temp1 = sqrtf(a);
    }

    if (temp1 < 1000 && b != 0 && d != 0) {
        temp2 = b - (temp1 * 255) / 1000;

        for (i = 0; i < 0x40; i++) {
            if (g_Rumble.unk_04[i] == 0) {
                if (temp2 > 0) {
                    g_Rumble.unk_04[i] = temp2;
                    g_Rumble.unk_44[i] = c;
                    g_Rumble.unk_84[i] = d;
                }
                break;
            }
        }
    }
}

void Rumble_Reset(void) {//called on GameState_Init
#ifdef N64_VERSION
    bzero(&g_Rumble, sizeof(UnkRumbleStruct));
#endif

    g_Rumble.unk_104 = 2;
    g_Rumble.unk_105 = 1;

    gPadMgr.retraceCallback = Rumble_Init;
    gPadMgr.retraceCallbackValue = 0;

    if (1) {}
}

void Rumble_Destroy(void) {//called on GameState_Destroy
    PadMgr* padmgr = &gPadMgr;

    if ((padmgr->retraceCallback == Rumble_Init) && (padmgr->retraceCallbackValue == 0)) {
        padmgr->retraceCallback = NULL;
        padmgr->retraceCallbackValue = 0;
    }

#ifdef N64_VERSION
    bzero(&g_Rumble, sizeof(UnkRumbleStruct));
#endif
}

u32 Rumble_IsEnabled(void) {
    return gPadMgr.pakType[0] == 1;
}

void Rumble_Stop(void) {//called on Environment_Init and game over
    g_Rumble.unk_104 = 2;
}

void Rumble_Decrease(void) {//called per frame for a specific gSaveContext.gameMode
    g_Rumble.unk_104 = 0;
}

void Rumble_Enable(u32 a) {
    g_Rumble.unk_105 = !!a;
}
