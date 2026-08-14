#pragma once

#include <JuceHeader.h>

#include "PluginProcessor.h"
#include "RecordingPanel.h"
#include "SketchyControls.h"

#include <functional>
#include <vector>

struct Particle
{
    int sliceIndex = 0;
    float x = 0.0f;
    float y = 0.0f;
    float vx = 0.0f;
    float vy = 0.0f;
    float size = 0.0f;
    juce::Colour color;
    juce::int64 lastHitTime = 0;
    bool isThrown = false;
    int pitchSemitones = 0;
    bool isReversed = false;
    std::vector<juce::Point<float>> trajectory;
    float trajectoryDistance = 0.0f;
    bool trajectoryForward = true;
};

class ResampleModeSlider final : public juce::Slider
{
public:
    std::function<void()> onRightClick;

    void mouseDown(const juce::MouseEvent& event) override
    {
        if (event.mods.isRightButtonDown())
        {
            if (onRightClick)
                onRightClick();
            return;
        }

        leftDragInProgress = event.mods.isLeftButtonDown();
        juce::Slider::mouseDown(event);
    }

    void mouseDrag(const juce::MouseEvent& event) override
    {
        if (leftDragInProgress)
            juce::Slider::mouseDrag(event);
    }

    void mouseUp(const juce::MouseEvent& event) override
    {
        if (leftDragInProgress)
        {
            juce::Slider::mouseUp(event);
            leftDragInProgress = false;
        }
    }

private:
    bool leftDragInProgress = false;
};

class NewProjectAudioProcessorEditor final : public juce::AudioProcessorEditor,
                                             public juce::FileDragAndDropTarget,
                                             public juce::DragAndDropContainer,
                                             private juce::Timer
{
public:
    explicit NewProjectAudioProcessorEditor(NewProjectAudioProcessor&);
    ~NewProjectAudioProcessorEditor() override;

    void paint(juce::Graphics&) override;
    void resized() override;

    bool isInterestedInFileDrag(const juce::StringArray& files) override;
    void filesDropped(const juce::StringArray& files, int x, int y) override;

    void mouseDown(const juce::MouseEvent&) override;
    void mouseDrag(const juce::MouseEvent&) override;
    void mouseUp(const juce::MouseEvent&) override;
    void mouseWheelMove(const juce::MouseEvent&,
                        const juce::MouseWheelDetails&) override;

private:
    void generateParticles();
    void updateParticleCount();
    void updateSettingsVisibility();
    void eraseLinesNear(juce::Point<float> position);
    void syncParticlesToProcessor();
    void syncStrokesToProcessor();
    bool restoreSavedEditorState();
    void selectParticle(int particleIndex);
    juce::Rectangle<float> getFragmentEditorBounds() const;
    juce::Rectangle<float> getFragmentWaveformBounds() const;
    juce::Rectangle<float> getTrajectoryButtonBounds() const;
    void drawFragmentEditor(juce::Graphics&);
    bool beginFragmentDrag(juce::Point<float> position);
    void updateFragmentFromMouse(juce::Point<float> position);
    void timerCallback() override;

    NewProjectAudioProcessor& audioProcessor;

    SketchyGearButton gearButton;
    SketchyStopwatchButton timeStopButton;
    SketchyBroomButton clearLinesButton;
    SketchyCollisionButton collisionButton;
    SketchyArrowButton toggleRecordingPanelButton;
    RecordingPanelView recordingPanel;

    float recordingPanelAnimationTarget = 0.0f;
    float recordingPanelCurrentPos = 0.0f;

    juce::Slider attackSlider;
    juce::Slider decaySlider;
    juce::Slider sustainSlider;
    juce::Slider releaseSlider;
    juce::Slider numParticlesSlider;
    ResampleModeSlider pitchSlider;
    juce::Slider randSlider;
    juce::Slider humanizeSlider;

    std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment> attackAttach;
    std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment> decayAttach;
    std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment> sustainAttach;
    std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment> releaseAttach;
    std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment> numParticlesAttach;
    std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment> pitchAttach;
    std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment> randAttach;
    std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment> humanizeAttach;

    SketchyLookAndFeel sketchyLnF;

    juce::AudioFormatManager formatManager;
    juce::AudioBuffer<float> loadedAudio;

    bool isAnimating = false;
    bool isTimeStopped = false;
    int frameCount = 0;

    int draggedParticleIndex = -1;
    int selectedParticleIndex = -1;
    juce::Point<float> lastMousePos;
    juce::Rectangle<float> fragmentEditorBounds;
    enum class FragmentDragMode
    {
        none,
        start,
        end,
        move
    };
    FragmentDragMode fragmentDragMode = FragmentDragMode::none;
    int activeFragmentSide = 0;
    int fragmentDragSide = 0;
    int fragmentDragInitialStart = 0;
    int fragmentDragInitialLength = 0;
    float fragmentDragInitialMouseX = 0.0f;

    using WallStroke = NewProjectAudioProcessor::SavedStrokeState;
    using WallType = NewProjectAudioProcessor::WallType;

    std::vector<Particle> particles;
    std::vector<WallStroke> strokes;
    WallStroke currentStroke;
    bool isDrawing = false;
    bool isErasingLines = false;
    float eraserRadius = 18.0f;
    WallType currentWallType = WallType::normal;
    juce::int64 wallBrushHintUntil = 0;
    juce::Point<float> wallBrushHintPosition;

    bool isTrajectoryArmed = false;
    bool isDrawingTrajectory = false;
    int trajectoryTargetParticleIndex = -1;
    std::vector<juce::Point<float>> currentTrajectory;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(NewProjectAudioProcessorEditor)
};
