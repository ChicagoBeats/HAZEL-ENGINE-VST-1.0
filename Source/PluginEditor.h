#pragma once
#include <juce_audio_utils/juce_audio_utils.h>
#include "PluginProcessor.h"

class F1nzEditor : public juce::AudioProcessorEditor, private juce::Timer
{
public:
    explicit F1nzEditor (F1nzProcessor&);
    ~F1nzEditor() override;
    void paint (juce::Graphics&) override;
    void resized() override;

private:
    void timerCallback() override;
    void paintScene (juce::Graphics&, juce::Rectangle<int>);
    void paintTach (juce::Graphics&, juce::Point<float> centre, float radius);
    void drawRim (juce::Graphics&, float cx, float cy, float diameter, float angle, float blur);
    void styleKnob (juce::Slider&, const juce::String& suffix);

    F1nzProcessor& proc;
    juce::Image rimImg, carImg, bgNight, bgDay;

    juce::ComboBox engineBox, viewBox, sceneBox;
    juce::Slider rpmSlider, throttleSlider, chargerSlider, volumeSlider;
    juce::ToggleButton ignitionBtn { "Ignition" }, limiterBtn { "Rev limiter" }, midiBtn { "MIDI drive" };

    using SA = juce::AudioProcessorValueTreeState::SliderAttachment;
    using BA = juce::AudioProcessorValueTreeState::ButtonAttachment;
    using CA = juce::AudioProcessorValueTreeState::ComboBoxAttachment;
    std::unique_ptr<SA> rpmA, thrA, chgA, volA;
    std::unique_ptr<BA> ignA, limA, midA;
    std::unique_ptr<CA> engA, viewA, sceneA;

    float wheelAngle = 0.f, roadOffset = 0.f, shownRpm = 0.f;
    double lastTime = 0.0;
    static constexpr int kSceneH = 300;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (F1nzEditor)
};
