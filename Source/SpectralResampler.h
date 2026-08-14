#pragma once

#include <JuceHeader.h>

#include <array>
#include <cstdint>

class SpectralResampler final
{
public:
    static constexpr int maximumFftLength = 1 << 10;
    static constexpr int maximumChannels = 2;

    void reset() noexcept;

    void process(float* interleavedSpectrum,
                 int fftLength,
                 int hopSize,
                 float semitones,
                 int channel) noexcept;

private:
    static constexpr int maximumBins = maximumFftLength / 2 + 1;
    using BinBuffer = std::array<float, maximumBins>;
    using ChannelBinBuffers =
        std::array<BinBuffer, maximumChannels>;

    std::array<float, maximumFftLength * 2> scratchSpectrum {};
    BinBuffer analysisMagnitude {};
    BinBuffer analysisFrequency {};
    BinBuffer analysisPhase {};
    BinBuffer synthesisMagnitude {};
    BinBuffer synthesisFrequency {};
    BinBuffer synthesisInitialPhase {};
    BinBuffer synthesisStrongestMagnitude {};
    ChannelBinBuffers previousAnalysisPhase {};
    ChannelBinBuffers synthesisPhase {};
    std::array<bool, maximumChannels> phaseInitialised {};
};

