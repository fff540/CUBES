Exit code: 0
Wall time: 5.4 seconds
Output:
#pragma once

#include <JuceHeader.h>

#include "SpectralResampler.h"

#include <array>
#include <cstdint>

class PitchResampler final
{
public:
    static constexpr int fftOrder = 10;
    static constexpr int fftLength = 1 << fftOrder;
    static constexpr int latencySamples = fftLength;

    PitchResampler();

    void prepare(double sampleRate) noexcept;
    void reset() noexcept;
    void process(juce::AudioBuffer<float>& buffer, float semitones) noexcept;

private:
    static constexpr int hopSize = fftLength / 2;
    static constexpr int maximumChannels = 2;

    using TimeBuffer = std::array<float, fftLength>;
    using SpectrumBuffer = std::array<float, fftLength * 2>;
    using ChannelTimeBuffers = std::array<TimeBuffer, maximumChannels>;
    using ChannelSpectrumBuffers = std::array<SpectrumBuffer, maximumChannels>;

    void processSpectralFrame(int numberOfChannels, float semitones) noexcept;

    juce::dsp::FFT fft { fftOrder };
    TimeBuffer analysisWindow {};
    TimeBuffer synthesisWindow {};
    ChannelTimeBuffers inputFifo {};
    ChannelTimeBuffers outputFifo {};
    ChannelTimeBuffers outputAccumulator {};
    ChannelSpectrumBuffers fftData {};
    SpectralResampler spectralResampler;

    int rover = 0;
    int activeChannels = maximumChannels;
    double currentSampleRate = 44100.0;
    float smoothedSemitones = 0.0f;
    bool pitchIsInitialised = false;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(PitchResampler)
};

