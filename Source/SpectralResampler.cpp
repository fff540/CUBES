Exit code: 0
Wall time: 5.3 seconds
Output:
#include "SpectralResampler.h"

#include <algorithm>
#include <cmath>

void SpectralResampler::reset() noexcept
{
    scratchSpectrum.fill(0.0f);
    analysisMagnitude.fill(0.0f);
    analysisFrequency.fill(0.0f);
    analysisPhase.fill(0.0f);
    synthesisMagnitude.fill(0.0f);
    synthesisFrequency.fill(0.0f);
    synthesisInitialPhase.fill(0.0f);
    synthesisStrongestMagnitude.fill(0.0f);

    for (auto& phases : previousAnalysisPhase)
        phases.fill(0.0f);
    for (auto& phases : synthesisPhase)
        phases.fill(0.0f);

    phaseInitialised.fill(false);
}

void SpectralResampler::process(float* spectrum,
                                int fftLength,
                                int hopSize,
                                float semitones,
                                int channel) noexcept
{
    if (spectrum == nullptr
        || fftLength <= 0
        || fftLength > maximumFftLength
        || hopSize <= 0
        || hopSize > fftLength
        || !juce::isPositiveAndBelow(channel, maximumChannels))
        return;

    const int halfLength = fftLength / 2;
    const int numberOfBins = halfLength + 1;
    const float pitchRatio =
        std::pow(2.0f, semitones / 12.0f);
    const float twoPi =
        juce::MathConstants<float>::twoPi;
    const float expectedPhaseAdvance =
        twoPi * static_cast<float>(hopSize)
        / static_cast<float>(fftLength);
    const bool firstFrame =
        !phaseInitialised[static_cast<size_t>(channel)];
    auto& previousPhase =
        previousAnalysisPhase[static_cast<size_t>(channel)];
    auto& accumulatedPhase =
        synthesisPhase[static_cast<size_t>(channel)];

    std::fill(analysisMagnitude.begin(),
              analysisMagnitude.begin() + numberOfBins,
              0.0f);
    std::fill(analysisFrequency.begin(),
              analysisFrequency.begin() + numberOfBins,
              0.0f);
    std::fill(analysisPhase.begin(),
              analysisPhase.begin() + numberOfBins,
              0.0f);
    std::fill(synthesisMagnitude.begin(),
              synthesisMagnitude.begin() + numberOfBins,
              0.0f);
    std::fill(synthesisFrequency.begin(),
              synthesisFrequency.begin() + numberOfBins,
              0.0f);
    std::fill(synthesisInitialPhase.begin(),
              synthesisInitialPhase.begin() + numberOfBins,
              0.0f);
    std::fill(synthesisStrongestMagnitude.begin(),
              synthesisStrongestMagnitude.begin() + numberOfBins,
              0.0f);

    for (int bin = 0; bin <= halfLength; ++bin)
    {
        const float real = spectrum[bin * 2];
        const float imaginary = spectrum[bin * 2 + 1];
        const float magnitude = std::hypot(real, imaginary);
        const float phase = std::atan2(imaginary, real);
        float trueBin = static_cast<float>(bin);

        if (!firstFrame)
        {
            float phaseDelta =
                phase
                - previousPhase[static_cast<size_t>(bin)]
                - static_cast<float>(bin)
                      * expectedPhaseAdvance;
            phaseDelta = std::remainder(phaseDelta, twoPi);
            trueBin += phaseDelta / expectedPhaseAdvance;
        }

        previousPhase[static_cast<size_t>(bin)] = phase;
        analysisMagnitude[static_cast<size_t>(bin)] = magnitude;
        analysisFrequency[static_cast<size_t>(bin)] = trueBin;
        analysisPhase[static_cast<size_t>(bin)] = phase;
    }

    for (int sourceBin = 0; sourceBin <= halfLength; ++sourceBin)
    {
        const float destinationPosition =
            static_cast<float>(sourceBin) * pitchRatio;
        if (destinationPosition > static_cast<float>(halfLength))
            continue;

        const int lowerBin =
            static_cast<int>(std::floor(destinationPosition));
        const int upperBin = lowerBin + 1;
        const float upperWeight =
            destinationPosition - static_cast<float>(lowerBin);
        const float lowerWeight = 1.0f - upperWeight;
        const float mappedFrequency =
            analysisFrequency[static_cast<size_t>(sourceBin)]
            * pitchRatio;
        const float magnitude =
            analysisMagnitude[static_cast<size_t>(sourceBin)];
        const float sourcePhase =
            analysisPhase[static_cast<size_t>(sourceBin)];

        const auto addContribution =
            [&](int destinationBin, float weight)
            {
                if (weight <= 0.0f
                    || !juce::isPositiveAndBelow(
                        destinationBin, numberOfBins))
                    return;

                const float contribution = magnitude * weight;
                const auto index =
                    static_cast<size_t>(destinationBin);
                synthesisMagnitude[index] += contribution;
                synthesisFrequency[index] +=
                    mappedFrequency * contribution;

                if (contribution
                    > synthesisStrongestMagnitude[index])
                {
                    synthesisStrongestMagnitude[index] =
                        contribution;
                    synthesisInitialPhase[index] = sourcePhase;
                }
            };

        addContribution(lowerBin, lowerWeight);
        addContribution(upperBin, upperWeight);
    }

    std::fill(scratchSpectrum.begin(),
              scratchSpectrum.begin() + fftLength * 2,
              0.0f);

    for (int bin = 0; bin <= halfLength; ++bin)
    {
        const auto index = static_cast<size_t>(bin);
        const float magnitude = synthesisMagnitude[index];
        const float trueBin =
            magnitude > 0.0000001f
                ? synthesisFrequency[index] / magnitude
                : static_cast<float>(bin);

        if (firstFrame)
            accumulatedPhase[index] =
                synthesisInitialPhase[index];
        else
            accumulatedPhase[index] = std::remainder(
                accumulatedPhase[index]
                    + expectedPhaseAdvance * trueBin,
                twoPi);

        scratchSpectrum[index * 2] =
            magnitude * std::cos(accumulatedPhase[index]);
        scratchSpectrum[index * 2 + 1] =
            magnitude * std::sin(accumulatedPhase[index]);
    }

    phaseInitialised[static_cast<size_t>(channel)] = true;
    std::copy(scratchSpectrum.begin(),
              scratchSpectrum.begin() + fftLength * 2,
              spectrum);
}

