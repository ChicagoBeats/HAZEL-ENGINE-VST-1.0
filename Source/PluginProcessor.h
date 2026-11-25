#pragma once
#include <juce_audio_processors/juce_audio_processors.h>
#include <juce_audio_formats/juce_audio_formats.h>
#include <atomic>
#include <array>
#include "EngineCore.h"

class F1nzProcessor : public juce::AudioProcessor
{
public:
    F1nzProcessor();
    ~F1nzProcessor() override = default;

    void prepareToPlay (double sampleRate, int samplesPerBlock) override;
    void releaseResources() override {}
    bool isBusesLayoutSupported (const BusesLayout& layouts) const override;
    void processBlock (juce::AudioBuffer<float>&, juce::MidiBuffer&) override;

    juce::AudioProcessorEditor* createEditor() override;
    bool hasEditor() const override { return true; }

    const juce::String getName() const override { return "F1nz Engine"; }
    bool acceptsMidi() const override { return true; }
    bool producesMidi() const override { return false; }
    bool isMidiEffect() const override { return false; }
    double getTailLengthSeconds() const override { return 0.6; }

    int getNumPrograms() override { return 1; }
    int getCurrentProgram() override { return 0; }
    void setCurrentProgram (int) override {}
    const juce::String getProgramName (int) override { return {}; }
    void changeProgramName (int, const juce::String&) override {}

    void getStateInformation (juce::MemoryBlock& destData) override;
    void setStateInformation (const void* data, int sizeInBytes) override;

    static juce::AudioProcessorValueTreeState::ParameterLayout createLayout();
    juce::AudioProcessorValueTreeState apvts;

    // read by the editor (message thread)
    std::atomic<float> uiRpm { 0.f };

private:
    f1nz::EngineCore::Sample decode (const void* data, size_t size, double targetRate);
    void handleMidi (juce::MidiBuffer& midi);

    f1nz::EngineCore core;
    juce::AudioFormatManager formats;

    std::array<int, 16> held {};
    int numHeld = 0;
    float midiThrottle = 0.6f;
    bool ccThrottle = false;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (F1nzProcessor)
};
