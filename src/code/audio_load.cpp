#define INTERNAL_SRC_CODE_AUDIO_LOAD_C
#include "ultra64.h"
#include "global.h"
#include "z64audio.h"
#include "audio/bank.h"
#include "audio/seq.h"
#include "audio/table.h"
#include <string.h>
#include "segment_symbols.h"
#include "def/audio_heap.h"
#include "def/audio_init_params.h"
#include "def/audio_load.h"
#include "def/audio_playback.h"
#include "def/audio_seqplayer.h"
#include "def/cartrominit.h"
#include "def/audio_rsp.h"
#include "def/code_800E6840.h"
#include "def/audio_bank.h"
#include "def/createmesgqueue.h"
#include "def/heaps.h"
#include "def/recvmesg.h"

#include "redef_msgqueue.h"

#define BEE32(s) s

extern u32 osTvType;

#define MK_ASYNC_MSG(retData, tableType, id, status) (((retData) << 24) | ((tableType) << 16) | ((id) << 8) | (status))
#define ASYNC_TBLTYPE(v) ((u8)(v >> 16))
#define ASYNC_ID(v) ((u8)(v >> 8))
#define ASYNC_STATUS(v) ((u8)(v >> 0))

typedef enum {
    /* 0 */ LOAD_STATUS_WAITING,
    /* 1 */ LOAD_STATUS_START,
    /* 2 */ LOAD_STATUS_LOADING,
    /* 3 */ LOAD_STATUS_DONE
} SlowLoadStatus;

typedef struct {
    /* 0x00 */ s32 sampleBankId1;
    /* 0x04 */ s32 sampleBankId2;
    /* 0x08 */ s32 baseAddr1;
    /* 0x0C */ s32 baseAddr2;
    /* 0x10 */ u32 medium1;
    /* 0x14 */ u32 medium2;
} RelocInfo; // size = 0x18

// opaque type for unpatched sound font data (should maybe get rid of this?)
typedef void SoundFontData;

/* forward declarations */
s32 AudioLoad_SyncInitSeqPlayerInternal(s32 playerIdx, s32 seqId, s32 skipTicks);
SoundFontData* AudioLoad_SyncLoadFont(u32 fontId);
SoundFontSample* AudioLoad_GetFontSample(s32 fontId, s32 instId);
void AudioLoad_ProcessAsyncLoads(s32 resetStatus);
void AudioLoad_ProcessAsyncLoadUnkMedium(AudioAsyncLoad* asyncLoad, s32 resetStatus);
void AudioLoad_ProcessAsyncLoad(AudioAsyncLoad* asyncLoad, s32 resetStatus);
void AudioLoad_RelocateFontAndPreloadSamples(s32 fontId, SoundFontData* mem, RelocInfo* relocInfo, s32 temporary);
void AudioLoad_RelocateSample(SoundFontSound* sound, SoundFontData* mem, RelocInfo* relocInfo);
void AudioLoad_DiscardFont(s32 fontId);
u32 AudioLoad_TrySyncLoadSampleBank(u32 sampleBankId, u32* outMedium, s32 noLoad);
void* AudioLoad_SyncLoad(u32 tableType, u32 tableId, s32* didAllocate);
u32 AudioLoad_GetRealTableIndex(s32 tableType, u32 tableId);
void* AudioLoad_SearchCaches(s32 tableType, s32 id);
AudioTable* AudioLoad_GetLoadTable(s32 tableType);
void AudioLoad_SyncDma(Pointer devAddr, Pointer addr, size_t size, s32 medium);
void AudioLoad_SyncDmaUnkMedium(Pointer devAddr, Pointer addr, size_t size, s32 unkMediumParam);
s32 AudioLoad_Dma(OSIoMesg* mesg, u32 priority, s32 direction, Pointer devAddr, Pointer ramAddr, size_t size,
                  OSMesgQueue* reqQueue, s32 medium, const char* dmaFuncType);
void* AudioLoad_AsyncLoadInner(s32 tableType, s32 id, s32 nChunks, s32 retData, OSMesgQueue* retQueue);
AudioAsyncLoad* AudioLoad_StartAsyncLoadUnkMedium(s32 unkMediumParam, u32 devAddr, void* ramAddr, s32 size, s32 medium,
                                                  s32 nChunks, OSMesgQueue* retQueue, s32 retMsg);
AudioAsyncLoad* AudioLoad_StartAsyncLoad(u32 devAddr, void* ramAddr, u32 size, s32 medium, s32 nChunks,
                                         OSMesgQueue* retQueue, s32 retMsg);
void AudioLoad_AsyncDma(AudioAsyncLoad* asyncLoad, u32 size);
void AudioLoad_AsyncDmaUnkMedium(u32 devAddr, void* ramAddr, u32 size, s16 arg3);
u8* AudioLoad_SyncLoadSeq(s32 seqId);
s32 AudioLoad_ProcessSamplePreloads(s32 resetStatus);
void AudioLoad_DmaSlowCopy(AudioSlowLoad* slowLoad, s32 size);
void AudioLoad_ProcessSlowLoads(s32 resetStatus);
void AudioLoad_DmaSlowCopyUnkMedium(Pointer devAddr, Pointer ramAddr, size_t size, s32 arg3);

OSMesgQueue sScriptLoadQueue;
OSMesg sScriptLoadMesgBuf[0x10];
s8* sScriptLoadDonePointers[0x10];
s32 sAudioLoadPad1[2]; // file padding

s32 D_8016B780;
s32 sAudioLoadPad2[4]; // double file padding?

DmaHandler sDmaHandler = 0;//osEPiStartDma;
void* sUnusedHandler = NULL;

s32 gAudioContextInitalized = false;

void AudioLoad_DecreaseSampleDmaTtls(void) {
    u32 i;

    for (i = 0; i < gAudioContext.sampleDmaListSize1; i++) {
        SampleDma* dma = &gAudioContext.sampleDmas[i];

        if (dma->ttl != 0) {
            dma->ttl--;
            if (dma->ttl == 0) {
                dma->reuseIndex = gAudioContext.sampleDmaReuseQueue1WrPos;
                gAudioContext.sampleDmaReuseQueue1[gAudioContext.sampleDmaReuseQueue1WrPos] = i;
                gAudioContext.sampleDmaReuseQueue1WrPos++;
            }
        }
    }

    for (i = gAudioContext.sampleDmaListSize1; i < gAudioContext.sampleDmaCount; i++) {
        SampleDma* dma = &gAudioContext.sampleDmas[i];

        if (dma->ttl != 0) {
            dma->ttl--;
            if (dma->ttl == 0) {
                dma->reuseIndex = gAudioContext.sampleDmaReuseQueue2WrPos;
                gAudioContext.sampleDmaReuseQueue2[gAudioContext.sampleDmaReuseQueue2WrPos] = i;
                gAudioContext.sampleDmaReuseQueue2WrPos++;
            }
        }
    }

    gAudioContext.unused2628 = 0;
}

void* AudioLoad_DmaSampleData(Pointer devAddr, size_t size, s32 arg2, u8* dmaIndexRef, s32 medium) {
    s32 pad1;
    SampleDma* dma = NULL;
    s32 hasDma = false;
    Pointer dmaDevAddr;
    u32 pad2;
    u32 dmaIndex;
    u32 transfer;
    s32 bufferPos;
    u32 i;
    return devAddr.buffer();

    if (arg2 != 0 || *dmaIndexRef >= gAudioContext.sampleDmaListSize1) {
        for (i = gAudioContext.sampleDmaListSize1; i < gAudioContext.sampleDmaCount; i++) {
            dma = &gAudioContext.sampleDmas[i];
            bufferPos = (devAddr - dma->devAddr).get();
            if (0 <= bufferPos && (u32)bufferPos <= dma->size - size) {
                // We already have a DMA request for this memory range.
                if (dma->ttl == 0 &&
                    gAudioContext.sampleDmaReuseQueue2RdPos != gAudioContext.sampleDmaReuseQueue2WrPos) {
                    // Move the DMA out of the reuse queue, by swapping it with the
                    // read pos, and then incrementing the read pos.
                    if (dma->reuseIndex != gAudioContext.sampleDmaReuseQueue2RdPos) {
                        gAudioContext.sampleDmaReuseQueue2[dma->reuseIndex] =
                            gAudioContext.sampleDmaReuseQueue2[gAudioContext.sampleDmaReuseQueue2RdPos];
                        gAudioContext
                            .sampleDmas[gAudioContext.sampleDmaReuseQueue2[gAudioContext.sampleDmaReuseQueue2RdPos]]
                            .reuseIndex = dma->reuseIndex;
                    }
                    gAudioContext.sampleDmaReuseQueue2RdPos++;
                }
                dma->ttl = 32;
                *dmaIndexRef = (u8)i;
                return &dma->ramAddr[(devAddr - dma->devAddr).get()];
            }
        }

        if (arg2 == 0) {
            goto search_short_lived;
        }

        if (gAudioContext.sampleDmaReuseQueue2RdPos != gAudioContext.sampleDmaReuseQueue2WrPos && arg2 != 0) {
            // Allocate a DMA from reuse queue 2, unless full.
            dmaIndex = gAudioContext.sampleDmaReuseQueue2[gAudioContext.sampleDmaReuseQueue2RdPos];
            gAudioContext.sampleDmaReuseQueue2RdPos++;
            dma = gAudioContext.sampleDmas + dmaIndex;
            hasDma = true;
        }
    } else {
    search_short_lived:
        dma = gAudioContext.sampleDmas + *dmaIndexRef;
        i = 0;
    again:
        bufferPos = (devAddr - dma->devAddr).get();
        if (0 <= bufferPos && (u32)bufferPos <= dma->size - size) {
            // We already have DMA for this memory range.
            if (dma->ttl == 0) {
                // Move the DMA out of the reuse queue, by swapping it with the
                // read pos, and then incrementing the read pos.
                if (dma->reuseIndex != gAudioContext.sampleDmaReuseQueue1RdPos) {
                    gAudioContext.sampleDmaReuseQueue1[dma->reuseIndex] =
                        gAudioContext.sampleDmaReuseQueue1[gAudioContext.sampleDmaReuseQueue1RdPos];
                    gAudioContext
                        .sampleDmas[gAudioContext.sampleDmaReuseQueue1[gAudioContext.sampleDmaReuseQueue1RdPos]]
                        .reuseIndex = dma->reuseIndex;
                }
                gAudioContext.sampleDmaReuseQueue1RdPos++;
            }
            dma->ttl = 2;
            return dma->ramAddr + (devAddr - dma->devAddr).get();
        }
        dma = gAudioContext.sampleDmas + i++;
        if (i <= gAudioContext.sampleDmaListSize1) {
            goto again;
        }
    }

    if (!hasDma) {
        if (gAudioContext.sampleDmaReuseQueue1RdPos == gAudioContext.sampleDmaReuseQueue1WrPos) {
            return NULL;
        }
        // Allocate a DMA from reuse queue 1.
        dmaIndex = gAudioContext.sampleDmaReuseQueue1[gAudioContext.sampleDmaReuseQueue1RdPos++];
        dma = gAudioContext.sampleDmas + dmaIndex;
        hasDma = true;
    }

    transfer = dma->size;
    dmaDevAddr = devAddr.get() & ~0xF;
    dma->ttl = 3;
    dma->devAddr = dmaDevAddr;
    dma->sizeUnused = transfer;
    AudioLoad_Dma(nullptr, OS_MESG_PRI_NORMAL,
                  OS_READ, dmaDevAddr.get(), dma->ramAddr, transfer, nullptr, medium,
                  "SUPERDMA");
    *dmaIndexRef = dmaIndex;
    return (devAddr - dmaDevAddr).get() + dma->ramAddr;
}

void AudioLoad_InitSampleDmaBuffers(s32 arg0) {
    SampleDma* dma;
    s32 i;
    s32 t2;
    s32 j;

    gAudioContext.sampleDmaBufSize = gAudioContext.sampleDmaBufSize1;
    gAudioContext.sampleDmas =
        (SampleDma*)AudioHeap_Alloc(&gAudioContext.notesAndBuffersPool,
                        4 * gAudioContext.numNotes * sizeof(SampleDma) * gAudioContext.audioBufferParameters.specUnk4);
    t2 = 3 * gAudioContext.numNotes * gAudioContext.audioBufferParameters.specUnk4;
    for (i = 0; i < t2; i++) {
        dma = &gAudioContext.sampleDmas[gAudioContext.sampleDmaCount];
        dma->ramAddr =
            (u8*)AudioHeap_AllocAttemptExternal(&gAudioContext.notesAndBuffersPool, gAudioContext.sampleDmaBufSize);
        if (dma->ramAddr == NULL) {
            break;
        } else {
            AudioHeap_WritebackDCache(dma->ramAddr, gAudioContext.sampleDmaBufSize);
            dma->size = gAudioContext.sampleDmaBufSize;
            dma->devAddr = 0;
            dma->sizeUnused = 0;
            dma->unused = 0;
            dma->ttl = 0;
            gAudioContext.sampleDmaCount++;
        }
    }

    for (i = 0; (u32)i < gAudioContext.sampleDmaCount; i++) {
        gAudioContext.sampleDmaReuseQueue1[i] = i;
        gAudioContext.sampleDmas[i].reuseIndex = i;
    }

    for (i = gAudioContext.sampleDmaCount; i < 0x100; i++) {
        gAudioContext.sampleDmaReuseQueue1[i] = 0;
    }

    gAudioContext.sampleDmaReuseQueue1RdPos = 0;
    gAudioContext.sampleDmaReuseQueue1WrPos = gAudioContext.sampleDmaCount;
    gAudioContext.sampleDmaListSize1 = gAudioContext.sampleDmaCount;
    gAudioContext.sampleDmaBufSize = gAudioContext.sampleDmaBufSize2;

    for (j = 0; j < gAudioContext.numNotes; j++) {
        dma = &gAudioContext.sampleDmas[gAudioContext.sampleDmaCount];
        dma->ramAddr =
            (u8*)AudioHeap_AllocAttemptExternal(&gAudioContext.notesAndBuffersPool, gAudioContext.sampleDmaBufSize);
        if (dma->ramAddr == NULL) {
            break;
        } else {
            AudioHeap_WritebackDCache(dma->ramAddr, gAudioContext.sampleDmaBufSize);
            dma->size = gAudioContext.sampleDmaBufSize;
            dma->devAddr = 0U;
            dma->sizeUnused = 0;
            dma->unused = 0;
            dma->ttl = 0;
            gAudioContext.sampleDmaCount++;
        }
    }

    for (i = gAudioContext.sampleDmaListSize1; (u32)i < gAudioContext.sampleDmaCount; i++) {
        gAudioContext.sampleDmaReuseQueue2[i - gAudioContext.sampleDmaListSize1] = i;
        gAudioContext.sampleDmas[i].reuseIndex = i - gAudioContext.sampleDmaListSize1;
    }

    for (i = gAudioContext.sampleDmaCount; i < 0x100; i++) {
        gAudioContext.sampleDmaReuseQueue2[i] = gAudioContext.sampleDmaListSize1;
    }

    gAudioContext.sampleDmaReuseQueue2RdPos = 0;
    gAudioContext.sampleDmaReuseQueue2WrPos = gAudioContext.sampleDmaCount - gAudioContext.sampleDmaListSize1;
}

s32 AudioLoad_IsFontLoadComplete(s32 fontId) {
    if (fontId == 0xFF) {
        return true;
    } else if (gAudioContext.fontLoadStatus[fontId] >= 2) {
        return true;
    } else if (gAudioContext.fontLoadStatus[AudioLoad_GetRealTableIndex(FONT_TABLE, fontId)] >= 2) {
        return true;
    } else {
        return false;
    }
}

s32 AudioLoad_IsSeqLoadComplete(s32 seqId) {
    if (seqId == 0xFF) {
        return true;
    } else if (gAudioContext.seqLoadStatus[seqId] >= 2) {
        return true;
    } else if (gAudioContext.seqLoadStatus[AudioLoad_GetRealTableIndex(SEQUENCE_TABLE, seqId)] >= 2) {
        return true;
    } else {
        return false;
    }
}

s32 AudioLoad_IsSampleLoadComplete(s32 sampleBankId) {
    if (sampleBankId == 0xFF) {
        return true;
    } else if (gAudioContext.sampleFontLoadStatus[sampleBankId] >= 2) {
        return true;
    } else if (gAudioContext.sampleFontLoadStatus[AudioLoad_GetRealTableIndex(SAMPLE_TABLE, sampleBankId)] >= 2) {
        return true;
    } else {
        return false;
    }
}

void AudioLoad_SetFontLoadStatus(s32 fontId, s32 status) {
    if ((fontId != 0xFF) && (gAudioContext.fontLoadStatus[fontId] != 5)) {
        gAudioContext.fontLoadStatus[fontId] = status;
    }
}

void AudioLoad_SetSeqLoadStatus(s32 seqId, s32 status) {
    if ((seqId != 0xFF) && (gAudioContext.seqLoadStatus[seqId] != 5)) {
        gAudioContext.seqLoadStatus[seqId] = status;
    }
}

void AudioLoad_SetSampleFontLoadStatusAndApplyCaches(s32 sampleBankId, s32 status) {
    if (sampleBankId != 0xFF) {
        if (gAudioContext.sampleFontLoadStatus[sampleBankId] != 5) {
            gAudioContext.sampleFontLoadStatus[sampleBankId] = status;
        }

        if ((gAudioContext.sampleFontLoadStatus[sampleBankId] == 5) ||
            (gAudioContext.sampleFontLoadStatus[sampleBankId] == 2)) {
            AudioHeap_ApplySampleBankCache(sampleBankId);
        }
    }
}

void AudioLoad_SetSampleFontLoadStatus(s32 sampleBankId, s32 status) {
    if ((sampleBankId != 0xFF) && (gAudioContext.sampleFontLoadStatus[sampleBankId] != 5)) {
        gAudioContext.sampleFontLoadStatus[sampleBankId] = status;
    }
}

void AudioLoad_InitTable(AudioTable* table, void* romAddr, u16 unkMediumParam) {
    s32 i;

    table->unkMediumParam = unkMediumParam;
    table->romAddr = (uintptr_t)romAddr;

    for (i = 0; i < table->numEntries; i++) {
        if ((table->entries[i].size != 0) && (table->entries[i].medium == MEDIUM_CART)) {
            table->entries[i].romAddr += (uintptr_t)romAddr;
        }
    }
}

SoundFontData* AudioLoad_SyncLoadSeqFonts(s32 seqId, u32* outDefaultFontId) {
    char pad[0x8];
    s32 index;
    SoundFontData* font = NULL;
    s32 numFonts;
    s32 fontId;
    s32 i;

    if (seqId >= gAudioContext.numSequences) {
        return NULL;
    }

    fontId = 0xFF;
    index = ((u16*)gAudioContext.sequenceFontTable)[seqId];
#ifdef LITTLE_ENDIAN
    numFonts = gAudioContext.sequenceFontTable[++index];
#else
    numFonts = gAudioContext.sequenceFontTable[index++];
#endif

    while (numFonts > 0) {
#ifdef LITTLE_ENDIAN
	    fontId = gAudioContext.sequenceFontTable[++index];
#else
        fontId = gAudioContext.sequenceFontTable[index++];
#endif
        font = AudioLoad_SyncLoadFont(fontId);
        numFonts--;
    }

    *outDefaultFontId = fontId;
    return font;
}

void AudioLoad_SyncLoadSeqParts(s32 seqId, s32 arg1) {
    s32 pad;
    u32 defaultFontId;

    if (seqId < gAudioContext.numSequences) {
        if (arg1 & 2) {
            AudioLoad_SyncLoadSeqFonts(seqId, &defaultFontId);
        }
        if (arg1 & 1) {
            AudioLoad_SyncLoadSeq(seqId);
        }
    }
}

s32 AudioLoad_SyncLoadSample(SoundFontSample* sample, s32 fontId) {
    void* sampleAddr;

    if (sample->unk_bit25 == 1) {
        if (sample->medium != MEDIUM_RAM) {
            sampleAddr = AudioHeap_AllocSampleCache(sample->size, fontId, (void*)sample->sampleAddr, sample->medium,
                                                    CACHE_PERSISTENT);
            if (sampleAddr == NULL) {
                return -1;
            }

            if (sample->medium == MEDIUM_UNK) {
                AudioLoad_SyncDmaUnkMedium(sample->sampleAddr, sampleAddr, sample->size,
                                           gAudioContext.sampleBankTable->unkMediumParam);
            } else {
                AudioLoad_SyncDma(sample->sampleAddr, sampleAddr, sample->size, sample->medium);
            }
            sample->medium = MEDIUM_RAM;
            sample->sampleAddr = (u8*)sampleAddr;
        }
    }
    return 0;
}

s32 AudioLoad_SyncLoadInstrument(s32 fontId, s32 instId, s32 drumId) {
    if (instId < 0x7F) {
        Instrument* instrument = Audio_GetInstrumentInner(fontId, instId);

        if (instrument == NULL) {
            return -1;
        }
        if (instrument->normalRangeLo != 0) {
            AudioLoad_SyncLoadSample(instrument->lowNotesSound.sample, fontId);
        }
        AudioLoad_SyncLoadSample(instrument->normalNotesSound.sample, fontId);
        if (instrument->normalRangeHi != 0x7F) {
            return AudioLoad_SyncLoadSample(instrument->highNotesSound.sample, fontId);
        }
    } else if (instId == 0x7F) {
        Drum* drum = Audio_GetDrum(fontId, drumId);

        if (drum == NULL) {
            return -1;
        }
        AudioLoad_SyncLoadSample(drum->sound.sample, fontId);
        return 0;
    }
    return 0;
}

void AudioLoad_AsyncLoad(s32 tableType, s32 id, s32 nChunks, s32 retData, OSMesgQueue* retQueue) {
    if (AudioLoad_AsyncLoadInner(tableType, id, nChunks, retData, retQueue) == NULL) {
        osSendMesg(retQueue, (OSMesg)0xFFFFFFFF, OS_MESG_NOBLOCK);
    }
}

void AudioLoad_AsyncLoadSeq(s32 seqId, s32 arg1, s32 retData, OSMesgQueue* retQueue) {
    AudioLoad_AsyncLoad(SEQUENCE_TABLE, seqId, 0, retData, retQueue);
}

void AudioLoad_AsyncLoadSampleBank(s32 sampleBankId, s32 arg1, s32 retData, OSMesgQueue* retQueue) {
    AudioLoad_AsyncLoad(SAMPLE_TABLE, sampleBankId, 0, retData, retQueue);
}

void AudioLoad_AsyncLoadFont(s32 fontId, s32 arg1, s32 retData, OSMesgQueue* retQueue) {
    AudioLoad_AsyncLoad(FONT_TABLE, fontId, 0, retData, retQueue);
}

u8* AudioLoad_GetFontsForSequence(s32 seqId, u32* outNumFonts) {
    s32 index;

    index = ((u16*)gAudioContext.sequenceFontTable)[seqId];

#ifdef LITTLE_ENDIAN
    *outNumFonts = gAudioContext.sequenceFontTable[index+1];
    if (*outNumFonts == 0) {
        return NULL;
    }
    return &gAudioContext.sequenceFontTable[index+0];
#else
    * outNumFonts = gAudioContext.sequenceFontTable[index + 0];
    if(*outNumFonts == 0)
    {
        return NULL;
    }
    return &gAudioContext.sequenceFontTable[index + 1];
#endif
}

void AudioLoad_DiscardSeqFonts(s32 seqId) {
    s32 fontId;
    s32 index;
    s32 numFonts;

    index = ((u16*)gAudioContext.sequenceFontTable)[seqId];
#ifdef LITTLE_ENDIAN
    numFonts = gAudioContext.sequenceFontTable[++index];
#else
    numFonts = gAudioContext.sequenceFontTable[index++];
#endif

    while (numFonts > 0) {
        numFonts--;
#ifdef LITTLE_ENDIAN
	    fontId = AudioLoad_GetRealTableIndex(FONT_TABLE, gAudioContext.sequenceFontTable[++index]);
#else
        fontId = AudioLoad_GetRealTableIndex(FONT_TABLE, gAudioContext.sequenceFontTable[index++]);
#endif
        if (AudioHeap_SearchPermanentCache(FONT_TABLE, fontId) == NULL) {
            AudioLoad_DiscardFont(fontId);
            AudioLoad_SetFontLoadStatus(fontId, 0);
        }
    }
}

void AudioLoad_DiscardFont(s32 fontId) {
    u32 i;
    AudioCache* pool = &gAudioContext.fontCache;
    AudioPersistentCache* persistent;

    if (fontId == pool->temporary.entries[0].id) {
        pool->temporary.entries[0].id = -1;
    } else if (fontId == pool->temporary.entries[1].id) {
        pool->temporary.entries[1].id = -1;
    }

    persistent = &pool->persistent;
    for (i = 0; i < persistent->numEntries; i++) {
        if (fontId == persistent->entries[i].id) {
            persistent->entries[i].id = -1;
        }
    }

    AudioHeap_DiscardFont(fontId);
}

s32 AudioLoad_SyncInitSeqPlayer(s32 playerIdx, s32 seqId, s32 arg2) {
    if (gAudioContext.resetTimer != 0) {
        return 0;
    }

    gAudioContext.seqPlayers[playerIdx].skipTicks = 0;
    return AudioLoad_SyncInitSeqPlayerInternal(playerIdx, seqId, arg2);
    // Intentionally missing return. Returning the result of the above function
    // call matches but is UB because it too is missing a return, and using the
    // result of a non-void function that has failed to return a value is UB.
    // The callers of this function do not use the return value, so it's fine.
}

s32 AudioLoad_SyncInitSeqPlayerSkipTicks(s32 playerIdx, s32 seqId, s32 skipTicks) {
    if (gAudioContext.resetTimer != 0) {
        return 0;
    }

    gAudioContext.seqPlayers[playerIdx].skipTicks = skipTicks;
    return AudioLoad_SyncInitSeqPlayerInternal(playerIdx, seqId, 0);
    // Missing return, see above.
}

s32 AudioLoad_SyncInitSeqPlayerInternal(s32 playerIdx, s32 seqId, s32 arg2) {
    SequencePlayer* seqPlayer = &gAudioContext.seqPlayers[playerIdx];
    u8* seqData;
    s32 index;
    s32 numFonts;
    s32 fontId;

    if (seqId >= gAudioContext.numSequences) {
        return 0;
    }

    AudioSeq_SequencePlayerDisable(seqPlayer);

    fontId = 0xFF;
    index = ((u16*)gAudioContext.sequenceFontTable)[seqId];
#ifdef LITTLE_ENDIAN
    numFonts = gAudioContext.sequenceFontTable[++index];
#else
    numFonts = gAudioContext.sequenceFontTable[index++];
#endif

    while (numFonts > 0) {
#ifdef LITTLE_ENDIAN
        fontId = gAudioContext.sequenceFontTable[++index];
#else
	    fontId = gAudioContext.sequenceFontTable[index++];
#endif
        AudioLoad_SyncLoadFont(fontId);
        numFonts--;
    }

    seqData = AudioLoad_SyncLoadSeq(seqId);
    if (seqData == NULL) {
        return 0;
    }

    AudioSeq_ResetSequencePlayer(seqPlayer);
    seqPlayer->seqId = seqId;
    seqPlayer->defaultFont = AudioLoad_GetRealTableIndex(FONT_TABLE, fontId);
    seqPlayer->seqData = seqData;
    seqPlayer->enabled = 1;
    seqPlayer->scriptState.pc = seqData;
    seqPlayer->scriptState.depth = 0;
    seqPlayer->delay = 0;
    seqPlayer->finished = 0;
    seqPlayer->playerIdx = playerIdx;
    AudioSeq_SkipForwardSequence(seqPlayer);
    //! @bug missing return (but the return value is not used so it's not UB)
    return 0;
}

u8* AudioLoad_SyncLoadSeq(s32 seqId) {
    s32 pad;
    s32 didAllocate;

    if (gAudioContext.seqLoadStatus[AudioLoad_GetRealTableIndex(SEQUENCE_TABLE, seqId)] == 1) {
        return NULL;
    }

    return (u8*)AudioLoad_SyncLoad(SEQUENCE_TABLE, seqId, &didAllocate);
}

u32 AudioLoad_GetSampleBank(u32 sampleBankId, u32* outMedium) {
    return AudioLoad_TrySyncLoadSampleBank(sampleBankId, outMedium, true);
}

static uintptr_t AudioLoad_TrySyncLoadSampleBank(u32 sampleBankId, u32* outMedium, s32 noLoad) {
    void* ret;
    AudioTable* sampleBankTable;
    u32 realTableId = AudioLoad_GetRealTableIndex(SAMPLE_TABLE, sampleBankId);
    s8 cachePolicy;

    sampleBankTable = AudioLoad_GetLoadTable(SAMPLE_TABLE);
    ret = AudioLoad_SearchCaches(SAMPLE_TABLE, realTableId);
    if (ret != NULL) {
        if (gAudioContext.sampleFontLoadStatus[realTableId] != 1) {
            AudioLoad_SetSampleFontLoadStatus(realTableId, 2);
        }
        *outMedium = MEDIUM_RAM;
        return (u32)ret;
    }

    cachePolicy = sampleBankTable->entries[sampleBankId].cachePolicy;
    if (cachePolicy == 4 || noLoad == true) {
        *outMedium = sampleBankTable->entries[sampleBankId].medium;
        return sampleBankTable->entries[realTableId].romAddr;
    }

    ret = AudioLoad_SyncLoad(SAMPLE_TABLE, sampleBankId, &noLoad);
    if (ret != NULL) {
        *outMedium = MEDIUM_RAM;
        return (uintptr_t)ret;
    }

    *outMedium = sampleBankTable->entries[sampleBankId].medium;
    return sampleBankTable->entries[realTableId].romAddr;
}

SoundFontData* AudioLoad_SyncLoadFont(u32 fontId) {
    SoundFontData* mem;
    s32 sampleBankId1;
    s32 sampleBankId2;
    s32 didAllocate;
    RelocInfo relocInfo;
    s32 realFontId = AudioLoad_GetRealTableIndex(FONT_TABLE, fontId);

    if (gAudioContext.fontLoadStatus[realFontId] == 1) {
        return NULL;
    }
    sampleBankId1 = gAudioContext.soundFonts[realFontId].sampleBankId1;
    sampleBankId2 = gAudioContext.soundFonts[realFontId].sampleBankId2;

    relocInfo.sampleBankId1 = sampleBankId1;
    relocInfo.sampleBankId2 = sampleBankId2;
    if (sampleBankId1 != 0xFF) {
        relocInfo.baseAddr1 = AudioLoad_TrySyncLoadSampleBank(sampleBankId1, &relocInfo.medium1, false);
    } else {
        relocInfo.baseAddr1 = 0;
    }

    if (sampleBankId2 != 0xFF) {
        relocInfo.baseAddr2 = AudioLoad_TrySyncLoadSampleBank(sampleBankId2, &relocInfo.medium2, false);
    } else {
        relocInfo.baseAddr2 = 0;
    }

    mem = AudioLoad_SyncLoad(FONT_TABLE, fontId, &didAllocate);
    if (mem == NULL) {
        return NULL;
    }
    if (didAllocate == true) {
        AudioLoad_RelocateFontAndPreloadSamples(realFontId, mem, &relocInfo, false);
    }

    return mem;
}

void* AudioLoad_SyncLoad(u32 tableType, u32 id, s32* didAllocate) {
    u32 size;
    AudioTable* table;
    s32 pad;
    u32 medium;
    s32 status;
    uintptr_t romAddr;
    AudioCacheLoadType cachePolicy;
    void* mem;
    u32 realId;

    realId = AudioLoad_GetRealTableIndex(tableType, id);
    mem = AudioLoad_SearchCaches(tableType, realId);
    if (mem != NULL) {
        *didAllocate = false;
        status = 2;
    } else {
        table = AudioLoad_GetLoadTable(tableType);
        size = table->entries[realId].size;
        size = ALIGN16(size);
        medium = table->entries[id].medium;
        cachePolicy = (AudioCacheLoadType)table->entries[id].cachePolicy;
        romAddr = table->entries[realId].romAddr;
        switch (cachePolicy) {
            case CACHE_LOAD_PERMANENT:
                mem = AudioHeap_AllocPermanent(tableType, realId, size);
                if (mem == NULL) {
                    return mem;
                }
                break;
            case CACHE_LOAD_PERSISTENT:
                mem = AudioHeap_AllocCached(tableType, size, CACHE_PERSISTENT, realId);
                if (mem == NULL) {
                    return mem;
                }
                break;
            case CACHE_LOAD_TEMPORARY:
                mem = AudioHeap_AllocCached(tableType, size, CACHE_TEMPORARY, realId);
                if (mem == NULL) {
                    return mem;
                }
                break;
            case CACHE_LOAD_EITHER:
            case CACHE_LOAD_EITHER_NOSYNC:
                mem = AudioHeap_AllocCached(tableType, size, CACHE_EITHER, realId);
                if (mem == NULL) {
                    return mem;
                }
                break;
        }

        *didAllocate = true;
        if (medium == MEDIUM_UNK) {
            AudioLoad_SyncDmaUnkMedium(romAddr, (u8*)mem, size, (s16)table->unkMediumParam);
        } else {
            AudioLoad_SyncDma(romAddr, (u8*)mem, size, medium);
        }

        status = cachePolicy == 0 ? 5 : 2;
    }

    switch (tableType) {
        case SEQUENCE_TABLE:
            AudioLoad_SetSeqLoadStatus(realId, status);
            break;
        case FONT_TABLE:
            AudioLoad_SetFontLoadStatus(realId, status);
            break;
        case SAMPLE_TABLE:
            AudioLoad_SetSampleFontLoadStatusAndApplyCaches(realId, status);
            break;
        default:
            break;
    }

    return mem;
}

u32 AudioLoad_GetRealTableIndex(s32 tableType, u32 id) {
    AudioTable* table = AudioLoad_GetLoadTable(tableType);

    if (table->entries[id].size == 0) {
        id = table->entries[id].romAddr;
    }

    return id;
}

void* AudioLoad_SearchCaches(s32 tableType, s32 id) {
    void* ret;

    ret = AudioHeap_SearchPermanentCache(tableType, id);
    if (ret != NULL) {
        return ret;
    }

    ret = AudioHeap_SearchCaches(tableType, CACHE_EITHER, id);
    if (ret != NULL) {
        return ret;
    }

    return NULL;
}

AudioTable* AudioLoad_GetLoadTable(s32 tableType) {
    AudioTable* ret;

    switch (tableType) {
        case SEQUENCE_TABLE:
            ret = gAudioContext.sequenceTable;
            break;
        case FONT_TABLE:
            ret = gAudioContext.soundFontTable;
            break;
        default:
            ret = NULL;
            break;
        case SAMPLE_TABLE:
            ret = gAudioContext.sampleBankTable;
            break;
    }
    return ret;
}

static_assert(sizeof(Drum) == 0x10, "Drum is incorrect size");
static_assert(sizeof(SoundFontSound) == 0x8, "SoundFontSound is incorrect size");
static_assert(sizeof(SoundFont) == 0x14, "SoundFont is incorrect size");

void AudioLoad_RelocateFont(s32 fontId, SoundFontData* mem, RelocInfo* relocInfo) {
	uintptr_t reloc;
    uintptr_t reloc2;
    Instrument* inst;
    Drum* drum;
    SoundFontSound* sfx;
    s32 i;
    s32 numDrums = gAudioContext.soundFonts[fontId].numDrums;
    s32 numInstruments = gAudioContext.soundFonts[fontId].numInstruments;
    s32 numSfx = gAudioContext.soundFonts[fontId].numSfx;
    void** ptrs = (void**)mem;

//#define BASE_OFFSET(x) (void*)(((uintptr_t)x) + (u32)(mem))
    reloc2 = (uintptr_t)ptrs[0];

    if ((reloc2 != 0) && (numDrums != 0)) {
        //ptrs[0] = BASE_OFFSET(reloc2);
        for (i = 0; i < numDrums; i++) {
            reloc = (uintptr_t)((Drum**)ptrs[0])[i];
            //reloc = (Drum*)ptrs[0] + i;
            if (reloc != 0) {
                //reloc = (uintptr_t)BASE_OFFSET(reloc);
                //drum = (Drum*)reloc;
                ((Drum**)ptrs[0])[i] = drum = (Drum*)reloc;
                if (!drum->loaded) {
                    AudioLoad_RelocateSample(&drum->sound, mem, relocInfo);
                    drum->loaded = 1;
                }
            }
        }
    }

    reloc2 = (uintptr_t)ptrs[1];

    if ((reloc2 != 0) && (numSfx != 0)) {
        //ptrs[1] = BASE_OFFSET(reloc2);
        for (i = 0; i < numSfx; i++) {
            reloc = (uintptr_t)((SoundFontSound*)ptrs[1] + i);
            if (reloc != 0) {
                sfx = (SoundFontSound*)reloc;
                if (sfx->sample != NULL) {
                    AudioLoad_RelocateSample(sfx, mem, relocInfo);
                }
            }
        }
    }

    if (numInstruments > 0x7E) {
        numInstruments = 0x7E;
    }

    for (i = 2; i <= 2 + numInstruments - 1; i++) {
        if (ptrs[i] != NULL) {
            //ptrs[i] = BASE_OFFSET(ptrs[i]);
            inst = (Instrument*)ptrs[i];
            if (!inst->loaded) {
                if (inst->normalRangeLo != 0) {
                    AudioLoad_RelocateSample(&inst->lowNotesSound, mem, relocInfo);
                }
                AudioLoad_RelocateSample(&inst->normalNotesSound, mem, relocInfo);
                if (inst->normalRangeHi != 0x7F) {
                    AudioLoad_RelocateSample(&inst->highNotesSound, mem, relocInfo);
                }

                //reloc = (uintptr_t)inst->envelope;
                //inst->envelope = (AdsrEnvelope*)BASE_OFFSET(reloc);
                inst->loaded = 1;
            }
        }
    }

//#undef BASE_OFFSET

    gAudioContext.soundFonts[fontId].drums = (Drum**)ptrs[0];
    gAudioContext.soundFonts[fontId].soundEffects = (SoundFontSound*)ptrs[1];
    gAudioContext.soundFonts[fontId].instruments = (Instrument**)(ptrs + 2);
}

static void AudioLoad_SyncDma(Pointer devAddr, Pointer addr, size_t size, s32 medium) {
    size = ALIGN16(size);

    Audio_InvalDCache(addr.buffer(), size);

    while (true) {
        if (size < 0x400) {
            break;
        }
        AudioLoad_Dma(nullptr, OS_MESG_PRI_HIGH, OS_READ, devAddr, addr, 0x400, nullptr, medium, "FastCopy");
        size -= 0x400;
        devAddr = devAddr + 0x400;
        addr = addr + 0x400;
    }

    if (size != 0) {
	    AudioLoad_Dma(nullptr, OS_MESG_PRI_HIGH, OS_READ, devAddr, addr, size, nullptr, medium, "FastCopy");
    }
}

static void AudioLoad_SyncDmaUnkMedium(Pointer devAddr, Pointer addr, size_t size, s32 unkMediumParam) {
	memcpy(addr.buffer(), devAddr.buffer(), size);
    return;
}

static s32 AudioLoad_Dma(OSIoMesg* mesg, u32 priority, s32 direction, Pointer devAddr, Pointer ramAddr, size_t size,
                  OSMesgQueue* reqQueue, s32 medium, const char* dmaFuncType) {
    OSPiHandle* handle;

    if (gAudioContext.resetTimer > 0x10) {
        return -1;
    }

    switch (medium) {
        case MEDIUM_CART:
            handle = gAudioContext.cartHandle;
            break;
        case MEDIUM_DISK_DRIVE:
            // driveHandle is uninitialized and corresponds to stubbed-out disk drive support.
            // SM64 Shindou called osDriveRomInit here.
            handle = gAudioContext.driveHandle;
            break;
        default:
            return 0;
    }

    if ((size % 0x10) != 0) {
        size = ALIGN16(size);
    }

    memcpy((void*)ramAddr.get(), (void*)devAddr.get(), size);
    return 0;
}

void AudioLoad_Unused1(void) {
}

void AudioLoad_SyncLoadSimple(u32 tableType, u32 fontId) {
    s32 didAllocate;

    AudioLoad_SyncLoad(tableType, fontId, &didAllocate);
}

void* AudioLoad_AsyncLoadInner(s32 tableType, s32 id, s32 nChunks, s32 retData, OSMesgQueue* retQueue) {
    u32 size;
    AudioTable* sp50;
    void* ret;
    s32 medium;
    s8 cachePolicy;
    u32 devAddr;
    s32 status;
    u32 temp_v0;
    u32 realId;

    realId = AudioLoad_GetRealTableIndex(tableType, id);
    switch (tableType) {
        case SEQUENCE_TABLE:
            if (gAudioContext.seqLoadStatus[realId] == 1) {
                return NULL;
            }
            break;
        case FONT_TABLE:
            if (gAudioContext.fontLoadStatus[realId] == 1) {
                return NULL;
            }
            break;
        case SAMPLE_TABLE:
            if (gAudioContext.sampleFontLoadStatus[realId] == 1) {
                return NULL;
            }
            break;
    }

    ret = AudioLoad_SearchCaches(tableType, realId);
    if (ret != NULL) {
        status = 2;
        osSendMesg(retQueue, (OSMesg)MK_ASYNC_MSG(retData, 0, 0, 0), OS_MESG_NOBLOCK);
    } else {
        sp50 = AudioLoad_GetLoadTable(tableType);
        size = sp50->entries[realId].size;
        size = ALIGN16(size);
        medium = sp50->entries[id].medium;
        cachePolicy = sp50->entries[id].cachePolicy;
        devAddr = sp50->entries[realId].romAddr;
        status = 2;
        switch (cachePolicy) {
            case 0:
                ret = AudioHeap_AllocPermanent(tableType, realId, size);
                if (ret == NULL) {
                    return ret;
                }
                status = 5;
                break;
            case 1:
                ret = AudioHeap_AllocCached(tableType, size, CACHE_PERSISTENT, realId);
                if (ret == NULL) {
                    return ret;
                }
                break;
            case 2:
                ret = AudioHeap_AllocCached(tableType, size, CACHE_TEMPORARY, realId);
                if (ret == NULL) {
                    return ret;
                }
                break;
            case 3:
            case 4:
                ret = AudioHeap_AllocCached(tableType, size, CACHE_EITHER, realId);
                if (ret == NULL) {
                    return ret;
                }
                break;
        }

        if (medium == MEDIUM_UNK) {
            AudioLoad_StartAsyncLoadUnkMedium((s16)sp50->unkMediumParam, devAddr, ret, size, medium, nChunks, retQueue,
                                              MK_ASYNC_MSG(retData, tableType, id, status));
        } else {
            AudioLoad_StartAsyncLoad(devAddr, ret, size, medium, nChunks, retQueue,
                                     MK_ASYNC_MSG(retData, tableType, realId, status));
        }
        status = 1;
    }

    switch (tableType) {
        case SEQUENCE_TABLE:
            AudioLoad_SetSeqLoadStatus(realId, status);
            break;
        case FONT_TABLE:
            AudioLoad_SetFontLoadStatus(realId, status);
            break;
        case SAMPLE_TABLE:
            AudioLoad_SetSampleFontLoadStatusAndApplyCaches(realId, status);
            break;
        default:
            break;
    }

    return ret;
}

void AudioLoad_ProcessLoads(s32 resetStatus) {
    AudioLoad_ProcessSlowLoads(resetStatus);
    AudioLoad_ProcessSamplePreloads(resetStatus);
    AudioLoad_ProcessAsyncLoads(resetStatus);
}

void AudioLoad_SetDmaHandler(DmaHandler callback) {
    sDmaHandler = callback;
}

void AudioLoad_SetUnusedHandler(void* callback) {
    sUnusedHandler = callback;
}

void AudioLoad_InitSoundFontMeta(s32 fontId) {
    SoundFont* font = &gAudioContext.soundFonts[fontId];
    AudioTableEntry* entry = &gAudioContext.soundFontTable->entries[fontId];

    font->sampleBankId1 = (entry->shortData1 >> 8) & 0xFF;
    font->sampleBankId2 = (entry->shortData1) & 0xFF;
    font->numInstruments = (entry->shortData2 >> 8) & 0xFF;
    font->numDrums = entry->shortData2 & 0xFF;
    font->numSfx = entry->shortData3;
}

void AudioLoad_Init(void* heap, u32 heapSize) {
    char pad[0x48];
    s32 numFonts;
    void* temp_v0_3;
    s32 i;
    u64* heapP;
    u8* ctxP;
    s16* u2974p;

    D_801755D0 = NULL;
    gAudioContext.resetTimer = 0;

    {
        s32 i;
        u8* ctxP = (u8*)&gAudioContext;
        for (i = sizeof(gAudioContext); i >= 0; i--) {
            *ctxP++ = 0;
        }
    }

    switch (osTvType) {
        case OS_TV_PAL:
            gAudioContext.unk_2960 = 20.03042f;
            gAudioContext.refreshRate = 50;
            break;
        case OS_TV_MPAL:
            gAudioContext.unk_2960 = 16.546f;
            gAudioContext.refreshRate = 60;
            break;
        case OS_TV_NTSC:
        default:
            gAudioContext.unk_2960 = 16.713f;
            gAudioContext.refreshRate = 60;
    }

    Audio_InitMesgQueues();

    for (i = 0; i < 3; i++) {
        gAudioContext.aiBufLengths[i] = 0xA0;
    }

    gAudioContext.totalTaskCnt = 0;
    gAudioContext.rspTaskIdx = 0;
    gAudioContext.curAIBufIdx = 0;
    gAudioContext.soundMode = 0;
    gAudioContext.currTask = NULL;
    gAudioContext.rspTask[0].task.t.data_size = 0;
    gAudioContext.rspTask[1].task.t.data_size = 0;
    gAudioContext.curAudioFrameDmaCount = 0;
    gAudioContext.sampleDmaCount = 0;
    //gAudioContext.cartHandle = osCartRomInit();

    if (heap == NULL) {
        gAudioContext.audioHeap = gAudioHeap;
        gAudioContext.audioHeapSize = D_8014A6C4.heapSize;
    } else {
        void** hp = &heap;
        gAudioContext.audioHeap = (u8*)*hp;
        gAudioContext.audioHeapSize = heapSize;
    }

    for (i = 0; i < (s32)gAudioContext.audioHeapSize / 8; i++) {
        ((u64*)gAudioContext.audioHeap)[i] = 0;
    }

    AudioHeap_InitMainPools(D_8014A6C4.initPoolSize);

    for (i = 0; i < 3; i++) {
        gAudioContext.aiBuffers[i] = (s16*)AudioHeap_AllocZeroed(&gAudioContext.audioInitPool, AIBUF_LEN * sizeof(s16));
    }

    gAudioContext.sequenceTable = (AudioTable*)&gSequenceTable;
    gAudioContext.soundFontTable = (AudioTable*)&gSoundFontTable;
    gAudioContext.sampleBankTable = (AudioTable*)&gSampleBankTable;
    gAudioContext.sequenceFontTable = (u8*)gSequenceFontTable;
    gAudioContext.numSequences = gAudioContext.sequenceTable->numEntries;

    gAudioContext.audioResetSpecIdToLoad = 0;
    gAudioContext.resetStatus = 1;

    AudioHeap_ResetStep();
    AudioLoad_InitTable(gAudioContext.sequenceTable, (void*)Audioseq, 0);
    //AudioLoad_InitTable(gAudioContext.soundFontTable, (void*)Audiobank_le, 0);
    AudioLoad_InitTable(gAudioContext.sampleBankTable, (void*)Audiotable, 0);
    numFonts = gAudioContext.soundFontTable->numEntries;
    gAudioContext.soundFonts = (SoundFont*)AudioHeap_Alloc(&gAudioContext.audioInitPool, numFonts * sizeof(SoundFont));

    for (i = 0; i < numFonts; i++) {
        AudioLoad_InitSoundFontMeta(i);
    }

    if (temp_v0_3 = AudioHeap_Alloc(&gAudioContext.audioInitPool, D_8014A6C4.permanentPoolSize), temp_v0_3 == NULL) {
        D_8014A6C4.permanentPoolSize = 0;
    }

    AudioHeap_AllocPoolInit(&gAudioContext.permanentPool, temp_v0_3, D_8014A6C4.permanentPoolSize);
    gAudioContextInitalized = true;
    osSendMesg(gAudioContext.taskStartQueueP, (void*)gAudioContext.totalTaskCnt, OS_MESG_NOBLOCK);
}

void AudioLoad_InitSlowLoads(void) {
    gAudioContext.slowLoads[0].status = 0;
    gAudioContext.slowLoads[1].status = 0;
}

s32 AudioLoad_SlowLoadSample(s32 fontId, s32 instId, s8* isDone) {
    SoundFontSample* sample;
    AudioSlowLoad* slowLoad;

    sample = AudioLoad_GetFontSample(fontId, instId);
    if (sample == NULL) {
        *isDone = 0;
        return -1;
    }

    if (sample->medium == MEDIUM_RAM) {
        *isDone = 2;
        return 0;
    }

    slowLoad = &gAudioContext.slowLoads[gAudioContext.slowLoadPos];
    if (slowLoad->status == LOAD_STATUS_DONE) {
        slowLoad->status = LOAD_STATUS_WAITING;
    }

    slowLoad->sample = *sample;
    slowLoad->isDone = isDone;
    slowLoad->curRamAddr =
        (u8*)AudioHeap_AllocSampleCache(sample->size, fontId, sample->sampleAddr, sample->medium, CACHE_TEMPORARY);

    if (slowLoad->curRamAddr == NULL) {
        if (sample->medium == MEDIUM_UNK || sample->codec == CODEC_S16_INMEMORY) {
            *isDone = 0;
            return -1;
        } else {
            *isDone = 3;
            return -1;
        }
    }

    slowLoad->status = LOAD_STATUS_START;
    slowLoad->bytesRemaining = ALIGN16(sample->size);
    slowLoad->ramAddr = slowLoad->curRamAddr;
    slowLoad->curDevAddr = sample->sampleAddr;
    slowLoad->medium = sample->medium;
    slowLoad->seqOrFontId = fontId;
    slowLoad->instId = instId;
    if (slowLoad->medium == MEDIUM_UNK) {
        slowLoad->unkMediumParam = gAudioContext.sampleBankTable->unkMediumParam;
    }

    gAudioContext.slowLoadPos ^= 1;
    return 0;
}

SoundFontSample* AudioLoad_GetFontSample(s32 fontId, s32 instId) {
    SoundFontSample* ret;

    if (instId < 0x80) {
        Instrument* instrument = Audio_GetInstrumentInner(fontId, instId);
        if (instrument == NULL) {
            return NULL;
        }
        ret = instrument->normalNotesSound.sample;
    } else if (instId < 0x100) {
        Drum* drum = Audio_GetDrum(fontId, instId - 0x80);
        if (drum == NULL) {
            return NULL;
        }
        ret = drum->sound.sample;
    } else {
        SoundFontSound* sound = Audio_GetSfx(fontId, instId - 0x100);
        if (sound == NULL) {
            return NULL;
        }
        ret = sound->sample;
    }
    return ret;
}

void AudioLoad_FinishSlowLoad(AudioSlowLoad* slowLoad) {
    SoundFontSample* sample;

    if (slowLoad->sample.sampleAddr == NULL) {
        return;
    }

    sample = AudioLoad_GetFontSample(slowLoad->seqOrFontId, slowLoad->instId);
    if (sample == NULL) {
        return;
    }

    slowLoad->sample = *sample;
    sample->sampleAddr = slowLoad->ramAddr;
    sample->medium = MEDIUM_RAM;
}

void AudioLoad_ProcessSlowLoads(s32 resetStatus) {
    AudioSlowLoad* slowLoad;
    s32 i;

    for (i = 0; i < ARRAY_COUNT(gAudioContext.slowLoads); i++) {
        slowLoad = &gAudioContext.slowLoads[i];
        switch (gAudioContext.slowLoads[i].status) {
            case LOAD_STATUS_LOADING:
                if (slowLoad->medium != MEDIUM_UNK) {
                }

                if (resetStatus != 0) {
                    slowLoad->status = LOAD_STATUS_DONE;
                    continue;
                }
            case LOAD_STATUS_START:
                slowLoad->status = LOAD_STATUS_LOADING;
                if (slowLoad->bytesRemaining == 0) {
                    AudioLoad_FinishSlowLoad(slowLoad);
                    slowLoad->status = LOAD_STATUS_DONE;
                    *slowLoad->isDone = 1;
                } else if (slowLoad->bytesRemaining < 0x400) {
                    if (slowLoad->medium == MEDIUM_UNK) {
                        u32 size = slowLoad->bytesRemaining;
                        AudioLoad_DmaSlowCopyUnkMedium(slowLoad->curDevAddr, slowLoad->curRamAddr, size,
                                                       slowLoad->unkMediumParam);
                    } else {
                        AudioLoad_DmaSlowCopy(slowLoad, slowLoad->bytesRemaining);
                    }
                    slowLoad->bytesRemaining = 0;
                } else {
                    if (slowLoad->medium == MEDIUM_UNK) {
                        AudioLoad_DmaSlowCopyUnkMedium(slowLoad->curDevAddr, slowLoad->curRamAddr, 0x400,
                                                       slowLoad->unkMediumParam);
                    } else {
                        AudioLoad_DmaSlowCopy(slowLoad, 0x400);
                    }
                    slowLoad->bytesRemaining -= 0x400;
                    slowLoad->curRamAddr += 0x400;
                    slowLoad->curDevAddr += 0x400;
                }
                break;
        }
    }
}

void AudioLoad_DmaSlowCopy(AudioSlowLoad* slowLoad, s32 size) {
    Audio_InvalDCache(slowLoad->curRamAddr, size);
    AudioLoad_Dma(&slowLoad->ioMesg, OS_MESG_PRI_NORMAL, 0, slowLoad->curDevAddr, slowLoad->curRamAddr, size,
                  nullptr, slowLoad->medium, "SLOWCOPY");
}

static void AudioLoad_DmaSlowCopyUnkMedium(Pointer devAddr, Pointer ramAddr, size_t size, s32 arg3) {
}

s32 AudioLoad_SlowLoadSeq(s32 seqId, u8* ramAddr, s8* isDone) {
    AudioSlowLoad* slowLoad;
    AudioTable* seqTable;
    u32 size;

    if (seqId >= gAudioContext.numSequences) {
        *isDone = 0;
        return -1;
    }

    seqId = AudioLoad_GetRealTableIndex(SEQUENCE_TABLE, seqId);
    seqTable = AudioLoad_GetLoadTable(SEQUENCE_TABLE);
    slowLoad = &gAudioContext.slowLoads[gAudioContext.slowLoadPos];
    if (slowLoad->status == LOAD_STATUS_DONE) {
        slowLoad->status = LOAD_STATUS_WAITING;
    }

    slowLoad->sample.sampleAddr = NULL;
    slowLoad->isDone = isDone;
    size = seqTable->entries[seqId].size;
    size = ALIGN16(size);
    slowLoad->curRamAddr = ramAddr;
    slowLoad->status = LOAD_STATUS_START;
    slowLoad->bytesRemaining = size;
    slowLoad->ramAddr = ramAddr;
    slowLoad->curDevAddr = seqTable->entries[seqId].romAddr;
    slowLoad->medium = seqTable->entries[seqId].medium;
    slowLoad->seqOrFontId = seqId;

    if (slowLoad->medium == MEDIUM_UNK) {
        slowLoad->unkMediumParam = seqTable->unkMediumParam;
    }

    gAudioContext.slowLoadPos ^= 1;
    return 0;
}

void AudioLoad_InitAsyncLoads(void) {
    s32 i;

    for (i = 0; i < ARRAY_COUNT(gAudioContext.asyncLoads); i++) {
        gAudioContext.asyncLoads[i].status = 0;
    }
}

AudioAsyncLoad* AudioLoad_StartAsyncLoadUnkMedium(s32 unkMediumParam, u32 devAddr, void* ramAddr, s32 size, s32 medium,
                                                  s32 nChunks, OSMesgQueue* retQueue, s32 retMsg) {
    AudioAsyncLoad* asyncLoad;

    asyncLoad = AudioLoad_StartAsyncLoad(devAddr, ramAddr, size, medium, nChunks, retQueue, retMsg);

    if (asyncLoad == NULL) {
        return NULL;
    }

    asyncLoad->unkMediumParam = unkMediumParam;
    return asyncLoad;
}

AudioAsyncLoad* AudioLoad_StartAsyncLoad(u32 devAddr, void* ramAddr, u32 size, s32 medium, s32 nChunks,
                                         OSMesgQueue* retQueue, s32 retMsg) {
    AudioAsyncLoad* asyncLoad = NULL;
    s32 i;

    for (i = 0; i < ARRAY_COUNT(gAudioContext.asyncLoads); i++) {
        if (gAudioContext.asyncLoads[i].status == 0) {
            asyncLoad = &gAudioContext.asyncLoads[i];
            break;
        }
    }

    // no more available async loads
    if (i == ARRAY_COUNT(gAudioContext.asyncLoads)) {
        return NULL;
    }

    asyncLoad->status = LOAD_STATUS_START;
    asyncLoad->curDevAddr = devAddr;
    asyncLoad->ramAddr = (u8*)ramAddr;
    asyncLoad->curRamAddr = (u8*)ramAddr;
    asyncLoad->bytesRemaining = size;

    if (nChunks == 0) {
        asyncLoad->chunkSize = 0x1000;
    } else if (nChunks == 1) {
        asyncLoad->chunkSize = size;
    } else {
        asyncLoad->chunkSize = (((s32)size / nChunks) + 0xFF) & ~0xFF;
        if (asyncLoad->chunkSize < 0x100) {
            asyncLoad->chunkSize = 0x100;
        }
    }

    asyncLoad->retQueue = retQueue;
    asyncLoad->delay = 3;
    asyncLoad->medium = medium;
    asyncLoad->retMsg = retMsg;
    osCreateMesgQueue(&asyncLoad->msgQueue, &asyncLoad->msg, 1);
    return asyncLoad;
}

void AudioLoad_ProcessAsyncLoads(s32 resetStatus) {
    AudioAsyncLoad* asyncLoad = NULL;
    s32 i;

    if (gAudioContext.resetTimer == 1) {
        return;
    }

    if (gAudioContext.curUnkMediumLoad == NULL) {
        if (resetStatus == 0) {
            gAudioContext.curUnkMediumLoad = asyncLoad;
        }
    }

    if (gAudioContext.curUnkMediumLoad != NULL) {
        AudioLoad_ProcessAsyncLoadUnkMedium(gAudioContext.curUnkMediumLoad, resetStatus);
    }

    for (i = 0; i < ARRAY_COUNT(gAudioContext.asyncLoads); i++) {
        if (gAudioContext.asyncLoads[i].status == 1) {
            asyncLoad = &gAudioContext.asyncLoads[i];
            if (asyncLoad->medium != MEDIUM_UNK) {
                AudioLoad_ProcessAsyncLoad(asyncLoad, resetStatus);
            }
        }
    }
}

void AudioLoad_ProcessAsyncLoadUnkMedium(AudioAsyncLoad* asyncLoad, s32 resetStatus) {
}

void AudioLoad_FinishAsyncLoad(AudioAsyncLoad* asyncLoad) {
    u32 retMsg = asyncLoad->retMsg;
    u32 fontId;
    u32 pad;
    OSMesg doneMsg;
    u32 sampleBankId1;
    u32 sampleBankId2;
    RelocInfo relocInfo;

    if (1) {}
    switch (ASYNC_TBLTYPE(retMsg)) {
        case SEQUENCE_TABLE:
            AudioLoad_SetSeqLoadStatus(ASYNC_ID(retMsg), ASYNC_STATUS(retMsg));
            break;
        case SAMPLE_TABLE:
            AudioLoad_SetSampleFontLoadStatusAndApplyCaches(ASYNC_ID(retMsg), ASYNC_STATUS(retMsg));
            break;
        case FONT_TABLE:
            fontId = ASYNC_ID(retMsg);
            sampleBankId1 = gAudioContext.soundFonts[fontId].sampleBankId1;
            sampleBankId2 = gAudioContext.soundFonts[fontId].sampleBankId2;
            relocInfo.sampleBankId1 = sampleBankId1;
            relocInfo.sampleBankId2 = sampleBankId2;
            relocInfo.baseAddr1 =
                sampleBankId1 != 0xFF ? AudioLoad_GetSampleBank(sampleBankId1, &relocInfo.medium1) : 0;
            relocInfo.baseAddr2 =
                sampleBankId2 != 0xFF ? AudioLoad_GetSampleBank(sampleBankId2, &relocInfo.medium2) : 0;
            AudioLoad_SetFontLoadStatus(fontId, ASYNC_STATUS(retMsg));
            AudioLoad_RelocateFontAndPreloadSamples(fontId, asyncLoad->ramAddr, &relocInfo, true);
            break;
    }

    doneMsg = (OSMesg)asyncLoad->retMsg;

    asyncLoad->status = LOAD_STATUS_WAITING;
    osSendMesg(asyncLoad->retQueue, doneMsg, OS_MESG_NOBLOCK);
}

void AudioLoad_ProcessAsyncLoad(AudioAsyncLoad* asyncLoad, s32 resetStatus) {
    AudioTable* sampleBankTable = gAudioContext.sampleBankTable;

    if (asyncLoad->delay >= 2) {
        asyncLoad->delay--;
        return;
    }

    if (asyncLoad->delay == 1) {
        asyncLoad->delay = 0;
    } else if (resetStatus != 0) {
        asyncLoad->status = LOAD_STATUS_WAITING;
        return;
    }

    if (asyncLoad->bytesRemaining == 0) {
        AudioLoad_FinishAsyncLoad(asyncLoad);
        return;
    }

    if (asyncLoad->bytesRemaining < asyncLoad->chunkSize) {
        if (asyncLoad->medium == MEDIUM_UNK) {
            AudioLoad_AsyncDmaUnkMedium(asyncLoad->curDevAddr, asyncLoad->curRamAddr, asyncLoad->bytesRemaining,
                                        sampleBankTable->unkMediumParam);
        } else {
            AudioLoad_AsyncDma(asyncLoad, asyncLoad->bytesRemaining);
        }
        asyncLoad->bytesRemaining = 0;
        return;
    }

    if (asyncLoad->medium == MEDIUM_UNK) {
        AudioLoad_AsyncDmaUnkMedium(asyncLoad->curDevAddr, asyncLoad->curRamAddr, asyncLoad->chunkSize,
                                    sampleBankTable->unkMediumParam);
    } else {
        AudioLoad_AsyncDma(asyncLoad, asyncLoad->chunkSize);
    }

    asyncLoad->bytesRemaining -= asyncLoad->chunkSize;
    asyncLoad->curDevAddr += asyncLoad->chunkSize;
    asyncLoad->curRamAddr += asyncLoad->chunkSize;
}

void AudioLoad_AsyncDma(AudioAsyncLoad* asyncLoad, u32 size) {
    size = ALIGN16(size);
    Audio_InvalDCache(asyncLoad->curRamAddr, size);
    osCreateMesgQueue(&asyncLoad->msgQueue, &asyncLoad->msg, 1);
    AudioLoad_Dma(&asyncLoad->ioMesg, 0, 0, asyncLoad->curDevAddr, asyncLoad->curRamAddr, size, &asyncLoad->msgQueue,
                  asyncLoad->medium, "BGCOPY");
}

void AudioLoad_AsyncDmaUnkMedium(u32 devAddr, void* ramAddr, u32 size, s16 arg3) {
    int xyz = 0;
}

#define RELOC(v, base) (reloc = (void*)(BEE32((u32)(v)) + (u32)(base)))
//#define RELOC(v, base) (reloc = (void*)((u32)(v) + (u32)(base)))

void AudioLoad_RelocateSample(SoundFontSound* sound, SoundFontData* mem, RelocInfo* relocInfo) {
    SoundFontSample* sample;
    void* reloc;

    //if (BEE32((u32)sound->sample) <= 0x80000000)
    {
        sample = sound->sample; // = (SoundFontSample*)RELOC(sound->sample, mem);
        if (sample->size != 0 && sample->unk_bit25 != 1) {
            //sample->loop = (AdpcmLoop*)RELOC(sample->loop, mem);
            //sample->book = (AdpcmBook*)RELOC(sample->book, mem);
            sample->medium = 0;//!!!
            // Resolve the sample medium 2-bit bitfield into a real value based on relocInfo.
            switch (BEE32(sample->medium)) {
                case 0: // read from baserom/Audiotable
                    sample->sampleAddr = (u8*)RELOC(sample->sampleAddr, relocInfo->baseAddr1);
                    sample->medium = relocInfo->medium1;
                    break;
                case 1:
                    sample->sampleAddr = (u8*)RELOC(sample->sampleAddr, relocInfo->baseAddr2);
                    sample->medium = relocInfo->medium2;
                    break;
                case 2:
                case 3:
                    // Invalid? This leaves sample->medium as MEDIUM_CART and MEDIUM_DISK_DRIVE
                    // respectively, and the sampleAddr unrelocated.
			sample->sampleAddr = sample->sampleAddr;
                    break;
            }

            sample->unk_bit25 = 1;
            if (sample->unk_bit26 && (sample->medium != MEDIUM_RAM)) {
		        if(gAudioContext.numUsedSamples < ARRAY_COUNT(gAudioContext.usedSamples))
		        {
			        gAudioContext.usedSamples[gAudioContext.numUsedSamples++] = sample;
		        }
            }
        }
    }
}

#undef RELOC

void AudioLoad_RelocateFontAndPreloadSamples(s32 fontId, SoundFontData* mem, RelocInfo* relocInfo, s32 async) {
    AudioPreloadReq* preload;
    AudioPreloadReq* topPreload;
    SoundFontSample* sample;
    s32 size;
    s32 nChunks;
    u8* addr;
    s32 preloadInProgress;
    s32 i;

    preloadInProgress = false;
    if (gAudioContext.preloadSampleStackTop != 0) {
        preloadInProgress = true;
    } else {
        D_8016B780 = 0;
    }

    gAudioContext.numUsedSamples = 0;
    AudioLoad_RelocateFont(fontId, mem, relocInfo);

    size = 0;
    for (i = 0; i < gAudioContext.numUsedSamples; i++) {
        size += ALIGN16(gAudioContext.usedSamples[i]->size);
    }

    for (i = 0; i < gAudioContext.numUsedSamples; i++) {
        if (gAudioContext.preloadSampleStackTop == ARRAY_COUNT(gAudioContext.usedSamples) - 8) {
            break;
        }

        sample = gAudioContext.usedSamples[i];
        addr = NULL;
        switch (async) {
            case false:
                if (sample->medium == relocInfo->medium1) {
                    addr = (u8*)AudioHeap_AllocSampleCache(sample->size, relocInfo->sampleBankId1, sample->sampleAddr,
                                                      sample->medium, CACHE_PERSISTENT);
                } else if (sample->medium == relocInfo->medium2) {
                    addr = (u8*)AudioHeap_AllocSampleCache(sample->size, relocInfo->sampleBankId2, sample->sampleAddr,
                                                      sample->medium, CACHE_PERSISTENT);
                } else if (sample->medium == MEDIUM_DISK_DRIVE) {
                    addr = (u8*)AudioHeap_AllocSampleCache(sample->size, 0xFE, sample->sampleAddr, sample->medium,
                                                      CACHE_PERSISTENT);
                }
                break;

            case true:
                if (sample->medium == relocInfo->medium1) {
                    addr = (u8*)AudioHeap_AllocSampleCache(sample->size, relocInfo->sampleBankId1, sample->sampleAddr,
                                                      sample->medium, CACHE_TEMPORARY);
                } else if (sample->medium == relocInfo->medium2) {
                    addr = (u8*)AudioHeap_AllocSampleCache(sample->size, relocInfo->sampleBankId2, sample->sampleAddr,
                                                      sample->medium, CACHE_TEMPORARY);
                } else if (sample->medium == MEDIUM_DISK_DRIVE) {
                    addr = (u8*)AudioHeap_AllocSampleCache(sample->size, 0xFE, sample->sampleAddr, sample->medium,
                                                      CACHE_TEMPORARY);
                }
                break;
        }
        if (addr == NULL) {
            continue;
        }

        switch (async) {
            case false:
                if (sample->medium == MEDIUM_UNK) {
                    AudioLoad_SyncDmaUnkMedium((u32)sample->sampleAddr, addr, sample->size,
                                               gAudioContext.sampleBankTable->unkMediumParam);
                    sample->sampleAddr = addr;
                    sample->medium = MEDIUM_RAM;
                } else {
                    AudioLoad_SyncDma((u32)sample->sampleAddr, addr, sample->size, sample->medium);
                    sample->sampleAddr = addr;
                    sample->medium = MEDIUM_RAM;
                }
                if (sample->medium == MEDIUM_DISK_DRIVE) {}
                break;

            case true:
                preload = &gAudioContext.preloadSampleStack[gAudioContext.preloadSampleStackTop];
                preload->sample = sample;
                preload->ramAddr = addr;
                preload->encodedInfo = (gAudioContext.preloadSampleStackTop << 24) | 0xFFFFFF;
                preload->isFree = false;
                preload->endAndMediumKey = (u32)sample->sampleAddr + sample->size + sample->medium;
                gAudioContext.preloadSampleStackTop++;
                break;
        }
    }
    gAudioContext.numUsedSamples = 0;

    if (gAudioContext.preloadSampleStackTop != 0 && !preloadInProgress) {
        topPreload = &gAudioContext.preloadSampleStack[gAudioContext.preloadSampleStackTop - 1];
        sample = topPreload->sample;
        nChunks = (sample->size >> 12) + 1;
        AudioLoad_StartAsyncLoad((u32)sample->sampleAddr, topPreload->ramAddr, sample->size, sample->medium, nChunks,
                                 nullptr, topPreload->encodedInfo);
    }
}

s32 AudioLoad_ProcessSamplePreloads(s32 resetStatus) {
    SoundFontSample* sample;
    AudioPreloadReq* preload;
    u32 preloadIndex = 0;
    u32 key;
    u32 nChunks;
    s32 pad;

    if (gAudioContext.preloadSampleStackTop > 0) {
        if (resetStatus != 0) {
            gAudioContext.preloadSampleStackTop = 0;
            return 0;
        }

        preloadIndex >>= 24;
        preload = &gAudioContext.preloadSampleStack[preloadIndex];

        if (preload->isFree == false) {
            sample = preload->sample;
            key = (u32)sample->sampleAddr + sample->size + sample->medium;
            if (key == preload->endAndMediumKey) {
                // Change storage for sample to the preloaded version.
                sample->sampleAddr = preload->ramAddr;
                sample->medium = MEDIUM_RAM;
            }
            preload->isFree = true;
        }

        // Pop requests with isFree = true off the stack, as far as possible,
        // and dispatch the next DMA.
        for (;;) {
            if (gAudioContext.preloadSampleStackTop <= 0) {
                break;
            }
            preload = &gAudioContext.preloadSampleStack[gAudioContext.preloadSampleStackTop - 1];
            if (preload->isFree == true) {
                gAudioContext.preloadSampleStackTop--;
                continue;
            }

            sample = preload->sample;
            nChunks = (sample->size >> 12) + 1;
            key = (u32)sample->sampleAddr + sample->size + sample->medium;
            if (key != preload->endAndMediumKey) {
                preload->isFree = true;
                gAudioContext.preloadSampleStackTop--;
            } else {
                AudioLoad_StartAsyncLoad((u32)sample->sampleAddr, preload->ramAddr, sample->size, sample->medium,
                                         nChunks, nullptr, preload->encodedInfo);
                break;
            }
        }
    }
    return 1;
}

s32 AudioLoad_AddToSampleSet(SoundFontSample* sample, s32 numSamples, SoundFontSample** sampleSet) {
    s32 i;

    for (i = 0; i < numSamples; i++) {
        if (sample->sampleAddr == sampleSet[i]->sampleAddr) {
            break;
        }
    }

    if (i == numSamples) {
        sampleSet[numSamples] = sample;
        numSamples++;
    }

    return numSamples;
}

s32 AudioLoad_GetSamplesForFont(s32 fontId, SoundFontSample** sampleSet) {
    s32 i;
    s32 numDrums;
    s32 numInstruments;
    s32 numSamples = 0;

    numDrums = gAudioContext.soundFonts[fontId].numDrums;
    numInstruments = gAudioContext.soundFonts[fontId].numInstruments;

    for (i = 0; i < numDrums; i++) {
        Drum* drum = Audio_GetDrum(fontId, i);
        if (1) {}
        if (drum != NULL) {
            numSamples = AudioLoad_AddToSampleSet(drum->sound.sample, numSamples, sampleSet);
        }
    }

    for (i = 0; i < numInstruments; i++) {
        Instrument* instrument = Audio_GetInstrumentInner(fontId, i);
        if (instrument != NULL) {
            if (instrument->normalRangeLo != 0) {
                numSamples = AudioLoad_AddToSampleSet(instrument->lowNotesSound.sample, numSamples, sampleSet);
            }
            if (instrument->normalRangeHi != 0x7F) {
                numSamples = AudioLoad_AddToSampleSet(instrument->highNotesSound.sample, numSamples, sampleSet);
            }
            numSamples = AudioLoad_AddToSampleSet(instrument->normalNotesSound.sample, numSamples, sampleSet);
        }
    }

    // Should really also process sfx, but this method is never called, so whatever.
    return numSamples;
}

void AudioLoad_AddUsedSample(SoundFontSound* sound) {
    SoundFontSample* sample = sound->sample;

    if ((sample->size != 0) && (sample->unk_bit26) && (sample->medium != MEDIUM_RAM)) {
	    if(gAudioContext.numUsedSamples < ARRAY_COUNT(gAudioContext.usedSamples))
	    {
		    gAudioContext.usedSamples[gAudioContext.numUsedSamples++] = sample;
	    }
    }
}

void AudioLoad_PreloadSamplesForFont(s32 fontId, s32 async, RelocInfo* relocInfo) {
    s32 numDrums;
    s32 numInstruments;
    s32 numSfx;
    Drum* drum = NULL;
    Instrument* instrument = NULL;
    SoundFontSound* sound = NULL;
    AudioPreloadReq* preload = NULL;
    AudioPreloadReq* topPreload = NULL;
    u8* addr = NULL;
    s32 size;
    s32 i;
    SoundFontSample* sample = NULL;
    s32 preloadInProgress;
    s32 nChunks;

    preloadInProgress = false;
    if (gAudioContext.preloadSampleStackTop != 0) {
        preloadInProgress = true;
    }

    gAudioContext.numUsedSamples = 0;

    numDrums = gAudioContext.soundFonts[fontId].numDrums;
    numInstruments = gAudioContext.soundFonts[fontId].numInstruments;
    numSfx = gAudioContext.soundFonts[fontId].numSfx;

    for (i = 0; i < numInstruments; i++) {
        instrument = Audio_GetInstrumentInner(fontId, i);
        if (instrument != NULL) {
            if (instrument->normalRangeLo != 0) {
                AudioLoad_AddUsedSample(&instrument->lowNotesSound);
            }
            if (instrument->normalRangeHi != 0x7F) {
                AudioLoad_AddUsedSample(&instrument->highNotesSound);
            }
            AudioLoad_AddUsedSample(&instrument->normalNotesSound);
        }
    }

    for (i = 0; i < numDrums; i++) {
        drum = Audio_GetDrum(fontId, i);
        if (drum != NULL) {
            AudioLoad_AddUsedSample(&drum->sound);
        }
    }

    for (i = 0; i < numSfx; i++) {
        sound = Audio_GetSfx(fontId, i);
        if (sound != NULL) {
            AudioLoad_AddUsedSample(sound);
        }
    }

    if (gAudioContext.numUsedSamples == 0) {
        return;
    }

    size = 0;
    for (i = 0; i < gAudioContext.numUsedSamples; i++) {
        size += ALIGN16(gAudioContext.usedSamples[i]->size);
    }
    if (size) {}

    for (i = 0; i < gAudioContext.numUsedSamples; i++) {
        if (gAudioContext.preloadSampleStackTop == ARRAY_COUNT(gAudioContext.usedSamples) - 8) {
            break;
        }

        sample = gAudioContext.usedSamples[i];
        if (sample->medium == MEDIUM_RAM) {
            continue;
        }

        switch (async) {
            case false:
                if (sample->medium == relocInfo->medium1) {
                    addr = (u8*)AudioHeap_AllocSampleCache(sample->size, relocInfo->sampleBankId1, sample->sampleAddr,
                                                      sample->medium, CACHE_PERSISTENT);
                } else if (sample->medium == relocInfo->medium2) {
                    addr = (u8*)AudioHeap_AllocSampleCache(sample->size, relocInfo->sampleBankId2, sample->sampleAddr,
                                                      sample->medium, CACHE_PERSISTENT);
                }
                break;

            case true:
                if (sample->medium == relocInfo->medium1) {
                    addr = (u8*)AudioHeap_AllocSampleCache(sample->size, relocInfo->sampleBankId1, sample->sampleAddr,
                                                      sample->medium, CACHE_TEMPORARY);
                } else if (sample->medium == relocInfo->medium2) {
                    addr = (u8*)AudioHeap_AllocSampleCache(sample->size, relocInfo->sampleBankId2, sample->sampleAddr,
                                                      sample->medium, CACHE_TEMPORARY);
                }
                break;
        }
        if (addr == NULL) {
            continue;
        }

        switch (async) {
            case false:
                if (sample->medium == MEDIUM_UNK) {
                    AudioLoad_SyncDmaUnkMedium((u32)sample->sampleAddr, addr, sample->size,
                                               gAudioContext.sampleBankTable->unkMediumParam);
                    sample->sampleAddr = addr;
                    sample->medium = MEDIUM_RAM;
                } else {
                    AudioLoad_SyncDma(sample->sampleAddr, addr, sample->size, sample->medium);
                    sample->sampleAddr = addr;
                    sample->medium = MEDIUM_RAM;
                }
                break;

            case true:
                preload = &gAudioContext.preloadSampleStack[gAudioContext.preloadSampleStackTop];
                preload->sample = sample;
                preload->ramAddr = addr;
                preload->encodedInfo = (gAudioContext.preloadSampleStackTop << 24) | 0xFFFFFF;
                preload->isFree = false;
                preload->endAndMediumKey = (uintptr_t)sample->sampleAddr + sample->size + sample->medium;
                gAudioContext.preloadSampleStackTop++;
                break;
        }
    }
    gAudioContext.numUsedSamples = 0;

    if (gAudioContext.preloadSampleStackTop != 0 && !preloadInProgress) {
        topPreload = &gAudioContext.preloadSampleStack[gAudioContext.preloadSampleStackTop - 1];
        sample = topPreload->sample;
        nChunks = (sample->size >> 12) + 1;
        AudioLoad_StartAsyncLoad((u32)sample->sampleAddr, topPreload->ramAddr, sample->size, sample->medium, nChunks,
                                 nullptr, topPreload->encodedInfo);
    }
}

void AudioLoad_LoadPermanentSamples(void) {
    s32 pad;
    u32 fontId;
    AudioTable* sampleBankTable;
    s32 pad2;
    s32 i;

    sampleBankTable = AudioLoad_GetLoadTable(SAMPLE_TABLE);
    for (i = 0; i < gAudioContext.permanentPool.count; i++) {
        RelocInfo relocInfo;

        if (gAudioContext.permanentCache[i].tableType == FONT_TABLE) {
            fontId = AudioLoad_GetRealTableIndex(FONT_TABLE, gAudioContext.permanentCache[i].id);
            relocInfo.sampleBankId1 = gAudioContext.soundFonts[fontId].sampleBankId1;
            relocInfo.sampleBankId2 = gAudioContext.soundFonts[fontId].sampleBankId2;

            if (relocInfo.sampleBankId1 != 0xFF) {
                relocInfo.sampleBankId1 = AudioLoad_GetRealTableIndex(SAMPLE_TABLE, relocInfo.sampleBankId1);
                relocInfo.medium1 = sampleBankTable->entries[relocInfo.sampleBankId1].medium;
            }

            if (relocInfo.sampleBankId2 != 0xFF) {
                relocInfo.sampleBankId2 = AudioLoad_GetRealTableIndex(SAMPLE_TABLE, relocInfo.sampleBankId2);
                relocInfo.medium2 = sampleBankTable->entries[relocInfo.sampleBankId2].medium;
            }
            AudioLoad_PreloadSamplesForFont(fontId, false, &relocInfo);
        }
    }
}

void AudioLoad_Unused3(void) {
}

void AudioLoad_Unused4(void) {
}

void AudioLoad_Unused5(void) {
}

void AudioLoad_ScriptLoad(s32 tableType, s32 id, s8* isDone) {
    static u32 sLoadIndex = 0;
    sScriptLoadDonePointers[sLoadIndex] = isDone;
    AudioLoad_AsyncLoad(tableType, id, 0, sLoadIndex, &sScriptLoadQueue);
    sLoadIndex++;
    if (sLoadIndex == 0x10) {
        sLoadIndex = 0;
    }
}

void AudioLoad_ProcessScriptLoads(void) {
    u32 temp = 0;
    u32 sp20;
    s8* isDone;

    if (osRecvMesg(&sScriptLoadQueue, (OSMesg*)&sp20, OS_MESG_NOBLOCK) != -1) {
        temp = sp20 >> 24;
        isDone = sScriptLoadDonePointers[temp];
        if (isDone != NULL) {
            *isDone = 0;
        }
    }
}

void AudioLoad_InitScriptLoads(void) {
    osCreateMesgQueue(&sScriptLoadQueue, sScriptLoadMesgBuf, ARRAY_COUNT(sScriptLoadMesgBuf));
}
