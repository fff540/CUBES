/*
  ==============================================================================
    This file contains the basic framework code for a JUCE plugin processor.
  ==============================================================================
*/

#include "PluginProcessor.h"
#include "PluginEditor.h"

#include <cmath>
#include <cstring>
#include <limits>

namespace
{
constexpr int cubesStateMagic = 0x43554245;
constexpr int cubesStateVersion = 4;
constexpr std::array<const char*, NewProjectAudioProcessor::numSliceSides>
    sliceStartProperties {
        "leftStart", "rightStart", "upStart", "downStart"
    };
constexpr std::array<const char*, NewProjectAudioProcessor::numSliceSides>
    sliceLengthProperties {
        "leftLength", "rightLength", "upLength", "downLength"
    };
constexpr std::array<int, NewProjectAudioProcessor::numSliceSides>
    legacySideOffsets { 0, 2, 1, 3 };

int hitTypeToSideIndex(int hitType)
{
    if (hitType == 2)
        return 1;
    if (hitType == 1)
        return 2;
    if (hitType == 3)
        return 3;
    return 0;
}

juce::MemoryBlock audioBufferToMemoryBlock(
    const juce::AudioBuffer<float>& buffer)
{
    const auto channels = buffer.getNumChannels();
    const auto samples = buffer.getNumSamples();

    if (channels <= 0 || samples <= 0)
        return {};

    const auto channelBytes =
        static_cast<size_t>(samples) * sizeof(float);
    const auto totalBytes =
        static_cast<size_t>(channels) * channelBytes;
    juce::MemoryBlock data(totalBytes);
    auto* destination = static_cast<char*>(data.getData());

    for (int channel = 0; channel < channels; ++channel)
        std::memcpy(destination + static_cast<size_t>(channel) * channelBytes,
                    buffer.getReadPointer(channel),
                    channelBytes);

    return data;
}

bool memoryBlockToAudioBuffer(const juce::MemoryBlock& data,
                              int channels,
                              int samples,
                              juce::AudioBuffer<float>& buffer)
{
    if (channels <= 0 || samples <= 0)
        return false;

    const auto channelBytes =
        static_cast<size_t>(samples) * sizeof(float);
    if (static_cast<size_t>(channels)
            > std::numeric_limits<size_t>::max() / channelBytes)
        return false;

    const auto totalBytes =
        static_cast<size_t>(channels) * channelBytes;
    if (data.getSize() != totalBytes)
        return false;

    buffer.setSize(channels, samples);
    const auto* source = static_cast<const char*>(data.getData());

    for (int channel = 0; channel < channels; ++channel)
        std::memcpy(buffer.getWritePointer(channel),
                    source + static_cast<size_t>(channel) * channelBytes,
                    channelBytes);

    return true;
}

juce::MemoryBlock pointsToMemoryBlock(
    const std::vector<juce::Point<float>>& points)
{
    juce::MemoryBlock data(points.size() * sizeof(float) * 2);
    auto* destination = static_cast<char*>(data.getData());

    for (size_t i = 0; i < points.size(); ++i)
    {
        std::memcpy(destination + i * sizeof(float) * 2,
                    &points[i].x,
                    sizeof(float));
        std::memcpy(destination + i * sizeof(float) * 2 + sizeof(float),
                    &points[i].y,
                    sizeof(float));
    }

    return data;
}

bool memoryBlockToPoints(const juce::MemoryBlock& data,
                         int pointCount,
                         std::vector<juce::Point<float>>& points)
{
    if (pointCount < 0
        || static_cast<size_t>(pointCount)
               > std::numeric_limits<size_t>::max() / (sizeof(float) * 2)
        || data.getSize()
               != static_cast<size_t>(pointCount) * sizeof(float) * 2)
        return false;

    points.resize(static_cast<size_t>(pointCount));
    const auto* source = static_cast<const char*>(data.getData());

    for (int i = 0; i < pointCount; ++i)
    {
        std::memcpy(&points[static_cast<size_t>(i)].x,
                    source + static_cast<size_t>(i) * sizeof(float) * 2,
                    sizeof(float));
        std::memcpy(&points[static_cast<size_t>(i)].y,
                    source + static_cast<size_t>(i) * sizeof(float) * 2
                        + sizeof(float),
                    sizeof(float));
    }

    return true;
}
}

juce::AudioProcessorValueTreeState::ParameterLayout NewProjectAudioProcessor::createParameterLayout()
{
    std::vector<std::unique_ptr<juce::RangedAudioParameter>> params;

    params.push_back(std::make_unique<juce::AudioParameterFloat>("attack", "Attack", juce::NormalisableRange<float>(0.01f, 3.0f, 0.01f), 0.015f));
    params.push_back(std::make_unique<juce::AudioParameterFloat>("decay", "Decay", juce::NormalisableRange<float>(0.01f, 3.0f, 0.01f), 0.1f));
    params.push_back(std::make_unique<juce::AudioParameterFloat>("sustain", "Sustain", juce::NormalisableRange<float>(0.0f, 1.0f, 0.01f), 1.0f));
    params.push_back(std::make_unique<juce::AudioParameterFloat>("release", "Release", juce::NormalisableRange<float>(0.01f, 5.0f, 0.01f), 0.1f));
    params.push_back(std::make_unique<juce::AudioParameterInt>("num_particles", "Cubes", 1, 20, 3));
    params.push_back(std::make_unique<juce::AudioParameterInt>("pitch", "Resample", -36, 36, 0));
    params.push_back(std::make_unique<juce::AudioParameterBool>("ordinary_pitch", "Ordinary Pitch", true));
    params.push_back(std::make_unique<juce::AudioParameterInt>("rand", "Rand", 0, 100, 0));
    params.push_back(std::make_unique<juce::AudioParameterInt>("humanize", "Humanize", 0, 100, 0));

    return { params.begin(), params.end() };
}

NewProjectAudioProcessor::NewProjectAudioProcessor()
#ifndef JucePlugin_PreferredChannelConfigurations
    : AudioProcessor(BusesProperties()
#if ! JucePlugin_IsMidiEffect
#if ! JucePlugin_IsSynth
        .withInput("Input", juce::AudioChannelSet::stereo(), true)
#endif
        .withOutput("Output", juce::AudioChannelSet::stereo(), true)
#endif
    ), apvts(*this, nullptr, "Parameters", createParameterLayout())
#else
    : apvts(*this, nullptr, "Parameters", createParameterLayout())
#endif
{
}

NewProjectAudioProcessor::~NewProjectAudioProcessor() {}

const juce::String NewProjectAudioProcessor::getName() const { return JucePlugin_Name; }
bool NewProjectAudioProcessor::acceptsMidi() const { return false; }
bool NewProjectAudioProcessor::producesMidi() const { return false; }
bool NewProjectAudioProcessor::isMidiEffect() const { return false; }
double NewProjectAudioProcessor::getTailLengthSeconds() const { return 5.0; }
int NewProjectAudioProcessor::getNumPrograms() { return 1; }
int NewProjectAudioProcessor::getCurrentProgram() { return 0; }
void NewProjectAudioProcessor::setCurrentProgram(int index) {}
const juce::String NewProjectAudioProcessor::getProgramName(int index) { return {}; }
void NewProjectAudioProcessor::changeProgramName(int index, const juce::String& newName) {}
void NewProjectAudioProcessor::releaseResources()
{
    pitchResampler.reset();
    ordinaryPitchDelay.reset();
}
bool NewProjectAudioProcessor::isBusesLayoutSupported(
    const BusesLayout& layouts) const
{
    const auto output = layouts.getMainOutputChannelSet();
    if (output != juce::AudioChannelSet::mono()
        && output != juce::AudioChannelSet::stereo())
        return false;

    return layouts.getMainInputChannelSet().isDisabled();
}

void NewProjectAudioProcessor::getStateInformation(
    juce::MemoryBlock& destData)
{
    juce::ValueTree root("CubesPluginState");
    root.setProperty("version", cubesStateVersion, nullptr);
    root.addChild(apvts.copyState(), -1, nullptr);

    const juce::ScopedLock lock(savedStateLock);

    if (sampleBuffer.getNumChannels() > 0
        && sampleBuffer.getNumSamples() > 0)
    {
        juce::ValueTree audio("Audio");
        audio.setProperty(
            "channels", sampleBuffer.getNumChannels(), nullptr);
        audio.setProperty(
            "samples", sampleBuffer.getNumSamples(), nullptr);
        audio.setProperty(
            "data", audioBufferToMemoryBlock(sampleBuffer), nullptr);
        root.addChild(audio, -1, nullptr);
    }

    juce::ValueTree slices("Slices");
    for (int i = 0; i < numSlices; ++i)
    {
        juce::ValueTree slice("Slice");
        slice.setProperty("index", i, nullptr);
        slice.setProperty(
            "start", sliceData[i].startSamples[0].load(), nullptr);
        slice.setProperty(
            "length", sliceData[i].numSamples[0].load(), nullptr);
        for (int side = 0; side < numSliceSides; ++side)
        {
            slice.setProperty(
                sliceStartProperties[static_cast<size_t>(side)],
                sliceData[i]
                    .startSamples[static_cast<size_t>(side)]
                    .load(),
                nullptr);
            slice.setProperty(
                sliceLengthProperties[static_cast<size_t>(side)],
                sliceData[i]
                    .numSamples[static_cast<size_t>(side)]
                    .load(),
                nullptr);
        }
        slice.setProperty(
            "pitch", sliceData[i].pitchSemitones.load(), nullptr);
        slice.setProperty(
            "reversed", sliceData[i].reversed.load(), nullptr);
        slices.addChild(slice, -1, nullptr);
    }
    root.addChild(slices, -1, nullptr);

    juce::ValueTree editor("Editor");
    editor.setProperty("hasState", hasSavedEditorState, nullptr);
    editor.setProperty("settingsOpen", uiSettingsOpen, nullptr);
    editor.setProperty("stopTime", uiStopTime, nullptr);
    editor.setProperty("clearLines", uiClearLines, nullptr);
    editor.setProperty("collisions", uiCollisions, nullptr);
    editor.setProperty(
        "recordingPanelOpen", uiRecordingPanelOpen, nullptr);
    editor.setProperty("wallBrush", uiWallBrush, nullptr);

    for (const auto& saved : savedParticles)
    {
        juce::ValueTree particle("Particle");
        particle.setProperty("slice", saved.sliceIndex, nullptr);
        particle.setProperty("x", saved.x, nullptr);
        particle.setProperty("y", saved.y, nullptr);
        particle.setProperty("vx", saved.vx, nullptr);
        particle.setProperty("vy", saved.vy, nullptr);
        particle.setProperty("size", saved.size, nullptr);
        particle.setProperty(
            "colour", static_cast<juce::int64>(saved.colour), nullptr);
        particle.setProperty("thrown", saved.isThrown, nullptr);
        particle.setProperty(
            "pitch", saved.pitchSemitones, nullptr);
        particle.setProperty(
            "reversed", saved.isReversed, nullptr);
        particle.setProperty(
            "trajectoryPoints",
            static_cast<int>(saved.trajectory.size()),
            nullptr);
        particle.setProperty(
            "trajectoryData",
            pointsToMemoryBlock(saved.trajectory),
            nullptr);
        particle.setProperty(
            "trajectoryDistance", saved.trajectoryDistance, nullptr);
        particle.setProperty(
            "trajectoryForward", saved.trajectoryForward, nullptr);
        editor.addChild(particle, -1, nullptr);
    }

    for (const auto& saved : savedStrokes)
    {
        juce::ValueTree stroke("Stroke");
        stroke.setProperty(
            "points", static_cast<int>(saved.points.size()), nullptr);
        stroke.setProperty(
            "data", pointsToMemoryBlock(saved.points), nullptr);
        stroke.setProperty(
            "type", static_cast<int>(saved.type), nullptr);
        editor.addChild(stroke, -1, nullptr);
    }

    root.addChild(editor, -1, nullptr);

    destData.reset();
    juce::MemoryOutputStream stream(destData, false);
    stream.writeInt(cubesStateMagic);
    root.writeToStream(stream);
}

void NewProjectAudioProcessor::setStateInformation(
    const void* data, int sizeInBytes)
{
    if (data == nullptr || sizeInBytes <= 0)
        return;

    juce::MemoryInputStream stream(
        data, static_cast<size_t>(sizeInBytes), false);

    if (sizeInBytes > static_cast<int>(sizeof(int))
        && stream.readInt() == cubesStateMagic)
    {
        const auto root = juce::ValueTree::readFromStream(stream);
        if (!root.isValid()
            || root.getType()
                   != juce::Identifier("CubesPluginState"))
            return;

        const auto parameters =
            root.getChildWithName(apvts.state.getType());
        if (parameters.isValid())
            apvts.replaceState(parameters);

        juce::AudioBuffer<float> restoredAudio;
        const auto audio = root.getChildWithName("Audio");
        if (audio.isValid())
        {
            const int channels = audio.getProperty("channels", 0);
            const int samples = audio.getProperty("samples", 0);
            const auto storedData = audio.getProperty("data");

            if (auto* block = storedData.getBinaryData())
                memoryBlockToAudioBuffer(
                    *block, channels, samples, restoredAudio);
        }

        std::array<
            std::array<int, numSliceSides>,
            numSlices> restoredStarts{};
        std::array<
            std::array<int, numSliceSides>,
            numSlices> restoredLengths{};
        std::array<int, numSlices> restoredPitches{};
        std::array<bool, numSlices> restoredReverse{};
        const auto slices = root.getChildWithName("Slices");

        for (const auto& slice : slices)
        {
            const int index = slice.getProperty("index", -1);
            if (juce::isPositiveAndBelow(index, numSlices))
            {
                const int legacyStart =
                    slice.getProperty("start", 0);
                const int legacyLength =
                    slice.getProperty("length", 0);
                const int totalSamples =
                    restoredAudio.getNumSamples();
                const int validLegacyLength = totalSamples > 0
                    ? juce::jlimit(1, totalSamples, legacyLength)
                    : legacyLength;
                const int maximumStart = juce::jmax(
                    0, totalSamples - validLegacyLength);
                const int baseShift = totalSamples > 0
                    ? std::max(2205, totalSamples / 50)
                    : 0;

                for (int side = 0; side < numSliceSides; ++side)
                {
                    const auto sideIndex =
                        static_cast<size_t>(side);
                    const auto startProperty =
                        juce::Identifier(
                            sliceStartProperties[sideIndex]);
                    const auto lengthProperty =
                        juce::Identifier(
                            sliceLengthProperties[sideIndex]);

                    if (slice.hasProperty(startProperty)
                        && slice.hasProperty(lengthProperty))
                    {
                        restoredStarts[
                            static_cast<size_t>(index)][sideIndex] =
                                slice.getProperty(startProperty, 0);
                        restoredLengths[
                            static_cast<size_t>(index)][sideIndex] =
                                slice.getProperty(lengthProperty, 0);
                    }
                    else
                    {
                        const int migratedStart =
                            maximumStart > 0
                            ? (juce::jmax(0, legacyStart)
                               + legacySideOffsets[sideIndex]
                                     * baseShift)
                                  % (maximumStart + 1)
                            : 0;
                        restoredStarts[
                            static_cast<size_t>(index)][sideIndex] =
                                migratedStart;
                        restoredLengths[
                            static_cast<size_t>(index)][sideIndex] =
                                validLegacyLength;
                    }
                }
                restoredPitches[static_cast<size_t>(index)] =
                    juce::jlimit(
                        -24, 24, static_cast<int>(
                            slice.getProperty("pitch", 0)));
                restoredReverse[static_cast<size_t>(index)] =
                    static_cast<bool>(
                        slice.getProperty("reversed", false));
            }
        }

        std::vector<SavedParticleState> restoredParticles;
        std::vector<SavedStrokeState> restoredStrokes;
        const auto editor = root.getChildWithName("Editor");
        bool restoredEditorState = false;

        if (editor.isValid())
        {
            restoredEditorState =
                static_cast<bool>(editor.getProperty("hasState", false));
            uiSettingsOpen =
                static_cast<bool>(editor.getProperty("settingsOpen", false));
            uiStopTime =
                static_cast<bool>(editor.getProperty("stopTime", false));
            uiClearLines =
                static_cast<bool>(editor.getProperty("clearLines", false));
            uiCollisions =
                static_cast<bool>(editor.getProperty("collisions", false));
            uiRecordingPanelOpen = static_cast<bool>(
                editor.getProperty("recordingPanelOpen", false));
            uiWallBrush = juce::jlimit(
                0,
                static_cast<int>(WallType::count) - 1,
                static_cast<int>(
                    editor.getProperty(
                        "wallBrush",
                        static_cast<int>(WallType::normal))));

            for (const auto& child : editor)
            {
                if (child.getType() == juce::Identifier("Particle"))
                {
                    SavedParticleState particle;
                    particle.sliceIndex =
                        child.getProperty("slice", 0);
                    particle.x = child.getProperty("x", 0.0f);
                    particle.y = child.getProperty("y", 0.0f);
                    particle.vx = child.getProperty("vx", 0.0f);
                    particle.vy = child.getProperty("vy", 0.0f);
                    particle.size = child.getProperty("size", 0.0f);
                    particle.colour = static_cast<juce::uint32>(
                        static_cast<juce::int64>(
                            child.getProperty("colour", 0)));
                    particle.isThrown = static_cast<bool>(
                        child.getProperty("thrown", false));
                    particle.pitchSemitones = juce::jlimit(
                        -24, 24, static_cast<int>(
                            child.getProperty("pitch", 0)));
                    particle.isReversed = static_cast<bool>(
                        child.getProperty("reversed", false));
                    particle.trajectoryDistance =
                        child.getProperty("trajectoryDistance", 0.0f);
                    particle.trajectoryForward = static_cast<bool>(
                        child.getProperty("trajectoryForward", true));
                    const int trajectoryPointCount =
                        child.getProperty("trajectoryPoints", 0);
                    const auto trajectoryData =
                        child.getProperty("trajectoryData");
                    if (auto* block = trajectoryData.getBinaryData())
                        memoryBlockToPoints(
                            *block,
                            trajectoryP…2665 tokens truncated…
    int lengthInSamples)
{
    if (index < 0 || index >= numSlices
        || side < 0 || side >= numSliceSides)
        return;

    const juce::ScopedLock lock(savedStateLock);
    const int totalSamples = sampleBuffer.getNumSamples();
    if (totalSamples <= 0)
        return;

    const int validStart =
        juce::jlimit(0, totalSamples - 1, startSample);
    const int validLength = juce::jlimit(
        1, totalSamples - validStart, lengthInSamples);
    sliceData[static_cast<size_t>(index)]
        .startSamples[static_cast<size_t>(side)]
        .store(validStart);
    sliceData[static_cast<size_t>(index)]
        .numSamples[static_cast<size_t>(side)]
        .store(validLength);
}

void NewProjectAudioProcessor::setSlicePitch(int index, int semitones)
{
    if (index >= 0 && index < numSlices)
    {
        const juce::ScopedLock lock(savedStateLock);
        sliceData[index].pitchSemitones.store(juce::jlimit(-24, 24, semitones));
    }
}

void NewProjectAudioProcessor::setSliceReversed(
    int index, bool shouldBeReversed)
{
    if (index >= 0 && index < numSlices)
        sliceData[static_cast<size_t>(index)].reversed.store(
            shouldBeReversed);
}

bool NewProjectAudioProcessor::getSliceRange(
    int index, int& startSample, int& lengthInSamples) const
{
    if (index < 0 || index >= numSlices)
        return false;

    startSample =
        sliceData[static_cast<size_t>(index)]
            .startSamples[0].load();
    lengthInSamples =
        sliceData[static_cast<size_t>(index)]
            .numSamples[0].load();
    return lengthInSamples > 0;
}

bool NewProjectAudioProcessor::getSliceRanges(
    int index,
    std::array<int, numSliceSides>& startSamples,
    std::array<int, numSliceSides>& lengthsInSamples) const
{
    if (index < 0 || index >= numSlices)
        return false;

    bool hasValidRange = false;
    const auto& slice = sliceData[static_cast<size_t>(index)];
    for (int side = 0; side < numSliceSides; ++side)
    {
        const auto sideIndex = static_cast<size_t>(side);
        startSamples[sideIndex] =
            slice.startSamples[sideIndex].load();
        lengthsInSamples[sideIndex] =
            slice.numSamples[sideIndex].load();
        hasValidRange =
            hasValidRange || lengthsInSamples[sideIndex] > 0;
    }
    return hasValidRange;
}

void NewProjectAudioProcessor::updateSavedParticles(
    const std::vector<SavedParticleState>& particles)
{
    const juce::ScopedLock lock(savedStateLock);
    savedParticles = particles;
    hasSavedEditorState = true;
}

void NewProjectAudioProcessor::updateSavedStrokes(
    const std::vector<SavedStrokeState>& strokes)
{
    const juce::ScopedLock lock(savedStateLock);
    savedStrokes = strokes;
    hasSavedEditorState = true;
}

bool NewProjectAudioProcessor::copySavedEditorState(
    std::vector<SavedParticleState>& particles,
    std::vector<SavedStrokeState>& strokes) const
{
    const juce::ScopedLock lock(savedStateLock);
    if (!hasSavedEditorState)
        return false;

    particles = savedParticles;
    strokes = savedStrokes;
    return true;
}

void NewProjectAudioProcessor::playSlice(int index, int hitType)
{
    if (index < 0 || index >= numSlices || !isReady)
        return;

    const juce::ScopedLock lock(savedStateLock);
    const int totalSamples = sampleBuffer.getNumSamples();
    if (sampleBuffer.getNumChannels() <= 0 || totalSamples <= 0)
        return;

    const int side = hitTypeToSideIndex(hitType);
    auto& slice = sliceData[static_cast<size_t>(index)];
    const int rangeStart =
        slice.startSamples[static_cast<size_t>(side)].load();
    const int rangeLength =
        slice.numSamples[static_cast<size_t>(side)].load();
    if (rangeLength <= 0)
        return;

    const float humanizeAmount = juce::jlimit(
        0.0f,
        1.0f,
        apvts.getRawParameterValue("humanize")->load() / 100.0f);
    float gain = 1.0f;
    float pan = 0.0f;
    int startJitter = 0;

    if (humanizeAmount > 0.0f)
    {
        const int maximumJitter = juce::jmax(
            1,
            juce::jmin(
                rangeLength / 12,
                static_cast<int>(currentSampleRate * 0.08)));
        const float bipolarStart =
            humanizeRandom.nextFloat() * 2.0f - 1.0f;
        startJitter = juce::roundToInt(
            bipolarStart * static_cast<float>(maximumJitter)
            * humanizeAmount);

        const float bipolarGain =
            humanizeRandom.nextFloat() * 2.0f - 1.0f;
        const float bipolarPan =
            humanizeRandom.nextFloat() * 2.0f - 1.0f;
        gain = 1.0f + bipolarGain * 0.22f * humanizeAmount;
        pan = bipolarPan * 0.65f * humanizeAmount;
    }

    const int maximumStart =
        juce::jmax(0, totalSamples - rangeLength);
    const int humanizedStart = juce::jlimit(
        0, maximumStart, rangeStart + startJitter);
    slice.pendingStartSample.store(humanizedStart);
    slice.pendingNumSamples.store(rangeLength);
    slice.pendingGain.store(gain);
    slice.pendingPan.store(pan);
    slice.trigger.store(true);
}

void NewProjectAudioProcessor::updateRandStartPositions(
    int blockSamples, int totalSamples) noexcept
{
    const int randAmount = juce::jlimit(
        0,
        100,
        juce::roundToInt(
            apvts.getRawParameterValue("rand")->load()));

    if (randAmount <= 0
        || blockSamples <= 0
        || totalSamples <= 0
        || currentSampleRate <= 0.0)
    {
        randMotionActive = false;
        return;
    }

    if (!randMotionActive)
    {
        for (int sliceIndex = 0; sliceIndex < numSlices; ++sliceIndex)
        {
            auto& slice = sliceData[static_cast<size_t>(sliceIndex)];
            for (int side = 0; side < numSliceSides; ++side)
            {
                const auto sideIndex = static_cast<size_t>(side);
                auto& motion = randMotionStates[
                    static_cast<size_t>(sliceIndex)][sideIndex];
                const int start = slice.startSamples[sideIndex].load();
                motion.position = static_cast<double>(start);
                motion.publishedStart = start;
                motion.direction = randMotionRandom.nextBool()
                    ? 1.0f
                    : -1.0f;
                motion.speedScale = 0.75f
                    + randMotionRandom.nextFloat() * 0.5f;
            }
        }
        randMotionActive = true;
    }

    const double blockDuration =
        static_cast<double>(blockSamples) / currentSampleRate;
    const double speedAmount =
        static_cast<double>(randAmount) / 1200.0;

    for (int sliceIndex = 0; sliceIndex < numSlices; ++sliceIndex)
    {
        auto& slice = sliceData[static_cast<size_t>(sliceIndex)];
        for (int side = 0; side < numSliceSides; ++side)
        {
            const auto sideIndex = static_cast<size_t>(side);
            const int fragmentLength = slice.numSamples[sideIndex].load();
            if (fragmentLength <= 0)
                continue;

            const int maximumStart = juce::jmax(
                0, totalSamples - fragmentLength);
            auto& motion = randMotionStates[
                static_cast<size_t>(sliceIndex)][sideIndex];
            const int currentStart = slice.startSamples[sideIndex].load();

            if (currentStart != motion.publishedStart)
            {
                motion.position = static_cast<double>(
                    juce::jlimit(0, maximumStart, currentStart));
                motion.publishedStart = currentStart;
            }

            if (maximumStart <= 0)
            {
                motion.position = 0.0;
                motion.publishedStart = 0;
                slice.startSamples[sideIndex].store(0);
                continue;
            }

            const double samplesPerSecond =
                static_cast<double>(maximumStart) * speedAmount
                * static_cast<double>(motion.speedScale);
            motion.position += static_cast<double>(motion.direction)
                * samplesPerSecond * blockDuration;

            if (motion.position <= 0.0)
            {
                motion.position = 0.0;
                motion.direction = 1.0f;
                motion.speedScale = 0.75f
                    + randMotionRandom.nextFloat() * 0.5f;
            }
            else if (motion.position >= static_cast<double>(maximumStart))
            {
                motion.position = static_cast<double>(maximumStart);
                motion.direction = -1.0f;
                motion.speedScale = 0.75f
                    + randMotionRandom.nextFloat() * 0.5f;
            }

            const int movedStart = juce::jlimit(
                0,
                maximumStart,
                juce::roundToInt(motion.position));
            slice.startSamples[sideIndex].store(movedStart);
            motion.publishedStart = movedStart;
        }
    }
}

void NewProjectAudioProcessor::processBlock(juce::AudioBuffer<float>& buffer, juce::MidiBuffer& midiMessages)
{
    juce::ScopedNoDenormals noDenormals;
    buffer.clear();

    if (!isReady
        || sampleBuffer.getNumChannels() <= 0
        || sampleBuffer.getNumSamples() == 0)
        return;

    adsrParams.attack = apvts.getRawParameterValue("attack")->load();
    adsrParams.decay = apvts.getRawParameterValue("decay")->load();
    adsrParams.sustain = apvts.getRawParameterValue("sustain")->load();
    adsrParams.release = apvts.getRawParameterValue("release")->load();

    for (int i = 0; i < numSlices; ++i) {
        spectralAdsrs[static_cast<size_t>(i)].setParameters(adsrParams);
        ordinaryAdsrs[static_cast<size_t>(i)].setParameters(adsrParams);
    }

    const int numOutputChannels = buffer.getNumChannels();
    const int blockSamples = buffer.getNumSamples();
    const int sampleBufferChannels = sampleBuffer.getNumChannels();
    const int totalSamplesInBuffer = sampleBuffer.getNumSamples();
    if (numOutputChannels <= 0 || blockSamples <= 0)
        return;

    updateRandStartPositions(blockSamples, totalSamplesInBuffer);

    spectralMixBuffer.setSize(
        numOutputChannels, blockSamples, false, false, true);
    ordinaryMixBuffer.setSize(
        numOutputChannels, blockSamples, false, false, true);
    spectralMixBuffer.clear();
    ordinaryMixBuffer.clear();

    const float globalPitch = apvts.getRawParameterValue("pitch")->load();
    const bool ordinaryPitchMode =
        apvts.getRawParameterValue("ordinary_pitch")->load() >= 0.5f;
    ordinaryModeBlend.setTargetValue(
        ordinaryPitchMode ? 1.0f : 0.0f);

    for (int i = 0; i < numSlices; ++i)
    {
        if (sliceData[i].trigger.exchange(false)) {
            sliceData[i].activeStartSample =
                sliceData[i].pendingStartSample.load();
            sliceData[i].activeNumSamples =
                sliceData[i].pendingNumSamples.load();
            sliceData[i].activeGain =
                sliceData[i].pendingGain.load();
            sliceData[i].activePan =
                sliceData[i].pendingPan.load();
            sliceData[i].activeReversed =
                sliceData[i].reversed.load();
            sliceData[i].spectralTransitionStart =
                sliceData[i].spectralLastOutput;
            sliceData[i].ordinaryTransitionStart =
                sliceData[i].ordinaryLastOutput;
            sliceData[i].spectralTransitionRemaining =
                retriggerTransitionSamples;
            sliceData[i].ordinaryTransitionRemaining =
                retriggerTransitionSamples;
            spectralAdsrs[static_cast<size_t>(i)].noteOn();
            ordinaryAdsrs[static_cast<size_t>(i)].noteOn();
            sliceData[i].spectralPlayhead = 0.0;
            sliceData[i].ordinaryPlayhead = 0.0;
            sliceData[i].spectralNoteOffTriggered = false;
            sliceData[i].ordinaryNoteOffTriggered = false;
        }

        const float slicePitch =
            static_cast<float>(sliceData[i].pitchSemitones.load());
        const double spectralPlaybackRate =
            std::pow(2.0, static_cast<double>(slicePitch) / 12.0);
        const double ordinaryPlaybackRate =
            std::pow(
                2.0,
                static_cast<double>(slicePitch + globalPitch)
                    / 12.0);

        const auto renderVoice =
            [&](juce::AudioBuffer<float>& destination,
                juce::ADSR& envelope,
                double& currentPosition,
                bool& noteOffTriggered,
                double playbackRate,
                std::array<float, 2>& lastOutput,
                const std::array<float, 2>& transitionStart,
                int& transitionRemaining)
        {
            if (currentPosition < 0.0
                || sliceData[i].activeNumSamples <= 0)
                return;

            const float pan = juce::jlimit(
                -1.0f, 1.0f, sliceData[i].activePan);
            const float leftGain =
                sliceData[i].activeGain
                * (pan > 0.0f ? 1.0f - pan : 1.0f);
            const float rightGain =
                sliceData[i].activeGain
                * (pan < 0.0f ? 1.0f + pan : 1.0f);

            for (int s = 0; s < blockSamples; ++s)
            {
                const float env = envelope.getNextSample();

                if (!envelope.isActive()) {
                    currentPosition = -1.0;
                    lastOutput.fill(0.0f);
                    transitionRemaining = 0;
                    break;
                }

                if (currentPosition
                        >= static_cast<double>(
                            sliceData[i].activeNumSamples)
                    && !noteOffTriggered) {
                    envelope.noteOff();
                    noteOffTriggered = true;
                }

                const double slicePosition =
                    sliceData[i].activeReversed
                    ? static_cast<double>(
                          sliceData[i].activeNumSamples - 1)
                          - currentPosition
                    : currentPosition;
                double wrappedPosition = std::fmod(
                    static_cast<double>(
                        sliceData[i].activeStartSample)
                        + slicePosition,
                    static_cast<double>(totalSamplesInBuffer));
                if (wrappedPosition < 0.0)
                    wrappedPosition += static_cast<double>(totalSamplesInBuffer);

                const int samplePos = static_cast<int>(wrappedPosition);
                const int nextSamplePos = (samplePos + 1) % totalSamplesInBuffer;
                const float fraction =
                    static_cast<float>(wrappedPosition - samplePos);
                const float transitionAmount =
                    transitionRemaining > 0
                    ? 1.0f
                        - static_cast<float>(transitionRemaining)
                          / static_cast<float>(
                              retriggerTransitionSamples)
                    : 1.0f;

                for (int ch = 0; ch < numOutputChannels; ++ch) {
                    int srcCh = std::min(ch, sampleBufferChannels - 1);
                    const float first =
                        sampleBuffer.getSample(srcCh, samplePos);
                    const float second =
                        sampleBuffer.getSample(srcCh, nextSamplePos);
                    const float channelGain =
                        numOutputChannels == 1
                        ? sliceData[i].activeGain
                        : (ch == 0 ? leftGain : rightGain);
                    const float newValue =
                        juce::jmap(fraction, first, second)
                        * env * channelGain;
                    const auto transitionChannel =
                        static_cast<size_t>(juce::jmin(ch, 1));
                    const float val = juce::jmap(
                        transitionAmount,
                        transitionStart[transitionChannel],
                        newValue);
                    destination.addSample(ch, s, val);
                    lastOutput[transitionChannel] = val;
                }
                if (transitionRemaining > 0)
                    --transitionRemaining;
                currentPosition += playbackRate;
            }
        };

        renderVoice(
            spectralMixBuffer,
            spectralAdsrs[static_cast<size_t>(i)],
            sliceData[i].spectralPlayhead,
            sliceData[i].spectralNoteOffTriggered,
            spectralPlaybackRate,
            sliceData[i].spectralLastOutput,
            sliceData[i].spectralTransitionStart,
            sliceData[i].spectralTransitionRemaining);
        renderVoice(
            ordinaryMixBuffer,
            ordinaryAdsrs[static_cast<size_t>(i)],
            sliceData[i].ordinaryPlayhead,
            sliceData[i].ordinaryNoteOffTriggered,
            ordinaryPlaybackRate,
            sliceData[i].ordinaryLastOutput,
            sliceData[i].ordinaryTransitionStart,
            sliceData[i].ordinaryTransitionRemaining);
    }

    pitchResampler.process(spectralMixBuffer, globalPitch);

    for (int sample = 0; sample < blockSamples; ++sample)
    {
        for (int channel = 0; channel < numOutputChannels; ++channel)
        {
            ordinaryPitchDelay.pushSample(
                channel,
                ordinaryMixBuffer.getSample(channel, sample));
            ordinaryMixBuffer.setSample(
                channel,
                sample,
                ordinaryPitchDelay.popSample(channel));
        }
    }

    for (int sample = 0; sample < blockSamples; ++sample)
    {
        const float ordinaryAmount =
            ordinaryModeBlend.getNextValue();
        const float spectralAmount = 1.0f - ordinaryAmount;

        for (int channel = 0; channel < numOutputChannels; ++channel)
            buffer.setSample(
                channel,
                sample,
                spectralMixBuffer.getSample(channel, sample)
                    * spectralAmount
                + ordinaryMixBuffer.getSample(channel, sample)
                    * ordinaryAmount);
    }

    // Keep a monotonic count of valid samples until the ring is full. Do not
    // reset it when the write position wraps: the export view relies on this
    // count to distinguish recorded audio from unwritten ring-buffer space.
    if (rollingCapacity > 0 && buffer.getNumChannels() > 0)
    {
        int writePosition = rollingWritePos.load();
        int visualSamples = rollingVisualSamples.load();

        for (int sample = 0; sample < buffer.getNumSamples(); ++sample)
        {
            for (int channel = 0; channel < rollingBuffer.getNumChannels(); ++channel)
            {
                const int sourceChannel = juce::jmin(
                    channel, buffer.getNumChannels() - 1);
                rollingBuffer.setSample(
                    channel, writePosition, buffer.getSample(sourceChannel, sample));
            }

            writePosition = (writePosition + 1) % rollingCapacity;
            visualSamples = juce::jmin(rollingCapacity, visualSamples + 1);
        }

        rollingWritePos.store(writePosition);
        rollingVisualSamples.store(visualSamples);
    }
}

bool NewProjectAudioProcessor::hasEditor() const { return true; }
juce::AudioProcessorEditor* NewProjectAudioProcessor::createEditor() { return new NewProjectAudioProcessorEditor(*this); }
juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter() { return new NewProjectAudioProcessor(); }

