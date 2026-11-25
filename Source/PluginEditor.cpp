#include "PluginEditor.h"
#include "BinaryData.h"

using namespace f1nz;
namespace
{
const juce::Colour kRed (0xffe0202a);
constexpr float kRoadY = 277.f;      // y (inside the 300px scene) where the lane dashes are drawn on Highway scenes
constexpr float kRoadDashH = 4.f;
}

F1nzEditor::F1nzEditor (F1nzProcessor& p) : juce::AudioProcessorEditor (&p), proc (p)
{
    setSize (780, 540);

    rimImg  = juce::ImageCache::getFromMemory (BinaryData::rim_png,       BinaryData::rim_pngSize);
    carImg  = juce::ImageCache::getFromMemory (BinaryData::car_png,       BinaryData::car_pngSize);
    bgNight = juce::ImageCache::getFromMemory (BinaryData::bg_night_png,  BinaryData::bg_night_pngSize);
    bgDay   = juce::ImageCache::getFromMemory (BinaryData::bg_day_png,    BinaryData::bg_day_pngSize);

    for (int i = 0; i < kNumEngines; ++i) engineBox.addItem (engineSpecs()[i].name, i + 1);
    viewBox.addItemList ({ "Rim", "Car" }, 1);
    sceneBox.addItemList ({ "Dyno", "Highway Night", "Highway Day" }, 1);
    for (auto* c : { &engineBox, &viewBox, &sceneBox })
    {
        c->setColour (juce::ComboBox::backgroundColourId, juce::Colour (0xcc101018));
        c->setColour (juce::ComboBox::textColourId, juce::Colours::white);
        c->setColour (juce::ComboBox::outlineColourId, kRed);
        c->setColour (juce::ComboBox::arrowColourId, juce::Colours::white);
        addAndMakeVisible (*c);
    }

    styleKnob (rpmSlider, " rpm");
    styleKnob (throttleSlider, "");
    styleKnob (chargerSlider, "");
    styleKnob (volumeSlider, "");
    throttleSlider.setNumDecimalPlacesToDisplay (2);
    chargerSlider.setNumDecimalPlacesToDisplay (2);
    volumeSlider.setNumDecimalPlacesToDisplay (2);

    for (auto* b : { &ignitionBtn, &limiterBtn, &midiBtn })
    {
        b->setColour (juce::ToggleButton::textColourId, juce::Colours::white);
        b->setColour (juce::ToggleButton::tickColourId, kRed);
        b->setColour (juce::ToggleButton::tickDisabledColourId, juce::Colours::grey);
        addAndMakeVisible (*b);
    }

    auto& v = proc.apvts;
    rpmA   = std::make_unique<SA> (v, "rpm", rpmSlider);
    thrA   = std::make_unique<SA> (v, "throttle", throttleSlider);
    chgA   = std::make_unique<SA> (v, "charger", chargerSlider);
    volA   = std::make_unique<SA> (v, "volume", volumeSlider);
    ignA   = std::make_unique<BA> (v, "ignition", ignitionBtn);
    limA   = std::make_unique<BA> (v, "limiter", limiterBtn);
    midA   = std::make_unique<BA> (v, "midi", midiBtn);
    engA   = std::make_unique<CA> (v, "engine", engineBox);
    viewA  = std::make_unique<CA> (v, "view", viewBox);
    sceneA = std::make_unique<CA> (v, "scene", sceneBox);

    lastTime = juce::Time::getMillisecondCounterHiRes() * 0.001;
    startTimerHz (60);
}

F1nzEditor::~F1nzEditor() { stopTimer(); }

void F1nzEditor::styleKnob (juce::Slider& s, const juce::String& suffix)
{
    s.setSliderStyle (juce::Slider::RotaryHorizontalVerticalDrag);
    s.setTextBoxStyle (juce::Slider::TextBoxBelow, false, 90, 20);
    s.setTextValueSuffix (suffix);
    s.setColour (juce::Slider::rotarySliderFillColourId, kRed);
    s.setColour (juce::Slider::thumbColourId, juce::Colours::white);
    s.setColour (juce::Slider::textBoxTextColourId, juce::Colours::white);
    s.setColour (juce::Slider::textBoxOutlineColourId, juce::Colours::transparentBlack);
    addAndMakeVisible (s);
}

void F1nzEditor::resized()
{
    engineBox.setBounds (12, 12, 170, 26);
    viewBox.setBounds (190, 12, 80, 26);
    sceneBox.setBounds (278, 12, 140, 26);

    const int y = kSceneH + 20;
    rpmSlider.setBounds (20, y, 190, 190);
    throttleSlider.setBounds (250, y + 42, 120, 140);
    chargerSlider.setBounds (385, y + 42, 120, 140);
    volumeSlider.setBounds (520, y + 42, 120, 140);
    ignitionBtn.setBounds (650, y + 25, 115, 28);
    limiterBtn.setBounds (650, y + 65, 115, 28);
    midiBtn.setBounds (650, y + 105, 115, 28);
}

void F1nzEditor::timerCallback()
{
    const double now = juce::Time::getMillisecondCounterHiRes() * 0.001;
    const float dt = (float) juce::jlimit (0.0, 0.1, now - lastTime);
    lastTime = now;

    shownRpm = proc.uiRpm.load();
    const float step = shownRpm / 60.f * juce::MathConstants<float>::twoPi * dt / 9.f;   // wheel turns at rpm/9
    wheelAngle = std::fmod (wheelAngle + step, juce::MathConstants<float>::twoPi);
    roadOffset = std::fmod (roadOffset + shownRpm * dt * 0.35f, 80.f);
    repaint (0, 0, getWidth(), kSceneH);
}

void F1nzEditor::drawRim (juce::Graphics& g, float cx, float cy, float d, float angle, float blur)
{
    if (! rimImg.isValid()) return;
    const float s = d / (float) rimImg.getWidth();
    auto draw = [&] (float a, float alpha)
    {
        g.setOpacity (alpha);
        g.drawImageTransformed (rimImg, juce::AffineTransform::translation (-0.5f * (float) rimImg.getWidth(),
                                                                          -0.5f * (float) rimImg.getHeight())
                                            .scaled (s).rotated (a).translated (cx, cy));
    };
    if (blur > 0.3f) { draw (angle - blur * 0.5f, 0.35f); draw (angle, 0.45f); draw (angle + blur * 0.5f, 0.35f); }
    else draw (angle, 1.f);
    g.setOpacity (1.f);
}

void F1nzEditor::paintTach (juce::Graphics& g, juce::Point<float> c, float r)
{
    const auto& sp = engineSpecs()[(int) proc.apvts.getRawParameterValue ("engine")->load()];
    const float maxRpm = std::ceil (sp.redline / 1000.f) * 1000.f;
    const float a0 = juce::MathConstants<float>::pi * 1.25f, a1 = juce::MathConstants<float>::pi * 2.75f;
    auto ang = [&] (float rpm) { return a0 + juce::jlimit (0.f, 1.f, rpm / maxRpm) * (a1 - a0); };

    g.setColour (juce::Colour (0xcc000000)); g.fillEllipse (c.x - r, c.y - r, 2 * r, 2 * r);
    juce::Path red; red.addCentredArc (c.x, c.y, r * 0.86f, r * 0.86f, 0.f, ang (sp.redline * 0.9f), ang (sp.redline), true);
    g.setColour (kRed); g.strokePath (red, juce::PathStrokeType (4.f));
    g.setColour (juce::Colours::white.withAlpha (0.85f));
    for (float t = 0; t <= maxRpm + 1; t += 1000.f)
    {
        const float a = ang (t), sn = std::sin (a), cs = -std::cos (a);
        g.drawLine (c.x + sn * r * 0.78f, c.y + cs * r * 0.78f, c.x + sn * r * 0.92f, c.y + cs * r * 0.92f, 2.f);
        g.setFont (11.f);
        g.drawText (juce::String ((int) (t / 1000)), juce::Rectangle<float> (c.x + sn * r * 0.62f - 10, c.y + cs * r * 0.62f - 8, 20, 16),
                    juce::Justification::centred);
    }
    const float na = ang (shownRpm);
    g.setColour (kRed);
    g.drawLine (c.x, c.y, c.x + std::sin (na) * r * 0.82f, c.y - std::cos (na) * r * 0.82f, 3.f);
    g.setColour (juce::Colours::white); g.fillEllipse (c.x - 4, c.y - 4, 8, 8);
    g.setFont (juce::Font (14.f, juce::Font::bold));
    g.drawText (juce::String ((int) shownRpm), juce::Rectangle<float> (c.x - 40, c.y + r * 0.35f, 80, 18), juce::Justification::centred);
}

void F1nzEditor::paintScene (juce::Graphics& g, juce::Rectangle<int> scene)
{
    const int sceneIdx = (int) proc.apvts.getRawParameterValue ("scene")->load();
    const int view     = (int) proc.apvts.getRawParameterValue ("view")->load();
    const auto& sp     = engineSpecs()[(int) proc.apvts.getRawParameterValue ("engine")->load()];
    const float rpmN   = juce::jlimit (0.f, 1.f, shownRpm / sp.redline);

    g.saveState();
    g.reduceClipRegion (scene);
    g.setImageResamplingQuality (juce::Graphics::highResamplingQuality);

    if (sceneIdx == 0)  // DYNO
    {
        g.setGradientFill (juce::ColourGradient (juce::Colour (0xff14141c), 0.f, 0.f, juce::Colour (0xff2a0508), 0.f, (float) kSceneH, false));
        g.fillRect (scene);
        g.setColour (juce::Colour (0xff1c1c22)); g.fillRect (0, 248, getWidth(), 52);
        g.setColour (kRed.withAlpha (0.5f)); g.fillRect (0, 246, getWidth(), 3);
        g.setColour (juce::Colour (0xff3a3a44));
        g.fillRoundedRectangle (getWidth() * 0.5f - 150.f, 256.f, 300.f, 34.f, 17.f);   // roller
        g.setColour (juce::Colours::white.withAlpha (0.08f));
        for (int i = 0; i < 12; ++i) g.drawLine (getWidth() * 0.5f - 150.f + std::fmod (i * 26.f + roadOffset * 2.f, 300.f), 258.f,
                                                 getWidth() * 0.5f - 150.f + std::fmod (i * 26.f + roadOffset * 2.f, 300.f), 288.f, 2.f);
    }
    else
    {
        const auto& bg = (sceneIdx == 1 ? bgNight : bgDay);
        if (bg.isValid()) g.drawImage (bg, scene.toFloat(), juce::RectanglePlacement::fillDestination);
        else g.fillAll (juce::Colours::black);
        g.setColour (juce::Colours::white.withAlpha (0.75f));
        for (float x = -80.f + roadOffset; x < (float) getWidth(); x += 80.f)
            g.fillRect (x, kRoadY, 44.f, kRoadDashH);
    }

    const float shake = 1.4f * rpmN * std::sin ((float) juce::Time::getMillisecondCounterHiRes() * 0.09f);
    if (view == 0)   // big rim
    {
        const float d = 250.f;
        drawRim (g, getWidth() * 0.5f, 150.f + shake * 0.4f, d, wheelAngle, juce::jlimit (0.f, 1.f, rpmN * 1.5f));
    }
    else if (carImg.isValid())
    {
        const float cw = 600.f, ch = cw * (float) carImg.getHeight() / (float) carImg.getWidth();
        const float x = ((float) getWidth() - cw) * 0.5f, y = 255.f - ch + shake;
        g.drawImage (carImg, juce::Rectangle<float> (x, y, cw, ch));
        const float rear[3]  = { 128.f / 1060.f, 168.f / 275.f, 70.f / 1060.f };
        const float front[3] = { 660.f / 1060.f, 180.f / 275.f, 84.f / 1060.f };
        const float* wheels[2] = { rear, front };
        for (auto* w : wheels)
            drawRim (g, x + w[0] * cw, y + w[1] * ch, 2.06f * w[2] * cw, wheelAngle, juce::jlimit (0.f, 1.f, rpmN * 1.5f));
    }
    paintTach (g, { (float) getWidth() - 72.f, 78.f }, 60.f);
    g.restoreState();
}

void F1nzEditor::paint (juce::Graphics& g)
{
    g.fillAll (juce::Colour (0xff0b0b10));
    paintScene (g, { 0, 0, getWidth(), kSceneH });

    g.setColour (juce::Colour (0xff15151c)); g.fillRect (0, kSceneH, getWidth(), getHeight() - kSceneH);
    g.setColour (kRed); g.fillRect (0, kSceneH, getWidth(), 2);
    g.setColour (juce::Colours::white.withAlpha (0.7f)); g.setFont (juce::Font (13.f, juce::Font::bold));
    const int y = kSceneH + 6;
    g.drawText ("RPM", 20, y, 190, 16, juce::Justification::centred);
    g.drawText ("THROTTLE", 250, y + 20, 120, 16, juce::Justification::centred);
    g.drawText ("SUPERCHARGER", 385, y + 20, 120, 16, juce::Justification::centred);
    g.drawText ("VOLUME", 520, y + 20, 120, 16, juce::Justification::centred);
    g.setColour (juce::Colours::white.withAlpha (0.35f)); g.setFont (11.f);
    g.drawText ("F1nz Engine  -  samples & concept: f1nz", 650, getHeight() - 22, 125, 14, juce::Justification::centredRight);
}
