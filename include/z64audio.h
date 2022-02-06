#pragma once
#include "ultra64/pi.h"
#include "ultra64/sptask.h"
#include "z64math.h"

#define MK_CMD(b0,b1,b2,b3) ((((b0) & 0xFF) << 0x18) | (((b1) & 0xFF) << 0x10) | (((b2) & 0xFF) << 0x8) | (((b3) & 0xFF) << 0))

#define NO_LAYER ((SequenceLayer*)(-1))

#define TATUMS_PER_BEAT 48

#define IS_SEQUENCE_CHANNEL_VALID(ptr) ((uintptr_t)(ptr) != (uintptr_t)&gAudioContext.sequenceChannelNone)

#define MAX_CHANNELS_PER_BANK 3

#define ADSR_DISABLE 0
#define ADSR_HANG -1
#define ADSR_GOTO -2
#define ADSR_RESTART -3

#define AIBUF_LEN 0x580

typedef enum {
    /* 0 */ ADSR_STATE_DISABLED,
    /* 1 */ ADSR_STATE_INITIAL,
    /* 2 */ ADSR_STATE_START_LOOP,
    /* 3 */ ADSR_STATE_LOOP,
    /* 4 */ ADSR_STATE_FADE,
    /* 5 */ ADSR_STATE_HANG,
    /* 6 */ ADSR_STATE_DECAY,
    /* 7 */ ADSR_STATE_RELEASE,
    /* 8 */ ADSR_STATE_SUSTAIN
} AdsrStatus;

typedef enum {
    /* 0 */ MEDIUM_RAM,
    /* 1 */ MEDIUM_UNK,
    /* 2 */ MEDIUM_CART,
    /* 3 */ MEDIUM_DISK_DRIVE
} SampleMedium;

typedef enum {
    /* 0 */ CODEC_ADPCM,
    /* 1 */ CODEC_S8,
    /* 2 */ CODEC_S16_INMEMORY,
    /* 3 */ CODEC_SMALL_ADPCM,
    /* 4 */ CODEC_REVERB,
    /* 5 */ CODEC_S16
} SampleCodec;

typedef enum {
    /* 0 */ SEQUENCE_TABLE,
    /* 1 */ FONT_TABLE,
    /* 2 */ SAMPLE_TABLE
} SampleBankTableType;

typedef enum {
    /* 0 */ CACHE_TEMPORARY,
    /* 1 */ CACHE_PERSISTENT,
    /* 2 */ CACHE_EITHER,
    /* 3 */ CACHE_PERMANENT
} AudioCacheType;

typedef s32 (*DmaHandler)(struct OSPiHandle* handle, struct OSIoMesg* mb, s32 direction);

struct Note;
struct NotePool;
struct SequenceChannel;
struct SequenceLayer;

typedef struct AudioListItem {
    // A node in a circularly linked list. Each node is either a head or an item:
    // - Items can be either detached (prev = NULL), or attached to a list.
    //   'value' points to something of interest.
    // - List heads are always attached; if a list is empty, its head points
    //   to itself. 'count' contains the size of the list.
    // If the list holds notes, 'pool' points back to the pool where it lives.
    // Otherwise, that member is NULL.
    /* 0x00 */ struct AudioListItem* prev;
    /* 0x04 */ struct AudioListItem* next;
    /* 0x08 */ union {
                   void* value; // either Note* or SequenceLayer*
                   s32 count;
               } u;
    /* 0x0C */ struct NotePool* pool;
} AudioListItem; // size = 0x10

typedef struct NotePool {
    /* 0x00 */ AudioListItem disabled;
    /* 0x10 */ AudioListItem decaying;
    /* 0x20 */ AudioListItem releasing;
    /* 0x30 */ AudioListItem active;
} NotePool;

// Pitch sliding by up to one octave in the positive direction. Negative
// direction is "supported" by setting extent to be negative. The code
// exterpolates exponentially in the wrong direction in that case, but that
// doesn't prevent seqplayer from doing it, AFAICT.
typedef struct {
    /* 0x00 */ u8 mode; // bit 0x80 denotes something; the rest are an index 0-5
    /* 0x02 */ u16 cur;
    /* 0x04 */ u16 speed;
    /* 0x08 */ f32 extent;
} Portamento; // size = 0xC

typedef struct {
    /* 0x0 */ s16 delay;
    /* 0x2 */ s16 arg;
} AdsrEnvelope; // size = 0x4

typedef struct {
    /* 0x00 */ u32 start;
    /* 0x04 */ u32 end;
    /* 0x08 */ u32 count;
    /* 0x0C */ char unk_0C[0x4];
    /* 0x10 */ s16 state[16]; // only exists if count != 0. 8-byte aligned
} AdpcmLoop; // size = 0x30 (or 0x10)

typedef struct {
    /* 0x00 */ s32 order;
    /* 0x04 */ s32 npredictors;
    /* 0x08 */ s16 book[1]; // size 8 * order * npredictors. 8-byte aligned
} AdpcmBook; // size >= 0x8

typedef struct {
#ifdef LITTLE_ENDIAN
    u32 size : 24;
    u32 unk_bit25 : 1;
    u32 unk_bit26 : 1;
    u32 medium : 2;
    u32 codec : 4;    
#else
    u32 codec : 4;
    u32 medium : 2;
    u32 unk_bit26 : 1;
    u32 unk_bit25 : 1;
    u32 size : 24;
#endif
    /* 0x04 */ u8* sampleAddr;
    /* 0x08 */ AdpcmLoop* loop;
    /* 0x0C */ AdpcmBook* book;
} SoundFontSample; // size = 0x10

typedef struct {
    /* 0x00 */ SoundFontSample* sample;
    /* 0x04 */ f32 tuning; // frequency scale factor
} SoundFontSound; // size = 0x8

typedef enum
{
    CACHE_LOAD_PERMANENT = 0,
    CACHE_LOAD_PERSISTENT,
    CACHE_LOAD_TEMPORARY,
    CACHE_LOAD_EITHER,
    CACHE_LOAD_EITHER_NOSYNC
} AudioCacheLoadType;

typedef struct {
    /* 0x00 */ s16 numSamplesAfterDownsampling; // never read
    /* 0x02 */ s16 chunkLen; // never read
    /* 0x04 */ s16* toDownsampleLeft;
    /* 0x08 */ s16* toDownsampleRight; // data pointed to by left and right are adjacent in memory
    /* 0x0C */ s32 startPos; // start pos in ring buffer
    /* 0x10 */ s16 lengthA; // first length in ring buffer (from startPos, at most until end)
    /* 0x12 */ s16 lengthB; // second length in ring buffer (from pos 0)
    /* 0x14 */ u16 unk_14;
    /* 0x16 */ u16 unk_16;
    /* 0x18 */ u16 unk_18;
} ReverbRingBufferItem; // size = 0x1C

typedef struct {
    /* 0x000 */ u8 resampleFlags;
    /* 0x001 */ u8 useReverb;
    /* 0x002 */ u8 framesToIgnore;
    /* 0x003 */ u8 curFrame;
    /* 0x004 */ u8 downsampleRate;
    /* 0x005 */ s8 unk_05;
    /* 0x006 */ u16 windowSize;
    /* 0x008 */ s16 unk_08;
    /* 0x00A */ s16 unk_0A;
    /* 0x00C */ u16 unk_0C;
    /* 0x00E */ u16 unk_0E;
    /* 0x010 */ s16 leakRtl;
    /* 0x012 */ s16 leakLtr;
    /* 0x014 */ u16 unk_14;
    /* 0x016 */ s16 unk_16;
    /* 0x018 */ u8 unk_18;
    /* 0x019 */ u8 unk_19;
    /* 0x01A */ u8 unk_1A;
    /* 0x01B */ u8 unk_1B;
    /* 0x01C */ s32 nextRingBufPos;
    /* 0x020 */ s32 unk_20;
    /* 0x024 */ s32 bufSizePerChan;
    /* 0x028 */ s16* leftRingBuf;
    /* 0x02C */ s16* rightRingBuf;
    /* 0x030 */ void* unk_30;
    /* 0x034 */ void* unk_34;
    /* 0x038 */ void* unk_38;
    /* 0x03C */ void* unk_3C;
    /* 0x040 */ ReverbRingBufferItem items[2][5];
    /* 0x158 */ ReverbRingBufferItem items2[2][5];
    /* 0x270 */ s16* filterLeft;
    /* 0x274 */ s16* filterRight;
    /* 0x278 */ s16* filterLeftState;
    /* 0x27C */ s16* filterRightState;
    /* 0x280 */ SoundFontSound sound;
    /* 0x288 */ SoundFontSample sample;
    /* 0x298 */ AdpcmLoop loop;
} SynthesisReverb; // size = 0x2C8

typedef struct {
    /* 0x00 */ u8 loaded;
    /* 0x01 */ u8 normalRangeLo;
    /* 0x02 */ u8 normalRangeHi;
    /* 0x03 */ u8 releaseRate;
    /* 0x04 */ AdsrEnvelope* envelope;
    /* 0x08 */ SoundFontSound lowNotesSound;
    /* 0x10 */ SoundFontSound normalNotesSound;
    /* 0x18 */ SoundFontSound highNotesSound;
} Instrument; // size = 0x20

typedef struct {
    /* 0x00 */ u8 releaseRate;
    /* 0x01 */ u8 pan;
    /* 0x02 */ u8 loaded;
    /* 0x04 */ SoundFontSound sound;
    /* 0x14 */ AdsrEnvelope* envelope;
} Drum; // size = 0x14

typedef struct {
    /* 0x00 */ u8 numInstruments;
    /* 0x01 */ u8 numDrums;
    /* 0x02 */ u8 sampleBankId1;
    /* 0x03 */ u8 sampleBankId2;
    /* 0x04 */ u16 numSfx;
    /* 0x08 */ Instrument** instruments;
    /* 0x0C */ Drum** drums;
    /* 0x10 */ SoundFontSound* soundEffects;
} SoundFont; // size = 0x14

typedef struct {
    /* 0x00 */ u8* pc;
    /* 0x04 */ u8* stack[4];
    /* 0x14 */ u8 remLoopIters[4];
    /* 0x18 */ u8 depth;
    /* 0x19 */ s8 value;
} SeqScriptState; // size = 0x1C

// Also known as a Group, according to debug strings.
typedef struct {
#ifdef LITTLE_ENDIAN
    u8 unk_0b1 : 1;
    u8 stopScript : 1;
    u8 recalculateVolume : 1;
    u8 fontDmaInProgress : 1;
    u8 seqDmaInProgress : 1;
    u8 muted : 1;
    u8 finished : 1;
    u8 enabled : 1;
#else
    u8 enabled : 1;
    u8 finished : 1;
    u8 muted : 1;
    u8 seqDmaInProgress : 1;
    u8 fontDmaInProgress : 1;
    u8 recalculateVolume : 1;
    u8 stopScript : 1;
    u8 unk_0b1 : 1;
#endif
    /* 0x001 */ u8 state;
    /* 0x002 */ u8 noteAllocPolicy;
    /* 0x003 */ u8 muteBehavior;
    /* 0x004 */ u8 seqId;
    /* 0x005 */ u8 defaultFont;
    /* 0x006 */ u8 unk_06[1];
    /* 0x007 */ s8 playerIdx;
    /* 0x008 */ u16 tempo; // tatums per minute
    /* 0x00A */ u16 tempoAcc;
    /* 0x00C */ u16 unk_0C;
    /* 0x00E */ s16 transposition;
    /* 0x010 */ u16 delay;
    /* 0x012 */ u16 fadeTimer;
    /* 0x014 */ u16 fadeTimerUnkEu;
    /* 0x018 */ u8* seqData;
    /* 0x01C */ f32 fadeVolume;
    /* 0x020 */ f32 fadeVelocity;
    /* 0x024 */ f32 volume;
    /* 0x028 */ f32 muteVolumeScale;
    /* 0x02C */ f32 fadeVolumeScale;
    /* 0x030 */ f32 appliedFadeVolume;
    /* 0x034 */ f32 unk_34;
    /* 0x038 */ struct SequenceChannel* channels[16];
    /* 0x078 */ SeqScriptState scriptState;
    /* 0x094 */ u8* shortNoteVelocityTable;
    /* 0x098 */ u8* shortNoteGateTimeTable;
    /* 0x09C */ NotePool notePool;
    /* 0x0DC */ s32 skipTicks;
    /* 0x0E0 */ u32 scriptCounter;
    /* 0x0E4 */ char unk_E4[0x74]; // unused struct members for sequence/sound font dma management, according to sm64 decomp
    /* 0x158 */ s8 soundScriptIO[8];
} SequencePlayer; // size = 0x160

typedef struct {
    /* 0x0 */ u8 releaseRate;
    /* 0x1 */ u8 sustain;
    /* 0x4 */ AdsrEnvelope* envelope;
} AdsrSettings; // size = 0x8

typedef struct {
    /* 0x00 */ union {
        struct A {
#ifdef LITTLE_ENDIAN
            u8 state : 4;
            u8 release : 1;
            u8 decay : 1;
            u8 hang : 1;
            u8 unk_0b80 : 1;
#else
            u8 unk_0b80 : 1;
            u8 hang : 1;
            u8 decay : 1;
            u8 release : 1;
            u8 state : 4;
#endif
        } s;
        /* 0x00 */ u8 asByte;
    } action;
    /* 0x01 */ u8 envIndex;
    /* 0x02 */ s16 delay;
    /* 0x04 */ f32 sustain;
    /* 0x08 */ f32 velocity;
    /* 0x0C */ f32 fadeOutVel;
    /* 0x10 */ f32 current;
    /* 0x14 */ f32 target;
    /* 0x18 */ char unk_18[4];
    /* 0x1C */ AdsrEnvelope* envelope;
} AdsrState;

typedef struct {
#ifdef LITTLE_ENDIAN
    u8 usesHeadsetPanEffects : 1;
    u8 stereoHeadsetEffects : 1;
    u8 strongLeft : 1;
    u8 strongRight : 1;
    u8 bit2 : 2;
    u8 unused : 2;
#else
    u8 unused : 2;
    u8 bit2 : 2;
    u8 strongRight : 1;
    u8 strongLeft : 1;
    u8 stereoHeadsetEffects : 1;
    u8 usesHeadsetPanEffects : 1;
#endif
} StereoData;

typedef union {
    /* 0x00 */ StereoData s;
    /* 0x00 */ u8 asByte;
} Stereo;

typedef struct {
    /* 0x00 */ u8 reverb;
    /* 0x01 */ u8 unk_1;
    /* 0x02 */ u8 pan;
    /* 0x03 */ Stereo stereo;
    /* 0x04 */ u8 unk_4;
    /* 0x06 */ u16 unk_6;
    /* 0x08 */ f32 freqScale;
    /* 0x0C */ f32 velocity;
    /* 0x10 */ s16* filter;
    /* 0x14 */ s16 filterBuf[8];
} NoteAttributes; // size = 0x24

// Also known as a SubTrack, according to sm64 debug strings.
typedef struct SequenceChannel {
#ifdef LITTLE_ENDIAN
    u8 unused : 1;
    u8 largeNotes : 1; // notes specify duration and velocity
    u8 stereoHeadsetEffects : 1;
    u8 hasInstrument : 1;
    u8 stopSomething2 : 1; // sets SequenceLayer.stopSomething
    u8 stopScript : 1;
    u8 finished : 1;
    u8 enabled : 1;
#else
    u8 enabled : 1;
    u8 finished : 1;
    u8 stopScript : 1;
    u8 stopSomething2 : 1; // sets SequenceLayer.stopSomething
    u8 hasInstrument : 1;
    u8 stereoHeadsetEffects : 1;
    u8 largeNotes : 1; // notes specify duration and velocity
    u8 unused : 1;
#endif
    union {
        struct {
#ifdef LITTLE_ENDIAN
            u8 pan : 1;
            u8 volume : 1;
            u8 freqScale : 1;
#else
            u8 freqScale : 1;
            u8 volume : 1;
            u8 pan : 1;
#endif
        } s;
        /* 0x01 */ u8 asByte;
    } changes;
    /* 0x02 */ u8 noteAllocPolicy;
    /* 0x03 */ u8 muteBehavior;
    /* 0x04 */ u8 reverb;       // or dry/wet mix
    /* 0x05 */ u8 notePriority; // 0-3
    /* 0x06 */ u8 someOtherPriority;
    /* 0x07 */ u8 fontId;
    /* 0x08 */ u8 reverbIndex;
    /* 0x09 */ u8 bookOffset;
    /* 0x0A */ u8 newPan;
    /* 0x0B */ u8 panChannelWeight;  // proportion of pan that comes from the channel (0..128)
    /* 0x0C */ u8 unk_0C;
    /* 0x0D */ u8 velocityRandomVariance;
    /* 0x0E */ u8 gateTimeRandomVariance;
    /* 0x0F */ u8 unk_0F;
    /* 0x10 */ u16 vibratoRateStart;
    /* 0x12 */ u16 vibratoExtentStart;
    /* 0x14 */ u16 vibratoRateTarget;
    /* 0x16 */ u16 vibratoExtentTarget;
    /* 0x18 */ u16 vibratoRateChangeDelay;
    /* 0x1A */ u16 vibratoExtentChangeDelay;
    /* 0x1C */ u16 vibratoDelay;
    /* 0x1E */ u16 delay;
    /* 0x20 */ u16 unk_20;
    /* 0x22 */ u16 unk_22;
    /* 0x24 */ s16 instOrWave; // either 0 (none), instrument index + 1, or
                             // 0x80..0x83 for sawtooth/triangle/sine/square waves.
    /* 0x26 */ s16 transposition;
    /* 0x28 */ f32 volumeScale;
    /* 0x2C */ f32 volume;
    /* 0x30 */ s32 pan;
    /* 0x34 */ f32 appliedVolume;
    /* 0x38 */ f32 freqScale;
    /* 0x3C */ u8 (*dynTable)[][2];
    /* 0x40 */ struct Note* noteUnused;
    /* 0x44 */ struct SequenceLayer* layerUnused;
    /* 0x48 */ Instrument* instrument;
    /* 0x4C */ SequencePlayer* seqPlayer;
    /* 0x50 */ struct SequenceLayer* layers[4];
    /* 0x60 */ SeqScriptState scriptState;
    /* 0x7C */ AdsrSettings adsr;
    /* 0x84 */ NotePool notePool;
    /* 0xC4 */ s8 soundScriptIO[8]; // bridge between sound script and audio lib, "io ports"
    /* 0xCC */ s16* filter;
    /* 0xD0 */ Stereo stereo;
} SequenceChannel; // size = 0xD4

// Might also be known as a Track, according to sm64 debug strings (?).
typedef struct SequenceLayer {
#ifdef LITTLE_ENDIAN
    u8 notePropertiesNeedInit : 1;
    u8 bit1 : 1; // "has initialized continuous notes"?
    u8 ignoreDrumPan : 1;
    u8 bit3 : 1; // "loaded"?
    u8 continuousNotes : 1; // keep the same note for consecutive notes with the same sound
    u8 stopSomething : 1;
    u8 finished : 1;
    u8 enabled : 1;
#else
    u8 enabled : 1;
    u8 finished : 1;
    u8 stopSomething : 1;
    u8 continuousNotes : 1; // keep the same note for consecutive notes with the same sound
    u8 bit3 : 1; // "loaded"?
    u8 ignoreDrumPan : 1;
    u8 bit1 : 1; // "has initialized continuous notes"?
    u8 notePropertiesNeedInit : 1;
#endif
    /* 0x01 */ Stereo stereo;
    /* 0x02 */ u8 instOrWave;
    /* 0x03 */ u8 gateTime;
    /* 0x04 */ u8 semitone;
    /* 0x05 */ u8 portamentoTargetNote;
    /* 0x06 */ u8 pan; // 0..128
    /* 0x07 */ u8 notePan;
    /* 0x08 */ s16 delay;
    /* 0x0A */ s16 gateDelay;
    /* 0x0C */ s16 delay2;
    /* 0x0E */ u16 portamentoTime;
    /* 0x10 */ s16 transposition; // #semitones added to play commands
                                  // (seq instruction encoding only allows referring to the limited range
                                  // 0..0x3F; this makes 0x40..0x7F accessible as well)
    /* 0x12 */ s16 shortNoteDefaultDelay;
    /* 0x14 */ s16 lastDelay;
    /* 0x18 */ AdsrSettings adsr;
    /* 0x20 */ Portamento portamento;
    /* 0x2C */ struct Note* note;
    /* 0x30 */ f32 freqScale;
    /* 0x34 */ f32 unk_34;
    /* 0x38 */ f32 velocitySquare2;
    /* 0x3C */ f32 velocitySquare; // not sure which one of those corresponds to the sm64 original
    /* 0x40 */ f32 noteVelocity;
    /* 0x44 */ f32 noteFreqScale;
    /* 0x48 */ Instrument* instrument;
    /* 0x4C */ SoundFontSound* sound;
    /* 0x50 */ SequenceChannel* channel;
    /* 0x54 */ SeqScriptState scriptState;
    /* 0x70 */ AudioListItem listItem;
} SequenceLayer; // size = 0x80

typedef struct {
    /* 0x0000 */ s16 adpcmdecState[0x10];
    /* 0x0020 */ s16 finalResampleState[0x10];
    /* 0x0040 */ s16 mixEnvelopeState[0x28];
    /* 0x0090 */ s16 panResampleState[0x10];
    /* 0x00B0 */ s16 panSamplesBuffer[0x20];
    /* 0x00F0 */ s16 dummyResampleState[0x10];
} NoteSynthesisBuffers; // size = 0x110

typedef struct {
    /* 0x00 */ u8 restart;
    /* 0x01 */ u8 sampleDmaIndex;
    /* 0x02 */ u8 prevHeadsetPanRight;
    /* 0x03 */ u8 prevHeadsetPanLeft;
    /* 0x04 */ u8 reverbVol;
    /* 0x05 */ u8 numParts;
    /* 0x06 */ u16 samplePosFrac;
    /* 0x08 */ s32 samplePosInt;
    /* 0x0C */ NoteSynthesisBuffers* synthesisBuffers;
    /* 0x10 */ s16 curVolLeft;
    /* 0x12 */ s16 curVolRight;
    /* 0x14 */ u16 unk_14;
    /* 0x16 */ u16 unk_16;
    /* 0x18 */ u16 unk_18;
    /* 0x1A */ u8 unk_1A;
    /* 0x1C */ u16 unk_1C;
    /* 0x1E */ u16 unk_1E;
} NoteSynthesisState; // size = 0x20

typedef struct {
    /* 0x00 */ struct SequenceChannel* channel;
    /* 0x04 */ u32 time;
    /* 0x08 */ s16* curve;
    /* 0x0C */ f32 extent;
    /* 0x10 */ f32 rate;
    /* 0x14 */ u8 active;
    /* 0x16 */ u16 rateChangeTimer;
    /* 0x18 */ u16 extentChangeTimer;
    /* 0x1A */ u16 delay;
} VibratoState; // size = 0x1C

typedef struct {
    /* 0x00 */ u8 priority;
    /* 0x01 */ u8 waveId;
    /* 0x02 */ u8 sampleCountIndex;
    /* 0x03 */ u8 fontId;
    /* 0x04 */ u8 unk_04;
    /* 0x05 */ u8 stereoHeadsetEffects;
    /* 0x06 */ s16 adsrVolScaleUnused;
    /* 0x08 */ f32 portamentoFreqScale;
    /* 0x0C */ f32 vibratoFreqScale;
    /* 0x10 */ SequenceLayer* prevParentLayer;
    /* 0x14 */ SequenceLayer* parentLayer;
    /* 0x18 */ SequenceLayer* wantedParentLayer;
    /* 0x1C */ NoteAttributes attributes;
    /* 0x40 */ AdsrState adsr;
    // may contain portamento, vibratoState, if those are not part of Note itself
} NotePlaybackState;

typedef struct {
    struct {
#ifdef LITTLE_ENDIAN
        u8 usesHeadsetPanEffects : 1; // ?
        u8 stereoHeadsetEffects : 1;
        u8 stereoStrongLeft : 1;
        u8 stereoStrongRight : 1;
        u8 unused : 1;
        u8 finished : 1; // ?
        u8 needsInit : 1;
        volatile u8 enabled : 1;
#else
        volatile u8 enabled : 1;
        u8 needsInit : 1;
        u8 finished : 1; // ?
        u8 unused : 1;
        u8 stereoStrongRight : 1;
        u8 stereoStrongLeft : 1;
        u8 stereoHeadsetEffects : 1;
        u8 usesHeadsetPanEffects : 1; // ?
#endif
    } bitField0;
    struct {
#ifdef LITTLE_ENDIAN
        u8 usesHeadsetPanEffects2 : 1;
        u8 hasTwoParts : 1;
        u8 isSyntheticWave : 1;
        u8 bookOffset : 2;
        u8 reverbIndex : 3;
#else
        u8 reverbIndex : 3;
        u8 bookOffset : 2;
        u8 isSyntheticWave : 1;
        u8 hasTwoParts : 1;
        u8 usesHeadsetPanEffects2 : 1;
#endif
    } bitField1;
    /* 0x02 */ u8 unk_2;
    /* 0x03 */ u8 headsetPanRight;
    /* 0x04 */ u8 headsetPanLeft;
    /* 0x05 */ u8 reverbVol;
    /* 0x06 */ u8 unk_06;
    /* 0x07 */ u8 unk_07;
    /* 0x08 */ u16 targetVolLeft;
    /* 0x0A */ u16 targetVolRight;
    /* 0x0C */ u16 resamplingRateFixedPoint;
    /* 0x0E */ u16 unk_0E;
    /* 0x10 */ union {
                 SoundFontSound* soundFontSound;
                 s16* samples; // used for synthetic waves
             } sound;
    /* 0x14 */ s16* filter;
    /* 0x18 */ char pad_18[0x8];
} NoteSubEu; // size = 0x20

typedef struct Note {
    /* 0x00 */ AudioListItem listItem;
    /* 0x10 */ NoteSynthesisState synthesisState;
    /* 0x30 */ NotePlaybackState playbackState;
    /* 0x90 */ Portamento portamento;
    /* 0x9C */ VibratoState vibratoState;
    /* 0xB8 */ char unk_B8[0x4];
    /* 0xBC */ u32 unk_BC;
    /* 0xC0 */ NoteSubEu noteSubEu;
} Note; // size = 0xE0

typedef struct {
    /* 0x00 */ u8 downsampleRate;
    /* 0x02 */ u16 windowSize;
    /* 0x04 */ u16 unk_4;
    /* 0x06 */ u16 unk_6;
    /* 0x08 */ u16 unk_8;
    /* 0x0A */ u16 unk_A;
    /* 0x0C */ u16 leakRtl;
    /* 0x0E */ u16 leakLtr;
    /* 0x10 */ s8 unk_10;
    /* 0x12 */ u16 unk_12;
    /* 0x14 */ s16 lowPassFilterCutoffLeft;
    /* 0x16 */ s16 lowPassFilterCutoffRight;
} ReverbSettings; // size = 0x18

typedef struct {
    /* 0x00 */ u32 frequency;
    /* 0x04 */ u8 unk_04;
    /* 0x05 */ u8 numNotes;
    /* 0x06 */ u8 numSequencePlayers;
    /* 0x07 */ u8 unk_07; // unused, set to zero
    /* 0x08 */ u8 unk_08; // unused, set to zero
    /* 0x09 */ u8 numReverbs;
    /* 0x0C */ ReverbSettings* reverbSettings;
    /* 0x10 */ u16 sampleDmaBufSize1;
    /* 0x12 */ u16 sampleDmaBufSize2;
    /* 0x14 */ u16 unk_14;
    /* 0x18 */ u32 persistentSeqMem;
    /* 0x1C */ u32 persistentFontMem;
    /* 0x20 */ u32 persistentSampleMem;
    /* 0x24 */ u32 temporarySeqMem;
    /* 0x28 */ u32 temporaryFontMem;
    /* 0x2C */ u32 temporarySampleMem;
    /* 0x30 */ s32 persistentSampleCacheMem;
    /* 0x34 */ s32 temporarySampleCacheMem;
} AudioSpec; // size = 0x38

typedef struct {
    /* 0x00 */ s16 specUnk4;
    /* 0x02 */ u16 frequency;
    /* 0x04 */ u16 aiFrequency;
    /* 0x06 */ s16 samplesPerFrameTarget;
    /* 0x08 */ s16 maxAiBufferLength;
    /* 0x0A */ s16 minAiBufferLength;
    /* 0x0C */ s16 updatesPerFrame;
    /* 0x0E */ s16 samplesPerUpdate;
    /* 0x10 */ s16 samplesPerUpdateMax;
    /* 0x12 */ s16 samplesPerUpdateMin;
    /* 0x14 */ s16 numSequencePlayers;
    /* 0x18 */ f32 resampleRate;
    /* 0x1C */ f32 updatesPerFrameInv;
    /* 0x20 */ f32 unkUpdatesPerFrameScaled;
    /* 0x24 */ f32 unk_24;
} AudioBufferParameters;

typedef struct {
    /* 0x0 */ u8* start;
    /* 0x4 */ u8* cur;
    /* 0x8 */ s32 size;
    /* 0xC */ s32 count;
} AudioAllocPool; // size = 0x10

typedef struct {
    /* 0x0 */ u8* ptr;
    /* 0x4 */ u32 size;
    /* 0x8 */ s16 tableType;
    /* 0xA */ s16 id;
} AudioCacheEntry; // size = 0xC

typedef struct {
    /* 0x00 */ s8 inUse;
    /* 0x01 */ s8 origMedium;
    /* 0x02 */ s8 sampleBankId;
    /* 0x03 */ char unk_03[0x5];
    /* 0x08 */ u8* allocatedAddr;
    /* 0x0C */ void* sampleAddr;
    /* 0x10 */ u32 size;
} SampleCacheEntry; // size = 0x14

typedef struct {
    /* 0x000 */ AudioAllocPool pool;
    /* 0x010 */ SampleCacheEntry entries[32];
    /* 0x290 */ s32 size;
} AudioSampleCache; // size = 0x294

typedef struct {
    /* 0x00*/ u32 numEntries;
    /* 0x04*/ AudioAllocPool pool;
    /* 0x14*/ AudioCacheEntry entries[16];
} AudioPersistentCache; // size = 0xD4

typedef struct {
    /* 0x00*/ u32 nextSide;
    /* 0x04*/ AudioAllocPool pool;
    /* 0x14*/ AudioCacheEntry entries[2];
} AudioTemporaryCache; // size = 0x3C

typedef struct {
    /* 0x000*/ AudioPersistentCache persistent;
    /* 0x0D4*/ AudioTemporaryCache temporary;
    /* 0x100*/ u8 unk_100[0x10];
} AudioCache; // size = 0x110

typedef struct {
    u32 wantPersistent;
    u32 wantTemporary;
} AudioPoolSplit2; // size = 0x8

typedef struct {
    u32 wantSeq;
    u32 wantFont;
    u32 wantSample;
} AudioPoolSplit3; // size = 0xC

typedef struct {
    u32 wantSeq;
    u32 wantFont;
    u32 wantSample;
    u32 wantCustom;
} AudioPoolSplit4; // size = 0x10

typedef struct {
    /* 0x00 */ u32 endAndMediumKey;
    /* 0x04 */ SoundFontSample* sample;
    /* 0x08 */ u8* ramAddr;
    /* 0x0C */ u32 encodedInfo;
    /* 0x10 */ s32 isFree;
} AudioPreloadReq; // size = 0x14

typedef struct {
    union{
        u32 opArgs;
#ifdef LITTLE_ENDIAN
        struct
        {
            u8 arg2;
            u8 arg1;
            u8 arg0;
            u8 op;
        };
#else
        struct {
            u8 op;
            u8 arg0;
            u8 arg1;
            u8 arg2;
        };
#endif
    };
    union {
        void* data;
        f32 asFloat;
        s32 asInt;
        u16 asUShort;
        s8 asSbyte;
        u8 asUbyte;
        u32 asUInt;
    };
} AudioCmd;

typedef struct {
    /* 0x00 */ s8 status;
    /* 0x01 */ s8 delay;
    /* 0x02 */ s8 medium;
    /* 0x04 */ u8* ramAddr;
    /* 0x08 */ u32 curDevAddr;
    /* 0x0C */ u8* curRamAddr;
    /* 0x10 */ u32 bytesRemaining;
    /* 0x14 */ u32 chunkSize;
    /* 0x18 */ s32 unkMediumParam;
    /* 0x1C */ u32 retMsg;
    /* 0x20 */ OSMesgQueue* retQueue;
    /* 0x24 */ OSMesgQueue msgQueue;
    /* 0x3C */ OSMesg msg;
    /* 0x40 */ OSIoMesg ioMesg;
} AudioAsyncLoad; // size = 0x58

typedef struct {
    /* 0x00 */ u8 medium;
    /* 0x01 */ u8 seqOrFontId;
    /* 0x02 */ u16 instId;
    /* 0x04 */ s32 unkMediumParam;
    /* 0x08 */ s32 curDevAddr;
    /* 0x0C */ u8* curRamAddr;
    /* 0x10 */ u8* ramAddr;
    /* 0x14 */ s32 status;
    /* 0x18 */ s32 bytesRemaining;
    /* 0x1C */ s8* isDone;
    /* 0x20 */ SoundFontSample sample;
    /* 0x30 */ OSMesgQueue msgqueue;
    /* 0x48 */ OSMesg msg;
    /* 0x4C */ OSIoMesg ioMesg;
} AudioSlowLoad; // size = 0x64

typedef struct AudioTableEntry {
    /* 0x00 */ uintptr_t romAddr;
    /* 0x04 */ u32 size;
    /* 0x08 */ s8 medium;
    /* 0x09 */ s8 cachePolicy;
    /* 0x0A */ s16 shortData1;
    /* 0x0C */ s16 shortData2;
    /* 0x0E */ s16 shortData3;
} AudioTableEntry; // size = 0x10

typedef struct AudioTable {
    /* 0x00 */ s16 numEntries;
    /* 0x02 */ s16 unkMediumParam;
    /* 0x04 */ uintptr_t romAddr;
    /* 0x08 */ char pad[0x8];
    /* 0x10 */ AudioTableEntry entries[1]; // (dynamic size)
} AudioTable; // size >= 0x20

typedef struct AudioTableDef
{
    /* 0x00 */ s16 numEntries;
    /* 0x02 */ s16 unkMediumParam;
    /* 0x04 */ uintptr_t romAddr;
    /* 0x08 */ char pad[0x8];
    /* 0x10 */ AudioTableEntry entries[]; // (dynamic size)
} AudioTableDef; // size >= 0x20

typedef struct {
    /* 0x00 */ OSTask task;
    /* 0x40 */ OSMesgQueue* taskQueue;
    /* 0x44 */ void* unk_44; // probably a message that gets unused.
    /* 0x48 */ char unk_48[0x8];
} AudioTask; // size = 0x50

typedef struct {
    /* 0x00 */ u8* ramAddr;
    /* 0x04 */ u32 devAddr;
    /* 0x08 */ u16 sizeUnused;
    /* 0x0A */ u16 size;
    /* 0x0C */ u8 unused;
    /* 0x0D */ u8 reuseIndex; // position in sSampleDmaReuseQueue1/2, if ttl == 0
    /* 0x0E */ u8 ttl;        // duration after which the DMA can be discarded
} SampleDma; // size = 0x10

typedef struct {
    /* 0x0000 */ char unk_0000;
    /* 0x0001 */ s8 numSynthesisReverbs;
    /* 0x0002 */ u16 unk_2;
    /* 0x0004 */ u16 unk_4;
    /* 0x0006 */ char unk_0006[0x0A];
    /* 0x0010 */ s16* curLoadedBook;
    /* 0x0014 */ NoteSubEu* noteSubsEu;
    /* 0x0018 */ SynthesisReverb synthesisReverbs[4];
    /* 0x0B38 */ char unk_0B38[0x30];
    /* 0x0B68 */ SoundFontSample* usedSamples[128];
    /* 0x0D68 */ AudioPreloadReq preloadSampleStack[128];
    /* 0x1768 */ s32 numUsedSamples;
    /* 0x176C */ s32 preloadSampleStackTop;
    /* 0x1770 */ AudioAsyncLoad asyncLoads[0x10];
    /* 0x1CF0 */ OSMesgQueue asyncLoadUnkMediumQueue;
    /* 0x1D08 */ char unk_1D08[0x40];
    /* 0x1D48 */ AudioAsyncLoad* curUnkMediumLoad;
    /* 0x1D4C */ u32 slowLoadPos;
    /* 0x1D50 */ AudioSlowLoad slowLoads[2];
    /* 0x1E18 */ OSPiHandle* cartHandle;
    /* 0x1E1C */ OSPiHandle* driveHandle;
    /* 0x1E20 */ OSMesgQueue externalLoadQueue;
    /* 0x1E38 */ OSMesg externalLoadMesgBuf[0x10];
    /* 0x1E78 */ OSMesgQueue preloadSampleQueue;
    /* 0x1E90 */ OSMesg preloadSampleMesgBuf[0x10];
    /* 0x1ED0 */ OSMesgQueue currAudioFrameDmaQueue;
    /* 0x1EE8 */ OSMesg currAudioFrameDmaMesgBuf[0x40];
    /* 0x1FE8 */ OSIoMesg currAudioFrameDmaIoMesgBuf[0x40];
    /* 0x25E8 */ OSMesgQueue syncDmaQueue;
    /* 0x2600 */ OSMesg syncDmaMesg;
    /* 0x2604 */ OSIoMesg syncDmaIoMesg;
    /* 0x261C */ SampleDma* sampleDmas;
    /* 0x2620 */ u32 sampleDmaCount;
    /* 0x2624 */ u32 sampleDmaListSize1;
    /* 0x2628 */ s32 unused2628;
    /* 0x262C */ u8 sampleDmaReuseQueue1[0x100]; // read pos <= write pos, wrapping mod 256
    /* 0x272C */ u8 sampleDmaReuseQueue2[0x100];
    /* 0x282C */ u8 sampleDmaReuseQueue1RdPos;
    /* 0x282D */ u8 sampleDmaReuseQueue2RdPos;
    /* 0x282E */ u8 sampleDmaReuseQueue1WrPos;
    /* 0x282F */ u8 sampleDmaReuseQueue2WrPos;
    /* 0x2830 */ AudioTable* sequenceTable;
    /* 0x2834 */ AudioTable* soundFontTable;
    /* 0x2838 */ AudioTable* sampleBankTable;
    /* 0x283C */ u8* sequenceFontTable;
    /* 0x2840 */ u16 numSequences;
    /* 0x2844 */ SoundFont* soundFonts;
    /* 0x2848 */ AudioBufferParameters audioBufferParameters;
    /* 0x2870 */ f32 unk_2870;
    /* 0x2874 */ s32 sampleDmaBufSize1;
    /* 0x2874 */ s32 sampleDmaBufSize2;
    /* 0x287C */ char unk_287C[0x10];
    /* 0x288C */ s32 sampleDmaBufSize;
    /* 0x2890 */ s32 maxAudioCmds;
    /* 0x2894 */ s32 numNotes;
    /* 0x2898 */ s16 tempoInternalToExternal;
    /* 0x289A */ s8 soundMode;
    /* 0x289C */ s32 totalTaskCnt;
    /* 0x28A0 */ s32 curAudioFrameDmaCount;
    /* 0x28A4 */ s32 rspTaskIdx;
    /* 0x28A8 */ s32 curAIBufIdx;
    /* 0x28AC */ struct Acmd* abiCmdBufs[2];
    /* 0x28B4 */ struct Acmd* curAbiCmdBuf;
    /* 0x28B8 */ AudioTask* currTask;
    /* 0x28BC */ char unk_28BC[0x4];
    /* 0x28C0 */ AudioTask rspTask[2];
    /* 0x2960 */ f32 unk_2960;
    /* 0x2964 */ s32 refreshRate;
    /* 0x2968 */ s16* aiBuffers[3];
    /* 0x2974 */ s16 aiBufLengths[3];
    /* 0x297C */ u32 audioRandom;
    /* 0x2980 */ s32 audioErrorFlags;
    /* 0x2984 */ volatile u32 resetTimer;
    /* 0x2988 */ char unk_2988[0x8];
    /* 0x2990 */ AudioAllocPool audioSessionPool;
    /* 0x29A0 */ AudioAllocPool externalPool;
    /* 0x29B0 */ AudioAllocPool audioInitPool;
    /* 0x29C0 */ AudioAllocPool notesAndBuffersPool;
    /* 0x29D0 */ char unk_29D0[0x20]; // probably two unused pools
    /* 0x29F0 */ AudioAllocPool cachePool;
    /* 0x2A00 */ AudioAllocPool persistentCommonPool;
    /* 0x2A10 */ AudioAllocPool temporaryCommonPool;
    /* 0x2A20 */ AudioCache seqCache;
    /* 0x2B30 */ AudioCache fontCache;
    /* 0x2C40 */ AudioCache sampleBankCache;
    /* 0x2D50 */ AudioAllocPool permanentPool;
    /* 0x2D60 */ AudioCacheEntry permanentCache[32];
    /* 0x2EE0 */ AudioSampleCache persistentSampleCache;
    /* 0x3174 */ AudioSampleCache temporarySampleCache;
    /* 0x3408 */ AudioPoolSplit4 sessionPoolSplit;
    /* 0x3418 */ AudioPoolSplit2 cachePoolSplit;
    /* 0x3420 */ AudioPoolSplit3 persistentCommonPoolSplit;
    /* 0x342C */ AudioPoolSplit3 temporaryCommonPoolSplit;
    /* 0x3438 */ u8 sampleFontLoadStatus[0x30];
    /* 0x3468 */ u8 fontLoadStatus[0x30];
    /* 0x3498 */ u8 seqLoadStatus[0x80];
    /* 0x3518 */ volatile u8 resetStatus;
    /* 0x3519 */ u8 audioResetSpecIdToLoad;
    /* 0x351C */ s32 audioResetFadeOutFramesLeft;
    /* 0x3520 */ f32* unk_3520;
    /* 0x3524 */ u8* audioHeap;
    /* 0x3528 */ u32 audioHeapSize;
    /* 0x352C */ Note* notes;
    /* 0x3530 */ SequencePlayer seqPlayers[4];
    /* 0x3AB0 */ SequenceLayer sequenceLayers[64];
    /* 0x5AB0 */ SequenceChannel sequenceChannelNone;
    /* 0x5B84 */ s32 noteSubEuOffset;
    /* 0x5B88 */ AudioListItem layerFreeList;
    /* 0x5B98 */ NotePool noteFreeLists;
    /* 0x5BD8 */ u8 cmdWrPos;
    /* 0x5BD9 */ u8 cmdRdPos;
    /* 0x5BDA */ u8 cmdQueueFinished;
    /* 0x5BDC */ u16 unk_5BDC[4];
    /* 0x5BE4 */ OSMesgQueue* audioResetQueueP;
    /* 0x5BE8 */ OSMesgQueue* taskStartQueueP;
    /* 0x5BEC */ OSMesgQueue* cmdProcQueueP;
    /* 0x5BF0 */ OSMesgQueue taskStartQueue;
    /* 0x5C08 */ OSMesgQueue cmdProcQueue;
    /* 0x5C20 */ OSMesgQueue audioResetQueue;
    /* 0x5C38 */ OSMesg taskStartMsgs[1];
    /* 0x5C3C */ OSMesg audioResetMesgs[1];
    /* 0x5C40 */ OSMesg cmdProcMsgs[4];
    /* 0x5C50 */ AudioCmd cmdBuf[0x100];
} AudioContext; // size = 0x6450

typedef struct {
    /* 0x00 */ u8 reverbVol;
    /* 0x01 */ u8 unk_1;
    /* 0x02 */ u8 pan;
    /* 0x03 */ Stereo stereo;
    /* 0x04 */ f32 frequency;
    /* 0x08 */ f32 velocity;
    /* 0x0C */ char unk_0C[0x4];
    /* 0x10 */ s16* filter;
    /* 0x14 */ u8 unk_14;
    /* 0x16 */ u16 unk_16;
} NoteSubAttributes; // size = 0x18

typedef struct {
    /* 0x00 */ u32 heapSize;
    /* 0x04 */ u32 initPoolSize;
    /* 0x08 */ u32 permanentPoolSize;
} AudioContextInitSizes; // size = 0xC

typedef struct {
    /* 0x00 */ f32 unk_00;
    /* 0x04 */ f32 unk_04;
    /* 0x08 */ f32 unk_08;
    /* 0x0C */ u16 unk_0C;
    /* 0x10 */ f32 unk_10;
    /* 0x14 */ f32 unk_14;
    /* 0x18 */ f32 unk_18;
    /* 0x1C */ u16 unk_1C;
} unk_50_s; // size = 0x20

typedef struct {
    /* 0x000 */ f32 volCur;
    /* 0x004 */ f32 volTarget;
    /* 0x008 */ f32 unk_08;
    /* 0x00C */ u16 unk_0C;
    /* 0x00E */ u8 volScales[0x4];
    /* 0x012 */ u8 volFadeTimer;
    /* 0x013 */ u8 fadeVolUpdate;
    /* 0x014 */ u32 unk_14;
    /* 0x018 */ u16 unk_18;
    /* 0x01C */ f32 unk_1C;
    /* 0x020 */ f32 unk_20;
    /* 0x024 */ f32 unk_24;
    /* 0x028 */ u16 unk_28;
    /* 0x02C */ u32 unk_2C[8];
    /* 0x04C */ u8 unk_4C;
    /* 0x04D */ u8 unk_4D;
    /* 0x04E */ u8 unk_4E;
    /* 0x050 */ unk_50_s unk_50[0x10];
    /* 0x250 */ u16 unk_250;
    /* 0x252 */ u16 unk_252;
    /* 0x254 */ u16 unk_254;
    /* 0x256 */ u16 unk_256;
    /* 0x258 */ u16 unk_258;
    /* 0x25C */ u32 unk_25C;
    /* 0x260 */ u8 unk_260;
} unk_D_8016E750; // size = 0x264

typedef enum {
    /* 0 */ BANK_PLAYER,
    /* 1 */ BANK_ITEM,
    /* 2 */ BANK_ENV,
    /* 3 */ BANK_ENEMY,
    /* 4 */ BANK_SYSTEM,
    /* 5 */ BANK_OCARINA,
    /* 6 */ BANK_VOICE
} SoundBankTypes;

typedef enum {
    /* 0 */ SFX_STATE_EMPTY,
    /* 1 */ SFX_STATE_QUEUED,
    /* 2 */ SFX_STATE_READY,
    /* 3 */ SFX_STATE_PLAYING_REFRESH,
    /* 4 */ SFX_STATE_PLAYING_1,
    /* 5 */ SFX_STATE_PLAYING_2
} SfxState;

typedef struct {
    /* 0x00 */ f32*     posX;
    /* 0x04 */ f32*     posY;
    /* 0x08 */ f32*     posZ;
    /* 0x0C */ u8       token;
    /* 0x10 */ f32*     freqScale;
    /* 0x14 */ f32*     vol;
    /* 0x18 */ s8*      reverbAdd;
    /* 0x1C */ f32      dist;
    /* 0x20 */ u32      priority; // lower is more prioritized
    /* 0x24 */ u8       sfxImportance;
    /* 0x26 */ u16      sfxParams;
    /* 0x28 */ u16      sfxId;
    /* 0x2A */ u8       state; // uses SfxState enum
    /* 0x2B */ u8       freshness;
    /* 0x2C */ u8       prev;
    /* 0x2D */ u8       next;
    /* 0x2E */ u8       channelIdx;
    /* 0x2F */ u8       unk_2F;
} SoundBankEntry; // size = 0x30

/*
 * SFX IDs
 *
 * index    0000000111111111    observed in audio code
 * & 200    0000001000000000    single bit
 * & 400    0000010000000000    single bit
 * & 800    0000100000000000    single bit, what we currently call SFX_FLAG
 * & 600    0000011000000000    2 bits
 * & A00    0000101000000000    2 bits
 * & C00    0000110000000000    2 bits, observed in audio code
 * & E00    0000111000000000    all 3 bits
 * bank     1111000000000000    observed in audio code
 */

#define SFX_BANK_SHIFT(sfxId)   (((sfxId) >> 12) & 0xFF)

#define SFX_BANK_MASK(sfxId)    ((sfxId) & 0xF000)

#define SFX_INDEX(sfxId)    ((sfxId) & 0x01FF)
#define SFX_BANK(sfxId)     SFX_BANK_SHIFT(SFX_BANK_MASK(sfxId))

typedef struct {
    u32 priority; // lower is more prioritized
    u8 entryIndex;
} ActiveSound;

typedef struct {
    u8 importance;
    u16 params;
} SoundParams;

typedef struct {
    /* 0x0000 */ u8 noteIdx;
    /* 0x0001 */ u8 unk_01;
    /* 0x0002 */ u16 unk_02;
    /* 0x0004 */ u8 volume;
    /* 0x0005 */ u8 vibrato;
    /* 0x0006 */ s8 tone;
    /* 0x0007 */ u8 semitone;
} OcarinaNote;  // size = 0x8

typedef struct {
    u8 len;
    u8 notesIdx[8];
} OcarinaSongInfo;

typedef struct {
    u8 noteIdx;
    u8 state;   // original name: "status"
    u8 pos;     // original name: "locate"
} OcarinaStaff;

typedef enum {
    /*  0 */ OCARINA_NOTE_A,
    /*  1 */ OCARINA_NOTE_C_DOWN,
    /*  2 */ OCARINA_NOTE_C_RIGHT,
    /*  3 */ OCARINA_NOTE_C_LEFT,
    /*  4 */ OCARINA_NOTE_C_UP,
    /* -1 */ OCARINA_NOTE_INVALID = 0xFF
} OcarinaNoteIdx;

#define gTatumsPerBeat (D_8014A6C0[1])

extern void(*D_801755D0)(void);

/*
Acmd* AudioSynth_Update(Acmd* cmdStart, s32* cmdCnt, s16* aiStart, s32 aiBufLen);
void AudioHeap_DiscardFont(s32 fontId);
void AudioHeap_DiscardSequence(s32 seqId);
void AudioHeap_WritebackDCache(void* mem, u32 size);
void* AudioHeap_AllocZeroedAttemptExternal(AudioAllocPool* pool, u32 size);
void* AudioHeap_AllocAttemptExternal(AudioAllocPool* pool, u32 size);
void* AudioHeap_AllocDmaMemory(AudioAllocPool* pool, u32 size);
void* AudioHeap_AllocDmaMemoryZeroed(AudioAllocPool* pool, u32 size);
void* AudioHeap_AllocZeroed(AudioAllocPool* pool, u32 size);
void* AudioHeap_Alloc(AudioAllocPool* pool, u32 size);
void AudioHeap_AllocPoolInit(AudioAllocPool* pool, void* mem, u32 size);
void AudioHeap_PersistentCacheClear(AudioPersistentCache* persistent);
void AudioHeap_TemporaryCacheClear(AudioTemporaryCache* temporary);
void AudioHeap_PopCache(s32 tableType);
void AudioHeap_InitMainPools(s32 sizeForAudioInitPool);
void* AudioHeap_AllocCached(s32 tableType, s32 size, s32 cache, s32 id);
void* AudioHeap_SearchCaches(s32 tableType, s32 arg1, s32 id);
void* AudioHeap_SearchRegularCaches(s32 tableType, s32 cache, s32 id);
void AudioHeap_LoadFilter(s16* filter, s32 filter1, s32 filter2);
s32 AudioHeap_ResetStep(void);
void AudioHeap_Init(void);
void* AudioHeap_SearchPermanentCache(s32 tableType, s32 id);
void* AudioHeap_AllocPermanent(s32 tableType, s32 id, u32 size);
void* AudioHeap_AllocSampleCache(u32 size, s32 fontId, void* sampleAddr, s8 medium, s32 cache);
void AudioHeap_ApplySampleBankCache(s32 sampleBankId);
void AudioLoad_DecreaseSampleDmaTtls(void);
void* AudioLoad_DmaSampleData(uintptr_t devAddr, u32 size, s32 arg2, u8* dmaIndexRef, s32 medium);
void AudioLoad_InitSampleDmaBuffers(s32 arg0);
s32 AudioLoad_IsFontLoadComplete(s32 fontId);
s32 AudioLoad_IsSeqLoadComplete(s32 seqId);
void AudioLoad_SetFontLoadStatus(s32 fontId, s32 status);
void AudioLoad_SetSeqLoadStatus(s32 seqId, s32 status);
void AudioLoad_SyncLoadSeqParts(s32 seqId, s32 arg1);
s32 AudioLoad_SyncLoadInstrument(s32 fontId, s32 instId, s32 drumId);
void AudioLoad_AsyncLoadSeq(s32 seqId, s32 arg1, s32 retData, OSMesgQueue* retQueue);
void AudioLoad_AsyncLoadSampleBank(s32 sampleBankId, s32 arg1, s32 retData, OSMesgQueue* retQueue);
void AudioLoad_AsyncLoadFont(s32 fontId, s32 arg1, s32 retData, OSMesgQueue* retQueue);
u8* AudioLoad_GetFontsForSequence(s32 seqId, u32* arg1);
void AudioLoad_DiscardSeqFonts(s32 seqId);
s32 AudioLoad_SyncInitSeqPlayer(s32 playerIdx, s32 seqId, s32 arg2);
s32 AudioLoad_SyncInitSeqPlayerSkipTicks(s32 playerIdx, s32 seqId, s32 arg2);
void AudioLoad_ProcessLoads(s32 resetStatus);
void AudioLoad_SetDmaHandler(DmaHandler callback);
void AudioLoad_Init(void* heap, u32 heapSize);
void AudioLoad_InitSlowLoads(void);
s32 AudioLoad_SlowLoadSample(s32 arg0, s32 arg1, s8* arg2);
s32 AudioLoad_SlowLoadSeq(s32 playerIdx, u8* ramAddr, s8* arg2);
void AudioLoad_InitAsyncLoads(void);
void AudioLoad_LoadPermanentSamples(void);
void AudioLoad_ScriptLoad(s32 tableType, s32 arg1, s8* arg2);
void AudioLoad_ProcessScriptLoads(void);
void AudioLoad_InitScriptLoads(void);
AudioTask* func_800E4FE0(void);
void Audio_QueueCmdF32(u32 arg0, f32 arg1);
void Audio_QueueCmdS32(u32 arg0, s32 arg1);
void Audio_QueueCmdS8(u32 arg0, s8 arg1);
void Audio_QueueCmdU16(u32 arg0, u16 arg1);
s32 Audio_ScheduleProcessCmds(void);
u32 func_800E5E20(u32* arg0);
u8* func_800E5E84(s32 arg0, u32* arg1);
s32 func_800E5EDC(void);
s32 func_800E5F88(s32 arg0);
void Audio_PreNMIInternal(void);
s32 func_800E6680(void);
u32 Audio_NextRandom(void);
void Audio_InitMesgQueues(void);
void Audio_InvalDCache(void* buf, s32 size);
void Audio_WritebackDCache(void* mem, s32 size);
s32 osAiSetNextBuffer(void*, u32);
void Audio_InitNoteSub(Note* note, NoteSubEu* sub, NoteSubAttributes* attrs);
void Audio_NoteSetResamplingRate(NoteSubEu* noteSubEu, f32 resamplingRateInput);
void Audio_NoteInit(Note* note);
void Audio_NoteDisable(Note* note);
void Audio_ProcessNotes(void);
SoundFontSound* Audio_InstrumentGetSound(Instrument* instrument, s32 semitone);
Instrument* Audio_GetInstrumentInner(s32 fontId, s32 instId);
Drum* Audio_GetDrum(s32 fontId, s32 drumId);
SoundFontSound* Audio_GetSfx(s32 fontId, s32 sfxId);
s32 Audio_SetFontInstrument(s32 instrumentType, s32 fontId, s32 index, void* value);
void Audio_SeqLayerDecayRelease(SequenceLayer* layer, s32 target);
void Audio_SeqLayerNoteDecay(SequenceLayer* layer);
void Audio_SeqLayerNoteRelease(SequenceLayer* layer);
s32 Audio_BuildSyntheticWave(Note* note, SequenceLayer* layer, s32 waveId);
void Audio_InitSyntheticWave(Note* note, SequenceLayer* layer);
void Audio_InitNoteList(AudioListItem* list);
void Audio_InitNoteLists(NotePool* pool);
void Audio_InitNoteFreeList(void);
void Audio_NotePoolClear(NotePool* pool);
void Audio_NotePoolFill(NotePool* pool, s32 count);
void Audio_AudioListPushFront(AudioListItem* list, AudioListItem* item);
void Audio_AudioListRemove(AudioListItem* item);
Note* Audio_FindNodeWithPrioLessThan(AudioListItem* list, s32 limit);
void Audio_NoteInitForLayer(Note* note, SequenceLayer* layer);
void func_800E82C0(Note* note, SequenceLayer* layer);
void Audio_NoteReleaseAndTakeOwnership(Note* note, SequenceLayer* layer);
Note* Audio_AllocNoteFromDisabled(NotePool* pool, SequenceLayer* layer);
Note* Audio_AllocNoteFromDecaying(NotePool* pool, SequenceLayer* layer);
Note* Audio_AllocNoteFromActive(NotePool* pool, SequenceLayer* layer);
Note* Audio_AllocNote(SequenceLayer* layer);
void Audio_NoteInitAll(void);
void Audio_SequenceChannelProcessSound(SequenceChannel* channel, s32 recalculateVolume, s32 b);
void Audio_SequencePlayerProcessSound(SequencePlayer* seqPlayer);
f32 Audio_GetPortamentoFreqScale(Portamento* p);
s16 Audio_GetVibratoPitchChange(VibratoState* vib);
f32 Audio_GetVibratoFreqScale(VibratoState* vib);
void Audio_NoteVibratoUpdate(Note* note);
void Audio_NoteVibratoInit(Note* note);
void Audio_NotePortamentoInit(Note* note);
void Audio_AdsrInit(AdsrState* adsr, AdsrEnvelope* envelope, s16* volOut);
f32 Audio_AdsrUpdate(AdsrState* adsr);
void AudioSeq_SequenceChannelDisable(SequenceChannel* channel);
void AudioSeq_SequencePlayerDisableAsFinished(SequencePlayer* seqPlayer);
void AudioSeq_SequencePlayerDisable(SequencePlayer* seqPlayer);
void AudioSeq_AudioListPushBack(AudioListItem* list, AudioListItem* item);
void* AudioSeq_AudioListPopBack(AudioListItem* list);
void AudioSeq_ProcessSequences(s32 arg0);
void AudioSeq_SkipForwardSequence(SequencePlayer* seqPlayer);
void AudioSeq_ResetSequencePlayer(SequencePlayer* seqPlayer);
void AudioSeq_InitSequencePlayerChannels(s32 playerIdx);
void AudioSeq_InitSequencePlayers(void);
void func_800ECC04(u16);
void Audio_OcaSetInstrument(u8);
void Audio_OcaSetSongPlayback(s8 songIdxPlusOne, s8 playbackState);
void Audio_OcaSetRecordingState(u8);
OcarinaStaff* Audio_OcaGetRecordingStaff(void);
OcarinaStaff* Audio_OcaGetPlayingStaff(void);
OcarinaStaff* Audio_OcaGetDisplayingStaff(void);
void Audio_OcaMemoryGameStart(u8 minigameIdx);
s32 Audio_OcaMemoryGameGenNote(void);
void func_800EE824(void);
void AudioDebug_ScrPrt(const s8* str, u16 num);
void func_800F3054(void);
void Audio_SetSoundProperties(u8 bankId, u8 entryIdx, u8 channelIdx);
void func_800F3F3C(u8);
void func_800F4010(Vec3f* pos, u16 sfxId, f32);
void Audio_PlaySoundRandom(Vec3f* pos, u16 baseSfxId, u8 randLim);
void func_800F4138(Vec3f* pos, u16 sfxId, f32);
void func_800F4190(Vec3f* pos, u16 sfxId);
void func_800F436C(Vec3f* pos, u16 sfxId, f32 arg2);
void func_800F4414(Vec3f* pos, u16 sfxId, f32);
void func_800F44EC(s8 arg0, s8 arg1);
void func_800F4524(Vec3f* pos, u16 sfxId, s8 arg2);
void func_800F4254(Vec3f* pos, u8 arg1);
void func_800F436C(Vec3f*, u16 sfxId, f32 arg2);
void func_800F4414(Vec3f*, u16 sfxId, f32 arg2);
void Audio_PlaySoundRiver(Vec3f* pos, f32 freqScale);
void Audio_PlaySoundWaterfall(Vec3f* pos, f32 freqScale);
void func_800F47BC(void);
void func_800F47FC(void);
void func_800F483C(u8 targetVol, u8 volFadeTimer);
void func_800F4870(u8);
void func_800F4A54(u8);
void Audio_PlaySoundIncreasinglyTransposed(Vec3f* pos, s16 sfxId, u8* semitones);
void Audio_ResetIncreasingTranspose(void);
void Audio_PlaySoundTransposed(Vec3f* pos, u16 sfxId, s8 semitone);
void func_800F4C58(Vec3f* pos, u16 sfxId, u8);
void func_800F4E30(Vec3f* pos, f32);
void Audio_ClearSariaBgm(void);
void Audio_ClearSariaBgmAtPos(Vec3f* pos);
void Audio_PlaySariaBgm(Vec3f* pos, u16 seqId, u16 distMax);
void Audio_ClearSariaBgm2(void);
void func_800F5510(u16 seqId);
void func_800F5550(u16 seqId);
void func_800F574C(f32 arg0, u8 arg2);
void func_800F5718(void);
void func_800F5918(void);
void func_800F595C(u16);
void func_800F59E8(u16);
s32 func_800F5A58(u8);
void func_800F5ACC(u16 bgmID);
void func_800F5B58(void);
void func_800F5BF0(u8 arg0);
void Audio_PlayFanfare(u16);
void func_800F5C2C(void);
void func_800F5E18(u8 playerIdx, u16 seqId, u8 fadeTimer, s8 arg3, s8 arg4);
void Audio_SetSequenceMode(u8 seqMode);
void Audio_SetBgmEnemyVolume(f32 dist);
void func_800F6268(f32 dist, u16);
void func_800F64E0(u8 arg0);
void func_800F6584(u8 arg0);
void Audio_SetEnvReverb(s8 reverb);
void Audio_SetCodeReverb(s8 reverb);
void func_800F6700(s8 outputMode);
void Audio_SetBaseFilter(u8);
void Audio_SetExtraFilter(u8);
void Audio_SetCutsceneFlag(s8 flag);
void Audio_PlaySoundIfNotInCutscene(u16 sfxId);
void func_800F6964(u16);
void func_800F6AB0(u16);
// ? Audio_DisableAllSeq(?);
// ? func_800F6BB8(?);
void Audio_PreNMI();
// ? func_800F6C34(?);
void func_800F6D58(u8, u8, u8);
// ? func_800F6E7C(?);
void func_800F6FB4(u8);
void Audio_Init();
void Audio_InitSound();
void func_800F7170(void);
void func_800F71BC(s32 arg0);
void Audio_SetSoundBanksMute(u16 muteMask);
void Audio_QueueSeqCmdMute(u8 channelIdx);
void Audio_ClearBGMMute(u8 channelIdx);
void Audio_PlaySoundGeneral(u16 sfxId, Vec3f* pos, u8 token, f32* freqScale, f32* a4, s8* reverbAdd);
void Audio_ProcessSoundRequest(void);
void Audio_ChooseActiveSounds(u8 bankId);
void Audio_PlayActiveSounds(u8 bankId);
void Audio_StopSfxByBank(u8 bankId);
void func_800F8884(u8, Vec3f*);
void Audio_StopSfxByPosAndBank(u8, Vec3f*);
void Audio_StopSfxByPos(Vec3f*);
void func_800F9280(u8 playerIdx, u8 seqId, u8 arg2, u16 fadeTimer);
void Audio_QueueSeqCmd(u32 bgmID);
void Audio_StopSfxByPosAndId(Vec3f* pos, u16 sfxId);
void Audio_StopSfxByTokenAndId(u8, u16);
void Audio_StopSfxById(u32 sfxId);
void Audio_ProcessSoundRequests(void);
void func_800F8F88(void);
u8 Audio_IsSfxPlaying(u32 sfxId);
void Audio_ResetSounds(void);
void func_800F9474(u8, u16);
void func_800F94FC(u32);
void Audio_ProcessSeqCmd(u32);
void Audio_ProcessSeqCmds(void);

u16 func_800FA0B4(u8);
s32 func_800FA11C(u32 arg0, u32 arg1);
void func_800FA174(u8);
void func_800FA18C(u8, u8);
void Audio_SetVolScale(u8 playerIdx, u8 scaleIdx, u8 targetVol, u8 volFadeTimer);
void func_800FA3DC(void);
u8 func_800FAD34(void);
void func_800FADF8(void);
void func_800FAEB4(void);
*/


extern AudioContext gAudioContext;
extern OcarinaNote* gScarecrowCustomSongPtr;
extern u8* gScarecrowSpawnSongPtr;
extern OcarinaSongInfo gOcarinaSongNotes[];
extern SoundParams* gSoundParams[7];
extern f32 gBendPitchOneOctaveFrequencies[256];

extern SoundBankEntry* gSoundBanks[7];

extern s16* gWaveSamples[9];
extern f32 gBendPitchOneOctaveFrequencies[256];
extern f32 gBendPitchTwoSemitonesFrequencies[256];
extern f32 gNoteFrequencies[];
extern u8 gDefaultShortNoteVelocityTable[16];
extern u8 gDefaultShortNoteGateTimeTable[16];
extern AdsrEnvelope gDefaultEnvelope[4];
extern NoteSubEu gZeroNoteSub;
extern NoteSubEu gDefaultNoteSub;
extern u16 gHeadsetPanQuantization[64];
extern s16 D_8012FBA8[];
extern f32 gHeadsetPanVolume[128];
extern f32 gStereoPanVolume[128];
extern f32 gDefaultPanVolume[128];
extern s16 sLowPassFilterData[16 * 8];
extern s16 sHighPassFilterData[15 * 8];
extern s32 gAudioContextInitalized;
extern u8 gIsLargeSoundBank[7];
extern u8 gChannelsPerBank[4][7];
extern u8 gUsedChannelsPerBank[4][7];
extern u8 gMorphaTransposeTable[16];
extern u8* gFrogsSongPtr;

extern char D_80133390[];
extern char D_80133398[];
extern u8 gSfxChannelLayout;
extern Vec3f D_801333D4;
extern f32 D_801333E0;
extern s8 D_801333E8;
extern u8 D_801333F0;
extern u8 gAudioSfxSwapOff;
extern u8 D_80133408;
extern u8 D_8013340C;
extern u8 gAudioSpecId;
extern u8 D_80133418;




extern const AudioContextInitSizes D_8014A6C4;
extern s16 gOcarinaSongItemMap[];
extern AudioTableDef gSoundFontTable;
extern u16 gSequenceFontTable[];
extern AudioTableDef gSequenceTable;
extern AudioTableDef gSampleBankTable;
extern u8 gAudioHeap[0x38000];

extern ActiveSound gActiveSounds[7][MAX_CHANNELS_PER_BANK]; // total size = 0xA8
extern u8 gSoundBankMuted[];
extern u16 gAudioSfxSwapSource[10];
extern u16 gAudioSfxSwapTarget[10];
extern u8 gAudioSfxSwapMode[10];
extern u8 D_8016E348[4];
extern u32 sAudioSeqCmds[0x100];
extern unk_D_8016E750 D_8016E750[4];
extern AudioContext gAudioContext;
extern void(*D_801755D0)(void);





extern const s16 D_8014A6C0[];
extern AudioSpec gAudioSpecs[18];
