#define INTERNAL_SRC_CODE_CODE_800D2E30_C
#include "global.h"
#include "padmgr.h"
#include <string.h>
#include "def/code_800D2E30.h"

void Rumble_Update(UnkRumbleStruct* arg0) {
    static u8 D_8012DBB0 = 1;
    s32 i;
    s32 unk_a3;
    s32 index = -1;

    for (i = 0; i < 4; i++) {
        arg0->rumbleEnable[i] = 0;
    }

    if (arg0->unk_105 == 0) {
        if (D_8012DBB0 != 0) {
            for (i = 0; i < 4; i++) {
                gPadMgr.pakType[i] = 0;
            }
        }
        D_8012DBB0 = arg0->unk_105;
        return;
    }

    D_8012DBB0 = arg0->unk_105;

    if (arg0->unk_104 == 2) {
        for (i = 0; i < 4; ++i) {
            gPadMgr.pakType[i] = 0;
        }

        for (i = 0; i < 0x40; i++) {
            arg0->unk_C4[i]       = 0;
            arg0->decayList[i]    = 0;
            arg0->lengthList[i]   = 0;
            arg0->strengthList[i] = 0;
        }
        arg0->unk_106 = arg0->unk_108 = arg0->strength = arg0->length = arg0->decay = arg0->unk_10D = 0;
        arg0->unk_104 = 1;
    }
    if (arg0->unk_104 != 0) {
        for (i = 0; i < 0x40; i++) {
            if (arg0->strengthList[i] != 0) {
                if (arg0->lengthList[i] > 0) {
                    arg0->lengthList[i]--;
                } else {
                    unk_a3 = arg0->strengthList[i] - arg0->decayList[i];
                    if (unk_a3 > 0) {
                        arg0->strengthList[i] = unk_a3;
                    } else {
                        arg0->strengthList[i] = 0;
                    }
                }

                unk_a3 = arg0->unk_C4[i] + arg0->strengthList[i];
                arg0->unk_C4[i] = unk_a3;
                if (index == -1) {
                    index = i;
                    arg0->rumbleEnable[0] = (unk_a3 >= 0x100);
                } else if (arg0->strengthList[index] < arg0->strengthList[i]) {
                    index = i;
                    arg0->rumbleEnable[0] = (unk_a3 >= 0x100);
                }
            }
        }
        if (arg0->strength != 0) {
            if (arg0->length > 0) {
                arg0->length--;
            } else {
                unk_a3 = arg0->strength - arg0->decay;
                if (unk_a3 > 0) {
                    arg0->strength = unk_a3;
                } else {
                    arg0->strength = 0;
                }
            }
            unk_a3 = arg0->unk_10D + arg0->strength;
            arg0->unk_10D = unk_a3;
            arg0->rumbleEnable[0] = (unk_a3 >= 0x100);
        }
        if (arg0->strength != 0) {
            unk_a3 = arg0->strength;
        } else {
            if (index == -1) {
                unk_a3 = 0;
            } else {
                unk_a3 = arg0->strengthList[index];
            }
        }
        if (unk_a3 == 0) {
            if ((++arg0->unk_108) >= 6) {
                arg0->unk_106 = 0;
                arg0->unk_108 = 5;
            }
        } else {
            arg0->unk_108 = 0;
            if ((++arg0->unk_106) >= 0x1C21) {
                arg0->unk_104 = 0;
            }
        }
    } else {
        for (i = 0; i < 0x40; i++) {
            arg0->unk_C4[i] = 0;
            arg0->strengthList[i] = 0;
            arg0->lengthList[i]   = 0;
            arg0->decayList[i]    = 0;
        }

        arg0->unk_106 = arg0->unk_108 = arg0->strength = arg0->length = arg0->decay = arg0->unk_10D = 0;
    }
}