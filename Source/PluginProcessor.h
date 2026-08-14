/*
  ==============================================================================
    This file contains the basic framework code for a JUCE plugin processor.
  ==============================================================================
*/

#pragma once

#include <JuceHeader.h>
#include "PitchResampler.h"
#include <atomic>
#include <array>
#include <vector>

//==============================================================================
class NewProjectAudioProcessor : public juce::AudioProcessor
{
public:
    static constexpr int numSliceSides = 4;

    enum class WallType
    {
        normal = 0,
        soft,
        boost,
        teleport,
        oneShot,
        count
    };

    struct SavedStrokeState
    {
        WallType type = WallType::normal;
        std::vector<juce::Point<float>> points;
    };

    struct SavedParticleState
    {
        int sliceIndex = 0;
        float x = 0.0f;
        float y = 0.0f;
        float vx = 0.0f;
        float vy = 0.0f;
        float size = 0.0f;
        juce::uint32 colour = 0;
        bool isThrown = false;
        int pitchSemitones = 0;
        bool isReversed = false;
        std::vector<juce::Point<float>> trajectory;
        float trajectoryDistance = 0.0f;
        bool trajectoryForward = true;
    };

    bool uiSettingsOpen = false;
    bool uiStopTime = false;
    bool uiClearLines = false;
    bool uiCollisions = false;
    bool uiRecordingPanelOpen = false;
    int uiWallBrush = static_cast<int>(WallType::normal);

    NewProjectAudioProcessor();
    ~NewProjectAudioProcessor() override;

    void prepareToPlay(double sampleRate, int samplesPerBlock) override;
    void releaseResources() override;

#ifndef JucePlugin_PreferredChannelConfigurations
    bool isBusesLayoutSupported(const BusesLayout& layouts) const override;
#endif

    void processBlock(juce::AudioBuffer<float>&, juce::MidiBuffer&) override;

    juce::AudioProcessorEditor* createEditor() override;
    bool hasEditor() const override;

    const juce::String getName() const override;

    bool acceptsMidi() const override;
    bool producesMidi() const override;
    bool isMidiEffect() const override;
    double getTailLengthSeconds() const override;

    int getNumPrograms() override;
    int getCurrentProgram() override;
    void setCurrentProgram(int index) override;
    const juce::String getProgramName(int index) override;
    void changeProgramName(int index, const juce::String& newName) override;

    void getStateInformation(juce::MemoryBlock& destData) override;
    void setStateInformation(const void* data, int sizeInBytes) override;

    //==============================================================================
    void loadAudioBuffer(const juce::AudioBuffer<float>& buffer);
    bool hasLoadedAudio() const noexcept;
    void copyLoadedAudioBuffer(juce::AudioBuffer<float>& destination) const;
    void setSlice(int index, int startSample, int lengthInSamples);
    void setSliceForSide(
        int index, int side, int startSample, int lengthInSamples);
    void setSlicePitch(int index, int semitones);
    void setSliceReversed(int index, bool shouldBeReversed);
    bool getSliceRange(int index, int& startSample, int& lengthInSamples) const;
    bool getSliceRanges(
        int index,
        std::array<int, numSliceSides>& startSamples,
        std::array<int, numSliceSides>& lengthsInSamples) const;
    void playSlice(int index, int hitType = 0);
    void updateSavedParticles(const std::vector<SavedParticleState>& particles);
    void updateSavedStrokes(
        const std::vector<SavedStrokeState>& strokes);
    bool copySavedEditorState(
        std::vector<SavedParticleState>& particles,
        std::vector<SavedStrokeState>& strokes) const;

    // --- Rolling Sampler Data ---
    juce::AudioBuffer<float> rollingBuffer;
    std::atomic<int> rollingWritePos{ 0 };
    std::atomic<int> rollingVisualSamples{ 0 };
    int rollingCapacity = 0;
    double currentSampleRate = 44100.0;

    juce::AudioProcessorValueTreeState apvts;

private:
    juce::AudioProcessorValueTreeState::ParameterLayout createParameterLayout();
    void updateRandStartPositions(int blockSamples, int totalSamples) noexcept;

    static constexpr int numSlices = 20;

    struct SliceData {
        SliceData()
        {
            for (int side = 0; side < numSliceSides; ++side)
            {
                startSamples[static_cast<size_t>(side)].store(0);
                numSamples[static_cast<size_t>(side)].store(0);
            }
        }

        std::array<std::atomic<int>, numSliceSides> startSamples;
        std::array<std::atomic<int>, numSliceSides> numSamples;
        std::atomic<int> pitchSemitones{ 0 };
        std::atomic<bool> reversed{ false };

        std::atomic<bool> trigger{ false };
        std::atomic<int> pendingStartSample{ 0 };
        std::atomic<int> pendingNumSamples{ 0 };
        std::atomic<float> pendingGain{ 1.0f };
        std::atomic<float> pendingPan{ 0.0f };

        double spectralPlayhead = -1.0;
        double ordinaryPlayhead = -1.0;
        bool spectralNoteOffTriggered = false;
        bool ordinaryNoteOffTriggered = false;

        int activeStartSample = 0;
        int activeNumSamples = 0;
        float activeGain = 1.0f;
        float activePan = 0.0f;
        bool activeReversed = false;

        std::array<float, 2> spectralLastOutput{};
        std::array<float, 2> ordinaryLastOutput{};
        std::array<float, 2> spectralTransitionStart{};
        std::array<float, 2> ordinaryTransitionStart{};
        int spectralTransitionRemaining = 0;
        int ordinaryTransitionRemaining = 0;
    };

    std::array<SliceData, numSlices> sliceData;

    struct RandMotionState
    {
        double position = 0.0;
        float direction = 1.0f;
        float speedScale = 1.0f;
        int publishedStart = 0;
    };

    std::array<std::array<RandMotionState, numSliceSides>, numSlices>
        randMotionStates{};
    juce::Random randMotionRandom;
    bool randMotionActive = false;

    std::array<juce::ADSR, numSlices> spectralAdsrs;
    std::array<juce::ADSR, numSlices> ordinaryAdsrs;
    juce::ADSR::Parameters adsrParams;

    juce::AudioBuffer<float> sampleBuffer;
    juce::AudioBuffer<float> spectralMixBuffer;
    juce::AudioBuffer<float> ordinaryMixBuffer;
    std::atomic<bool> isReady{ false };
    PitchResampler pitchResampler;
    juce::dsp::DelayLine<
        float,
        juce::dsp::DelayLineInterpolationTypes::None>
        ordinaryPitchDelay { PitchResampler::latencySamples + 1 };
    juce::SmoothedValue<
        float,
        juce::ValueSmoothingTypes::Linear>
        ordinaryModeBlend;
    juce::Random humanizeRandom;
    int retriggerTransitionSamples = 128;

    mutable juce::CriticalSection savedStateLock;
    std::vector<SavedParticleState> savedParticles;
    std::vector<SavedStrokeState> savedStrokes;
    bool hasSavedEditorState = false;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(NewProjectAudioProcessor)
};

