#pragma once
#include <cassert>
#include <cmath>
#include <cstdint>
namespace sampleedit
{
inline float overdriveGain(float gain, unsigned type)
{
    return type == 1 ? gain * 4 : type == 2 ? gain * 10 : gain;
}
// Native CE formulas. Keep double exp/Euler evaluation and PCM truncation order.
inline int16_t overdriveSample(int16_t sample, float gainNorm, unsigned type)
{
    enum
    {
        QUADRATIC,
        EXPONENTIAL,
        RECIPROCAL
    };
    constexpr double kEuler = 2.71828182845904523536;
    int16_t outVal = 0;

    // https://ccrma.stanford.edu/~orchi/Documents/DAFx.pdf
    // https://www.dafx12.york.ac.uk/papers/dafx12_submission_45.pdf
    // https://www.desmos.com/calculator/n6kgglhn9b

    switch (type)
    {
        case QUADRATIC:
        {
            const int32_t in = int32_t(sample * gainNorm);
            const int32_t inAbs = in >= 0 ? in : -in;
            const int32_t inSign = in >= 0 ? INT16_MAX : INT16_MIN;
            if (inAbs > (int32_t)2 * INT16_MAX / 3)
            {
                // y = 1
                outVal = int16_t(inSign);
            }
            else if (inAbs > INT16_MAX / 3)
            {
                // y = (3 - (2 - 3x)^2) / 3
                int32_t temp = 2 * (int32_t)INT16_MAX - 3 * (int32_t)inAbs;
                outVal = int16_t((3 * (int32_t)INT16_MAX - (temp * temp / 0x8000)) / 3);
                if (inSign < 0)
                {
                    outVal = -outVal;
                }
            }
            else
            {
                // y = 2x
                outVal = int16_t(2 * in);
            }
            break;
        }
        case EXPONENTIAL:
        {
            const float in = (float)sample / INT16_MAX;
            const float inSign = in >= 0 ? 1.0f : -1.0f;

            outVal = int16_t((inSign * (kEuler - exp(1.0f - gainNorm * in * inSign)) / kEuler) *
                             INT16_MAX);
            break;
        }
        case RECIPROCAL:
        {
            const float in = (float)sample / INT16_MAX;
            const float inSign = in == 0 ? 0.0f : (in >= 0 ? 1.0f : -1.0f);

            outVal = int16_t(inSign * (1 - 1 / (abs(gainNorm * in) + 1)) * INT16_MAX);
            break;
        }
        default: assert(0);
    }

    return outVal;
}
inline unsigned crusherStep(unsigned rate)
{
    if (!rate)
        return 64;
    const int n = int(44117.64706 / rate + 0.5);
    return unsigned(n < 1 ? 1 : n > 64 ? 64 : n);
}
struct CrushState
{
    int16_t held = 0;
    uint16_t remaining = 0;
};
inline int16_t crushSample(int16_t sample,
                           unsigned bits,
                           unsigned step,
                           uint32_t selectionPosition,
                           CrushState& state)
{
    // CE starts a new hold at every native RAM processBlock boundary, even
    // when step does not divide 8192. SD block boundaries must not reset it.
    if ((selectionPosition & 8191u) == 0)
        state.remaining = 0;
    if (!state.remaining)
    {
        const unsigned shift = 16 - bits;
        // Arithmetic right shift matches supported CE targets. Multiplication
        // avoids the native left-shift-of-negative UB with identical PCM bits.
        state.held = int16_t((int32_t(sample) >> shift) * (int32_t(1) << shift));
        state.remaining = uint16_t(step);
    }
    --state.remaining;
    return state.held;
}
}
