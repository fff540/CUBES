#include "RecordingPanel.h"

#include "PluginProcessor.h"

#include <cmath>
#include <functional>

namespace
{
const auto pencilColour = juce::Colour(0xff232529);

class RecordingExportJob final : public juce::ThreadPoolJob
{
public:
    using Completion = std::function<void(
        const juce::File&, juce::uint64, bool)>;

    RecordingExportJob(NewProjectAudioProcessor& processor,
                       int sourceCapacity,
                       int sourceWritePosition,
                       int sourceStartOffset,
                       int outputLength,
                       double outputSampleRate,
                       const juce::File& destination,
                       juce::uint64 requestGeneration,
                       Completion completionCallback)
        : ThreadPoolJob("Cubes recording export"),
          audioProcessor(processor),
          capacity(sourceCapacity),
          writePosition(sourceWritePosition),
          startOffset(sourceStartOffset),
          length(outputLength),
          sampleRate(outputSampleRate),
          outputFile(destination),
          generation(requestGeneration),
          completion(std::move(completionCallback))
    {
    }

    JobStatus runJob() override
    {
        bool succeeded = false;

        if (capacity > 0
            && length > 0
            && sampleRate > 0.0
            && audioProcessor.rollingBuffer.getNumChannels() > 0)
        {
            juce::AudioBuffer<float> exportBuffer(2, length);
            int readStart =
                (writePosition - capacity + startOffset) % capacity;
            if (readStart < 0)
                readStart += capacity;

            for (int sample = 0; sample < length; ++sample)
            {
                if ((sample & 4095) == 0 && shouldExit())
                {
                    outputFile.deleteFile();
                    return jobHasFinished;
                }

                const int readPosition =
                    (readStart + sample) % capacity;
                exportBuffer.setSample(
                    0, sample,
                    audioProcessor.rollingBuffer.getSample(
                        0, readPosition));
                exportBuffer.setSample(
                    1, sample,
                    audioProcessor.rollingBuffer.getSample(
                        juce::jmin(
                            1,
                            audioProcessor.rollingBuffer
                                    .getNumChannels()
                                - 1),
                        readPosition));
            }

            if (!shouldExit())
            {
                std::unique_ptr<juce::OutputStream> outputStream =
                    outputFile.createOutputStream();
                if (outputStream != nullptr)
                {
                    juce::WavAudioFormat wavFormat;
                    const auto options =
                        juce::AudioFormatWriterOptions()
                            .withSampleRate(sampleRate)
                            .withNumChannels(2)
                            .withBitsPerSample(16);
                    auto writer = wavFormat.createWriterFor(
                        outputStream, options);

                    if (writer != nullptr)
                    {
                        succeeded =
                            writer->writeFromAudioSampleBuffer(
                                exportBuffer, 0, length);
                        writer.reset();
                    }
                }
            }
        }

        if (shouldExit())
        {
            outputFile.deleteFile();
            return jobHasFinished;
        }

        succeeded = succeeded
            && outputFile.existsAsFile()
            && outputFile.getSize() > 44;
        completion(outputFile, generation, succeeded);
        return jobHasFinished;
    }

private:
    NewProjectAudioProcessor& audioProcessor;
    const int capacity;
    const int writePosition;
    const int startOffset;
    const int length;
    const double sampleRate;
    const juce::File outputFile;
    const juce::uint64 generation;
    Completion completion;
};
}

RecordingPanelView::RecordingPanelView(NewProjectAudioProcessor& processor)
    : audioProcessor(processor)
{
}

RecordingPanelView::~RecordingPanelView()
{
    ++exportGeneration;
    exportThreadPool.removeAllJobs(true, 10000);
    clearPreparedExport();
}

void RecordingPanelView::paint(juce::Graphics& graphics)
{
    const auto bounds = getLocalBounds().toFloat().reduced(1.0f);
    graphics.setColour(juce::Colour(0xffeae5dc));
    graphics.fillRect(bounds);

    juce::Random random(999 + static_cast<int>(juce::Time::getMillisecondCounter() / 300));
    auto addSketchyLine = [&random](juce::Path& path,
                                    juce::Point<float> start,
                                    juce::Point<float> end)
    {
        path.startNewSubPath(start);
        const float length = start.getDistanceFrom(end);
        const int steps = juce::jmax(2, static_cast<int>(length / 15.0f));
        for (int i = 1; i < steps; ++i)
        {
            const float amount = static_cast<float>(i) / steps;
            auto point = start + (end - start) * amount;
            point += { random.nextFloat() * 2.0f - 1.0f,
                       random.nextFloat() * 2.0f - 1.0f };
            path.lineTo(point);
        }
        path.lineTo(end);
    };

    juce::Path border;
    addSketchyLine(border, bounds.getTopLeft(), bounds.getTopRight());
    addSketchyLine(border, bounds.getTopRight(), bounds.getBottomRight());
    addSketchyLine(border, bounds.getBottomRight(), bounds.getBottomLeft());
    addSketchyLine(border, bounds.getBottomLeft(), bounds.getTopLeft());
    graphics.setColour(pencilColour.withAlpha(0.85f));
    graphics.strokePath(border,
                        juce::PathStrokeType(1.6f,
                                             juce::PathStrokeType::curved,
                                             juce::PathStrokeType::rounded));

    const int capacity = audioProcessor.rollingCapacity;
    const int width = getWidth();
    if (capacity > 0 && width > 0)
    {
        const int writePosition = audioProcessor.rollingWritePos.load();
        const int validSamples = juce::jlimit(
            0, capacity, audioProcessor.rollingVisualSamples.load());
        const int firstValidOffset = capacity - validSamples;
        const float centreY = getHeight() * 0.5f;
        juce::Path waveform;
        bool pathStarted = false;

        for (int pixel = 0; pixel < width; ++pixel)
        {
            const int startOffset = static_cast<int>(
                (static_cast<double>(pixel) / width) * capacity);
            const int endOffset = juce::jmin(
                capacity,
                static_cast<int>((static_cast<double>(pixel + 1) / width) * capacity));

            if (endOffset <= firstValidOffset)
                continue;

            float peak = 0.0f;
            const int firstOffset = juce::jmax(firstValidOffset, startOffset);
            const int stride = juce::jmax(1, (endOffset - firstOffset) / 12);
            for (int offset = firstOffset; offset < endOffset; offset += stride)
            {
                int readPosition = (writePosition - capacity + offset) % capacity;
                if (readPosition < 0)
                    readPosition += capacity;
                peak = juce::jmax(
                    peak,
                    std::abs(audioProcessor.rollingBuffer.getSample(0, readPosition)));
            }

            const float top = centreY - peak * getHeight() * 0.40f;
            const float bottom = centreY + peak * getHeight() * 0.40f;
            if (!pathStarted)
            {
                waveform.startNewSubPath(static_cast<float>(pixel), centreY);
                pathStarted = true;
            }
            waveform.lineTo(static_cast<float>(pixel), top);
            waveform.lineTo(static_cast<float>(pixel), bottom);
        }

        graphics.setColour(pencilColour.withAlpha(0.52f));
        graphics.strokePath(waveform, juce::PathStrokeType(1.0f));
    }

    if (isSelecting || hasSelection)
    {
        const float left = juce::jmin(selectionStartX, selectionEndX);
        const float right = juce::jmax(selectionStartX, selectionEndX);
        graphics.setColour(juce::Colour(0xb0b8c4d0).withAlpha(0.4f));
        graphics.fillRect(left, 1.0f, right - left,
                          static_cast<float>(getHeight()) - 2.0f);
        graphics.setColour(pencilColour.withAlpha(0.8f));
        graphics.drawVerticalLine(juce::roundToInt(left), 1.0f,
                                  static_cast<float>(getHeight()) - 1.0f);
        graphics.drawVerticalLine(juce::roundToInt(right), 1.0f,
                                  static_cast<float>(getHeight()) - 1.0f);
    }
}

void RecordingPanelView::mouseDown(const juce::MouseEvent& event)
{
    if (hasSelection)
    {
        const float left = juce::jmin(selectionStartX, selectionEndX);
        const float right = juce::jmax(selectionStartX, selectionEndX);
        if (event.x >= left && event.x <= right)
        {
            isDraggingOut = true;
            dragRequested = false;
            return;
        }
    }

    ++exportGeneration;
    exportIsPreparing = false;
    dragRequested = false;
    clearPreparedExport();
    isSelecting = true;
    hasSelection = false;
    isDraggingOut = false;
    selectionStartX = static_cast<float>(event.x);
    selectionEndX = selectionStartX;
    repaint();
}

void RecordingPanelView::mouseDrag(const juce::MouseEvent& event)
{
    if (isDraggingOut && hasSelection)
    {
        if (auto* container = juce::DragAndDropContainer::findParentDragContainerFor(this))
            exportAndDrag(container);
        isDraggingOut = false;
        return;
    }

    if (isSelecting)
    {
        selectionEndX = juce::jlimit(0.0f, static_cast<float>(getWidth()),
                                     static_cast<float>(event.x));
        repaint();
    }
}

void RecordingPanelView::mouseUp(const juce::MouseEvent&)
{
    if (isSelecting)
    {
        isSelecting = false;
        hasSelection = std::abs(selectionEndX - selectionStartX) > 2.0f;
        repaint();
    }
    isDraggingOut = false;
    dragRequested = false;
}

void RecordingPanelView::exportAndDrag(juce::DragAndDropContainer* dragContainer)
{
    juce::ignoreUnused(dragContainer);

    if (preparedExportFile.existsAsFile())
    {
        startPreparedDrag();
        return;
    }

    dragRequested = true;
    if (!exportIsPreparing)
        prepareExportAsync();
}

void RecordingPanelView::prepareExportAsync()
{
    const float left = juce::jmin(selectionStartX, selectionEndX);
    const float right = juce::jmax(selectionStartX, selectionEndX);
    const int capacity = audioProcessor.rollingCapacity;

    if (capacity <= 0 || getWidth() <= 0 || right - left < 1.0f)
    {
        exportIsPreparing = false;
        return;
    }

    const int writePosition = audioProcessor.rollingWritePos.load();
    const int validSamples = juce::jlimit(
        0, capacity, audioProcessor.rollingVisualSamples.load());
    const int firstValidOffset = capacity - validSamples;
    const int startOffset = juce::jmax(
        firstValidOffset,
        static_cast<int>((left / static_cast<float>(getWidth())) * capacity));
    const int endOffset = juce::jmax(
        firstValidOffset,
        static_cast<int>((right / static_cast<float>(getWidth())) * capacity));
    const int length = endOffset - startOffset;

    if (length <= 0)
    {
        exportIsPreparing = false;
        return;
    }

    ++exportGeneration;
    const auto generation = exportGeneration;
    exportIsPreparing = true;
    clearPreparedExport();

    const auto exportDirectory =
        juce::File::getSpecialLocation(
            juce::File::userDocumentsDirectory)
            .getChildFile("Cubes")
            .getChildFile("Recordings");
    const auto directoryResult = exportDirectory.createDirectory();
    if (directoryResult.failed())
    {
        exportIsPreparing = false;
        dragRequested = false;
        return;
    }

    const auto timestamp =
        juce::Time::getCurrentTime().formatted(
            "%Y-%m-%d_%H-%M-%S");
    const auto outputFile =
        exportDirectory.getNonexistentChildFile(
            "CubesRecording_" + timestamp,
            ".wav",
            false);
    const juce::Component::SafePointer<RecordingPanelView> safeThis(this);
    auto completion = [safeThis](
                          const juce::File& file,
                          juce::uint64 completedGeneration,
                          bool succeeded)
    {
        juce::MessageManager::callAsync(
            [safeThis, file, completedGeneration, succeeded]()
            {
                if (safeThis != nullptr)
                    safeThis->handleExportFinished(
                        file, completedGeneration, succeeded);
                else
                    file.deleteFile();
            });
    };

    exportThreadPool.addJob(
        new RecordingExportJob(
            audioProcessor,
            capacity,
            writePosition,
            startOffset,
            length,
            audioProcessor.currentSampleRate,
            outputFile,
            generation,
            std::move(completion)),
        true);
}

void RecordingPanelView::handleExportFinished(
    const juce::File& file,
    juce::uint64 generation,
    bool succeeded)
{
    if (generation != exportGeneration)
    {
        file.deleteFile();
        return;
    }

    exportIsPreparing = false;
    if (!succeeded)
    {
        file.deleteFile();
        return;
    }

    preparedExportFile = file;
    if (dragRequested)
        startPreparedDrag();
}

void RecordingPanelView::startPreparedDrag()
{
    if (!preparedExportFile.existsAsFile())
        return;

    const auto file = preparedExportFile;
    juce::StringArray files;
    files.add(file.getFullPathName());

    const bool started =
        juce::DragAndDropContainer::performExternalDragDropOfFiles(
            files,
            false,
            this,
            []() {});

    dragRequested = false;
    if (started)
        preparedExportFile = {};
}

void RecordingPanelView::clearPreparedExport()
{
    if (preparedExportFile.existsAsFile())
        preparedExportFile.deleteFile();
    preparedExportFile = {};
}

