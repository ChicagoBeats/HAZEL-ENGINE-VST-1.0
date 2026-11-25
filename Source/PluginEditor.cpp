#include "PluginEditor.h"
#include "BinaryData.h"

using namespace f1nz;
using namespace f1nz::ui;
namespace
{
const juce::Colour kRed (0xffe0202a);
constexpr float kRoadY = 300.f;      // y of the lane dashes on Highway scenes (Showroom layout)
constexpr int kW = 960, kH = 600;
}

F1nzEditor::F1nzEditor (F1nzProcessor& p) : juce::AudioProcessorEditor (&p), proc (p), browser (p)
{
    setSize (kW, kH);
    setWantsKeyboardFocus (true);

    rimImg  = juce::ImageCache::getFromMemory (BinaryData::rim_png,      BinaryData::rim_pngSize);
    carImg  = juce::ImageCache::getFromMemory (BinaryData::car_png,      BinaryData::car_pngSize);
    bgNight = juce::ImageCache::getFromMemory (BinaryData::bg_night_png, BinaryData::bg_night_pngSize);
    bgDay   = juce::ImageCache::getFromMemory (BinaryData::bg_day_png,   BinaryData::bg_day_pngSize);
    tach.rim = rimImg;

    // ---- combos (items must match the parameter choices) ----
    int lastGroup = -1;
    for (int i = 0; i < engineCount(); ++i)
    {
        if (engineSpec (i).group != lastGroup) { lastGroup = engineSpec (i).group; engineBox.addSectionHeading (groupName (lastGroup)); }
        engineBox.addItem (engineSpec (i).name, i + 1);
    }
    for (int i = 0; i < kNumInductions; ++i) inductionBox.addItem (inductionName (i), i + 1);
    stageBox.addItemList ({ "Stock", "Stage 1", "Stage 2", "Stage 3" }, 1);
    valveBox.addItemList ({ "Recirculating", "Vent BOV", "Flutter" }, 1);
    controlBox.addItemList ({ "Pedal & gearbox", "RPM knob", "MIDI notes" }, 1);
    viewBox.addItemList ({ "Rim", "Car" }, 1);
    sceneBox.addItemList ({ "Dyno", "Highway Night", "Highway Day" }, 1);
    fxBox.addItemList ({ "FX off", "Garage", "Tunnel", "Street" }, 1);
    for (auto* c : { &engineBox, &inductionBox, &stageBox, &valveBox, &controlBox, &viewBox, &sceneBox, &fxBox })
    { styleCombo (*c); addAndMakeVisible (*c); }

    auto& v = proc.apvts;
    auto attachCombo = [&] (const char* id, juce::ComboBox& c) { comboAtt.push_back (std::make_unique<CA> (v, id, c)); };
    attachCombo ("engine", engineBox); attachCombo ("induction", inductionBox); attachCombo ("stage", stageBox);
    attachCombo ("valve", valveBox);   attachCombo ("control", controlBox);     attachCombo ("view", viewBox);
    attachCombo ("scene", sceneBox);   attachCombo ("fxmode", fxBox);

    // ---- slider bars ----
    auto attachSlider = [&] (const char* id, juce::Slider& s, const juce::String& name, int dec, const juce::String& unit = {})
    { sliderAtt.push_back (std::make_unique<SA> (v, id, s)); styleBar (s, name, dec, unit); addAndMakeVisible (s); };
    attachSlider ("charger", chargerS, "Induction", 2);
    attachSlider ("pedalramp", pedalRampS, "Pedal feel", 2, " s");
    attachSlider ("volume", volumeS, "Volume", 2);
    attachSlider ("enginelvl", engLvlS, "Synth engine", 2);
    attachSlider ("samplerlvl", smpLvlS, "Sampler", 2);
    attachSlider ("fxmix", fxMixS, "FX mix", 2);
    attachSlider ("fxlp", fxLpS, "Low-pass", 0, " Hz");

    limAtt = std::make_unique<BA> (v, "limiter", limiterBtn);
    limiterBtn.setColour (juce::ToggleButton::textColourId, juce::Colours::white);
    limiterBtn.setColour (juce::ToggleButton::tickColourId, kRed);
    addAndMakeVisible (limiterBtn);

    // ---- layout tabs ----
    for (int i = 0; i < 3; ++i)
    {
        layoutBtn[i].setClickingTogglesState (true); layoutBtn[i].setRadioGroupId (7);
        layoutBtn[i].setColour (juce::TextButton::buttonOnColourId, kRed);
        layoutBtn[i].setColour (juce::TextButton::buttonColourId, juce::Colour (0xff2c2c34));
        layoutBtn[i].onClick = [this, i] { if (layoutBtn[(size_t) i].getToggleState()) { setParam ("layout", (float) i); layout = i; applyLayout(); } };
        addAndMakeVisible (layoutBtn[i]);
    }

    // ---- cockpit controls ----
    for (auto* b : { &btnBack, &btnNext, &btnPower, &btnGearUp, &btnGearDown, &btnBrake, &btnAuto }) addAndMakeVisible (*b);
    addAndMakeVisible (tach); addAndMakeVisible (pedalCtl);
    btnBack.onClick = [this] { const int n = engineCount(); setParam ("engine", (float) (((int) getParam ("engine") + n - 1) % n)); };
    btnNext.onClick = [this] { const int n = engineCount(); setParam ("engine", (float) (((int) getParam ("engine") + 1) % n)); };
    btnPower.onClick = [this] { setParam ("ignition", getParam ("ignition") > 0.5f ? 0.f : 1.f); };
    btnAuto.onClick = [this] { setParam ("auto", getParam ("auto") > 0.5f ? 0.f : 1.f); };
    btnGearUp.onClick = [this] { setParam ("gear", (float) juce::jmin (6, proc.uiGear.load() + 1)); };
    btnGearDown.onClick = [this] { setParam ("gear", (float) juce::jmax (0, proc.uiGear.load() - 1)); };
    btnBrake.onHold = [this] (bool down) { setParam ("brake", down ? 1.f : 0.f); };
    pedalCtl.onGesture = [this] (bool down) { if (down) grabKeyboardFocus(); };

    tach.onRpmDrag = nullptr;

    // ---- studio ----
    addAndMakeVisible (browser);
    for (size_t i = 0; i < cards.size(); ++i)
    {
        cards[i] = std::make_unique<SlotCard> (proc);
        cards[i]->onChanged = [this] { refreshCards(); };
        addAndMakeVisible (*cards[i]);
    }
    for (int i = 0; i < 4; ++i)
    {
        tabBtn[i].setClickingTogglesState (true); tabBtn[i].setRadioGroupId (8);
        tabBtn[i].setColour (juce::TextButton::buttonOnColourId, kRed);
        tabBtn[i].setColour (juce::TextButton::buttonColourId, juce::Colour (0xff2c2c34));
        tabBtn[i].onClick = [this, i] { slotTab = i; refreshCards(); };
        addAndMakeVisible (tabBtn[i]);
    }
    tabBtn[0].setToggleState (true, juce::dontSendNotification);
    browser.onLoadRequested = [this] (const juce::File& f) { loadIntoFreeSlot (f); };
    addAndMakeVisible (xy);
    xy.onMove = [this] (float x01, float y01)
    {
        const auto& sp = engineSpec ((int) getParam ("engine"));
        setParam ("control", 1.f);                                  // the XY pad drives the RPM knob mode
        setParam ("rpm", x01 * sp.redline);
        setParam ("throttle", y01);
    };
    saveB.onClick = [this]
    {
        presetChooser = std::make_unique<juce::FileChooser> ("Save preset", juce::File::getSpecialLocation (juce::File::userDocumentsDirectory), "*.f1nzpreset");
        presetChooser->launchAsync (juce::FileBrowserComponent::saveMode | juce::FileBrowserComponent::canSelectFiles | juce::FileBrowserComponent::warnAboutOverwriting,
            [this] (const juce::FileChooser& fc)
            { auto f = fc.getResult(); if (f == juce::File()) return; juce::MemoryBlock mb; proc.getStateInformation (mb); f.withFileExtension ("f1nzpreset").replaceWithData (mb.getData(), mb.getSize()); });
    };
    loadB.onClick = [this]
    {
        presetChooser = std::make_unique<juce::FileChooser> ("Load preset", juce::File::getSpecialLocation (juce::File::userDocumentsDirectory), "*.f1nzpreset");
        presetChooser->launchAsync (juce::FileBrowserComponent::openMode | juce::FileBrowserComponent::canSelectFiles,
            [this] (const juce::FileChooser& fc)
            { auto f = fc.getResult(); if (! f.existsAsFile()) return; juce::MemoryBlock mb; if (f.loadFileAsData (mb)) { proc.setStateInformation (mb.getData(), (int) mb.getSize()); refreshCards(); browser.rescan(); } });
    };
    addAndMakeVisible (saveB); addAndMakeVisible (loadB);

    layout = juce::jlimit (0, 2, (int) getParam ("layout"));
    layoutBtn[layout].setToggleState (true, juce::dontSendNotification);
    refreshCards();
    makeCarbon (kW, kH);
    applyLayout();
    lastTime = juce::Time::getMillisecondCounterHiRes() * 0.001;
    startTimerHz (60);
    grabKeyboardFocus();
}

F1nzEditor::~F1nzEditor() { stopTimer(); if (pedalGestureOpen) setPedalGesture (false); }

// ------------------------------------------------------------------ helpers
void F1nzEditor::styleCombo (juce::ComboBox& c)
{
    c.setColour (juce::ComboBox::backgroundColourId, juce::Colour (0xcc101018));
    c.setColour (juce::ComboBox::textColourId, juce::Colours::white);
    c.setColour (juce::ComboBox::outlineColourId, kRed);
    c.setColour (juce::ComboBox::arrowColourId, juce::Colours::white);
}

void F1nzEditor::styleBar (juce::Slider& s, const juce::String& name, int decimals, const juce::String& unit)
{
    s.setSliderStyle (juce::Slider::LinearBar);
    s.setTextBoxStyle (juce::Slider::TextBoxLeft, true, 10, 10);
    s.setTextBoxIsEditable (false);
    s.textFromValueFunction = [name, decimals, unit] (double x) { return name + "  " + juce::String (x, decimals) + unit; };
    s.updateText();
    s.setColour (juce::Slider::trackColourId, juce::Colour (0xff7a1018));
    s.setColour (juce::Slider::backgroundColourId, juce::Colour (0xff23232b));
    s.setColour (juce::Slider::textBoxTextColourId, juce::Colours::white);
    s.setColour (juce::Slider::textBoxOutlineColourId, juce::Colour (0xff4a4a56));
}

void F1nzEditor::setParam (const char* id, float plain)
{
    if (auto* p = proc.apvts.getParameter (id))
    { p->beginChangeGesture(); p->setValueNotifyingHost (p->convertTo0to1 (plain)); p->endChangeGesture(); }
}
float F1nzEditor::getParam (const char* id) const { return proc.apvts.getRawParameterValue (id)->load(); }

void F1nzEditor::setPedalGesture (bool open)
{
    if (auto* p = proc.apvts.getParameter ("pedal"))
    {
        if (open && ! pedalGestureOpen) p->beginChangeGesture();
        if (! open && pedalGestureOpen) { p->setValueNotifyingHost (0.f); p->endChangeGesture(); }
    }
    pedalGestureOpen = open;
}

void F1nzEditor::makeCarbon (int w, int h)
{
    carbon = juce::Image (juce::Image::RGB, w, h, false);
    juce::Image::BitmapData bd (carbon, juce::Image::BitmapData::writeOnly);
    for (int y = 0; y < h; ++y)
        for (int x = 0; x < w; ++x)
        {
            const int a = ((x + y) >> 2) & 1, b = ((x - y + 4096) >> 2) & 1;
            float v = 26.f + (a ? 12.f : -4.f) + (b ? 5.f : 0.f);
            const float dx = (float) (x - w / 2) / (float) w, dy = (float) (y - h / 2) / (float) h;
            v *= 1.15f - 0.9f * (dx * dx + dy * dy);
            const auto c = (juce::uint8) juce::jlimit (0, 255, (int) v);
            bd.setPixelColour (x, y, juce::Colour (c, c, (juce::uint8) juce::jlimit (0, 255, (int) c + 2)));
        }
}

void F1nzEditor::refreshCards()
{
    for (size_t i = 0; i < cards.size(); ++i) cards[i]->bind (slotTab * 4 + (int) i);
}

void F1nzEditor::loadIntoFreeSlot (const juce::File& f)
{
    int target = -1;
    for (int k = 0; k < 16 && target < 0; ++k)
    {
        const int s = (slotTab * 4 + k) % 16;
        if (proc.slotPath (s).isEmpty()) target = s;
    }
    if (target < 0) target = slotTab * 4;
    proc.loadSlotFile (target, f);
    refreshCards();
}

// ------------------------------------------------------------------ layouts
void F1nzEditor::applyLayout()
{
    const bool cockpit = layout == Cockpit, show = layout == Showroom, studio = layout == Studio;
    for (auto* c : std::initializer_list<juce::Component*> { &tach, &pedalCtl, &btnBack, &btnNext, &btnPower, &btnGearUp, &btnGearDown, &btnBrake, &btnAuto })
        c->setVisible (cockpit || show);
    for (auto* c : std::initializer_list<juce::Component*> { &controlBox, &viewBox, &sceneBox, &limiterBtn, &pedalRampS })
        c->setVisible (cockpit || show);
    viewBox.setVisible (show); sceneBox.setVisible (show);
    chargerS.setVisible (true); volumeS.setVisible (true);
    for (auto* c : std::initializer_list<juce::Component*> { &browser, &xy, &saveB, &loadB, &fxBox, &fxMixS, &fxLpS, &engLvlS, &smpLvlS })
        c->setVisible (studio);
    for (auto& c : cards) c->setVisible (studio);
    for (auto& t : tabBtn) t.setVisible (studio);
    resized();
    repaint();
}

void F1nzEditor::resized()
{
    // header (all layouts)
    engineBox.setBounds (10, 8, 270, 26); inductionBox.setBounds (286, 8, 200, 26);
    stageBox.setBounds (492, 8, 80, 26);  valveBox.setBounds (578, 8, 110, 26);
    for (int i = 0; i < 3; ++i) layoutBtn[i].setBounds (704 + i * 84, 8, 80, 26);
    if (layout == Cockpit) layoutCockpit(); else if (layout == Showroom) layoutShowroom(); else layoutStudio();
}

void F1nzEditor::layoutCockpit()
{
    tach.setBounds (250, 40, 460, 460);
    btnBack.setBounds (34, 50, 80, 80); btnNext.setBounds (122, 50, 80, 80);
    lampAmber = { 238.f, 92.f }; lampRed = { 722.f, 92.f }; lampR = 22.f;
    btnPower.setBounds (846, 50, 80, 80);
    gearPlate = { 26, 262, 214, 232 };
    btnGearUp.setBounds (38, 272, 84, 84); btnGearDown.setBounds (38, 396, 84, 84);
    gearLcd = { 138, 330, 90, 88 };
    btnAuto.setBounds (138, 432, 90, 44);
    btnBrake.setBounds (720, 400, 88, 88);
    pedalCtl.setBounds (786, 262, 160, 238);
    controlBox.setBounds (10, 566, 150, 26);
    limiterBtn.setBounds (170, 566, 110, 26);
    chargerS.setBounds (290, 566, 150, 26); pedalRampS.setBounds (448, 566, 160, 26); volumeS.setBounds (616, 566, 150, 26);
    sceneRect = {};
}

void F1nzEditor::layoutShowroom()
{
    sceneRect = { 0, 40, kW, 340 };
    tach.setBounds (700, 44, 250, 250);
    gearPlate = { 18, 392, 176, 190 };
    btnGearUp.setBounds (26, 398, 72, 72); btnGearDown.setBounds (26, 500, 72, 72);
    gearLcd = { 104, 440, 82, 80 };
    btnAuto.setBounds (200, 440, 70, 40);
    btnPower.setBounds (284, 420, 72, 72);
    btnBack.setBounds (364, 420, 72, 72); btnNext.setBounds (444, 420, 72, 72);
    lampAmber = { 556.f, 440.f }; lampRed = { 604.f, 440.f }; lampR = 16.f;
    btnBrake.setBounds (640, 480, 80, 80);
    pedalCtl.setBounds (780, 388, 168, 196);
    controlBox.setBounds (284, 506, 150, 26); limiterBtn.setBounds (444, 506, 110, 26);
    viewBox.setBounds (284, 540, 80, 26); sceneBox.setBounds (370, 540, 150, 26);
    chargerS.setBounds (530, 540, 120, 26); pedalRampS.setBounds (530, 506, 120, 26); volumeS.setBounds (656, 400, 112, 26);
}

void F1nzEditor::layoutStudio()
{
    browser.setBounds (6, 40, 246, 554);
    for (int i = 0; i < 4; ++i) tabBtn[i].setBounds (258 + i * 86, 40, 82, 26);
    const int cw = 330, ch = 198, x0 = 258, y0 = 70;
    for (size_t i = 0; i < cards.size(); ++i) cards[i]->setBounds (x0 + (int) (i % 2) * (cw + 4), y0 + (int) (i / 2) * (ch + 4), cw, ch);
    xy.setBounds (258, 480, 190, 114);
    fxBox.setBounds (456, 482, 120, 26); fxMixS.setBounds (456, 512, 176, 24); fxLpS.setBounds (456, 540, 176, 24);
    volumeS.setBounds (642, 482, 160, 24); engLvlS.setBounds (642, 512, 160, 24); smpLvlS.setBounds (642, 540, 160, 24);
    chargerS.setBounds (642, 568, 160, 24);
    saveB.setBounds (812, 482, 134, 26); loadB.setBounds (812, 512, 134, 26);
}

// ------------------------------------------------------------------ keyboard (needs focus; hosts may keep some keys)
bool F1nzEditor::keyPressed (const juce::KeyPress& k)
{
    const auto c = juce::CharacterFunctions::toUpperCase (k.getTextCharacter());
    if (c == 'E' || c == ']' || k == juce::KeyPress::pageUpKey)   { btnGearUp.triggerClick();   return true; }
    if (c == 'Q' || c == '[' || k == juce::KeyPress::pageDownKey) { btnGearDown.triggerClick(); return true; }
    if (c == 'I') { btnPower.triggerClick(); return true; }
    if (c == 'A') { btnAuto.triggerClick();  return true; }
    return false;
}

bool F1nzEditor::keyStateChanged (bool)
{
    const bool thr = juce::KeyPress::isKeyCurrentlyDown (juce::KeyPress::upKey) || juce::KeyPress::isKeyCurrentlyDown ('W');
    const bool brk = juce::KeyPress::isKeyCurrentlyDown (juce::KeyPress::downKey) || juce::KeyPress::isKeyCurrentlyDown ('S');
    if (thr && ! thrKey) pedalCtl.pedal.press (PedalInput::Keyboard);          // hold = build up, tap = blip
    if (! thr && thrKey) pedalCtl.pedal.release();
    if (brk != brkKey) setParam ("brake", brk ? 1.f : 0.f);
    thrKey = thr; brkKey = brk;
    return thr || brk;
}

// ------------------------------------------------------------------ frame update
void F1nzEditor::timerCallback()
{
    const double now = juce::Time::getMillisecondCounterHiRes() * 0.001;
    const float dt = (float) juce::jlimit (0.0, 0.1, now - lastTime);
    lastTime = now;

    // pedal: mouse / keyboard / pressure -> parameter (host can record it as automation)
    pedalCtl.pedal.setRampSeconds (getParam ("pedalramp"));
    const float d = pedalCtl.pedal.update (dt);
    if (pedalCtl.pedal.isHeld() || d > 0.f)
    {
        setPedalGesture (true);
        if (auto* p = proc.apvts.getParameter ("pedal")) p->setValueNotifyingHost (d);
    }
    else if (pedalGestureOpen) setPedalGesture (false);
    pedalCtl.shown = juce::jmax (d, getParam ("pedal"));

    rpmShown = proc.uiRpm.load(); speedShown = proc.uiSpeed.load(); boostShown = proc.uiBoost.load();
    gearShown = proc.uiGear.load(); shiftLamp = proc.uiShiftLamp.load(); limiterLamp = proc.uiLimiterLamp.load();
    const auto& sp = engineSpec ((int) getParam ("engine"));

    const float omega = 0.12f * speedShown;
    wheelAngle = std::fmod (wheelAngle + omega * dt, juce::MathConstants<float>::twoPi);
    roadOffset = std::fmod (roadOffset + speedShown * dt * 3.f, 80.f);

    tach.rpm = rpmShown; tach.speed = speedShown; tach.redline = sp.redline; tach.powerLabel = sp.kind != Piston;
    tach.rimAngle = wheelAngle; tach.rimBlur = juce::jlimit (0.f, 1.f, speedShown / 150.f);
    if ((int) getParam ("control") == 1) tach.onRpmDrag = [this] (float rpm) { setParam ("rpm", rpm); };
    else tach.onRpmDrag = nullptr;

    btnPower.lit = getParam ("ignition") > 0.5f; btnAuto.lit = getParam ("auto") > 0.5f;
    btnPower.repaint(); btnAuto.repaint();
    xy.x = juce::jlimit (0.f, 1.f, getParam ("rpm") / sp.redline); xy.y = getParam ("throttle");

    if (layout != Studio)
    {
        tach.repaint(); pedalCtl.repaint();
        repaint (gearPlate.expanded (4));
        repaint (juce::Rectangle<int> ((int) lampAmber.x - 40, (int) lampAmber.y - 40, 80, 80));
        repaint (juce::Rectangle<int> ((int) lampRed.x - 40, (int) lampRed.y - 40, 80, 80));
        if (layout == Showroom) repaint (sceneRect);
    }
    else { xy.repaint(); repaint (800, 560, 160, 40); }
    if (++purgeTick >= 60) { purgeTick = 0; proc.sampler.purgeRetired(); }
}

// ------------------------------------------------------------------ painting
void F1nzEditor::paintScene (juce::Graphics& g, juce::Rectangle<int> scene)
{
    const int sceneIdx = (int) getParam ("scene"), view = (int) getParam ("view");
    const float rpmN = juce::jlimit (0.f, 1.f, rpmShown / engineSpec ((int) getParam ("engine")).redline);
    g.saveState();
    g.reduceClipRegion (scene);
    g.setImageResamplingQuality (juce::Graphics::highResamplingQuality);
    const float top = (float) scene.getY(), bottom = (float) scene.getBottom();

    if (sceneIdx == 0)   // dyno
    {
        g.setGradientFill (juce::ColourGradient (juce::Colour (0xff14141c), 0.f, top, juce::Colour (0xff2a0508), 0.f, bottom, false));
        g.fillRect (scene);
        g.setColour (juce::Colour (0xff1c1c22)); g.fillRect (0.f, bottom - 62.f, (float) kW, 62.f);
        g.setColour (kRed.withAlpha (0.5f)); g.fillRect (0.f, bottom - 64.f, (float) kW, 3.f);
        g.setColour (juce::Colour (0xff3a3a44)); g.fillRoundedRectangle (kW * 0.5f - 170.f, bottom - 54.f, 340.f, 38.f, 19.f);
        g.setColour (juce::Colours::white.withAlpha (0.08f));
        for (int i = 0; i < 14; ++i) { const float x = kW * 0.5f - 170.f + std::fmod (i * 26.f + roadOffset * 2.f, 340.f); g.drawLine (x, bottom - 52.f, x, bottom - 20.f, 2.f); }
    }
    else
    {
        const auto& bg = (sceneIdx == 1 ? bgNight : bgDay);
        if (bg.isValid()) g.drawImage (bg, scene.toFloat(), juce::RectanglePlacement::fillDestination);
        g.setColour (juce::Colours::white.withAlpha (0.75f));
        for (float x = -80.f + roadOffset; x < (float) kW; x += 80.f) g.fillRect (x, kRoadY + (float) scene.getY() - 40.f, 44.f, 4.f);
    }

    const float shake = 1.4f * rpmN * std::sin ((float) juce::Time::getMillisecondCounterHiRes() * 0.09f);
    const float blur = juce::jlimit (0.f, 1.f, speedShown / 150.f);
    if (view == 0) drawRimImage (g, rimImg, kW * 0.5f - 120.f, top + 175.f + shake * 0.4f, 270.f, wheelAngle, blur);
    else if (carImg.isValid())
    {
        const float cw = 640.f, ch = cw * (float) carImg.getHeight() / (float) carImg.getWidth();
        const float x = kW * 0.5f - 120.f - cw * 0.5f, y = bottom - 66.f - ch + shake;
        g.drawImage (carImg, juce::Rectangle<float> (x, y, cw, ch));
        const float rear[3] = { 128.f / 1060.f, 168.f / 275.f, 70.f / 1060.f }, front[3] = { 660.f / 1060.f, 180.f / 275.f, 84.f / 1060.f };
        const float* wheels[2] = { rear, front };
        for (auto* w : wheels) drawRimImage (g, rimImg, x + w[0] * cw, y + w[1] * ch, 2.06f * w[2] * cw, wheelAngle, blur);
    }
    g.restoreState();
}

void F1nzEditor::paintDashboard (juce::Graphics& g)
{
    // gear plate + LCD
    g.setGradientFill (juce::ColourGradient (juce::Colour (0xff6a6a6a), (float) gearPlate.getX(), (float) gearPlate.getY(), juce::Colour (0xff2c2c2c), (float) gearPlate.getRight(), (float) gearPlate.getBottom(), false));
    g.fillRoundedRectangle (gearPlate.toFloat(), 26.f);
    g.setColour (juce::Colours::black.withAlpha (0.5f)); g.drawRoundedRectangle (gearPlate.toFloat(), 26.f, 2.f);
    g.setColour (juce::Colour (0xff9db09d)); g.fillRoundedRectangle (gearLcd.toFloat().withHeight (gearLcd.getHeight() * 0.62f), 10.f);
    g.setColour (juce::Colour (0xff141a14));
    g.setFont (juce::Font (juce::Font::getDefaultMonospacedFontName(), gearLcd.getHeight() * 0.5f, juce::Font::bold));
    g.drawText (gearShown == 0 ? "N" : juce::String (gearShown), gearLcd.withHeight ((int) (gearLcd.getHeight() * 0.62f)), juce::Justification::centred);
    g.setColour (juce::Colours::white.withAlpha (0.6f)); g.setFont (11.f);
    g.drawText ("GEAR", gearLcd.withTrimmedTop ((int) (gearLcd.getHeight() * 0.66f)), juce::Justification::centredTop);
    // lamps: amber = shift light, red = rev limiter
    drawLamp (g, lampAmber, lampR, juce::Colour (0xffffc400), shiftLamp);
    drawLamp (g, lampRed, lampR * 0.8f, juce::Colour (0xffff2020), limiterLamp);
    g.setColour (juce::Colours::white.withAlpha (0.5f)); g.setFont (10.f);
    g.drawText ("SHIFT", (int) lampAmber.x - 30, (int) (lampAmber.y + lampR + 4.f), 60, 12, juce::Justification::centred);
    g.drawText ("LIMIT", (int) lampRed.x - 30, (int) (lampRed.y + lampR + 4.f), 60, 12, juce::Justification::centred);
    // boost bar for forced induction
    if (boostShown > 0.01f)
    {
        const juce::Rectangle<float> bb ((float) gearPlate.getX(), (float) gearPlate.getBottom() + 10.f, (float) gearPlate.getWidth(), 10.f);
        g.setColour (juce::Colour (0xff101014)); g.fillRoundedRectangle (bb, 5.f);
        g.setColour (juce::Colour (0xff3ba3ff)); g.fillRoundedRectangle (bb.withWidth (bb.getWidth() * boostShown), 5.f);
        g.setColour (juce::Colours::white.withAlpha (0.6f)); g.setFont (10.f); g.drawText ("BOOST", bb.translated (0.f, 10.f).toNearestInt(), juce::Justification::centredLeft);
    }
}

void F1nzEditor::paint (juce::Graphics& g)
{
    g.fillAll (juce::Colour (0xff0b0b10));
    if (layout == Studio)
    {
        g.setColour (juce::Colour (0xff121217)); g.fillRect (0, 38, kW, kH - 38);
        g.setColour (juce::Colours::white.withAlpha (0.5f)); g.setFont (juce::Font (11.f, juce::Font::bold));
        g.drawText ("PERFORM", 258, 466, 100, 14, juce::Justification::left);
        g.drawText ("QUICK FX", 456, 466, 100, 14, juce::Justification::left);
        g.drawText ("MIXER", 642, 466, 100, 14, juce::Justification::left);
        g.setColour (juce::Colours::white.withAlpha (0.8f)); g.setFont (13.f);
        g.drawText ("RPM " + juce::String ((int) rpmShown) + "   GEAR " + (gearShown == 0 ? juce::String ("N") : juce::String (gearShown)) + "   " + juce::String ((int) speedShown) + " km/h",
                    812, 548, 140, 18, juce::Justification::left);
        g.setColour (juce::Colours::white.withAlpha (0.35f)); g.setFont (10.f);
        g.drawText ("Click a file to audition  |  double-click or drag into a layer  |  drop files from Explorer", 258, 598 - 14, 540, 14, juce::Justification::left);
        return;
    }
    if (layout == Cockpit) g.drawImageAt (carbon, 0, 0);
    else { g.drawImageAt (carbon, 0, 0); paintScene (g, sceneRect); }
    g.setColour (juce::Colour (0xcc000000)); g.fillRect (0, 0, kW, 38);
    paintDashboard (g);
    g.setColour (juce::Colours::white.withAlpha (0.35f)); g.setFont (10.f);
    g.drawText ("W / Up = throttle (hold longer = harder)   S / Down = brake   E / Q = gear up / down   I = ignition   A = auto",
                10, layout == Cockpit ? 540 : kH - 14, 700, 12, juce::Justification::left);
}
