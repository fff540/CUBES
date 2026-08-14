/*
  ==============================================================================
    This file contains the basic framework code for a JUCE plugin editor.
  ==============================================================================
*/

#include "PluginProcessor.h"
#include "PluginEditor.h"
#include "Branding.h"
#include <algorithm>
#include <random>
#include <cmath>
#include <limits>

namespace
{
void drawSketchyPitchValue(juce::Graphics& graphics,
                           const Particle& particle,
                           int particleIndex,
                           int frameCount)
{
    juce::Graphics::ScopedSaveState savedGraphicsState(graphics);

    if (particle.pitchSemitones == 0)
        return;

    const auto text = (particle.pitchSemitones > 0 ? "+" : "")
                    + juce::String(particle.pitchSemitones);
    const float fontHeight = juce::jlimit(
        13.0f, 22.0f, particle.size * 0.42f);
    const auto textBounds = juce::Rectangle<float>(
        particle.x + 2.0f, particle.y + 1.0f,
        particle.size - 4.0f, particle.size - 2.0f);

    juce::Random random(
        8100 + particleIndex * 97 + frameCount / 10);
    graphics.setFont(juce::FontOptions(
        "Segoe Print", fontHeight, juce::Font::plain));

    for (int pass = 0; pass < 2; ++pass)
    {
        const float offsetX = random.nextFloat() * 1.0f - 0.5f;
        const float offsetY = random.nextFloat() * 1.0f - 0.5f;
        graphics.setColour(juce::Colour(0xff232529)
                               .withAlpha(pass == 0 ? 0.76f : 0.24f));
        graphics.drawText(
            text,
            textBounds.translated(offsetX, offsetY),
            juce::Justification::centred,
            false);
    }
}

void drawSketchyReverseSymbol(juce::Graphics& graphics,
                              const Particle& particle,
                              int particleIndex,
                              int frameCount)
{
    if (!particle.isReversed)
        return;

    const float iconSize = juce::jlimit(
        11.0f, 18.0f, particle.size * 0.36f);
    const float x = particle.x + particle.size - iconSize - 2.5f;
    const float y = particle.y + 2.5f;

    juce::Path symbol;
    symbol.startNewSubPath(x + iconSize * 0.18f,
                           y + iconSize * 0.48f);
    symbol.cubicTo(x + iconSize * 0.28f,
                   y + iconSize * 0.10f,
                   x + iconSize * 0.72f,
                   y + iconSize * 0.10f,
                   x + iconSize * 0.82f,
                   y + iconSize * 0.42f);
    symbol.startNewSubPath(x + iconSize * 0.66f,
                           y + iconSize * 0.30f);
    symbol.lineTo(x + iconSize * 0.84f,
                  y + iconSize * 0.43f);
    symbol.lineTo(x + iconSize * 0.88f,
                  y + iconSize * 0.22f);

    symbol.startNewSubPath(x + iconSize * 0.82f,
                           y + iconSize * 0.55f);
    symbol.cubicTo(x + iconSize * 0.70f,
                   y + iconSize * 0.92f,
                   x + iconSize * 0.28f,
                   y + iconSize * 0.92f,
                   x + iconSize * 0.17f,
                   y + iconSize * 0.60f);
    symbol.startNewSubPath(x + iconSize * 0.33f,
                           y + iconSize * 0.72f);
    symbol.lineTo(x + iconSize * 0.15f,
                  y + iconSize * 0.59f);
    symbol.lineTo(x + iconSize * 0.11f,
                  y + iconSize * 0.80f);

    juce::Random random(
        19200 + particleIndex * 131 + frameCount / 10);
    for (int pass = 0; pass < 2; ++pass)
    {
        const auto transform = juce::AffineTransform::translation(
            random.nextFloat() * 0.8f - 0.4f,
            random.nextFloat() * 0.8f - 0.4f);
        graphics.setColour(juce::Colour(0xff232529)
                               .withAlpha(pass == 0 ? 0.82f : 0.28f));
        graphics.strokePath(
            symbol,
            juce::PathStrokeType(
                pass == 0 ? 1.35f : 0.85f,
                juce::PathStrokeType::curved,
                juce::PathStrokeType::rounded),
            transform);
    }
}

void drawSketchyDirectionArrow(juce::Graphics& graphics,
                               juce::Point<float> centre,
                               int side,
                               int seed)
{
    juce::Point<float> direction;
    if (side == 0)
        direction = { -1.0f, 0.0f };
    else if (side == 1)
        direction = { 1.0f, 0.0f };
    else if (side == 2)
        direction = { 0.0f, -1.0f };
    else
        direction = { 0.0f, 1.0f };

    const juce::Point<float> perpendicular(
        -direction.y, direction.x);
    const auto tail = centre - direction * 5.0f;
    const auto tip = centre + direction * 5.0f;

    juce::Path arrow;
    arrow.startNewSubPath(tail);
    arrow.lineTo(tip);
    arrow.startNewSubPath(
        tip - direction * 3.5f + perpendicular * 2.8f);
    arrow.lineTo(tip);
    arrow.lineTo(
        tip - direction * 3.5f - perpendicular * 2.8f);

    juce::Random random(seed);
    for (int pass = 0; pass < 2; ++pass)
    {
        const auto transform = juce::AffineTransform::translation(
            random.nextFloat() * 0.7f - 0.35f,
            random.nextFloat() * 0.7f - 0.35f);
        graphics.setColour(juce::Colour(0xff232529)
                               .withAlpha(pass == 0 ? 0.78f : 0.25f));
        graphics.strokePath(
            arrow,
            juce::PathStrokeType(
                pass == 0 ? 1.15f : 0.75f,
                juce::PathStrokeType::curved,
                juce::PathStrokeType::rounded),
            transform);
    }
}

const char* getWallTypeName(NewProjectAudioProcessor::WallType type)
{
    using WallType = NewProjectAudioProcessor::WallType;
    switch (type)
    {
        case WallType::soft:     return "soft";
        case WallType::boost:    return "boost";
        case WallType::teleport: return "teleport";
        case WallType::oneShot:  return "one-shot";
        case WallType::normal:
        case WallType::count:
        default:                 return "normal";
    }
}

juce::Colour getWallTypeColour(NewProjectAudioProcessor::WallType type)
{
    using WallType = NewProjectAudioProcessor::WallType;
    switch (type)
    {
        case WallType::soft:     return juce::Colour(0xff607985);
        case WallType::boost:    return juce::Colour(0xffa26443);
        case WallType::teleport: return juce::Colour(0xff735f91);
        case WallType::oneShot:  return juce::Colour(0xff92586a);
        case WallType::normal:
        case WallType::count:
        default:                 return juce::Colour(0xff232529);
    }
}

float getTrajectoryLength(
    const std::vector<juce::Point<float>>& trajectory)
{
    float length = 0.0f;
    for (size_t i = 1; i < trajectory.size(); ++i)
        length += trajectory[i - 1].getDistanceFrom(trajectory[i]);
    return length;
}

juce::Point<float> getTrajectoryPointAtDistance(
    const std::vector<juce::Point<float>>& trajectory,
    float distance,
    juce::Point<float>* tangent)
{
    if (trajectory.empty())
        return {};
    if (trajectory.size() == 1)
        return trajectory.front();

    float remaining = juce::jmax(0.0f, distance);
    for (size_t i = 1; i < trajectory.size(); ++i)
    {
        const auto segment = trajectory[i] - trajectory[i - 1];
        const float segmentLength = segment.getDistanceFromOrigin();
        if (segmentLength < 0.001f)
            continue;

        if (remaining <= segmentLength)
        {
            const auto direction = segment / segmentLength;
            if (tangent != nullptr)
                *tangent = direction;
            return trajectory[i - 1] + direction * remaining;
        }
        remaining -= segmentLength;
    }

    const auto finalSegment =
        trajectory.back() - trajectory[trajectory.size() - 2];
    const float finalLength = finalSegment.getDistanceFromOrigin();
    if (tangent != nullptr && finalLength > 0.001f)
        *tangent = finalSegment / finalLength;
    return trajectory.back();
}

float getNearestTrajectoryDistance(
    const std::vector<juce::Point<float>>& trajectory,
    juce::Point<float> target)
{
    float bestDistanceSquared = std::numeric_limits<float>::max();
    float bestPathDistance = 0.0f;
    float accumulatedDistance = 0.0f;

    for (size_t i = 1; i < trajectory.size(); ++i)
    {
        const auto start = trajectory[i - 1];
        const auto segment = trajectory[i] - start;
        const float segmentLengthSquared =
            segment.getDistanceSquaredFromOrigin();
        if (segmentLengthSquared < 0.0001f)
            continue;

        const float amount = juce::jlimit(
            0.0f,
            1.0f,
            (target - start).getDotProduct(segment)
                / segmentLengthSquared);
        const auto closest = start + segment * amount;
        const float distanceSquared =
            target.getDistanceSquaredFrom(closest);
        const float segmentLength = std::sqrt(segmentLengthSquared);
        if (distanceSquared < bestDistanceSquared)
        {
            bestDistanceSquared = distanceSquared;
            bestPathDistance =
                accumulatedDistance + segmentLength * amount;
        }
        accumulatedDistance += segmentLength;
    }

    return bestPathDistance;
}

bool moveParticleAlongTrajectory(Particle& particle)
{
    const float totalLength = getTrajectoryLength(particle.trajectory);
    if (totalLength < 1.0f)
        return false;

    const float speed = juce::jlimit(
        0.6f,
        25.0f,
        std::sqrt(particle.vx * particle.vx
                  + particle.vy * particle.vy));
    const float period = totalLength * 2.0f;
    float phase = particle.trajectoryForward
        ? particle.trajectoryDistance
        : period - particle.trajectoryDistance;
    phase = std::fmod(phase + speed, period);
    if (phase < 0.0f)
        phase += period;

    particle.trajectoryForward = phase <= totalLength;
    particle.trajectoryDistance = particle.trajectoryForward
        ? phase
        : period - phase;

    juce::Point<float> tangent { 1.0f, 0.0f };
    const auto centre = getTrajectoryPointAtDistance(
        particle.trajectory,
        particle.trajectoryDistance,
        &tangent);
    if (!particle.trajectoryForward)
        tangent = { -tangent.x, -tangent.y };

    particle.vx = tangent.x * speed;
    particle.vy = tangent.y * speed;
    particle.x = centre.x - particle.size * 0.5f;
    particle.y = centre.y - particle.size * 0.5f;
    return true;
}
}

//==============================================================================
NewProjectAudioProcessorEditor::NewProjectAudioProcessorEditor(NewProjectAudioProcessor& p)
    : AudioProcessorEditor(&p), audioProcessor(p), recordingPanel(p)
{
    setSize(700, 700);
    formatManager.registerBasicFormats();
    currentWallType = static_cast<WallType>(juce::jlimit(
        0,
        static_cast<int>(WallType::count) - 1,
        audioProcessor.uiWallBrush));
    currentStroke.type = currentWallType;

    addAndMakeVisible(gearButton);
    gearButton.setVisible(false);

    timeStopButton.setName("timeStopBtn");
    timeStopButton.setToggleState(audioProcessor.uiStopTime, juce::dontSendNotification);
    isTimeStopped = audioProcessor.uiStopTime;
    timeStopButton.onClick = [this]()
        {
            isTimeStopped = timeStopButton.getToggleState();
            audioProcessor.uiStopTime = isTimeStopped;

            if (isTimeStopped)
            {
                draggedParticleIndex = -1;
                for (auto& particle : particles)
                    particle.isThrown = false;
            }

            syncParticlesToProcessor();
        };
    addChildComponent(timeStopButton);

    clearLinesButton.setName("clearBtn");
    clearLinesButton.onClick = [this]()
        {
            strokes.clear();
            currentStroke.points.clear();
            syncStrokesToProcessor();
            repaint();
        };
    addChildComponent(clearLinesButton);

    collisionButton.setName("collisionBtn");
    collisionButton.setToggleState(audioProcessor.uiCollisions, juce::dontSendNotification);
    collisionButton.onClick = [this]()
        {
            audioProcessor.uiCollisions = collisionButton.getToggleState();
        };
    addChildComponent(collisionButton);

    // Добавляем UI панели записи (стрелочка изначально скрыта на главном экране)
    addChildComponent(toggleRecordingPanelButton);
    toggleRecordingPanelButton.setToggleState(
        audioProcessor.uiRecordingPanelOpen,
        juce::dontSendNotification);
    recordingPanelAnimationTarget =
        audioProcessor.uiRecordingPanelOpen ? 300.0f : 0.0f;
    recordingPanelCurrentPos = recordingPanelAnimationTarget;
    toggleRecordingPanelButton.onClick = [this]() {
        audioProcessor.uiRecordingPanelOpen =
            toggleRecordingPanelButton.getToggleState();
        if (toggleRecordingPanelButton.getToggleState()) {
            recordingPanelAnimationTarget = 300.0f; // Ширина выезжающей панели
        }
        else {
            recordingPanelAnimationTarget = 0.0f;
        }
        };
    addChildComponent(recordingPanel);
    recordingPanel.setVisible(true);

    auto setupSketchySlider = [this](juce::Slider& slider, std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment>& attach, const juce::String& id) {
        slider.setName(id);
        slider.setSliderStyle(juce::Slider::RotaryHorizontalVerticalDrag);
        slider.setTextBoxStyle(juce::Slider::NoTextBox, false, 0, 0);
        slider.setLookAndFeel(&sketchyLnF);
        addChildComponent(slider);
        attach = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment>(audioProcessor.apvts, id, slider);
        };

    setupSketchySlider(attackSlider, attackAttach, "attack");
    setupSketchySlider(decaySlider, decayAttach, "decay");
    setupSketchySlider(sustainSlider, sustainAttach, "sustain");
    setupSketchySlider(releaseSlider, releaseAttach, "release");
    setupSketchySlider(numParticlesSlider, numParticlesAttach, "num_particles");
    setupSketchySlider(pitchSlider, pitchAttach, "pitch");
    setupSketchySlider(randSlider, randAttach, "rand");
    setupSketchySlider(humanizeSlider, humanizeAttach, "humanize");
    pitchSlider.setDoubleClickReturnValue(true, 0.0);
    randSlider.setDoubleClickReturnValue(true, 0.0);
    humanizeSlider.setDoubleClickReturnValue(true, 0.0);
    pitchSlider.onRightClick = [this]()
        {
            if (auto* parameter =
                    audioProcessor.apvts.getParameter("ordinary_pitch"))
            {
                const bool ordinaryPitchMode =
                    audioProcessor.apvts
                        .getRawParameterValue("ordinary_pitch")->load() >= 0.5f;
                parameter->beginChangeGesture();
                parameter->setValueNotifyingHost(
                    ordinaryPitchMode ? 0.0f : 1.0f);
                parameter->endChangeGesture();
                repaint();
            }
        };

    gearButton.onClick = [this]() {
        audioProcessor.uiSettingsOpen = !audioProcessor.uiSettingsOpen;
        updateSettingsVisibility();
        resized();
        repaint();
        };

    if (audioProcessor.hasLoadedAudio())
    {
        audioProcessor.copyLoadedAudioBuffer(loadedAudio);
        if (!restoreSavedEditorState())
            generateParticles();
        isAnimating = true;
        gearButton.setVisible(true);
        toggleRecordingPanelButton.setVisible(true);
    }

    updateSettingsVisibility();
    startTimerHz(60);
}

NewProjectAudioProcessorEditor::~NewProjectAudioProcessorEditor() {
    syncParticlesToProcessor();
    syncStrokesToProcessor();
    attackSlider.setLookAndFeel(nullptr);
    decaySlider.setLookAndFeel(nullptr);
    sustainSlider.setLookAndFeel(nullptr);
    releaseSlider.setLookAndFeel(nullptr);
    numParticlesSlider.setLookAndFeel(nullptr);
    pitchSlider.setLookAndFeel(nullptr);
    randSlider.setLookAndFeel(nullptr);
    humanizeSlider.setLookAndFeel(nullptr);
    stopTimer();
}

void NewProjectAudioProcessorEditor::paint(juce::Graphics& g)
{
    g.fillAll(juce::Colour(0xFFF3EFE9));

    int width = getWidth();
    int height = getHeight();
    int seedOffset = (frameCount / 8);
    juce::Colour pencilColor(0xFF232529);

    auto addSketchyLine = [&](juce::Path& p, float x1, float y1, float x2, float y2, int aSeed) {
        float s = 0.9f;
        juce::Random rAnim(aSeed);
        p.startNewSubPath(x1, y1);

        float length = std::hypot(x2 - x1, y2 - y1);
        if (length < 1.0f) { p.lineTo(x2, y2); return; }

        float stepSize = 15.0f;
        int steps = static_cast<int>(length / stepSize);
        float dx = (x2 - x1) / length;
        float dy = (y2 - y1) / length;

        for (int i = 1; i <= steps; ++i) {
            float baseX = x1 + dx * (i * stepSize);
            float baseY = y1 + dy * (i * stepSize);
            float ax = baseX + (rAnim.nextFloat() * s * 2.0f - s);
            float ay = baseY + (rAnim.nextFloat() * s * 2.0f - s);
            p.lineTo(ax, ay);
        }
        p.lineTo(x2, y2);
        };

    juce::Path borderPath;
    float margin = 8.0f;
    addSketchyLine(borderPath, margin, margin, width - margin, margin, 111 + seedOffset);
    addSketchyLine(borderPath, width - margin, margin, width - margin, height - margin, 444 + seedOffset);
    addSketchyLine(borderPath, width - margin, height - margin, margin, height - margin, 222 + seedOffset);
    addSketchyLine(borderPath, margin, height - margin, margin, margin, 333 + seedOffset);
    g.setColour(pencilColor.withAlpha(0.85f));
    g.strokePath(borderPath, juce::PathStrokeType(1.6f, juce::PathStrokeType::mitered, juce::PathStrokeType::rounded));

    cubes::ui::drawBranding(g, frameCount);

    if (!isAnimating)
    {
        juce::Path notePath;
        float cx = width / 2.0f; float cy = height / 2.0f;
        addSketchyLine(notePath, cx + 15.0f, cy - 40.0f, cx + 15.0f, cy + 20.0f, 101 + seedOffset);
        addSketchyLine(notePath, cx + 12.0f, cy - 40.0f, cx + 12.0f, cy + 20.0f, 102 + seedOffset);
        addSketchyLine(notePath, cx + 15.0f, cy - 40.0f, cx + 40.0f, cy - 10.0f, 103 + seedOffset);
        addSketchyLine(notePath, cx + 12.0f, cy - 35.0f, cx + 38.0f, cy - 5.0f, 104 + seedOffset);

        juce::Random headRng(500 + seedOffset);
        for (int i = 0; i < 6; ++i) {
            float ox = headRng.nextFloat() * 4.0f - 2.0f;
            float oy = headRng.nextFloat() * 4.0f - 2.0f;
            notePath.addEllipse(cx - 20.0f + ox, cy + 5.0f + oy, 35.0f, 25.0f);
        }
        g.setColour(pencilColor.withAlpha(0.9f));
        g.strokePath(notePath, juce::PathStrokeType(1.5f, juce::PathStrokeType::mitered, juce::PathStrokeType::rounded));
    }
    else
    {
        int lineSeed = 5000 + seedOffset;

        auto drawDrawnStroke = [&](const WallStroke& stroke) {
            if (stroke.points.size() < 2)
                return;

            juce::Path path;
            for (size_t i = 0; i + 1 < stroke.points.size(); ++i) {
                addSketchyLine(
                    path,
                    stroke.points[i].x,
                    stroke.points[i].y,
                    stroke.points[i + 1].x,
                    stroke.points[i + 1].y,
                    lineSeed++);
            }

            float thickness = 1.4f;
            if (stroke.type == WallType::soft)
                thickness = 2.2f;
            else if (stroke.type == WallType::boost)
                thickness = 1.8f;
            else if (stroke.type == WallType::oneShot)
                thickness = 1.0f;

            const auto colour = getWallTypeColour(stroke.type);
            g.setColour(colour.withAlpha(0.86f));
            g.strokePath…10357 tokens truncated…r.uiWallBrush = nextIndex;
        wallBrushHintPosition = event.position;
        wallBrushHintUntil =
            juce::Time::currentTimeMillis() + 1100;
        repaint();
        return;
    }

    const auto position = event.position;
    for (int i = static_cast<int>(particles.size()) - 1; i >= 0; --i)
    {
        auto& particle = particles[static_cast<size_t>(i)];
        const juce::Rectangle<float> particleBounds(
            particle.x, particle.y, particle.size, particle.size);

        if (!particleBounds.contains(position))
            continue;

        const int direction = wheel.deltaY > 0.0f ? 1 : -1;
        const int newPitch = juce::jlimit(
            -24, 24, particle.pitchSemitones + direction);
        if (newPitch != particle.pitchSemitones)
        {
            particle.pitchSemitones = newPitch;
            audioProcessor.setSlicePitch(particle.sliceIndex, newPitch);
            syncParticlesToProcessor();
            repaint();
        }
        return;
    }

    juce::AudioProcessorEditor::mouseWheelMove(event, wheel);
}

void NewProjectAudioProcessorEditor::generateParticles()
{
    selectParticle(-1);
    isTrajectoryArmed = false;
    isDrawingTrajectory = false;
    trajectoryTargetParticleIndex = -1;
    currentTrajectory.clear();
    particles.clear();
    strokes.clear();
    updateParticleCount();
    syncStrokesToProcessor();
}

void NewProjectAudioProcessorEditor::syncParticlesToProcessor()
{
    std::vector<NewProjectAudioProcessor::SavedParticleState> saved;
    saved.reserve(particles.size());

    for (const auto& particle : particles)
    {
        NewProjectAudioProcessor::SavedParticleState state;
        state.sliceIndex = particle.sliceIndex;
        state.x = particle.x;
        state.y = particle.y;
        state.vx = particle.vx;
        state.vy = particle.vy;
        state.size = particle.size;
        state.colour = particle.color.getARGB();
        state.isThrown = particle.isThrown;
        state.pitchSemitones = particle.pitchSemitones;
        state.isReversed = particle.isReversed;
        state.trajectory = particle.trajectory;
        state.trajectoryDistance = particle.trajectoryDistance;
        state.trajectoryForward = particle.trajectoryForward;
        saved.push_back(state);
    }

    audioProcessor.updateSavedParticles(saved);
}

void NewProjectAudioProcessorEditor::syncStrokesToProcessor()
{
    audioProcessor.updateSavedStrokes(strokes);
}

bool NewProjectAudioProcessorEditor::restoreSavedEditorState()
{
    std::vector<NewProjectAudioProcessor::SavedParticleState>
        savedParticles;
    std::vector<WallStroke> savedStrokes;

    if (!audioProcessor.copySavedEditorState(
            savedParticles, savedStrokes))
        return false;

    std::vector<Particle> restoredParticles;
    restoredParticles.reserve(savedParticles.size());

    for (const auto& state : savedParticles)
    {
        if (!juce::isPositiveAndBelow(state.sliceIndex, 20)
            || !std::isfinite(state.x)
            || !std::isfinite(state.y)
            || !std::isfinite(state.vx)
            || !std::isfinite(state.vy)
            || !std::isfinite(state.size)
            || state.size <= 0.0f)
            continue;

        Particle particle;
        particle.sliceIndex = state.sliceIndex;
        particle.x = state.x;
        particle.y = state.y;
        particle.vx = state.vx;
        particle.vy = state.vy;
        particle.size = state.size;
        particle.color = juce::Colour(state.colour);
        particle.isThrown = state.isThrown;
        particle.pitchSemitones =
            juce::jlimit(-24, 24, state.pitchSemitones);
        particle.isReversed = state.isReversed;
        particle.trajectory.reserve(state.trajectory.size());
        for (const auto point : state.trajectory)
            if (std::isfinite(point.x) && std::isfinite(point.y))
                particle.trajectory.push_back(point);
        const float trajectoryLength =
            getTrajectoryLength(particle.trajectory);
        particle.trajectoryDistance = std::isfinite(
            state.trajectoryDistance)
            ? juce::jlimit(
                0.0f,
                trajectoryLength,
                state.trajectoryDistance)
            : 0.0f;
        particle.trajectoryForward = state.trajectoryForward;
        audioProcessor.setSliceReversed(
            particle.sliceIndex, particle.isReversed);
        restoredParticles.push_back(particle);
    }

    if (restoredParticles.empty())
        return false;

    std::vector<WallStroke> restoredStrokes;
    restoredStrokes.reserve(savedStrokes.size());
    for (const auto& savedStroke : savedStrokes)
    {
        WallStroke stroke;
        stroke.type = savedStroke.type;
        stroke.points.reserve(savedStroke.points.size());
        for (const auto point : savedStroke.points)
            if (std::isfinite(point.x) && std::isfinite(point.y))
                stroke.points.push_back(point);
        if (stroke.points.size() > 1)
            restoredStrokes.push_back(std::move(stroke));
    }

    particles = std::move(restoredParticles);
    strokes = std::move(restoredStrokes);
    return true;
}

void NewProjectAudioProcessorEditor::updateParticleCount()
{
    int targetNum = (int)numParticlesSlider.getValue();
    int currentNum = (int)particles.size();
    if (currentNum == targetNum) return;

    if (currentNum > targetNum) {
        particles.resize(targetNum);
        if (draggedParticleIndex >= targetNum) draggedParticleIndex = -1;
        if (trajectoryTargetParticleIndex >= targetNum)
        {
            isTrajectoryArmed = false;
            isDrawingTrajectory = false;
            trajectoryTargetParticleIndex = -1;
            currentTrajectory.clear();
        }
        if (selectedParticleIndex >= targetNum)
            selectParticle(-1);
    }
    else {
        int totalSamples = loadedAudio.getNumSamples();
        if (totalSamples == 0) return;

        std::mt19937 rng((unsigned int)juce::Time::currentTimeMillis());
        std::uniform_real_distribution<float> angleDist(0.0f, juce::MathConstants<float>::twoPi);
        std::uniform_real_distribution<float> posDistX(50.0f, std::max(51.0f, (float)getWidth() - 100.0f));
        std::uniform_real_distribution<float> posDistY(50.0f, std::max(51.0f, (float)getHeight() - 100.0f));
        std::uniform_real_distribution<float> speedDist(1.5f, 4.0f);
        std::uniform_real_distribution<float> sizeDist(30.0f, 60.0f);
        std::uniform_int_distribution<int> lengthDist(4410, 44100);

        for (int i = currentNum; i < targetNum; ++i) {
            Particle p; p.sliceIndex = i;
            p.pitchSemitones = 0;
            p.isReversed = false;
            int sliceLength = juce::jmin(
                totalSamples, lengthDist(rng));
            std::uniform_int_distribution<int> startDist(
                0, std::max(0, totalSamples - sliceLength));
            int startSample = startDist(rng);
            const int baseShift =
                std::max(2205, totalSamples / 50);
            const int maximumStart =
                juce::jmax(0, totalSamples - sliceLength);
            constexpr std::array<int, 4>
                sideOffsets { 0, 2, 1, 3 };
            for (int side = 0;
                 side < NewProjectAudioProcessor::numSliceSides;
                 ++side)
            {
                const auto modulus =
                    static_cast<juce::int64>(maximumStart) + 1;
                const auto shiftedStart =
                    static_cast<juce::int64>(startSample)
                    + static_cast<juce::int64>(
                        sideOffsets[static_cast<size_t>(side)])
                          * baseShift;
                const int sideStart = static_cast<int>(
                    shiftedStart % modulus);
                audioProcessor.setSliceForSide(
                    i, side, sideStart, sliceLength);
            }
            audioProcessor.setSlicePitch(i, p.pitchSemitones);
            audioProcessor.setSliceReversed(i, p.isReversed);

            p.x = posDistX(rng); p.y = posDistY(rng);
            float speed = speedDist(rng); float angle = angleDist(rng);
            p.vx = std::cos(angle) * speed; p.vy = std::sin(angle) * speed;
            p.size = sizeDist(rng);
            p.color = juce::Colour((juce::uint8)rng(), (juce::uint8)rng(), (juce::uint8)rng()).withAlpha(1.0f);
            particles.push_back(p);
        }
    }

    syncParticlesToProcessor();
}

void NewProjectAudioProcessorEditor::timerCallback()
{
    frameCount++;

    // Анимация выезжающей панели
    if (std::abs(recordingPanelCurrentPos - recordingPanelAnimationTarget) > 1.0f) {
        recordingPanelCurrentPos += (recordingPanelAnimationTarget - recordingPanelCurrentPos) * 0.2f;
    }

    int pw = (int)recordingPanelCurrentPos;
    int panelHeight = 100;

    // Сдвигаем панель вниз (15px отступ от низа, на уровне с ручками)
    int panelY = getHeight() - panelHeight - 15;
    recordingPanel.setBounds(getWidth() - pw - 10, panelY, pw, panelHeight);

    // Стрелочка приклеена к левому краю панели и двигается синхронно с ней
    int arrowWidth = 25;
    int arrowHeight = 40;
    int arrowX = getWidth() - pw - 10 - arrowWidth;
    int arrowY = panelY + (panelHeight - arrowHeight) / 2;
    toggleRecordingPanelButton.setBounds(arrowX, arrowY, arrowWidth, arrowHeight);

    if (recordingPanelCurrentPos > 5.0f) {
        recordingPanel.repaint();
    }

    if (isAnimating)
    {
        updateParticleCount();
        if (collisionButton.getToggleState()) {
            for (int i = 0; i < particles.size(); ++i) {
                for (int j = i + 1; j < particles.size(); ++j) {
                    if (i == draggedParticleIndex || j == draggedParticleIndex) continue;

                    auto& p1 = particles[i]; auto& p2 = particles[j];
                    if (isTimeStopped && !p1.isThrown && !p2.isThrown) continue;

                    float dx = (p2.x + p2.size / 2.0f) - (p1.x + p1.size / 2.0f);
                    float dy = (p2.y + p2.size / 2.0f) - (p1.y + p1.size / 2.0f);
                    float dist2 = dx * dx + dy * dy;
                    float minDist = (p1.size + p2.size) / 2.0f;

                    if (dist2 < minDist * minDist && dist2 > 0.001f) {
                        float dist = std::sqrt(dist2); float nx = dx / dist; float ny = dy / dist;
                        float dot = (p1.vx - p2.vx) * nx + (p1.vy - p2.vy) * ny;
                        if (dot > 0) { p1.vx -= dot * nx; p1.vy -= dot * ny; p2.vx += dot * nx; p2.vy += dot * ny; }

                        float overlap = minDist - dist; float corrX = nx * overlap * 0.5f; float corrY = ny * overlap * 0.5f;
                        if (isTimeStopped && !p1.isThrown) { p2.x += corrX * 2.0f; p2.y += corrY * 2.0f; }
                        else if (isTimeStopped && !p2.isThrown) { p1.x -= corrX * 2.0f; p1.y -= corrY * 2.0f; }
                        else { p1.x -= corrX; p1.y -= corrY; p2.x += corrX; p2.y += corrY; }
                    }
                }
            }
        }

        std::vector<size_t> oneShotStrokesToRemove;

        for (int i = 0; i < particles.size(); ++i) {
            if (i == draggedParticleIndex) continue;
            auto& p = particles[i];
            if (isTimeStopped && !p.isThrown) continue;

            const bool followsTrajectory =
                moveParticleAlongTrajectory(p);
            if (!followsTrajectory)
            {
                p.x += p.vx;
                p.y += p.vy;
            }
            bool bounced = false; int hitType = 0; float wallMargin = 6.0f;
            bool outerWallCollision = false;

            if (p.x <= wallMargin) { p.x = wallMargin; p.vx *= -1.0f; bounced = true; outerWallCollision = true; hitType = 0; }
            else if (p.x + p.size >= getWidth() - wallMargin) { p.x = getWidth() - wallMargin - p.size; p.vx *= -1.0f; bounced = true; outerWallCollision = true; hitType = 2; }
            if (p.y <= wallMargin) { p.y = wallMargin; p.vy *= -1.0f; bounced = true; outerWallCollision = true; hitType = 1; }
            else if (p.y + p.size >= getHeight() - wallMargin) { p.y = getHeight() - wallMargin - p.size; p.vy *= -1.0f; bounced = true; outerWallCollision = true; hitType = 3; }

            if (outerWallCollision && followsTrajectory)
                p.trajectoryForward = !p.trajectoryForward;

            float cx = p.x + p.size / 2.0f; float cy = p.y + p.size / 2.0f; float radius = p.size / 2.0f;

            auto checkLineCollision =
                [&](const WallStroke& stroke, int savedStrokeIndex)
            {
                if (stroke.points.size() < 2)
                    return;
                if (savedStrokeIndex >= 0
                    && std::find(
                        oneShotStrokesToRemove.begin(),
                        oneShotStrokesToRemove.end(),
                        static_cast<size_t>(savedStrokeIndex))
                           != oneShotStrokesToRemove.end())
                    return;

                for (size_t j = 0; j + 1 < stroke.points.size(); ++j) {
                    juce::Point<float> pt1 = stroke.points[j]; juce::Point<float> pt2 = stroke.points[j + 1];
                    float l2 = (pt2.x - pt1.x) * (pt2.x - pt1.x) + (pt2.y - pt1.y) * (pt2.y - pt1.y);
                    float t = 0;
                    if (l2 > 0) { t = ((cx - pt1.x) * (pt2.x - pt1.x) + (cy - pt1.y) * (pt2.y - pt1.y)) / l2; t = std::max(0.0f, std::min(1.0f, t)); }
                    float projX = pt1.x + t * (pt2.x - pt1.x); float projY = pt1.y + t * (pt2.y - pt1.y);
                    float dx = cx - projX; float dy = cy - projY; float dist = std::sqrt(dx * dx + dy * dy);

                    if (dist < radius && dist > 0.001f) {
                        float nx = dx / dist; float ny = dy / dist; float dot = p.vx * nx + p.vy * ny;
                        if (dot < 0) {
                            bounced = true;
                            if (std::abs(nx) > std::abs(ny))
                                hitType = nx > 0.0f ? 0 : 2;
                            else
                                hitType = ny > 0.0f ? 1 : 3;

                            if (stroke.type == WallType::teleport)
                            {
                                const juce::Point<float> projection(
                                    projX, projY);
                                const bool firstIsNearer =
                                    projection.getDistanceSquaredFrom(
                                        stroke.points.front())
                                    <= projection.getDistanceSquaredFrom(
                                        stroke.points.back());
                                const auto destination = firstIsNearer
                                    ? stroke.points.back()
                                    : stroke.points.front();
                                const float speed = juce::jmax(
                                    0.001f,
                                    std::sqrt(
                                        p.vx * p.vx + p.vy * p.vy));
                                const juce::Point<float> travelDirection(
                                    p.vx / speed, p.vy / speed);
                                const auto destinationCentre =
                                    destination
                                    + travelDirection * (radius + 5.0f);
                                p.x = juce::jlimit(
                                    wallMargin,
                                    getWidth() - wallMargin - p.size,
                                    destinationCentre.x - radius);
                                p.y = juce::jlimit(
                                    wallMargin,
                                    getHeight() - wallMargin - p.size,
                                    destinationCentre.y - radius);
                                cx = p.x + radius;
                                cy = p.y + radius;
                                if (followsTrajectory)
                                    p.trajectoryDistance =
                                        getNearestTrajectoryDistance(
                                            p.trajectory,
                                            { cx, cy });
                                return;
                            }

                            p.vx -= 2.0f * dot * nx;
                            p.vy -= 2.0f * dot * ny;
                            float overlap = radius - dist;
                            p.x += nx * overlap;
                            p.y += ny * overlap;
                            cx = p.x + radius;
                            cy = p.y + radius;

                            if (stroke.type == WallType::soft
                                || stroke.type == WallType::boost)
                            {
                                const float speed = std::sqrt(
                                    p.vx * p.vx + p.vy * p.vy);
                                if (speed > 0.001f)
                                {
                                    const float factor =
                                        stroke.type == WallType::soft
                                        ? 0.65f
                                        : 1.35f;
                                    const float newSpeed = juce::jlimit(
                                        0.35f, 25.0f, speed * factor);
                                    p.vx *= newSpeed / speed;
                                    p.vy *= newSpeed / speed;
                                }
                            }

                            if (followsTrajectory)
                                p.trajectoryForward =
                                    !p.trajectoryForward;
                            if (stroke.type == WallType::oneShot
                                && savedStrokeIndex >= 0)
                                oneShotStrokesToRemove.push_back(
                                    static_cast<size_t>(
                                        savedStrokeIndex));
                            return;
                        }
                    }
                }
            };

            for (int strokeIndex = 0;
                 strokeIndex < static_cast<int>(strokes.size());
                 ++strokeIndex)
                checkLineCollision(
                    strokes[static_cast<size_t>(strokeIndex)],
                    strokeIndex);
            if (isDrawing)
                checkLineCollision(currentStroke, -1);

            if (bounced) {
                auto now = juce::Time::currentTimeMillis();
                if (now - p.lastHitTime > 50) { audioProcessor.playSlice(p.sliceIndex, hitType); p.lastHitTime = now; }
                if (isTimeStopped) { p.isThrown = false; }
            }
        }

        if (!oneShotStrokesToRemove.empty())
        {
            std::sort(
                oneShotStrokesToRemove.begin(),
                oneShotStrokesToRemove.end());
            oneShotStrokesToRemove.erase(
                std::unique(
                    oneShotStrokesToRemove.begin(),
                    oneShotStrokesToRemove.end()),
                oneShotStrokesToRemove.end());
            for (auto i = oneShotStrokesToRemove.rbegin();
                 i != oneShotStrokesToRemove.rend();
                 ++i)
                if (*i < strokes.size())
                    strokes.erase(
                        strokes.begin()
                        + static_cast<std::ptrdiff_t>(*i));
            syncStrokesToProcessor();
        }
    }
    syncParticlesToProcessor();
    repaint();
}

