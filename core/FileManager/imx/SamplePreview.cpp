#if TRACKER_CE_ENABLE_SD_MONO || TRACKER_CE_ENABLE_SD_EDITS
#include "SamplePreview.h"
#include "AudioStream.h"
#include "SampleFileEdit.h"
#include "SamplePreviewEffects.h"
#include "VoiceStreamManager.h"
#include "fileManager.h"
#include "fileManagerDefs.h"
#include "mtStructs.h"
#include "sampleProcessing/PcmEffects.h"
#include <algorithm>
#include <cstring>
#include <new>
namespace samplepreview
{
namespace
{
constexpr uint32_t kReadFrames = 1024;
enum class Phase
{
    Scan,
    Load,
    Process,
    Ready,
    Failed
};
struct Job
{
    unsigned slot;
    strInstrument instrument;
    strSampleMemory memory;
    uint32_t first, last, count, cursor = 0;
    unsigned channel = 0;
    int peaks[2]{};
    const EffectCopy* spec;
    mtEffect* effect = nullptr;
    int16_t* input[2];
    int16_t* output[2];
    Phase phase;
    Job(unsigned s, uint32_t a, uint32_t b, const EffectCopy* e)
      : slot(s)
      , first(a)
      , last(b)
      , count(std::min(b - a, kMaximumFrames))
      , spec(e)
      , phase(e->fullSelectionPeaks ? Phase::Scan : Phase::Load)
    {
        std::memcpy(&instrument, &mtProject.instrument[s], sizeof(instrument));
        std::memcpy(&memory, &mtProject.sampleMemory[s], sizeof(memory));
    }
};
Job* job = nullptr;
const char* message = "";
uintptr_t aligned(uintptr_t p)
{
    return (p + 31u) & ~uintptr_t(31u);
}
void fail(const char* text)
{
    message = text;
    job->phase = Phase::Failed;
}
}
bool supported(unsigned effect)
{
    return effectCopy(effect) != nullptr;
}
bool active()
{
    return job != nullptr;
}
bool busy()
{
    return job && job->phase != Phase::Ready && job->phase != Phase::Failed;
}
bool ready()
{
    return job && job->phase == Phase::Ready;
}
bool valid()
{
    if (!job)
        return false;
    const auto& m = mtProject.sampleMemory[job->slot];
    return !sampleedit::active() && newFileManager.getStatus() == fmIdle &&
           newFileManager.getCurrentOperation() == fmNoOperation &&
           !std::memcmp(
             &job->instrument, &mtProject.instrument[job->slot], sizeof(job->instrument)) &&
           m.address[0] == job->memory.address[0] && m.address[1] == job->memory.address[1] &&
           m.isStereo == job->memory.isStereo && m.isStreamed == job->memory.isStreamed &&
           m.sampleHash == job->memory.sampleHash &&
           m.fileDataOffset == job->memory.fileDataOffset && m.ramFrames == job->memory.ramFrames &&
           m.preCacheFrames == job->memory.preCacheFrames;
}
bool begin(unsigned slot, unsigned effect, const mtEffect& original, uint16_t a, uint16_t b)
{
    if (job || sampleedit::active())
    {
        message = "Sample editor busy.";
        return false;
    }
    if (original.hasPreviewWorkspaceConflict())
    {
        message = "Finish current RAM edits before preview.";
        return false;
    }
    const auto* spec = effectCopy(effect);
    if (!spec || slot >= 48 || !mtProject.instrument[slot].isActive)
    {
        message = "Preview is unavailable.";
        return false;
    }
    if (newFileManager.getStatus() != fmIdle ||
        newFileManager.getCurrentOperation() != fmNoOperation)
    {
        message = "File manager busy.";
        return false;
    }
    if (a > b)
        std::swap(a, b);
    const auto total = mtProject.instrument[slot].sample.length;
    const auto first = std::min(total, uint32_t(uint32_t(a) * float(total) / MAX_16BIT)),
               last = std::min(total, uint32_t(uint32_t(b) * float(total) / MAX_16BIT));
    if (first >= last)
    {
        message = "Empty preview selection.";
        return false;
    }
    const auto& m = mtProject.sampleMemory[slot];
    const auto ram = m.ramFrames ? m.ramFrames : total;
    if (!m.address[0] ||
        (m.isStereo && uint64_t(uintptr_t(m.address[1])) !=
                         uint64_t(uintptr_t(m.address[0])) + uint64_t(ram) * 2) ||
        (!m.isStreamed && ram != total) ||
        uint64_t(total) * (m.isStereo ? 4 : 2) + m.fileDataOffset > UINT32_MAX ||
        (m.isStreamed && (!m.ramFrames || m.preCacheFrames > ram)))
    {
        message = "Invalid preview source layout.";
        return false;
    }
    const auto count = std::min(last - first, kMaximumFrames);
    const auto bytes =
      unsigned(aligned(sizeof(Job)) + aligned(spec->bytes) + uint64_t(count) * 4 + 64);
    void* storage = sampleedit::workBuffer(bytes);
    if (!storage)
    {
        message = sampleedit::result();
        return false;
    }
    job = new (storage) Job(slot, first, last, spec);
    auto ptr = aligned(uintptr_t(storage) + sizeof(Job));
    job->effect = spec->copy(reinterpret_cast<void*>(ptr), original);
    ptr = aligned(ptr + spec->bytes);
    for (unsigned ch = 0; ch < 2; ++ch)
    {
        // All registered native kernels are verified in-place safe. Keep one
        // private PCM copy per channel rather than duplicate bounded buffers.
        job->input[ch] = reinterpret_cast<int16_t*>(ptr);
        job->output[ch] = job->input[ch];
        ptr = aligned(ptr + count * 2);
    }
    job->effect->bindPreviewBuffers(job->input, job->output, count);
    if (job->effect->getExpectedProcessLength() != count)
    {
        fail("Preview effect changes the excerpt length.");
        return true;
    }
    message = "";
    return true;
}
void tick()
{
    if (!busy())
        return;
    if (!valid())
    {
        fail("Instrument changed; preview cancelled.");
        return;
    }
    auto& j = *job;
    if (j.phase == Phase::Scan)
    {
        const auto n = std::min(kReadFrames, j.last - j.first - j.cursor);
        if (!voiceStreamManager.readInstrumentFramesBlocking(
              j.slot, j.channel, j.first + j.cursor, n, j.input[0]))
        {
            fail("Cannot read preview selection.");
            return;
        }
        for (unsigned i = 0; i < n; ++i)
            j.peaks[j.channel] = std::max(j.peaks[j.channel], ce_pcm_peak(j.input[0][i]));
        j.cursor += n;
        if (j.cursor == j.last - j.first)
        {
            j.cursor = 0;
            if (++j.channel == (j.memory.isStereo ? 2u : 1u))
            {
                if (!j.memory.isStereo)
                    j.peaks[1] = j.peaks[0];
                j.channel = 0;
                j.phase = Phase::Load;
            }
        }
        return;
    }
    if (j.phase == Phase::Load)
    {
        const auto n = std::min(kReadFrames, j.count - j.cursor);
        if (!voiceStreamManager.readInstrumentFramesBlocking(
              j.slot, j.channel, j.first + j.cursor, n, j.input[j.channel] + j.cursor))
        {
            fail("Cannot read preview excerpt.");
            return;
        }
        j.cursor += n;
        if (j.cursor == j.count)
        {
            j.cursor = 0;
            if (++j.channel == (j.memory.isStereo ? 2u : 1u))
            {
                if (!j.memory.isStereo)
                    std::memcpy(j.input[1], j.input[0], j.count * 2);
                j.channel = 0;
                if (!j.spec->start(j.effect, j.peaks, j.memory.isStereo))
                {
                    fail("Cannot process preview.");
                    return;
                }
                j.phase = Phase::Process;
            }
        }
        return;
    }
    if (j.phase == Phase::Process)
    {
        const auto n = j.effect->processBlock();
        if (n > 0)
            j.cursor += uint32_t(n);
        if (j.cursor > j.count)
        {
            fail("Preview length exceeded.");
            return;
        }
        if (!j.effect->getProcessState())
        {
            if (j.cursor != j.count)
            {
                fail("Incomplete preview.");
                return;
            }
            j.phase = Phase::Ready;
        }
    }
}
unsigned progress()
{
    if (!job)
        return 0;
    const auto& j = *job;
    if (j.phase == Phase::Scan)
        return unsigned(uint64_t(j.cursor + (j.last - j.first) * j.channel) * 40 /
                        ((j.last - j.first) * (j.memory.isStereo ? 2ull : 1ull)));
    if (j.phase == Phase::Load)
        return 40 + unsigned(uint64_t(j.cursor + j.count * j.channel) * 30 /
                             (j.count * (j.memory.isStereo ? 2ull : 1ull)));
    return 70 + unsigned(uint64_t(std::min(j.cursor, j.count)) * 30 / j.count);
}
const char* error()
{
    return message;
}
int16_t* data(unsigned channel)
{
    return ready() ? job->output[channel ? 1 : 0] : nullptr;
}
uint32_t frames()
{
    return ready() ? job->count : 0;
}
void release()
{
    if (!job)
        return;
    job->spec->destroy(job->effect);
    job->~Job();
    job = nullptr;
}
}
#endif
