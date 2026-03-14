#pragma once

#include <vector>
#include <atomic>
#include <cmath>
#include <algorithm>

// Thread-safe ring buffer for audio.
// Write happens on the audio thread; read also on audio thread (safe).
// UI thread may read for visualisation — we accept occasional torn reads there.
template <typename T>
class RingBuffer
{
public:
    RingBuffer() = default;

    void prepare(int numChannels, int numSamples)
    {
        bufferSize = numSamples;
        buffer.assign(numChannels, std::vector<T>(numSamples, T(0)));
        writePos.store(0, std::memory_order_relaxed);
    }

    int getNumChannels() const { return (int)buffer.size(); }
    int getSize()        const { return bufferSize; }

    // Write interleaved block. Called only from the audio thread.
    void write(const T* const* channelData, int numChannels, int numSamples)
    {
        int wp = writePos.load(std::memory_order_relaxed);
        const int nch = std::min(numChannels, (int)buffer.size());

        for (int i = 0; i < numSamples; ++i)
        {
            for (int ch = 0; ch < nch; ++ch)
                buffer[ch][wp] = channelData[ch][i];

            if (++wp >= bufferSize) wp = 0;
        }
        writePos.store(wp, std::memory_order_release);
    }

    // Read with linear interpolation.
    // offset = how many samples behind the write head (0 = most-recent written sample).
    T readSample(int channel, double offset) const
    {
        if (channel >= (int)buffer.size()) return T(0);

        int wp = writePos.load(std::memory_order_acquire);

        // Wrap offset into valid range
        if (offset < 0.0) offset = 0.0;
        if (offset >= (double)(bufferSize - 1)) offset = (double)(bufferSize - 2);

        double pos = (double)wp - offset - 1.0;
        // Bring pos into [0, bufferSize)
        while (pos < 0.0)            pos += bufferSize;
        while (pos >= (double)bufferSize) pos -= bufferSize;

        int   idx0 = (int)pos;
        int   idx1 = (idx0 + 1) % bufferSize;
        double frac = pos - idx0;

        return static_cast<T>(buffer[channel][idx0] * (1.0 - frac)
                            + buffer[channel][idx1] * frac);
    }

    // Read by absolute ring-buffer position (fractional, wrapping).
    T readAtAbsolutePosition(int channel, double position) const
    {
        if (channel >= (int)buffer.size()) return T(0);

        // Normalise to [0, bufferSize)
        double pos = std::fmod(position, (double)bufferSize);
        if (pos < 0.0) pos += bufferSize;

        int   idx0 = (int)pos;
        int   idx1 = (idx0 + 1) % bufferSize;
        double frac = pos - idx0;

        return static_cast<T>(buffer[channel][idx0] * (1.0 - frac)
                            + buffer[channel][idx1] * frac);
    }

    int getWritePosition() const { return writePos.load(std::memory_order_acquire); }

    // Unsafe direct pointer — visualisation only.
    const T* getChannelPointer(int ch) const
    {
        return (ch < (int)buffer.size()) ? buffer[ch].data() : nullptr;
    }

private:
    std::vector<std::vector<T>> buffer;
    int bufferSize = 0;
    std::atomic<int> writePos { 0 };
};
