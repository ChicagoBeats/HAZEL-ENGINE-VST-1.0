#pragma once
#include <juce_audio_utils/juce_audio_utils.h>
#include "PluginProcessor.h"
#include "UI/Cockpit.h"
#include "UI/Studio.h"

class F1nzEditor : public juce::AudioProcessorEditor, public juce::DragAndDropContainer, private juce::Timer
{
public:
    explicit F1nzEditor (F1nzProcessor&);
    ~F1nzEditor() override;
    void paint (juce::Graphics&) override;
    void resized() override;
    bool keyPressed (const juce::KeyPress&) override;
    bool keyStateChanged (bool isKeyDown) override;
    void mouseDown (const juce::MouseEvent&) override { grabKeyboardFocus(); }

private:
    enum Layout { Cockpit = 0, Showroom, Studio };

    void timerCallback() override;
    void applyLayout();
    void layoutCockpit();
    void layoutShowroom();
    void layoutStudio();
    void paintScene (juce::Graphics&, juce::Rectangle<int>);
    void paintDashboard (juce::Graphics&);
    void setParam (const char* id, float plainValue);
    float getParam (const char* id) const;
    void setPedalGesture (bool open);
    void loadIntoFreeSlot (const juce::File&);
    void refreshCards();
    void makeCarbon (int w, int h);
    void styleBar (juce::Slider&, const juce::String& name, int decimals, const juce::String& unit = {});
    void styleCombo (juce::ComboBox&);

    F1nzProcessor& proc;
    int layout = Cockpit, slotTab = 0;
    juce::Image rimImg, carImg, bgNight, bgDay, carbon;

    // header
    juce::ComboBox engineBox, inductionBox, stageBox, valveBox;
    juce::TextButton layoutBtn[3] { juce::TextButton ("Cockpit"), juce::TextButton ("Showroom"), juce::TextButton ("Studio") };

    // cockpit
    f1nz::ui::TachGauge tach;
    f1nz::ui::PedalControl pedalCtl;
    f1nz::ui::ChromeButton btnBack { f1nz::ui::ChromeButton::Left }, btnNext { f1nz::ui::ChromeButton::Right },
        btnPower { f1nz::ui::ChromeButton::Power }, btnGearUp { f1nz::ui::ChromeButton::Up }, btnGearDown { f1nz::ui::ChromeButton::Down },
        btnBrake { f1nz::ui::ChromeButton::Brake }, btnAuto { f1nz::ui::ChromeButton::Text, "AUTO" };
    juce::ComboBox controlBox, viewBox, sceneBox;
    juce::ToggleButton limiterBtn { "Rev limiter" };
    juce::Slider chargerS, pedalRampS, volumeS, engLvlS, smpLvlS, fxMixS, fxLpS;
    juce::ComboBox fxBox;

    // studio
    f1nz::ui::BrowserPanel browser;
    std::array<std::unique_ptr<f1nz::ui::SlotCard>, 4> cards;
    juce::TextButton tabBtn[4] { juce::TextButton ("A  1-4"), juce::TextButton ("B  5-8"), juce::TextButton ("C  9-12"), juce::TextButton ("D  13-16") };
    f1nz::ui::XYPad xy;
    juce::TextButton saveB { "Save preset" }, loadB { "Load preset" };
    std::unique_ptr<juce::FileChooser> presetChooser;

    // geometry used by paint()
    juce::Rectangle<int> gearPlate, gearLcd, sceneRect;
    juce::Point<float> lampAmber, lampRed;
    float lampR = 22.f;

    using SA = juce::AudioProcessorValueTreeState::SliderAttachment;
    using BA = juce::AudioProcessorValueTreeState::ButtonAttachment;
    using CA = juce::AudioProcessorValueTreeState::ComboBoxAttachment;
    std::vector<std::unique_ptr<SA>> sliderAtt;
    std::unique_ptr<BA> limAtt;
    std::vector<std::unique_ptr<CA>> comboAtt;

    float wheelAngle = 0.f, roadOffset = 0.f, rpmShown = 0.f, speedShown = 0.f, boostShown = 0.f;
    int gearShown = 0, purgeTick = 0;
    bool shiftLamp = false, limiterLamp = false, thrKey = false, brkKey = false, pedalGestureOpen = false;
    double lastTime = 0.0;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (F1nzEditor)
};
