#include "Branding.h"

#include <array>

namespace
{
const auto pencilColour = juce::Colour(0xff232529);

juce::Point<float> jittered(juce::Random& random,
                            juce::Point<float> point,
                            float amount)
{
    return point + juce::Point<float>(random.nextFloat() * amount * 2.0f - amount,
                                      random.nextFloat() * amount * 2.0f - amount);
}

void addCube(juce::Path& path,
             juce::Random& random,
             juce::Rectangle<float> front,
             juce::Point<float> depth,
             float jitter)
{
    const auto topLeft = front.getTopLeft();
    const auto topRight = front.getTopRight();
    const auto bottomRight = front.getBottomRight();
    const auto bottomLeft = front.getBottomLeft();

    const auto backTopLeft = topLeft + depth;
    const auto backTopRight = topRight + depth;
    const auto backBottomRight = bottomRight + depth;

    const std::array<std::pair<juce::Point<float>, juce::Point<float>>, 9> lines {{
        { topLeft, topRight },
        { topRight, bottomRight },
        { bottomRight, bottomLeft },
        { bottomLeft, topLeft },
        { topLeft, backTopLeft },
        { topRight, backTopRight },
        { bottomRight, backBottomRight },
        { backTopLeft, backTopRight },
        { backTopRight, backBottomRight }
    }};

    for (const auto& line : lines)
    {
        path.startNewSubPath(jittered(random, line.first, jitter));
        path.lineTo(jittered(random, line.second, jitter));
    }
}
}

namespace cubes::ui
{
void drawBranding(juce::Graphics& graphics, int frameCount)
{
    const auto seedOffset = frameCount / 12;

    for (int pass = 0; pass < 2; ++pass)
    {
        juce::Random random(5197 + seedOffset + pass * 131);
        juce::Path cubes;

        addCube(cubes, random, { 18.0f, 20.0f, 20.0f, 18.0f },
                { 7.0f, -5.0f }, pass == 0 ? 0.40f : 0.65f);
        addCube(cubes, random, { 31.0f, 30.0f, 20.0f, 18.0f },
                { 7.0f, -5.0f }, pass == 0 ? 0.40f : 0.65f);

        graphics.setColour(pencilColour.withAlpha(pass == 0 ? 0.72f : 0.28f));
        graphics.strokePath(cubes,
                            juce::PathStrokeType(pass == 0 ? 1.05f : 0.65f,
                                                 juce::PathStrokeType::curved,
                                                 juce::PathStrokeType::rounded));
    }

    graphics.setColour(pencilColour.withAlpha(0.72f));
    graphics.setFont(juce::FontOptions(15.5f));
    graphics.drawText("C U B E S", 66, 18, 150, 25,
                      juce::Justification::centredLeft, false);
}
}
