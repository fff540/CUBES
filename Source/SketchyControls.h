Exit code: 0
Wall time: 5.3 seconds
Output:
#pragma once

#include <JuceHeader.h>

namespace SketchCursors
{
juce::MouseCursor eraser();
}

class SketchyArrowButton final : public juce::Button
{
public:
    SketchyArrowButton();
    void paintButton(juce::Graphics&, bool isMouseOverButton, bool isButtonDown) override;
};

class SketchyGearButton final : public juce::Button
{
public:
    SketchyGearButton();
    void paintButton(juce::Graphics&, bool isMouseOverButton, bool isButtonDown) override;
};

class SketchyStopwatchButton final : public juce::Button
{
public:
    SketchyStopwatchButton();
    void paintButton(juce::Graphics&, bool isMouseOverButton, bool isButtonDown) override;
};

class SketchyBroomButton final : public juce::Button
{
public:
    SketchyBroomButton();
    void paintButton(juce::Graphics&, bool isMouseOverButton, bool isButtonDown) override;
};

class SketchyCollisionButton final : public juce::Button
{
public:
    SketchyCollisionButton();
    void paintButton(juce::Graphics&, bool isMouseOverButton, bool isButtonDown) override;
};

class SketchyLookAndFeel final : public juce::LookAndFeel_V4
{
public:
    void drawRotarySlider(juce::Graphics&, int x, int y, int width, int height,
                          float sliderPos, float rotaryStartAngle,
                          float rotaryEndAngle, juce::Slider&) override;
};

