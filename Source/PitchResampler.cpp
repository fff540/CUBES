#include "PitchResampler.h"

#include <algorithm>
#include <cmath>
#include <cstring>

PitchResampler::PitchResampler()
{
    for (int sample = 0; sample < fftLength; ++sample)
    {
        const auto phase = juce::MathConstants<float>::twoPi
                         * static_cast<float>(sample)
                         / static_cast<float>(fftLength);
        const float hann = 0.5f - std::cos(phase) * 0.5f;
        const float squareRootHann =
            std::sqrt(juce::jmax(0.0f, hann));
        analysisWindow[static_cast<size_t>(sample)] =
            squareRootHann;
        synthesisWindow[static_cast<size_t>(sample)] =
            squareRootHann;
    }

    reset();
}

void PitchResampler::prepare(double sampleRate) noexcept
{
    currentSampleRate = sampleRate > 0.0 ? sampleRate : 44100.0;
    reset();
}

void PitchResampler::reset() noexcept
{
    for (auto& channel : inputFifo)
        channel.fill(0.0f);
    for (auto& channel : outputFifo)
        channel.fill(0.0f);
    for (auto& channel : outputAccumulator)
        channel.fill(0.0f);
    for (auto& channel : fftData)
        channel.fill(0.0f);

    spectralResampler.reset();
    rover = 0;
    smoothedSemitones = 0.0f;
    pitchIsInitialised = false;
}

void PitchResampler::process(juce::AudioBuffer<float>& buffer,
                             float semitones) noexcept
{
    const int numberOfSamples = buffer.getNumSamples();
    const int channelsToProcess = juce::jmin(maximumChannels,
                                             buffer.getNumChannels());
    if (channelsToProcess <= 0)
        return;

    if (channelsToProcess != activeChannels)
    {
        activeChannels = channelsToProcess;
        reset();
    }

    std::array<float*, maximumChannels> channelData {};
    for (int channel = 0; channel < channelsToProcess; ++channel)
        channelData[static_cast<size_t>(channel)] = buffer.getWritePointer(channel);

    if (rover == 0)
        rover = fftLength - hopSize;

    for (int sample = 0; sample < numberOfSamples; ++sample)
    {
        for (int channel = 0; channel < channelsToProcess; ++channel)
        {
            auto* data = channelData[static_cast<size_t>(channel)];
            inputFifo[static_cast<size_t>(channel)][static_cast<size_t>(rover)]
                = data[sample];
            data[sample]
                = outputFifo[static_cast<size_t>(channel)]
                            [static_cast<size_t>(rover - (fftLength - hopSize))];
        }

        if (++rover >= fftLength)
        {
            rover = fftLength - hopSize;
            processSpectralFrame(channelsToProcess, semitones);
        }
    }
}

void PitchResampler::processSpectralFrame(int numberOfChannels,
                                          float semitones) noexcept
{
    if (!pitchIsInitialised)
    {
        smoothedSemitones = semitones;
        pitchIsInitialised = true;
    }
    else
    {
        constexpr double smoothingTimeSeconds = 0.035;
        const float smoothingAmount = static_cast<float>(
            1.0 - std::exp(
                -static_cast<double>(hopSize)
                / (currentSampleRate * smoothingTimeSeconds)));
        smoothedSemitones +=
            (semitones - smoothedSemitones) * smoothingAmount;
    }

    for (int channel = 0; channel < numberOfChannels; ++channel)
    {
        auto& spectrum = fftData[static_cast<size_t>(channel)];
        std::fill(spectrum.begin(), spectrum.end(), 0.0f);
        const auto& input =
            inputFifo[static_cast<size_t>(channel)];
        for (int sample = 0; sample < fftLength; ++sample)
            spectrum[static_cast<size_t>(sample)] =
                input[static_cast<size_t>(sample)]
                * analysisWindow[static_cast<size_t>(sample)];

        fft.performRealOnlyForwardTransform(spectrum.data(), true);
        spectralResampler.process(
            spectrum.data(),
            fftLength,
            hopSize,
            smoothedSemitones,
            channel);

        spectrum[0] = 0.0f;
        spectrum[1] = 0.0f;
        spectrum[static_cast<size_t>(fftLength + 1)] = 0.0f;
        fft.performRealOnlyInverseTransform(spectrum.data());

        auto& accumulator = outputAccumulator[static_cast<size_t>(channel)];
        auto& fifo = outputFifo[static_cast<size_t>(channel)];

        for (int sample = 0; sample < fftLength; ++sample)
            accumulator[static_cast<size_t>(sample)]
                += spectrum[static_cast<size_t>(sample)]
                   * synthesisWindow[static_cast<size_t>(sample)];

        for (int sample = 0; sample < hopSize; ++sample)
            fifo[static_cast<size_t>(sample)] =
                accumulator[static_cast<size_t>(sample)];

        std::memmove(accumulator.data(),
                     accumulator.data() + hopSize,
                     static_cast<size_t>(hopSize) * sizeof(float));
        std::fill(accumulator.begin() + hopSize,
                  accumulator.end(), 0.0f);

        auto& shiftedInput = inputFifo[static_cast<size_t>(channel)];
        std::memmove(shiftedInput.data(),
                     shiftedInput.data() + hopSize,
                     static_cast<size_t>(hopSize) * sizeof(float));
    }
}
