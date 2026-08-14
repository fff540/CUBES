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
            g.strokePath(
                path,
                juce::PathStrokeType(
                    thickness,
                    juce::PathStrokeType::curved,
                    juce::PathStrokeType::rounded));

            if (stroke.type == WallType::teleport)
            {
                g.setColour(colour.withAlpha(0.72f));
                g.drawEllipse(
                    juce::Rectangle<float>(8.0f, 8.0f)
                        .withCentre(stroke.points.front()),
                    1.1f);
                g.drawEllipse(
                    juce::Rectangle<float>(8.0f, 8.0f)
                        .withCentre(stroke.points.back()),
                    1.1f);
            }
        };

        for (const auto& stroke : strokes)
            drawDrawnStroke(stroke);
        if (isDrawing)
            drawDrawnStroke(currentStroke);

        auto drawTrajectory =
            [&](const std::vector<juce::Point<float>>& trajectory,
                juce::Colour colour,
                int seed)
            {
                if (trajectory.size() < 2)
                    return;

                juce::Path path;
                for (size_t i = 0; i + 1 < trajectory.size(); ++i)
                    addSketchyLine(
                        path,
                        trajectory[i].x,
                        trajectory[i].y,
                        trajectory[i + 1].x,
                        trajectory[i + 1].y,
                        seed + static_cast<int>(i));

                g.setColour(colour.withAlpha(0.34f));
                g.strokePath(
                    path,
                    juce::PathStrokeType(
                        1.05f,
                        juce::PathStrokeType::curved,
                        juce::PathStrokeType::rounded));
            };

        for (int i = 0; i < static_cast<int>(particles.size()); ++i)
        {
            const auto& particle = particles[static_cast<size_t>(i)];
            drawTrajectory(
                particle.trajectory,
                particle.color,
                31000 + i * 503 + seedOffset);
        }
        if (isDrawingTrajectory)
            drawTrajectory(
                currentTrajectory,
                pencilColor,
                39000 + seedOffset);

        for (int i = 0; i < particles.size(); ++i) {
            const auto& p = particles[i];
            g.setColour(p.color.withAlpha(0.25f));
            g.fillRoundedRectangle(p.x, p.y, p.size, p.size, 2.0f);

            juce::Path rectPath;
            int animSeed = (i * 1000) + seedOffset;
            addSketchyLine(rectPath, p.x, p.y, p.x + p.size, p.y, animSeed + 1);
            addSketchyLine(rectPath, p.x + p.size, p.y, p.x + p.size, p.y + p.size, animSeed + 2);
            addSketchyLine(rectPath, p.x + p.size, p.y + p.size, p.x, p.y + p.size, animSeed + 3);
            addSketchyLine(rectPath, p.x, p.y + p.size, p.x, p.y, animSeed + 4);

            g.setColour(pencilColor.withAlpha(0.85f));
            g.strokePath(rectPath, juce::PathStrokeType(1.4f, juce::PathStrokeType::mitered, juce::PathStrokeType::rounded));
            drawSketchyPitchValue(g, p, i, frameCount);
            drawSketchyReverseSymbol(g, p, i, frameCount);
        }

        drawFragmentEditor(g);

        if (audioProcessor.uiSettingsOpen)
        {
            juce::Rectangle<float> panel(
                getWidth() - 225.0f, 55.0f, 210.0f, 265.0f);
            g.setColour(juce::Colour(0xFFF3EFE9).withAlpha(0.97f));
            g.fillRoundedRectangle(panel, 4.0f);

            juce::Path border;
            border.addRoundedRectangle(panel, 4.0f);
            juce::Path sketchyBorder;
            juce::PathFlatteningIterator it(border, juce::AffineTransform(), 2.0f);
            juce::Random r(888 + seedOffset);
            float s = 1.0f;
            bool isFirst = true;
            while (it.next()) {
                float nx = it.x2 + (r.nextFloat() * s * 2 - s); float ny = it.y2 + (r.nextFloat() * s * 2 - s);
                if (isFirst) { sketchyBorder.startNewSubPath(nx, ny); isFirst = false; }
                else { sketchyBorder.lineTo(nx, ny); }
            }
            g.setColour(pencilColor.withAlpha(0.6f));
            g.strokePath(sketchyBorder, juce::PathStrokeType(1.5f));

            juce::Path separators;
            addSketchyLine(separators, panel.getX() + 12.0f, 112.0f,
                           panel.getRight() - 12.0f, 112.0f, 1201 + seedOffset);
            addSketchyLine(separators, panel.getX() + 12.0f, 178.0f,
                           panel.getRight() - 12.0f, 178.0f, 1202 + seedOffset);
            addSketchyLine(separators, panel.getX() + 12.0f, 242.0f,
                           panel.getRight() - 12.0f, 242.0f, 1203 + seedOffset);
            g.setColour(pencilColor.withAlpha(0.65f));
            g.strokePath(separators, juce::PathStrokeType(1.2f,
                juce::PathStrokeType::curved, juce::PathStrokeType::rounded));

            g.setColour(juce::Colours::black);
            g.setFont(10.0f);
            g.drawText("A", attackSlider.getX(), attackSlider.getBottom() - 2,
                       attackSlider.getWidth(), 12, juce::Justification::centred, false);
            g.drawText("D", decaySlider.getX(), decaySlider.getBottom() - 2,
                       decaySlider.getWidth(), 12, juce::Justification::centred, false);
            g.drawText("S", sustainSlider.getX(), sustainSlider.getBottom() - 2,
                       sustainSlider.getWidth(), 12, juce::Justification::centred, false);
            g.drawText("R", releaseSlider.getX(), releaseSlider.getBottom() - 2,
                       releaseSlider.getWidth(), 12, juce::Justification::centred, false);

            g.setFont(12.0f);
            g.drawText(juce::String(static_cast<int>(numParticlesSlider.getValue())),
                       numParticlesSlider.getX(), numParticlesSlider.getBottom() - 1,
                       numParticlesSlider.getWidth(), 17,
                       juce::Justification::centred, false);
            const bool ordinaryPitchMode =
                audioProcessor.apvts
                    .getRawParameterValue("ordinary_pitch")->load() >= 0.5f;
            g.setFont(9.5f);
            g.drawText(ordinaryPitchMode ? "pitch" : "resample",
                       pitchSlider.getX(), pitchSlider.getBottom() - 1,
                       pitchSlider.getWidth(), 17,
                       juce::Justification::centred, false);
            g.drawText("rand",
                       randSlider.getX(), randSlider.getBottom() - 1,
                       randSlider.getWidth(), 17,
                       juce::Justification::centred, false);
            g.drawText("humanize",
                       humanizeSlider.getX(),
                       humanizeSlider.getBottom() - 1,
                       humanizeSlider.getWidth(), 17,
                       juce::Justification::centred, false);
        }

        if (juce::Time::currentTimeMillis() < wallBrushHintUntil)
        {
            const juce::Rectangle<float> hint(
                juce::jlimit(
                    12.0f,
                    juce::jmax(12.0f, getWidth() - 142.0f),
                    wallBrushHintPosition.x - 61.0f),
                juce::jlimit(
                    12.0f,
                    juce::jmax(12.0f, getHeight() - 42.0f),
                    wallBrushHintPosition.y - 36.0f),
                122.0f,
                28.0f);
            g.setColour(juce::Colour(0xff232529));
            g.setFont(juce::FontOptions(
                "Segoe Print", 16.0f, juce::Font::plain));
            g.drawText(
                getWallTypeName(currentWallType),
                hint,
                juce::Justification::centred,
                false);
        }
    }
}

void NewProjectAudioProcessorEditor::resized()
{
    gearButton.setBounds(getWidth() - 45, 15, 30, 30);
    const int panelX = getWidth() - 225;
    timeStopButton.setBounds(panelX + 67, 68, 30, 30);
    clearLinesButton.setBounds(panelX + 113, 68, 30, 30);
    collisionButton.setBounds(panelX + 10, 118, 190, 54);

    constexpr int adsrSize = 36; // Меняй это значение
    constexpr int adsrCentreY = 206;

    attackSlider.setBounds(
        panelX + 37 - adsrSize / 2,
        adsrCentreY - adsrSize / 2,
        adsrSize,
        adsrSize);

    decaySlider.setBounds(
        panelX + 81 - adsrSize / 2,
        adsrCentreY - adsrSize / 2,
        adsrSize,
        adsrSize);

    sustainSlider.setBounds(
        panelX + 125 - adsrSize / 2,
        adsrCentreY - adsrSize / 2,
        adsrSize,
        adsrSize);

    releaseSlider.setBounds(
        panelX + 169 - adsrSize / 2,
        adsrCentreY - adsrSize / 2,
        adsrSize,
        adsrSize);
    numParticlesSlider.setBounds(panelX + 10, 254, 42, 42);
    pitchSlider.setBounds(panelX + 59, 254, 42, 42);
    randSlider.setBounds(panelX + 108, 254, 42, 42);
    humanizeSlider.setBounds(panelX + 158, 254, 42, 42);

    // Позиционирование toggleRecordingPanelButton теперь полностью управляется в timerCallback
}

void NewProjectAudioProcessorEditor::updateSettingsVisibility()
{
    const bool shouldBeVisible = isAnimating && audioProcessor.uiSettingsOpen;
    timeStopButton.setVisible(shouldBeVisible);
    clearLinesButton.setVisible(shouldBeVisible);
    collisionButton.setVisible(shouldBeVisible);
    attackSlider.setVisible(shouldBeVisible);
    decaySlider.setVisible(shouldBeVisible);
    sustainSlider.setVisible(shouldBeVisible);
    releaseSlider.setVisible(shouldBeVisible);
    numParticlesSlider.setVisible(shouldBeVisible);
    pitchSlider.setVisible(shouldBeVisible);
    randSlider.setVisible(shouldBeVisible);
    humanizeSlider.setVisible(shouldBeVisible);
}

bool NewProjectAudioProcessorEditor::isInterestedInFileDrag(const juce::StringArray& files)
{
    for (auto file : files) if (file.endsWithIgnoreCase(".wav")) return true;
    return false;
}

void NewProjectAudioProcessorEditor::filesDropped(const juce::StringArray& files, int x, int y)
{
    juce::File wavFile;
    for (const auto& path : files)
    {
        if (path.endsWithIgnoreCase(".wav"))
        {
            wavFile = juce::File(path);
            break;
        }
    }

    if (!wavFile.existsAsFile())
        return;

    std::unique_ptr<juce::AudioFormatReader> reader(
        formatManager.createReaderFor(wavFile));
    if (reader == nullptr
        || reader->numChannels == 0
        || reader->numChannels
               > static_cast<unsigned int>(
                   std::numeric_limits<int>::max())
        || reader->lengthInSamples <= 0
        || reader->lengthInSamples
               > static_cast<juce::int64>(
                   std::numeric_limits<int>::max()))
        return;

    const int channels = static_cast<int>(reader->numChannels);
    const int samples = static_cast<int>(reader->lengthInSamples);
    loadedAudio.setSize(channels, samples);

    if (!reader->read(
            &loadedAudio, 0, samples, 0, true, true))
    {
        loadedAudio.setSize(0, 0);
        return;
    }

    audioProcessor.loadAudioBuffer(loadedAudio);
    generateParticles();
    isAnimating = true;

        toggleRecordingPanelButton.setVisible(true); // Показываем стрелочку после загрузки проекта
    gearButton.setVisible(true);
    updateSettingsVisibility();
    repaint();
}

void NewProjectAudioProcessorEditor::selectParticle(int particleIndex)
{
    if (selectedParticleIndex != particleIndex)
        activeFragmentSide = 0;
    selectedParticleIndex = particleIndex;
    fragmentDragMode = FragmentDragMode::none;

    if (!juce::isPositiveAndBelow(
            selectedParticleIndex,
            static_cast<int>(particles.size())))
    {
        selectedParticleIndex = -1;
        fragmentEditorBounds = {};
        return;
    }

    const auto& particle =
        particles[static_cast<size_t>(selectedParticleIndex)];
    constexpr float editorWidth = 276.0f;
    constexpr float editorHeight = 112.0f;
    constexpr float margin = 14.0f;

    float editorX = particle.x + particle.size * 0.5f
                    - editorWidth * 0.5f;
    float editorY = particle.y + particle.size + 10.0f;
    if (editorY + editorHeight > getHeight() - margin)
        editorY = particle.y - editorHeight - 10.0f;

    editorX = juce::jlimit(
        margin,
        juce::jmax(margin, getWidth() - margin - editorWidth),
        editorX);
    editorY = juce::jlimit(
        margin,
        juce::jmax(margin, getHeight() - margin - editorHeight),
        editorY);
    fragmentEditorBounds = {
        editorX, editorY, editorWidth, editorHeight
    };
}

juce::Rectangle<float>
NewProjectAudioProcessorEditor::getFragmentEditorBounds() const
{
    if (!juce::isPositiveAndBelow(
            selectedParticleIndex,
            static_cast<int>(particles.size()))
        || loadedAudio.getNumSamples() <= 0)
        return {};

    return fragmentEditorBounds;
}

juce::Rectangle<float>
NewProjectAudioProcessorEditor::getFragmentWaveformBounds() const
{
    auto bounds = getFragmentEditorBounds();
    if (bounds.isEmpty())
        return {};

    bounds.removeFromTop(27.0f);
    bounds.removeFromBottom(25.0f);
    return bounds.reduced(10.0f, 4.0f);
}

juce::Rectangle<float>
NewProjectAudioProcessorEditor::getTrajectoryButtonBounds() const
{
    const auto panel = getFragmentEditorBounds();
    if (panel.isEmpty())
        return {};

    return {
        panel.getRight() - 51.0f,
        panel.getY() + 4.0f,
        41.0f,
        22.0f
    };
}

void NewProjectAudioProcessorEditor::drawFragmentEditor(
    juce::Graphics& graphics)
{
    juce::Graphics::ScopedSaveState savedGraphicsState(graphics);
    const auto panel = getFragmentEditorBounds();
    const auto waveform = getFragmentWaveformBounds();
    if (panel.isEmpty() || waveform.isEmpty())
        return;

    std::array<int, NewProjectAudioProcessor::numSliceSides>
        fragmentStarts{};
    std::array<int, NewProjectAudioProcessor::numSliceSides>
        fragmentLengths{};
    const int sliceIndex =
        particles[static_cast<size_t>(selectedParticleIndex)].sliceIndex;
    if (!audioProcessor.getSliceRanges(
            sliceIndex, fragmentStarts, fragmentLengths))
        return;

    const int totalSamples = loadedAudio.getNumSamples();
    if (totalSamples <= 0)
        return;
    activeFragmentSide = juce::jlimit(
        0,
        NewProjectAudioProcessor::numSliceSides - 1,
        activeFragmentSide);
    const int fragmentStart =
        fragmentStarts[static_cast<size_t>(activeFragmentSide)];
    const int fragmentLength =
        fragmentLengths[static_cast<size_t>(activeFragmentSide)];

    const juce::Colour pencil(0xff232529);
    graphics.setColour(juce::Colour(0xfff3efe9).withAlpha(0.97f));
    graphics.fillRoundedRectangle(panel, 4.0f);

    juce::Random borderRandom(
        24000 + selectedParticleIndex * 101 + frameCount / 10);
    for (int pass = 0; pass < 2; ++pass)
    {
        const float offsetX =
            borderRandom.nextFloat() * 1.0f - 0.5f;
        const float offsetY =
            borderRandom.nextFloat() * 1.0f - 0.5f;
        graphics.setColour(
            pencil.withAlpha(pass == 0 ? 0.64f : 0.24f));
        graphics.drawRoundedRectangle(
            panel.translated(offsetX, offsetY),
            4.0f,
            pass == 0 ? 1.25f : 0.8f);
    }

    const auto trajectoryButton = getTrajectoryButtonBounds();
    const bool trajectoryButtonActive =
        isTrajectoryArmed
        && trajectoryTargetParticleIndex == selectedParticleIndex;
    if (trajectoryButtonActive)
    {
        graphics.setColour(
            particles[static_cast<size_t>(selectedParticleIndex)]
                .color.withAlpha(0.18f));
        graphics.fillRoundedRectangle(trajectoryButton, 4.0f);
    }

    juce::Path trajectoryIcon;
    const auto iconBounds = trajectoryButton.reduced(8.0f, 6.0f);

    const float iconX = iconBounds.getX();
    const float iconY = iconBounds.getY();
    const float iconW = iconBounds.getWidth();
    const float iconH = iconBounds.getHeight();
    trajectoryIcon.startNewSubPath(
        iconX,
        iconY + iconH * 0.72f);
    trajectoryIcon.cubicTo(
        iconX + iconW * 0.14f,
        iconY + iconH * 0.16f,
        iconX + iconW * 0.34f,
        iconY + iconH * 0.12f,
        iconX + iconW * 0.46f,
        iconY + iconH * 0.49f);
    trajectoryIcon.cubicTo(
        iconX + iconW * 0.58f,
        iconY + iconH * 0.91f,
        iconX + iconW * 0.76f,
        iconY + iconH * 0.91f,
        iconX + iconW * 0.88f,
        iconY + iconH * 0.51f);
    trajectoryIcon.lineTo(
        iconX + iconW * 0.98f,
        iconY + iconH * 0.18f);
    const juce::Point<float> arrowTip(
        iconX + iconW * 0.98f,
        iconY + iconH * 0.18f);

    const float arrowHeadLength = iconW * 0.26f;

    trajectoryIcon.startNewSubPath(
        arrowTip.x - arrowHeadLength,
        arrowTip.y);

    trajectoryIcon.lineTo(arrowTip);

    trajectoryIcon.lineTo(
        arrowTip.x,
        arrowTip.y + arrowHeadLength);
    graphics.setColour(pencil.withAlpha(
        trajectoryButtonActive ? 0.94f : 0.76f));
    graphics.strokePath(
        trajectoryIcon,
        juce::PathStrokeType(
            1.55f,
            juce::PathStrokeType::curved,
            juce::PathStrokeType::rounded));

    const double sampleRate =
        juce::jmax(1.0, audioProcessor.currentSampleRate);
    const auto startText = juce::String(
        static_cast<double>(fragmentStart) / sampleRate, 3);
    const auto lengthText = juce::String(
        static_cast<double>(fragmentLength) / sampleRate, 3);
    graphics.setColour(pencil.withAlpha(0.76f));
    graphics.setFont(juce::FontOptions(
        "Segoe Print", 14.0f, juce::Font::plain));
    graphics.drawText(
        "start " + startText + "s   length " + lengthText + "s",
        panel.reduced(10.0f, 3.0f).removeFromTop(23.0f),
        juce::Justification::centredLeft,
        false);

    juce::Path waveformPath;
    const int pixelCount =
        juce::jmax(1, juce::roundToInt(waveform.getWidth()));
    const int numChannels = loadedAudio.getNumChannels();
    const float centreY = waveform.getCentreY();
    const float amplitude = waveform.getHeight() * 0.43f;

    for (int pixel = 0; pixel < pixelCount; ++pixel)
    {
        const int rangeStart = juce::jlimit(
            0,
            totalSamples - 1,
            static_cast<int>(
                static_cast<double>(pixel)
                / static_cast<double>(pixelCount)
                * totalSamples));
        const int rangeEnd = juce::jlimit(
            rangeStart + 1,
            totalSamples,
            static_cast<int>(
                static_cast<double>(pixel + 1)
                / static_cast<double>(pixelCount)
                * totalSamples));
        const int step = juce::jmax(1, (rangeEnd - rangeStart) / 8);
        float minimum = 0.0f;
        float maximum = 0.0f;

        for (int sample = rangeStart;
             sample < rangeEnd;
             sample += step)
        {
            for (int channel = 0; channel < numChannels; ++channel)
            {
                const float value =
                    loadedAudio.getSample(channel, sample);
                minimum = juce::jmin(minimum, value);
                maximum = juce::jmax(maximum, value);
            }
        }

        const float x = waveform.getX()
                        + static_cast<float>(pixel);
        waveformPath.startNewSubPath(
            x, centreY - maximum * amplitude);
        waveformPath.lineTo(
            x, centreY - minimum * amplitude);
    }

    graphics.setColour(pencil.withAlpha(0.48f));
    graphics.strokePath(
        waveformPath,
        juce::PathStrokeType(
            0.8f,
            juce::PathStrokeType::curved,
            juce::PathStrokeType::rounded));

    const auto selectionColour =
        particles[static_cast<size_t>(selectedParticleIndex)].color;
    for (int drawPass = 0;
         drawPass < NewProjectAudioProcessor::numSliceSides;
         ++drawPass)
    {
        const int side = drawPass
            == NewProjectAudioProcessor::numSliceSides - 1
            ? activeFragmentSide
            : (drawPass < activeFragmentSide
                   ? drawPass
                   : drawPass + 1);
        const auto sideIndex = static_cast<size_t>(side);
        const float startRatio =
            static_cast<float>(fragmentStarts[sideIndex])
            / static_cast<float>(totalSamples);
        const float endRatio =
            static_cast<float>(juce::jmin(
                totalSamples,
                fragmentStarts[sideIndex]
                    + fragmentLengths[sideIndex]))
            / static_cast<float>(totalSamples);
        const float selectionStart =
            waveform.getX() + waveform.getWidth() * startRatio;
        const float selectionEnd =
            waveform.getX() + waveform.getWidth() * endRatio;
        const juce::Rectangle<float> selection(
            selectionStart,
            waveform.getY(),
            juce::jmax(1.0f, selectionEnd - selectionStart),
            waveform.getHeight());
        const bool isActive = side == activeFragmentSide;

        graphics.setColour(selectionColour.withAlpha(
            isActive ? 0.24f : 0.11f));
        graphics.fillRect(selection);
        graphics.setColour(pencil.withAlpha(
            isActive ? 0.82f : 0.45f));
        graphics.drawVerticalLine(
            juce::roundToInt(selectionStart),
            waveform.getY() - 2.0f,
            waveform.getBottom() + 2.0f);
        graphics.drawVerticalLine(
            juce::roundToInt(selectionEnd),
            waveform.getY() - 2.0f,
            waveform.getBottom() + 2.0f);

        drawSketchyDirectionArrow(
            graphics,
            {
                juce::jlimit(
                    panel.getX() + 12.0f,
                    panel.getRight() - 12.0f,
                    selection.getCentreX()),
                waveform.getBottom() + 13.0f
            },
            side,
            27000 + selectedParticleIndex * 113
                + side * 31 + frameCount / 10);
    }
}

bool NewProjectAudioProcessorEditor::beginFragmentDrag(
    juce::Point<float> position)
{
    const auto panel = getFragmentEditorBounds();
    if (!panel.contains(position))
        return false;

    fragmentDragMode = FragmentDragMode::none;
    if (getTrajectoryButtonBounds().contains(position))
    {
        isTrajectoryArmed = true;
        isDrawingTrajectory = false;
        trajectoryTargetParticleIndex = selectedParticleIndex;
        currentTrajectory.clear();
        repaint();
        return true;
    }

    const auto waveform = getFragmentWaveformBounds();
    if (!waveform.contains(position))
        return true;

    const int sliceIndex =
        particles[static_cast<size_t>(selectedParticleIndex)].sliceIndex;
    std::array<int, NewProjectAudioProcessor::numSliceSides>
        fragmentStarts{};
    std::array<int, NewProjectAudioProcessor::numSliceSides>
        fragmentLengths{};
    if (!audioProcessor.getSliceRanges(
            sliceIndex, fragmentStarts, fragmentLengths))
        return true;

    const int totalSamples = loadedAudio.getNumSamples();
    constexpr float handleRadius = 8.0f;
    float nearestHandleDistance =
        std::numeric_limits<float>::max();
    int nearestHandleSide = 0;
    FragmentDragMode nearestHandleMode =
        FragmentDragMode::start;
    float nearestSelectionCentreDistance =
        std::numeric_limits<float>::max();
    int containingSide = -1;

    for (int side = 0;
         side < NewProjectAudioProcessor::numSliceSides;
         ++side)
    {
        const auto sideIndex = static_cast<size_t>(side);
        const float startX = waveform.getX()
            + waveform.getWidth()
              * static_cast<float>(fragmentStarts[sideIndex])
              / static_cast<float>(totalSamples);
        const float endX = waveform.getX()
            + waveform.getWidth()
              * static_cast<float>(
                  fragmentStarts[sideIndex]
                    + fragmentLengths[sideIndex])
              / static_cast<float>(totalSamples);
        const float startDistance =
            std::abs(position.x - startX);
        const float endDistance =
            std::abs(position.x - endX);

        if (startDistance < nearestHandleDistance)
        {
            nearestHandleDistance = startDistance;
            nearestHandleSide = side;
            nearestHandleMode = FragmentDragMode::start;
        }
        if (endDistance < nearestHandleDistance)
        {
            nearestHandleDistance = endDistance;
            nearestHandleSide = side;
            nearestHandleMode = FragmentDragMode::end;
        }

        if (position.x > startX && position.x < endX)
        {
            if (side == activeFragmentSide)
            {
                containingSide = side;
                nearestSelectionCentreDistance = 0.0f;
            }
            else if (nearestSelectionCentreDistance > 0.0f)
            {
                const float centreDistance = std::abs(
                    position.x - (startX + endX) * 0.5f);
                if (centreDistance
                    < nearestSelectionCentreDistance)
                {
                    containingSide = side;
                    nearestSelectionCentreDistance =
                        centreDistance;
                }
            }
        }
    }

    if (nearestHandleDistance <= handleRadius)
    {
        fragmentDragSide = nearestHandleSide;
        fragmentDragMode = nearestHandleMode;
    }
    else if (containingSide >= 0)
    {
        fragmentDragSide = containingSide;
        fragmentDragMode = FragmentDragMode::move;
    }
    else
    {
        fragmentDragSide = nearestHandleSide;
        fragmentDragMode = nearestHandleMode;
    }

    activeFragmentSide = fragmentDragSide;
    fragmentDragInitialStart =
        fragmentStarts[static_cast<size_t>(fragmentDragSide)];
    fragmentDragInitialLength =
        fragmentLengths[static_cast<size_t>(fragmentDragSide)];
    fragmentDragInitialMouseX = position.x;
    repaint();
    return true;
}

void NewProjectAudioProcessorEditor::updateFragmentFromMouse(
    juce::Point<float> position)
{
    if (fragmentDragMode == FragmentDragMode::none
        || !juce::isPositiveAndBelow(
            selectedParticleIndex,
            static_cast<int>(particles.size())))
        return;

    const int totalSamples = loadedAudio.getNumSamples();
    const auto waveform = getFragmentWaveformBounds();
    if (totalSamples <= 0 || waveform.getWidth() <= 0.0f)
        return;

    const int deltaSamples = juce::roundToInt(
        (position.x - fragmentDragInitialMouseX)
        / waveform.getWidth()
        * static_cast<float>(totalSamples));
    const int minimumLength = juce::jmax(
        1, juce::jmin(64, totalSamples));
    int newStart = fragmentDragInitialStart;
    int newLength = fragmentDragInitialLength;

    if (fragmentDragMode == FragmentDragMode::start)
    {
        const int fixedEnd =
            fragmentDragInitialStart + fragmentDragInitialLength;
        const int startHandleMinimumLength =
            juce::jmin(minimumLength, fixedEnd);
        newStart = juce::jlimit(
            0,
            juce::jmax(0, fixedEnd - startHandleMinimumLength),
            fragmentDragInitialStart + deltaSamples);
        newLength = fixedEnd - newStart;
    }
    else if (fragmentDragMode == FragmentDragMode::end)
    {
        const int endHandleMinimumLength = juce::jmin(
            minimumLength,
            totalSamples - fragmentDragInitialStart);
        const int newEnd = juce::jlimit(
            fragmentDragInitialStart + endHandleMinimumLength,
            totalSamples,
            fragmentDragInitialStart
                + fragmentDragInitialLength
                + deltaSamples);
        newLength = newEnd - fragmentDragInitialStart;
    }
    else if (fragmentDragMode == FragmentDragMode::move)
    {
        newStart = juce::jlimit(
            0,
            juce::jmax(0, totalSamples - fragmentDragInitialLength),
            fragmentDragInitialStart + deltaSamples);
    }

    const int sliceIndex =
        particles[static_cast<size_t>(selectedParticleIndex)].sliceIndex;
    audioProcessor.setSliceForSide(
        sliceIndex, fragmentDragSide, newStart, newLength);
    repaint();
}

void NewProjectAudioProcessorEditor::mouseDown(const juce::MouseEvent& e)
{
    if (isAnimating && e.mods.isRightButtonDown())
    {
        isErasingLines = true;
        setMouseCursor(SketchCursors::eraser());
        eraseLinesNear(e.position);
        repaint();
        return;
    }

    if (attackSlider.getBounds().contains(e.getPosition()) ||
        decaySlider.getBounds().contains(e.getPosition()) ||
        sustainSlider.getBounds().contains(e.getPosition()) ||
        releaseSlider.getBounds().contains(e.getPosition()) ||
        numParticlesSlider.getBounds().contains(e.getPosition()) ||
        pitchSlider.getBounds().contains(e.getPosition()) ||
        humanizeSlider.getBounds().contains(e.getPosition()) ||
        (collisionButton.isVisible() && collisionButton.getBounds().contains(e.getPosition())) ||
        (timeStopButton.isVisible() && timeStopButton.getBounds().contains(e.getPosition())) ||
        (clearLinesButton.isVisible() && clearLinesButton.getBounds().contains(e.getPosition())) ||
        gearButton.getBounds().contains(e.getPosition()) ||
        (toggleRecordingPanelButton.isVisible() && toggleRecordingPanelButton.getBounds().contains(e.getPosition())) ||
        recordingPanel.getBounds().contains(e.getPosition()))
        return;

    if (!isAnimating) return;
    auto pos = e.getPosition().toFloat();

    if (isTrajectoryArmed
        && e.mods.isLeftButtonDown()
        && !getFragmentEditorBounds().contains(pos))
    {
        if (!juce::isPositiveAndBelow(
                trajectoryTargetParticleIndex,
                static_cast<int>(particles.size())))
        {
            isTrajectoryArmed = false;
            trajectoryTargetParticleIndex = -1;
            return;
        }

        const auto& particle = particles[
            static_cast<size_t>(trajectoryTargetParticleIndex)];
        const float radius = particle.size * 0.5f;
        pos.x = juce::jlimit(
            8.0f + radius,
            getWidth() - 8.0f - radius,
            pos.x);
        pos.y = juce::jlimit(
            8.0f + radius,
            getHeight() - 8.0f - radius,
            pos.y);
        currentTrajectory.clear();
        currentTrajectory.push_back(pos);
        isDrawingTrajectory = true;
        repaint();
        return;
    }

    if (e.mods.isLeftButtonDown() && beginFragmentDrag(pos))
        return;

    for (int i = (int)particles.size() - 1; i >= 0; --i) {
        auto& p = particles[i];
        if (pos.x >= p.x && pos.x <= p.x + p.size && pos.y >= p.y && pos.y <= p.y + p.size) {
            if (e.mods.isMiddleButtonDown())
            {
                p.isReversed = !p.isReversed;
                audioProcessor.setSliceReversed(
                    p.sliceIndex, p.isReversed);
                syncParticlesToProcessor();
                repaint();
                return;
            }

            if (e.mods.isLeftButtonDown())
            {
                selectParticle(i);
                draggedParticleIndex = i;
                lastMousePos = pos;
                p.isThrown = false;
            }
            return;
        }
    }

    if (e.mods.isMiddleButtonDown())
        return;

    selectParticle(-1);
    currentStroke.type = currentWallType;
    currentStroke.points.clear();
    currentStroke.points.push_back(pos);
    isDrawing = true;
}

void NewProjectAudioProcessorEditor::mouseDrag(const juce::MouseEvent& e)
{
    if (isErasingLines || e.mods.isRightButtonDown())
    {
        isErasingLines = true;
        setMouseCursor(SketchCursors::eraser());
        eraseLinesNear(e.position);
        repaint();
        return;
    }

    if (isDrawingTrajectory)
    {
        if (!juce::isPositiveAndBelow(
                trajectoryTargetParticleIndex,
                static_cast<int>(particles.size())))
            return;

        auto pos = e.getPosition().toFloat();
        const auto& particle = particles[
            static_cast<size_t>(trajectoryTargetParticleIndex)];
        const float radius = particle.size * 0.5f;
        pos.x = juce::jlimit(
            8.0f + radius,
            getWidth() - 8.0f - radius,
            pos.x);
        pos.y = juce::jlimit(
            8.0f + radius,
            getHeight() - 8.0f - radius,
            pos.y);
        if (currentTrajectory.empty()
            || currentTrajectory.back().getDistanceFrom(pos) > 6.0f)
        {
            currentTrajectory.push_back(pos);
            repaint();
        }
        return;
    }

    if (fragmentDragMode != FragmentDragMode::none)
    {
        updateFragmentFromMouse(e.position);
        return;
    }

    if (draggedParticleIndex >= 0 && draggedParticleIndex < particles.size()) {
        auto pos = e.getPosition().toFloat(); auto& p = particles[draggedParticleIndex];
        const auto dragDelta = pos - lastMousePos;
        for (auto& trajectoryPoint : p.trajectory)
            trajectoryPoint += dragDelta;
        p.vx = juce::jlimit(-25.0f, 25.0f, pos.x - lastMousePos.x); p.vy = juce::jlimit(-25.0f, 25.0f, pos.y - lastMousePos.y);
        p.x = pos.x - p.size / 2.0f; p.y = pos.y - p.size / 2.0f; lastMousePos = pos;
        bool hitWall = false; int hitType = 0; float wallMargin = 6.0f;

        if (p.x <= wallMargin) { p.x = wallMargin; hitWall = true; hitType = 0; }
        else if (p.x + p.size >= getWidth() - wallMargin) { p.x = getWidth() - wallMargin - p.size; hitWall = true; hitType = 2; }
        if (p.y <= wallMargin) { p.y = wallMargin; hitWall = true; hitType = 1; }
        else if (p.y + p.size >= getHeight() - wallMargin) { p.y = getHeight() - wallMargin - p.size; hitWall = true; hitType = 3; }

        if (hitWall) {
            auto now = juce::Time::currentTimeMillis();
            if (now - p.lastHitTime > 150) { audioProcessor.playSlice(p.sliceIndex, hitType); p.lastHitTime = now; }
        }
    }
    else if (isDrawing) {
        auto pos = e.getPosition().toFloat();
        if (currentStroke.points.empty()
            || currentStroke.points.back().getDistanceFrom(pos) > 10.0f)
            currentStroke.points.push_back(pos);
    }
}

void NewProjectAudioProcessorEditor::mouseUp(const juce::MouseEvent& e)
{
    if (isErasingLines)
    {
        isErasingLines = false;
        setMouseCursor(juce::MouseCursor::NormalCursor);
        syncStrokesToProcessor();
        syncParticlesToProcessor();
        return;
    }

    if (isDrawingTrajectory)
    {
        if (juce::isPositiveAndBelow(
                trajectoryTargetParticleIndex,
                static_cast<int>(particles.size()))
            && currentTrajectory.size() > 1)
        {
            auto& particle = particles[
                static_cast<size_t>(trajectoryTargetParticleIndex)];
            particle.trajectory = currentTrajectory;
            particle.trajectoryDistance = 0.0f;
            particle.trajectoryForward = true;
            const auto centre = particle.trajectory.front();
            particle.x = centre.x - particle.size * 0.5f;
            particle.y = centre.y - particle.size * 0.5f;
            if (std::sqrt(
                    particle.vx * particle.vx
                    + particle.vy * particle.vy) < 0.6f)
            {
                particle.vx = 2.5f;
                particle.vy = 0.0f;
            }
        }

        currentTrajectory.clear();
        isDrawingTrajectory = false;
        isTrajectoryArmed = false;
        trajectoryTargetParticleIndex = -1;
        syncParticlesToProcessor();
        repaint();
        return;
    }

    if (fragmentDragMode != FragmentDragMode::none)
    {
        updateFragmentFromMouse(e.position);
        fragmentDragMode = FragmentDragMode::none;
        return;
    }

    if (draggedParticleIndex >= 0 && draggedParticleIndex < particles.size()) {
        particles[draggedParticleIndex].isThrown = !isTimeStopped;
        draggedParticleIndex = -1;
    }
    else if (isDrawing) {
        if (currentStroke.points.size() > 1)
            strokes.push_back(currentStroke);
        currentStroke.points.clear();
        currentStroke.type = currentWallType;
        isDrawing = false;
        syncStrokesToProcessor();
    }

    syncParticlesToProcessor();
}

void NewProjectAudioProcessorEditor::eraseLinesNear(
    juce::Point<float> position)
{
    const float radiusSquared = eraserRadius * eraserRadius;
    const auto isNearSegment =
        [position, radiusSquared](juce::Point<float> start,
                                  juce::Point<float> end)
        {
            const auto segment = end - start;
            const float lengthSquared =
                segment.getDistanceSquaredFromOrigin();

            if (lengthSquared < 0.0001f)
                return position.getDistanceSquaredFrom(start)
                       <= radiusSquared;

            float amount =
                (position - start).getDotProduct(segment)
                / lengthSquared;
            amount = juce::jlimit(0.0f, 1.0f, amount);

            return position.getDistanceSquaredFrom(
                       start + segment * amount)
                   <= radiusSquared;
        };

    std::vector<WallStroke> remainingStrokes;

    for (const auto& stroke : strokes)
    {
        WallStroke fragment;
        fragment.type = stroke.type;

        for (size_t pointIndex = 0;
             pointIndex + 1 < stroke.points.size();
             ++pointIndex)
        {
            const auto start = stroke.points[pointIndex];
            const auto end = stroke.points[pointIndex + 1];

            if (isNearSegment(start, end))
            {
                if (fragment.points.size() > 1)
                    remainingStrokes.push_back(std::move(fragment));
                fragment = {};
                fragment.type = stroke.type;
                continue;
            }

            if (fragment.points.empty())
                fragment.points.push_back(start);
            fragment.points.push_back(end);
        }

        if (fragment.points.size() > 1)
            remainingStrokes.push_back(std::move(fragment));
    }

    strokes = std::move(remainingStrokes);

    for (auto& particle : particles)
    {
        if (particle.trajectory.size() < 2)
            continue;

        bool erasedTrajectorySegment = false;
        std::vector<std::vector<juce::Point<float>>> fragments;
        std::vector<juce::Point<float>> fragment;

        for (size_t pointIndex = 0;
             pointIndex + 1 < particle.trajectory.size();
             ++pointIndex)
        {
            const auto start = particle.trajectory[pointIndex];
            const auto end = particle.trajectory[pointIndex + 1];

            if (isNearSegment(start, end))
            {
                erasedTrajectorySegment = true;
                if (fragment.size() > 1)
                    fragments.push_back(std::move(fragment));
                fragment.clear();
                continue;
            }

            if (fragment.empty())
                fragment.push_back(start);
            fragment.push_back(end);
        }

        if (!erasedTrajectorySegment)
            continue;

        if (fragment.size() > 1)
            fragments.push_back(std::move(fragment));

        if (fragments.empty())
        {
            particle.trajectory.clear();
            particle.trajectoryDistance = 0.0f;
            particle.trajectoryForward = true;
            continue;
        }

        const auto longestFragment = std::max_element(
            fragments.begin(),
            fragments.end(),
            [](const auto& first, const auto& second)
            {
                return getTrajectoryLength(first)
                       < getTrajectoryLength(second);
            });

        const juce::Point<float> particleCentre(
            particle.x + particle.size * 0.5f,
            particle.y + particle.size * 0.5f);
        particle.trajectory = std::move(*longestFragment);
        particle.trajectoryDistance = getNearestTrajectoryDistance(
            particle.trajectory,
            particleCentre);
    }
}

void NewProjectAudioProcessorEditor::mouseWheelMove(
    const juce::MouseEvent& event,
    const juce::MouseWheelDetails& wheel)
{
    if (!isAnimating || std::abs(wheel.deltaY) < 0.0001f)
    {
        juce::AudioProcessorEditor::mouseWheelMove(event, wheel);
        return;
    }

    if (event.mods.isCtrlDown())
    {
        const int brushCount = static_cast<int>(WallType::count);
        const int direction = wheel.deltaY > 0.0f ? 1 : -1;
        const int currentIndex = static_cast<int>(currentWallType);
        const int nextIndex =
            (currentIndex + direction + brushCount) % brushCount;
        currentWallType = static_cast<WallType>(nextIndex);
        currentStroke.type = currentWallType;
        audioProcessor.uiWallBrush = nextIndex;
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
