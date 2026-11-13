#include "Audio.h"
#include "play_memory.h"
#include "utility/dspinst.h"

AUDIO_CORE_FLASH void AudioPlayMemory::playBeatSlice(uint8_t instrIdx, int8_t note)
{
    if (instrIdx >= kSampleInstrumentsCount)
        return;
    const uint8_t sliceCount = mtProject.instrument[instrIdx].sliceNumber;
    if (sliceCount > MAX_SLICE_NUMBER)
        return;

    AudioNoInterrupts();

    uint8_t localSlice = 0;
    if (sliceCount > 0 && note >= FIRST_BEAT_SLICE_NOTE)
    {
        const uint8_t requestedSlice = note - FIRST_BEAT_SLICE_NOTE;
        localSlice = requestedSlice < sliceCount ? requestedSlice : sliceCount - 1;
    }
    // Revalidate S2 at the actual start: slices may have been removed since FX setup.
    const uint8_t requestedForcedSlice = forcedSlice;
    const uint8_t nativeSlice = (sliceForcedFlag && sliceCount > 0)
                                 ? (requestedForcedSlice < sliceCount ? requestedForcedSlice : sliceCount - 1)
                                 : localSlice;

#if TRACKER_CE_ENABLE_WR2
    const uint8_t activeSlice=wr2Slice(instrIdx,nativeSlice);
#else
    const uint8_t activeSlice=nativeSlice;
#endif

    note = 60; // zerowy pitch

    lastInstrIdxInPlay = currentInstrIdx;
    currentInstrIdx = instrIdx;

    refreshStartParameters();

    constrainCurrentTune(note);

    setStartPitch(note);

    applyFinetuneOnPitch(note);

    calculateGlidePitch(note);

    lastNote = note;

    const uint16_t startSlice = sliceCount > 0
                                  ? mtProject.instrument[instrIdx].slices[activeSlice]
                                  : 0;
    const uint16_t endSlice = (!mtProject.instrument[instrIdx].slicePlayThru &&
                               sliceCount > 1 && activeSlice < sliceCount - 1)
                                ? mtProject.instrument[instrIdx].slices[activeSlice + 1]
                                : MAX_16BIT;

#if TRACKER_CE_ENABLE_WR2
    if (wr2Prepared(instrIdx,startSlice,endSlice)) {}
    else
#endif
    if (samplePointsSource != nullptr)
    {
        pointsInSamples.start = samplePointsSource->pointsInSamples.start;
        pointsInSamples.end = samplePointsSource->pointsInSamples.end;
    }
    else
    {
        pointsInSamples.start = calculatePointPositionAndSnapToZero(
          startSlice, (uint32_t)currentSampleLength, (int16_t*)currentStartAddress);
        pointsInSamples.end = calculatePointPositionAndSnapToZero(
          endSlice, (uint32_t)currentSampleLength, (int16_t*)currentStartAddress);
    }

    pointsInSamples.loop1 = 0;
    pointsInSamples.loop2 = 0;

    constrainsInSamples.endPoint = pointsInSamples.end - pointsInSamples.start;

    next = currentStartAddress + pointsInSamples.start;
    length = currentSampleLength - pointsInSamples.start;
    iPitchCounter = reverseDirectionFlag ? constrainsInSamples.endPoint - 1 : 0;

    if (currentIsStreamed)
    {
        if (!reverseDirectionFlag && sliceCount > 0)
        {
            voiceStreamManager.getSlicePreCache(instrIdx,
                                                activeSlice,
                                                channel,
                                                pointsInSamples.start,
                                                currentSliceCacheAddress,
                                                currentSliceCacheFrames);
        }
        if (channel == 0 && voiceIndex >= 0)
        {
            voiceStreamManager.notifyNoteOn((uint8_t)voiceIndex,
                                            instrIdx,
                                            pointsInSamples.start,
                                            pointsInSamples.start,
                                            pointsInSamples.end,
                                            pointsInSamples.end,
                                            currentPlayMode,
                                            reverseDirectionFlag != 0,
                                            pitchControl,
                                            currentSliceCacheFrames);
        }
    }

#if TRACKER_CE_ENABLE_WR2
    wr2Commit(instrIdx);
#endif
    playing = 1;
    AudioInterrupts();
}

AUDIO_CORE_RAM audio_block_t* AudioPlayMemory::updateBeatSlice()
{
    return updateSingleShot();
}
