#pragma once
#include <juce_audio_processors/juce_audio_processors.h>
#include <juce_audio_formats/juce_audio_formats.h>
#include <array>
#include <atomic>
#include "EngineCore.h"
#include "Drivetrain.h"
#include "Sampler.h"

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
    double getTailLengthSeconds() const override { return 2.0; }

    int getNumPrograms() override { return 1; }
    int getCurrentProgram() override { return 0; }
    void setCurrentProgram (int) override {}
    const juce::String getProgramName (int) override { return {}; }
    void changeProgramName (int, const juce::String&) override {}

    void getStateInformation (juce::MemoryBlock& destData) override;
    void setStateInformation (const void* data, int sizeInBytes) override;

    static juce::AudioProcessorValueTreeState::ParameterLayout createLayout();
    juce::AudioProcessorValueTreeState apvts;

    // ---- read by the editor ----
    std::atomic<float> uiRpm { 0.f }, uiSpeed { 0.f }, uiBoost { 0.f };
    std::atomic<int>   uiGear { 0 };
    std::atomic<bool>  uiShiftLamp { false }, uiLimiterLamp { false };

    // ---- sampler (message-thread API) ----
    f1nz::SamplerEngine sampler;
    bool loadSlotFile (int slot, const juce::File& file);
    void clearSlot (int slot);
    juce::String slotPath (int slot) const { return slotPaths[(size_t) slot]; }
    void previewFile (const juce::File& file);
    juce::File getSampleFolder() const { return juce::File (sampleFolder); }
    void setSampleFolder (const juce::File& f) { sampleFolder = f.getFullPathName(); }
    static juce::File defaultSampleFolder();

private:
    f1nz::ClipPtr loadClip (const juce::File& file);
    f1nz::EngineCore::Sample decode (const void* data, size_t size, double targetRate);
    void handleMidi (juce::MidiBuffer& midi);
    juce::XmlElement* samplerToXml() const;
    void samplerFromXml (const juce::XmlElement& xml);

    f1nz::EngineCore core;
    f1nz::Drivetrain drive;
    juce::AudioFormatManager formats;
    juce::Reverb reverb;
    double sr = 48000.0;

    // cached parameter pointers
    std::atomic<float> *pEngine {}, *pInduction {}, *pStage {}, *pValve {}, *pControl {}, *pRpm {}, *pThrottle {},
        *pPedal {}, *pBrake {}, *pGear {}, *pAuto {}, *pIgnition {}, *pLimiter {}, *pBoostSnd {}, *pVolume {},
        *pEngineLvl {}, *pSamplerLvl {}, *pFxMode {}, *pFxMix {}, *pFxLP {};

    // MIDI state
    std::array<int, 16> held {};
    int numHeld = 0;
    float noteVel = 0.f, modWheel = 0.f, aftertouch = -1.f;

    // drivetrain bookkeeping
    int lastGearParam = 0, manualGear = 0, lastEngine = -1, lastInd = -1, lastStage = -1, lastValve = -1;
    bool prevAuto = false, prevShift = false, prevLimiter = false, prevIgnition = false;
    float prevPedal = 0.f, liftCooldown = 0.f;
    float lpStateL = 0.f, lpStateR = 0.f;

    std::array<juce::String, f1nz::SamplerEngine::kSlots> slotPaths;
    juce::String sampleFolder;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (F1nzProcessor)
};
