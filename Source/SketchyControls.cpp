#include "SketchyControls.h"

#include <array>
#include <cmath>
#include <functional>
#include <initializer_list>

namespace
{
const auto pencilColour = juce::Colour(0xff232529);

void addSketchyLine(juce::Path& path,
                    juce::Random& random,
                    juce::Point<float> start,
                    juce::Point<float> end,
                    float amount = 0.9f)
{
    const auto midpoint = (start + end) * 0.5f;
    const auto jitter = [&random, amount](juce::Point<float> point)
    {
        return point + juce::Point<float>(random.nextFloat() * amount * 2.0f - amount,
                                          random.nextFloat() * amount * 2.0f - amount);
    };

    path.startNewSubPath(jitter(start));
    path.lineTo(jitter(midpoint));
    path.lineTo(jitter(end));
}

void strokeSketch(juce::Graphics& graphics,
                  const std::function<void(juce::Path&, juce::Random&, float)>& draw,
                  int seed,
                  float alpha)
{
    for (int pass = 0; pass < 2; ++pass)
    {
        juce::Random random(seed + pass * 137);
        juce::Path path;
        draw(path, random, pass == 0 ? 0.55f : 0.95f);
        graphics.setColour(pencilColour.withAlpha(alpha * (pass == 0 ? 1.0f : 0.38f)));
        graphics.strokePath(path,
                            juce::PathStrokeType(pass == 0 ? 1.45f : 0.70f,
                                                 juce::PathStrokeType::curved,
                                                 juce::PathStrokeType::rounded));
    }
}

void drawHandDrawnLine(juce::Graphics& graphics,
                       juce::Point<float> start,
                       juce::Point<float> end,
                       int seed,
                       juce::Colour colour)
{
    juce::Random random(seed);
    const float dx = end.x - start.x;
    const float dy = end.y - start.y;
    const float length = std::sqrt(dx * dx + dy * dy);

    if (length < 2.0f)
        return;

    const int numSegments = juce::jmax(2, static_cast<int>(length / 15.0f));
    for (int stroke = 0; stroke < 2; ++stroke)
    {
        juce::Path path;
        path.startNewSubPath(start.x + random.nextFloat() * 1.2f - 0.6f,
                             start.y + random.nextFloat() * 1.2f - 0.6f);

        for (int segment = 1; segment <= numSegments; ++segment)
        {
            const float amount = static_cast<float>(segment) / numSegments;
            float px = start.x + amount * dx;
            float py = start.y + amount * dy;

            if (segment < numSegments)
            {
                const float perpendicularX = -dy / length;
                const float perpendicularY = dx / length;
                px += perpendicularX * (random.nextFloat() * 2.0f - 1.0f) * 1.1f;
                py += perpendicularY * (random.nextFloat() * 2.0f - 1.0f) * 1.1f;
            }
            path.lineTo(px, py);
        }

        graphics.setColour(colour.withAlpha(stroke == 0 ? 0.75f : 0.55f));
        graphics.strokePath(path,
                            juce::PathStrokeType(0.8f + random.nextFloat() * 0.4f,
                                                 juce::PathStrokeType::mitered,
                                                 juce::PathStrokeType::rounded));
    }
}

void drawRoughCircle(juce::Graphics& graphics,
                     juce::Point<float> centre,
                     float radius,
                     juce::Random& random,
                     float width)
{
    juce::Path path;
    constexpr int points = 36;
    for (int pointIndex = 0; pointIndex <= points; ++pointIndex)
    {
        const float angle = juce::MathConstants<float>::twoPi
                            * static_cast<float>(pointIndex) / points;
        const float roughRadius = radius + random.nextFloat() * 2.0f - 1.0f;
        const auto point = centre + juce::Point<float>(std::cos(angle), std::sin(angle))
                                        * roughRadius;
        if (pointIndex == 0)
            path.startNewSubPath(point);
        else
            path.lineTo(point);
    }

    graphics.strokePath(path,
                        juce::PathStrokeType(width,
                                             juce::PathStrokeType::mitered,
                                             juce::PathStrokeType::rounded));
}

void addCube(juce::Path& path,
             juce::Random& random,
             juce::Point<float> centre,
             float scale,
             bool facesRight,
             float jitter)
{
    const float direction = facesRight ? 1.0f : -1.0f;
    const auto point = [centre, scale](float x, float y)
    {
        return centre + juce::Point<float>(x * scale, y * scale);
    };

    const auto outerTop = point(-direction * 1.05f, -0.42f);
    const auto topPeak = point(-direction * 0.30f, -0.88f);
    const auto innerTop = point(direction * 0.76f, -0.42f);
    const auto middle = point(0.0f, -0.02f);
    const auto outerBottom = point(-direction * 0.78f, 0.64f);
    const auto bottom = point(direction * 0.05f, 0.98f);
    const auto innerBottom = point(direction * 0.76f, 0.54f);

    const std::array<std::pair<juce::Point<float>, juce::Point<float>>, 10> edges {{
        { outerTop, topPeak },
        { topPeak, innerTop },
        { innerTop, middle },
        { middle, outerTop },
        { outerTop, outerBottom },
        { outerBottom, bottom },
        { bottom, middle },
        { middle, innerBottom },
        { innerBottom, innerTop },
        { innerBottom, bottom }
    }};

    for (const auto& edge : edges)
        addSketchyLine(path, random, edge.first, edge.second, jitter);
}

void addFilledCube(juce::Path& path,
                   juce::Point<float> centre,
                   float scale,
                   bool facesRight)
{
    const float direction = facesRight ? 1.0f : -1.0f;
    const auto point = [centre, scale](float x, float y)
    {
        return centre + juce::Point<float>(x * scale, y * scale);
    };

    const auto outerTop = point(-direction * 1.05f, -0.42f);
    const auto topPeak = point(-direction * 0.30f, -0.88f);
    const auto innerTop = point(direction * 0.76f, -0.42f);
    const auto middle = point(0.0f, -0.02f);
    const auto outerBottom = point(-direction * 0.78f, 0.64f);
    const auto bottom = point(direction * 0.05f, 0.98f);
    const auto innerBottom = point(direction * 0.76f, 0.54f);

    const auto addFace = [&path](std::initializer_list<juce::Point<float>> points)
    {
        auto pointIterator = points.begin();
        path.startNewSubPath(*pointIterator++);
        while (pointIterator != points.end())
            path.lineTo(*pointIterator++);
        path.closeSubPath();
    };

    addFace({ outerTop, topPeak, innerTop, middle });
    addFace({ outerTop, middle, bottom, outerBottom });
    addFace({ middle, innerTop, innerBottom, bottom });
}

void addFilledBurst(juce::Path& path,
                    juce::Point<float> centre,
                    float scale)
{
    constexpr std::array<juce::Point<float>, 12> burst {{
        { 0.0f, -0.20f }, { -0.18f, -0.88f },
        { 0.12f, -0.48f }, { 0.34f, -1.18f },
        { 0.38f, -0.32f }, { 0.82f, -0.18f },
        { 0.34f, 0.08f }, { 0.48f, 0.82f },
        { 0.08f, 0.34f }, { -0.22f, 0.94f },
        { -0.30f, 0.24f }, { -0.82f, 0.10f }
    }};

    path.startNewSubPath(centre + burst.front() * scale);
    for (size_t index = 1; index < burst.size(); ++index)
        path.lineTo(centre + burst[index] * scale);
    path.closeSubPath();
}

juce::Image makeEraserImage()
{
    juce::Image image(juce::Image::ARGB, 32, 32, true);
    juce::Graphics graphics(image);

    graphics.setColour(juce::Colour(0xffeae6df));

    juce::Path body;
    body.startNewSubPath(5.0f, 19.0f);
    body.lineTo(18.0f, 5.0f);
    body.lineTo(28.0f, 14.0f);
    body.lineTo(15.0f, 28.0f);
    body.closeSubPath();

    graphics.fillPath(body);
    graphics.setColour(pencilColour);
    graphics.strokePath(body, juce::PathStrokeType(1.5f));
    graphics.drawLine(10.0f, 24.0f, 23.0f, 10.0f, 1.2f);
    graphics.drawLine(15.0f, 28.0f, 28.0f, 14.0f, 0.7f);

    return image;
}
}

namespace SketchCursors
{
juce::MouseCursor eraser()
{
    return juce::MouseCursor(makeEraserImage(), 5, 27);
}
}

SketchyArrowButton::SketchyArrowButton() : Button("Recording panel")
{
    setClickingTogglesState(true);
}

void SketchyArrowButton::paintButton(juce::Graphics& graphics,
                                     bool isMouseOverButton,
                                     bool)
{
    const auto bounds = getLocalBounds().toFloat().reduced(2.0f);
    const auto centre = bounds.getCentre();
    const float width = bounds.getWidth() * 0.25f;
    const float height = bounds.getHeight() * 0.40f;
    const float direction = getToggleState() ? 1.0f : -1.0f;
    const int seed = 777 + static_cast<int>(juce::Time::getMillisecondCounter() / 300);

    strokeSketch(graphics,
                 [=](juce::Path& path, juce::Random& random, float jitter)
                 {
                     addSketchyLine(path, random,
                                    { centre.x - direction * width, centre.y - height },
                                    { centre.x + direction * width, centre.y }, jitter);
                     addSketchyLine(path, random,
                                    { centre.x + direction * width, centre.y },
                                    { centre.x - direction * width, centre.y + height }, jitter);
                 },
                 seed, isMouseOverButton ? 1.0f : 0.72f);
}

SketchyGearButton::SketchyGearButton() : Button("Settings") {}

void SketchyGearButton::paintButton(juce::Graphics& graphics,
                                    bool isMouseOverButton,
                                    bool)
{
    const auto bounds = getLocalBounds().toFloat().reduced(2.0f);
    const float centreX = bounds.getCentreX();
    const float centreY = bounds.getCentreY();
    const float outer = bounds.getWidth() * 0.45f;
    const float inner = bounds.getWidth() * 0.25f;

    juce::Path gear;
    gear.addEllipse(centreX - inner, centreY - inner, inner * 2.0f, inner * 2.0f);
    gear.addEllipse(centreX - inner * 0.4f, centreY - inner * 0.4f,
                    inner * 0.8f, inner * 0.8f);
    for (int tooth = 0; tooth < 8; ++tooth)
    {
        const float angle = tooth * juce::MathConstants<float>::pi / 4.0f;
        gear.startNewSubPath(centreX + inner * std::cos(angle),
                             centreY + inner * std::sin(angle));
        gear.lineTo(centreX + outer * std::cos(angle),
                    centreY + outer * std::sin(angle));
    }

    juce::Path sketchyGear;
    juce::PathFlatteningIterator iterator(gear, juce::AffineTransform(), 2.0f);
    juce::Random random(1000
                        + static_cast<int>(juce::Time::getMillisecondCounter() / 300));
    bool firstPoint = true;
    while (iterator.next())
    {
        const float px = iterator.x2 + random.nextFloat() * 2.0f - 1.0f;
        const float py = iterator.y2 + random.nextFloat() * 2.0f - 1.0f;
        if (firstPoint)
        {
            sketchyGear.startNewSubPath(px, py);
            firstPoint = false;
        }
        else
        {
            sketchyGear.lineTo(px, py);
        }
    }

    graphics.setColour(pencilColour.withAlpha(isMouseOverButton ? 1.0f : 0.7f));
    graphics.strokePath(sketchyGear,
                        juce::PathStrokeType(1.5f,
                                             juce::PathStrokeType::mitered,
                                             juce::PathStrokeType::rounded));
}

SketchyStopwatchButton::SketchyStopwatchButton() : Button("Pause time")
{
    setClickingTogglesState(true);
}

void SketchyStopwatchButton::paintButton(juce::Graphics& graphics,
                                         bool isMouseOverButton,
                                         bool)
{
    const auto bounds = getLocalBounds().toFloat().reduced(3.0f);
    const float x = bounds.getX();
    const float y = bounds.getY();
    const float width = bounds.getWidth();
    const float height = bounds.getHeight();
    const float left = x + width * 0.25f;
    const float right = x + width * 0.62f;
    const float barWidth = width * 0.15f;
    const float top = y + height * 0.12f;
    const float bottom = y + height * 0.88f;
    const int seed = 2000 + static_cast<int>(juce::Time::getMillisecondCounter() / 300);

    if (getToggleState())
    {
        graphics.setColour(pencilColour.withAlpha(0.13f));
        graphics.fillRect(juce::Rectangle<float>(
            left, top, barWidth, bottom - top));
        graphics.fillRect(juce::Rectangle<float>(
            right, top, barWidth, bottom - top));
    }

    strokeSketch(graphics,
                 [=](juce::Path& path, juce::Random& random, float jitter)
                 {
                     addSketchyLine(path, random,
                                    { left, top },
                                    { left + barWidth, top }, jitter);
                     addSketchyLine(path, random,
                                    { left + barWidth, top },
                                    { left + barWidth, bottom }, jitter);
                     addSketchyLine(path, random,
                                    { left + barWidth, bottom },
                                    { left, bottom }, jitter);
                     addSketchyLine(path, random,
                                    { left, bottom },
                                    { left, top }, jitter);

                     addSketchyLine(path, random,
                                    { right, top },
                                    { right + barWidth, top }, jitter);
                     addSketchyLine(path, random,
                                    { right + barWidth, top },
                                    { right + barWidth, bottom }, jitter);
                     addSketchyLine(path, random,
                                    { right + barWidth, bottom },
                                    { right, bottom }, jitter);
                     addSketchyLine(path, random,
                                    { right, bottom },
                                    { right, top }, jitter);
                 },
                 seed, isMouseOverButton ? 1.0f : 0.72f);
}

SketchyBroomButton::SketchyBroomButton() : Button("Clear lines") {}

void SketchyBroomButton::paintButton(juce::Graphics& graphics,
                                     bool isMouseOverButton,
                                     bool isButtonDown)
{
    const auto bounds = getLocalBounds().toFloat().reduced(3.0f);
    const auto centre = bounds.getCentre();
    const float scale = juce::jmin(bounds.getWidth(), bounds.getHeight()) / 30.0f;
    const int seed = 3000 + static_cast<int>(juce::Time::getMillisecondCounter() / 300);

    strokeSketch(graphics,
                 [=](juce::Path& path, juce::Random& random, float jitter)
                 {
                     const auto transform = juce::AffineTransform::rotation(
                         juce::MathConstants<float>::pi / 4.0f, centre.x, centre.y);
                     const auto transformed = [transform](juce::Point<float> point)
                     {
                         return point.transformedBy(transform);
                     };

                     addSketchyLine(path, random,
                                    transformed({ centre.x - 1.5f * scale, centre.y - 12.0f * scale }),
                                    transformed({ centre.x - 1.5f * scale, centre.y + 1.0f * scale }), jitter);
                     addSketchyLine(path, random,
                                    transformed({ centre.x + 1.5f * scale, centre.y - 12.0f * scale }),
                                    transformed({ centre.x + 1.5f * scale, centre.y + 1.0f * scale }), jitter);
                     addSketchyLine(path, random,
                                    transformed({ centre.x - 6.0f * scale, centre.y + 1.0f * scale }),
                                    transformed({ centre.x + 6.0f * scale, centre.y + 1.0f * scale }), jitter);
                     addSketchyLine(path, random,
                                    transformed({ centre.x - 6.0f * scale, centre.y + 1.0f * scale }),
                                    transformed({ centre.x - 9.0f * scale, centre.y + 10.0f * scale }), jitter);
                     addSketchyLine(path, random,
                                    transformed({ centre.x - 9.0f * scale, centre.y + 10.0f * scale }),
                                    transformed({ centre.x + 9.0f * scale, centre.y + 10.0f * scale }), jitter);
                     addSketchyLine(path, random,
                                    transformed({ centre.x + 9.0f * scale, centre.y + 10.0f * scale }),
                                    transformed({ centre.x + 6.0f * scale, centre.y + 1.0f * scale }), jitter);
                     for (int i = -2; i <= 2; ++i)
                         addSketchyLine(path, random,
                                        transformed({ centre.x + i * 3.0f * scale, centre.y + 3.0f * scale }),
                                        transformed({ centre.x + i * 3.5f * scale, centre.y + 10.0f * scale }),
                                        jitter);
                 },
                 seed, (isMouseOverButton || isButtonDown) ? 1.0f : 0.72f);
}

SketchyCollisionButton::SketchyCollisionButton() : Button("Cube collisions")
{
    setClickingTogglesState(true);
    setTooltip("Cube collisions");
}

void SketchyCollisionButton::paintButton(juce::Graphics& graphics,
                                         bool isMouseOverButton,
                                         bool isButtonDown)
{
    const auto bounds = getLocalBounds().toFloat().reduced(4.0f);
    const auto centre = bounds.getCentre();
    const float scale = juce::jmin(bounds.getHeight() * 0.36f,
                                   bounds.getWidth() * 0.13f);
    const float separation = scale * 1.45f;
    const float alpha = (isMouseOverButton || isButtonDown) ? 1.0f : 0.78f;
    const int seed = 4100 + static_cast<int>(juce::Time::getMillisecondCounter() / 300);

    if (getToggleState())
    {
        juce::Path activeFill;
        addFilledCube(activeFill,
                      centre + juce::Point<float>(-separation, 1.0f),
                      scale, true);
        addFilledCube(activeFill,
                      centre + juce::Point<float>(separation, 1.0f),
                      scale, false);
        addFilledBurst(activeFill, centre, scale * 0.60f);
        graphics.setColour(pencilColour.withAlpha(0.16f));
        graphics.fillPath(activeFill);
    }

    strokeSketch(graphics,
                 [=](juce::Path& path, juce::Random& random, float jitter)
                 {
                     addCube(path, random,
                             centre + juce::Point<float>(-separation, 1.0f),
                             scale, true, jitter);
                     addCube(path, random,
                             centre + juce::Point<float>(separation, 1.0f),
                             scale, false, jitter);

                     constexpr std::array<juce::Point<float>, 12> burst {{
                         { 0.0f, -0.20f }, { -0.18f, -0.88f },
                         { 0.12f, -0.48f }, { 0.34f, -1.18f },
                         { 0.38f, -0.32f }, { 0.82f, -0.18f },
                         { 0.34f, 0.08f }, { 0.48f, 0.82f },
                         { 0.08f, 0.34f }, { -0.22f, 0.94f },
                         { -0.30f, 0.24f }, { -0.82f, 0.10f }
                     }};

                     for (size_t i = 0; i < burst.size(); ++i)
                     {
                         const auto start = centre + burst[i] * (scale * 0.60f);
                         const auto end = centre + burst[(i + 1) % burst.size()] * (scale * 0.60f);
                         addSketchyLine(path, random, start, end, jitter * 0.75f);
                     }

                     const std::array<std::pair<juce::Point<float>, juce::Point<float>>, 4> shards {{
                         { { -0.42f, -1.20f }, { -0.62f, -1.62f } },
                         { { 0.55f, -1.18f }, { 0.72f, -1.48f } },
                         { { -0.50f, 1.15f }, { -0.70f, 1.48f } },
                         { { 0.52f, 1.08f }, { 0.72f, 1.42f } }
                     }};

                     for (const auto& shard : shards)
                         addSketchyLine(path, random,
                                        centre + shard.first * scale,
                                        centre + shard.second * scale,
                                        jitter);
                 },
                 seed, alpha);
}

void SketchyLookAndFeel::drawRotarySlider(juce::Graphics& graphics,
                                          int x, int y, int width, int height,
                                          float sliderPos,
                                          float rotaryStartAngle,
                                          float rotaryEndAngle,
                                          juce::Slider& slider)
{
    const auto bounds = juce::Rectangle<int>(x, y, width, height).toFloat().reduced(3.0f);
    const float radius = juce::jmin(bounds.getWidth(), bounds.getHeight()) * 0.5f;
    const auto centre = bounds.getCentre();
    const float angle = rotaryStartAngle + sliderPos * (rotaryEndAngle - rotaryStartAngle);
    juce::Random random(slider.getName().hashCode()
                        + static_cast<int>(juce::Time::getMillisecondCounter() / 400));
    graphics.setColour(pencilColour.withAlpha(0.82f));
    drawRoughCircle(graphics, centre, radius * 0.88f, random, 1.25f);
    drawRoughCircle(graphics, centre + juce::Point<float>(0.4f, -0.3f),
                    radius * 0.83f, random, 0.7f);

    const auto indicator = centre
                           + juce::Point<float>(std::sin(angle), -std::cos(angle))
                                 * (radius * 0.74f);
    drawHandDrawnLine(graphics, centre, indicator,
                      slider.getName().hashCode() + random.nextInt(), pencilColour);
}
