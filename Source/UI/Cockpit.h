// Revheadz-style cockpit widgets: chrome buttons, lamps, tachometer (rim spins in the centre), pressure pedal.
#pragma once
#include <juce_audio_utils/juce_audio_utils.h>
#include <functional>
#include "../Drivetrain.h"

namespace f1nz::ui
{
inline void drawRimImage (juce::Graphics& g, const juce::Image& img, float cx, float cy, float d, float angle, float blur)
{
    if (! img.isValid()) return;
    const float s = d / (float) img.getWidth();
    auto draw = [&] (float a, float alpha)
    {
        g.setOpacity (alpha);
        g.drawImageTransformed (img, juce::AffineTransform::translation (-0.5f * (float) img.getWidth(), -0.5f * (float) img.getHeight())
                                         .scaled (s).rotated (a).translated (cx, cy));
    };
    if (blur > 0.3f) { draw (angle - blur * 0.5f, 0.35f); draw (angle, 0.45f); draw (angle + blur * 0.5f, 0.35f); }
    else draw (angle, 1.f);
    g.setOpacity (1.f);
}

inline void drawLamp (juce::Graphics& g, juce::Point<float> c, float r, juce::Colour col, bool on)
{
    if (on)
        for (int i = 3; i >= 1; --i)
        {
            g.setColour (col.withAlpha (0.10f * (float) (4 - i)));
            g.fillEllipse (c.x - r - 5.f * (float) i, c.y - r - 5.f * (float) i, 2.f * (r + 5.f * (float) i), 2.f * (r + 5.f * (float) i));
        }
    g.setGradientFill (juce::ColourGradient (juce::Colour (0xffdddddd), c.x, c.y - r, juce::Colour (0xff444444), c.x, c.y + r, false));
    g.fillEllipse (c.x - r, c.y - r, 2 * r, 2 * r);
    const float ri = r * 0.78f;
    const auto base = on ? col : col.darker (0.85f);
    g.setGradientFill (juce::ColourGradient (base.brighter (on ? 0.7f : 0.1f), c.x - ri * 0.3f, c.y - ri * 0.4f, base.darker (0.5f), c.x + ri, c.y + ri, true));
    g.fillEllipse (c.x - ri, c.y - ri, 2 * ri, 2 * ri);
}

// ------------------------------------------------------------------ round chrome button
class ChromeButton : public juce::Button
{
public:
    enum Icon { Left, Right, Up, Down, Power, Brake, Text };
    explicit ChromeButton (Icon i, const juce::String& text = {}) : juce::Button (text), icon (i) { setMouseClickGrabsKeyboardFocus (false); }
    bool lit = false;
    juce::Colour litColour { 0xff9dff7a };
    std::function<void (bool)> onHold;               // momentary buttons (brake)

    void paintButton (juce::Graphics& g, bool over, bool down) override
    {
        auto b = getLocalBounds().toFloat().reduced (2.f);
        const float d = juce::jmin (b.getWidth(), b.getHeight());
        juce::Rectangle<float> r (d, d); r.setCentre (b.getCentre());
        g.setColour (juce::Colours::black.withAlpha (0.45f)); g.fillEllipse (r.translated (0.f, d * 0.04f));
        g.setGradientFill (juce::ColourGradient (juce::Colour (0xfff0f0f0), r.getX(), r.getY(), juce::Colour (0xff3c3c3c), r.getRight(), r.getBottom(), false));
        g.fillEllipse (r);
        auto in = r.reduced (d * 0.08f);
        g.setGradientFill (juce::ColourGradient (down ? juce::Colour (0xff4a4a4a) : (over ? juce::Colour (0xffb4b4b4) : juce::Colour (0xffa0a0a0)),
                                                 in.getCentreX(), in.getY(), juce::Colour (0xff262626), in.getCentreX(), in.getBottom(), false));
        g.fillEllipse (in);
        const auto glyph = (icon == Power && lit) ? litColour : juce::Colour (0xffe4e4e4).withAlpha (isEnabled() ? 1.f : 0.4f);
        g.setColour (glyph);
        const auto c = in.getCentre(); const float u = in.getWidth() * 0.5f;
        juce::Path p;
        switch (icon)
        {
            case Up:    p.addTriangle (c.x, c.y - u * 0.5f, c.x - u * 0.5f, c.y + u * 0.4f, c.x + u * 0.5f, c.y + u * 0.4f); g.fillPath (p); break;
            case Down:  p.addTriangle (c.x, c.y + u * 0.5f, c.x - u * 0.5f, c.y - u * 0.4f, c.x + u * 0.5f, c.y - u * 0.4f); g.fillPath (p); break;
            case Left:  p.addTriangle (c.x - u * 0.5f, c.y, c.x + u * 0.4f, c.y - u * 0.5f, c.x + u * 0.4f, c.y + u * 0.5f); g.fillPath (p); break;
            case Right: p.addTriangle (c.x + u * 0.5f, c.y, c.x - u * 0.4f, c.y - u * 0.5f, c.x - u * 0.4f, c.y + u * 0.5f); g.fillPath (p); break;
            case Power:
                p.addCentredArc (c.x, c.y, u * 0.45f, u * 0.45f, 0.f, 0.6f, juce::MathConstants<float>::twoPi - 0.6f, true);
                p.startNewSubPath (c.x, c.y - u * 0.55f); p.lineTo (c.x, c.y - u * 0.05f);
                g.strokePath (p, juce::PathStrokeType (u * 0.12f, juce::PathStrokeType::curved, juce::PathStrokeType::rounded)); break;
            case Brake:
                g.drawEllipse (c.x - u * 0.28f, c.y - u * 0.28f, u * 0.56f, u * 0.56f, u * 0.09f);
                p.addCentredArc (c.x, c.y, u * 0.5f, u * 0.5f, 0.f, 0.5f, 1.2f, true);
                p.addCentredArc (c.x, c.y, u * 0.5f, u * 0.5f, 0.f, 2.1f, 2.8f, true);
                p.addCentredArc (c.x, c.y, u * 0.5f, u * 0.5f, 0.f, 3.6f, 4.3f, true);
                p.addCentredArc (c.x, c.y, u * 0.5f, u * 0.5f, 0.f, 5.1f, 5.8f, true);
                g.strokePath (p, juce::PathStrokeType (u * 0.08f)); break;
            case Text:
                g.setFont (juce::Font (u * 0.42f, juce::Font::bold));
                g.setColour (lit ? litColour : juce::Colour (0xffe4e4e4));
                g.drawText (getButtonText(), in.toNearestInt(), juce::Justification::centred); break;
        }
    }
    void mouseDown (const juce::MouseEvent& e) override { juce::Button::mouseDown (e); if (onHold) onHold (true); }
    void mouseUp (const juce::MouseEvent& e) override   { juce::Button::mouseUp (e);   if (onHold) onHold (false); }
private:
    Icon icon;
};

// ------------------------------------------------------------------ tachometer
class TachGauge : public juce::Component
{
public:
    float rpm = 0.f, speed = 0.f, redline = 7000.f, rimAngle = 0.f, rimBlur = 0.f;
    bool powerLabel = false;                  // jets / sci-fi: "POWER" instead of "RPM"
    juce::Image rim;
    std::function<void (float)> onRpmDrag;    // set while the RPM-knob mode is active

    void paint (juce::Graphics& g) override
    {
        const auto c = getLocalBounds().toFloat().getCentre();
        const float R = juce::jmin ((float) getWidth(), (float) getHeight()) * 0.5f - 2.f;
        const float maxK = std::ceil (redline / 1000.f), maxRpm = maxK * 1000.f;
        auto ang = [&] (float v) { return kA0 + juce::jlimit (0.f, 1.f, v / maxRpm) * (kA1 - kA0); };

        g.setColour (juce::Colours::black.withAlpha (0.5f)); g.fillEllipse (c.x - R, c.y - R + R * 0.03f, 2 * R, 2 * R);
        g.setGradientFill (juce::ColourGradient (juce::Colour (0xfff2f2f2), c.x - R, c.y - R, juce::Colour (0xff2e2e2e), c.x + R, c.y + R, false));
        g.fillEllipse (c.x - R, c.y - R, 2 * R, 2 * R);
        g.setGradientFill (juce::ColourGradient (juce::Colour (0xff3a3a3a), c.x - R, c.y - R, juce::Colour (0xffe0e0e0), c.x + R, c.y + R, false));
        g.fillEllipse (c.x - R * 0.94f, c.y - R * 0.94f, 1.88f * R, 1.88f * R);
        g.setGradientFill (juce::ColourGradient (juce::Colour (0xff1c1c1c), c.x, c.y - R * 0.9f, juce::Colour (0xff000000), c.x, c.y + R * 0.9f, false));
        g.fillEllipse (c.x - R * 0.9f, c.y - R * 0.9f, 1.8f * R, 1.8f * R);

        // red zone
        juce::Path red;
        red.addCentredArc (c.x, c.y, R * 0.83f, R * 0.83f, 0.f, ang (redline * 0.88f), ang (maxRpm), true);
        g.setColour (juce::Colour (0xffe01010)); g.strokePath (red, juce::PathStrokeType (R * 0.065f));

        // ticks + numbers
        g.setColour (juce::Colours::white);
        for (int k = 0; k <= (int) maxK * 2; ++k)
        {
            const bool major = (k % 2 == 0);
            const float a = ang ((float) k * 500.f), sn = std::sin (a), cs = -std::cos (a);
            const float r0 = R * (major ? 0.74f : 0.79f), r1 = R * 0.87f;
            g.drawLine (c.x + sn * r0, c.y + cs * r0, c.x + sn * r1, c.y + cs * r1, major ? R * 0.014f : R * 0.007f);
            if (major)
            {
                g.setFont (juce::Font (R * 0.10f, juce::Font::italic));
                g.drawText (juce::String (k / 2), juce::Rectangle<float> (c.x + sn * R * 0.62f - R * 0.1f, c.y + cs * R * 0.62f - R * 0.07f, R * 0.2f, R * 0.14f),
                            juce::Justification::centred);
            }
        }

        // centre disc with the spinning rim
        const float rd = R * 0.40f;
        {
            juce::Graphics::ScopedSaveState ss (g);
            juce::Path clip; clip.addEllipse (c.x - rd, c.y - rd, 2 * rd, 2 * rd);
            g.reduceClipRegion (clip);
            g.setGradientFill (juce::ColourGradient (juce::Colour (0xffc8c8c8), c.x - rd, c.y - rd, juce::Colour (0xff6a6a6a), c.x + rd, c.y + rd, false));
            g.fillEllipse (c.x - rd, c.y - rd, 2 * rd, 2 * rd);
            if (rim.isValid()) drawRimImage (g, rim, c.x, c.y, rd * 2.05f, rimAngle, rimBlur);
            g.setGradientFill (juce::ColourGradient (juce::Colours::white.withAlpha (0.25f), c.x - rd, c.y - rd, juce::Colours::transparentBlack, c.x, c.y, false));
            g.fillEllipse (c.x - rd, c.y - rd, 2 * rd, 2 * rd);
        }
        g.setColour (juce::Colour (0xff222222)); g.drawEllipse (c.x - rd, c.y - rd, 2 * rd, 2 * rd, R * 0.02f);

        g.setColour (juce::Colours::white);
        g.setFont (juce::Font (R * 0.075f, juce::Font::bold));
        g.drawText (powerLabel ? "POWER" : "RPM", juce::Rectangle<float> (c.x - R * 0.3f, c.y - R * 0.53f, R * 0.6f, R * 0.09f), juce::Justification::centred);
        g.setFont (juce::Font (R * 0.06f, juce::Font::bold));
        g.drawText ("X1000", juce::Rectangle<float> (c.x - R * 0.3f, c.y - R * 0.46f, R * 0.6f, R * 0.07f), juce::Justification::centred);

        // needle
        const float na = ang (rpm), sn = std::sin (na), cs = -std::cos (na);
        g.setColour (juce::Colours::black.withAlpha (0.5f));
        g.drawLine (c.x - sn * R * 0.12f + 3.f, c.y - cs * R * 0.12f + 3.f, c.x + sn * R * 0.82f + 3.f, c.y + cs * R * 0.82f + 3.f, R * 0.016f);
        g.setColour (juce::Colour (0xffff1a1a));
        g.drawLine (c.x - sn * R * 0.12f, c.y - cs * R * 0.12f, c.x + sn * R * 0.82f, c.y + cs * R * 0.82f, R * 0.016f);
        g.setColour (juce::Colours::black); g.fillEllipse (c.x - R * 0.06f, c.y - R * 0.06f, R * 0.12f, R * 0.12f);
        g.setColour (juce::Colour (0xff555555)); g.drawEllipse (c.x - R * 0.06f, c.y - R * 0.06f, R * 0.12f, R * 0.12f, 1.5f);

        // LCD speed readout
        const juce::Rectangle<float> lcd (c.x - R * 0.2f, c.y + R * 0.56f, R * 0.4f, R * 0.13f);
        g.setColour (juce::Colour (0xff9db09d)); g.fillRoundedRectangle (lcd, lcd.getHeight() * 0.35f);
        g.setColour (juce::Colour (0xff333a33)); g.drawRoundedRectangle (lcd, lcd.getHeight() * 0.35f, 1.5f);
        g.setColour (juce::Colour (0xff141a14));
        g.setFont (juce::Font (juce::Font::getDefaultMonospacedFontName(), lcd.getHeight() * 0.8f, juce::Font::bold));
        g.drawText (juce::String ((int) std::round (speed)), lcd.toNearestInt(), juce::Justification::centred);
        g.setColour (juce::Colours::white.withAlpha (0.6f)); g.setFont (R * 0.05f);
        g.drawText ("km/h", juce::Rectangle<float> (lcd.getX(), lcd.getBottom(), lcd.getWidth(), R * 0.07f), juce::Justification::centred);
    }

    void mouseDown (const juce::MouseEvent& e) override { drag (e); }
    void mouseDrag (const juce::MouseEvent& e) override { drag (e); }

private:
    static constexpr float kA0 = juce::MathConstants<float>::pi * 1.25f, kA1 = juce::MathConstants<float>::pi * 2.75f;
    void drag (const juce::MouseEvent& e)
    {
        if (! onRpmDrag) return;
        const auto c = getLocalBounds().toFloat().getCentre();
        float a = std::atan2 (e.position.x - c.x, -(e.position.y - c.y));            // 0 at 12 o'clock, clockwise
        if (a < 0.f) a += juce::MathConstants<float>::twoPi;
        float t = (a - kA0) / (kA1 - kA0);                                            // 7:30 position = 0
        if (a < kA0) t = (a + juce::MathConstants<float>::twoPi - kA0) / (kA1 - kA0);
        if (a > juce::MathConstants<float>::pi * 0.75f && a < kA0) t = (a < juce::MathConstants<float>::pi) ? 1.f : 0.f;   // dead zone at the bottom
        onRpmDrag (juce::jlimit (0.f, 1.f, t) * std::ceil (redline / 1000.f) * 1000.f);
    }
};

// ------------------------------------------------------------------ pressure pedal
// Mouse: where you press on the travel (higher = harder) + how long you hold it.  Keyboard: handled by the editor via `pedal`.
// Pen / touch with pressure support: real pressure.
class PedalControl : public juce::Component
{
public:
    f1nz::PedalInput pedal;
    float shown = 0.f;                                          // pedal position to draw (from the parameter)
    std::function<void (bool)> onGesture;                       // true = pressed, false = released

    void paint (juce::Graphics& g) override
    {
        auto b = getLocalBounds().toFloat().reduced (3.f);
        g.setGradientFill (juce::ColourGradient (juce::Colour (0xff6d6d6d), b.getX(), b.getY(), juce::Colour (0xff2a2a2a), b.getRight(), b.getBottom(), false));
        g.fillRoundedRectangle (b, 22.f);
        g.setColour (juce::Colours::black.withAlpha (0.5f)); g.drawRoundedRectangle (b, 22.f, 2.f);

        const float x = b.getRight() - b.getWidth() * 0.18f, y0 = b.getY() + 24.f, y1 = b.getBottom() - 44.f;
        g.setColour (juce::Colour (0xff101010)); g.fillRoundedRectangle (x - 2.5f, y0, 5.f, y1 - y0, 2.5f);
        g.setColour (juce::Colour (0xffe01010)); g.fillRoundedRectangle (x - 2.5f, y1 - shown * (y1 - y0), 5.f, shown * (y1 - y0), 2.5f);

        const float ty = y1 - shown * (y1 - y0), d = juce::jmin (b.getWidth() * 0.45f, 74.f);
        const float tx = b.getRight() - b.getWidth() * 0.18f - d * 0.5f - 8.f;
        g.setColour (juce::Colours::black.withAlpha (0.4f)); g.fillEllipse (tx, ty + 4.f, d, d);
        g.setGradientFill (juce::ColourGradient (juce::Colour (0xfff0f0f0), tx, ty, juce::Colour (0xff3c3c3c), tx + d, ty + d, false));
        g.fillEllipse (tx, ty, d, d);
        auto in = juce::Rectangle<float> (tx, ty, d, d).reduced (d * 0.09f);
        g.setGradientFill (juce::ColourGradient (juce::Colour (0xffa6a6a6), in.getCentreX(), in.getY(), juce::Colour (0xff262626), in.getCentreX(), in.getBottom(), false));
        g.fillEllipse (in);
        juce::Path p; const auto c = in.getCentre(); const float u = in.getWidth() * 0.27f;
        p.addTriangle (c.x, c.y - u, c.x - u, c.y + u * 0.8f, c.x + u, c.y + u * 0.8f);
        g.setColour (juce::Colour (0xffe8e8e8)); g.fillPath (p);
        g.setColour (juce::Colours::white.withAlpha (0.7f)); g.setFont (12.f);
        g.drawText ("PEDAL", getLocalBounds().removeFromBottom (18), juce::Justification::centred);
    }

    void mouseDown (const juce::MouseEvent& e) override
    {
        const bool hw = e.source.isPen() || e.source.isTouch();
        if (hw && e.pressure > 0.f && e.pressure < 1.f) pedal.press (f1nz::PedalInput::Pressure, std::max (depthFromY (e.position.y), e.pressure));
        else pedal.press (f1nz::PedalInput::Mouse, depthFromY (e.position.y));
        if (onGesture) onGesture (true);
    }
    void mouseDrag (const juce::MouseEvent& e) override
    {
        const bool hw = e.source.isPen() || e.source.isTouch();
        float d = depthFromY (e.position.y);
        if (hw && e.pressure > 0.f && e.pressure < 1.f) d = std::max (d, e.pressure);
        pedal.setDepth (d);
    }
    void mouseUp (const juce::MouseEvent&) override { pedal.release(); if (onGesture) onGesture (false); }

private:
    float depthFromY (float y) const
    {
        const float y0 = 24.f + 3.f, y1 = (float) getHeight() - 44.f - 3.f;
        return juce::jlimit (0.f, 1.f, (y1 - y) / std::max (1.f, y1 - y0));
    }
};
} // namespace f1nz::ui
