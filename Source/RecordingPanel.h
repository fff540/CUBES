#pragma once

#include <JuceHeader.h>

class NewProjectAudioProcessor;

class RecordingPanelView final : public juce::Component
{
public:
    explicit RecordingPanelView(NewProjectAudioProcessor&);
    ~RecordingPanelView() override;

    void paint(juce::Graphics&) override;
    void mouseDown(const juce::MouseEvent&) override;
    void mouseDrag(const juce::MouseEvent&) override;
    void mouseUp(const juce::MouseEvent&) override;

private:
    void exportAndDrag(juce::DragAndDropContainer*);
    void prepareExportAsync();
    void handleExportFinished(
        const juce::File&, juce::uint64 generation, bool succeeded);
    void startPreparedDrag();
    void clearPreparedExport();

    NewProjectAudioProcessor& audioProcessor;
    bool isSelecting = false;
    bool hasSelection = false;
    bool isDraggingOut = false;
    float selectionStartX = 0.0f;
    float selectionEndX = 0.0f;
    juce::ThreadPool exportThreadPool{ 1 };
    juce::File preparedExportFile;
    juce::uint64 exportGeneration = 0;
    bool exportIsPreparing = false;
    bool dragRequested = false;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(RecordingPanelView)
};

