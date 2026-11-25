#include "PluginProcessor.h"
#include "PluginEditor.h"
#include "BinaryData.h"

using namespace f1nz;

juce::AudioProcessorValueTreeState::ParameterLayout F1nzProcessor::createLayout()
{
    juce::StringArray engines;
    for (int i = 0; i < kNumEngines; ++i) engines.add (engineSpecs()[i].name);

    std::vector<std::unique_ptr<juce::RangedAudioParameter>> p;
    p.push_back (std::make_unique<juce::AudioParameterChoice> ("engine", "Engine", engines, 2));
    p.push_back (std::make_unique<juce::AudioParameterFloat> ("rpm", "RPM",
                    juce::NormalisableRange<float> (0.f, 10000.f, 1.f), 900.f));
    p.push_back (std::make_unique<juce::AudioParameterFloat> ("throttle", "Throttle",
                    juce::NormalisableRange<float> (0.f, 1.f, 0.001f), 0.6f));
    p.push_back (std::make_unique<juce::AudioParameterFloat> ("charger", "Supercharger",
                    juce::NormalisableRange<float> (0.f, 1.f, 0.001f), 0.f));
    p.push_back (std::make_unique<juce::AudioParameterFloat> ("volume", "Volume",
                    juce::NormalisableRange<float> (0.f, 1.f, 0.001f), 0.7f));
    p.push_back (std::make_unique<juce::AudioParameterBool> ("ignition", "Ignition", true));
    p.push_back (std::make_unique<juce::AudioParameterBool> ("limiter", "Rev Limiter", true));
    p.push_back (std::make_unique<juce::AudioParameterBool> ("midi", "MIDI Drive", false));
    p.push_back (std::make_unique<juce::AudioParameterChoice> ("view", "View",
                    juce::StringArray { "Rim", "Car" }, 0));
    p.push_back (std::make_unique<juce::AudioParameterChoice> ("scene", "Scene",
                    juce::StringArray { "Dyno", "Highway Night", "Highway Day" }, 0));
    return { p.begin(), p.end() };
}

F1nzProcessor::F1nzProcessor()
    : AudioProcessor (BusesProperties().withOutput ("Output", juce::AudioChannelSet::stereo(), true)),
      apvts (*this, nullptr, "STATE", createLayout())
{
    formats.registerBasicFormats();
}

f1nz::EngineCore::Sample F1nzProcessor::decode (const void* data, size_t size, double targetRate)
{
    juce::WavAudioFormat wav;
    std::unique_ptr<juce::AudioFormatReader> reader (
        wav.createReaderFor (new juce::MemoryInputStream (data, size, false), true));
    if (reader == nullptr || reader->lengthInSamples <= 0) return {};

    const int n = (int) reader->lengthInSamples;
    juce::AudioBuffer<float> buf ((int) reader->numChannels, n);
    reader->read (&buf, 0, n, 0, true, true);

    std::vector<float> mono ((size_t) n, 0.f);
    for (int c = 0; c < buf.getNumChannels(); ++c)
        for (int i = 0; i < n; ++i)
            mono[(size_t) i] += buf.getSample (c, i) / (float) buf.getNumChannels();

    const double ratio = reader->sampleRate / targetRate;
    std::vector<float> out ((size_t) ((double) n / ratio));
    for (size_t i = 0; i < out.size(); ++i)
    {
        const double pos = (double) i * ratio;
        const size_t k = (size_t) pos;
        const float fr = (float) (pos - (double) k);
        const float a = mono[std::min (k, mono.size() - 1)];
        const float b = mono[std::min (k + 1, mono.size() - 1)];
        out[i] = a + (b - a) * fr;
    }
    float pk = 1.0e-9f;
    for (float v : out) pk = std::max (pk, std::abs (v));
    for (float& v : out) v /= pk;
    return out;
}

void F1nzProcessor::prepareToPlay (double sampleRate, int)
{
    std::array<EngineCore::Sample, NumSlots> s;
    s[KickAiry] = decode (BinaryData::kick_airy_wav,    (size_t) BinaryData::kick_airy_wavSize,    sampleRate);
    s[Kick444]  = decode (BinaryData::kick_444_222_wav, (size_t) BinaryData::kick_444_222_wavSize, sampleRate);
    s[Rim]      = decode (BinaryData::rim_kaisa_wav,    (size_t) BinaryData::rim_kaisa_wavSize,    sampleRate);
    s[Perc]     = decode (BinaryData::perc_blocc_wav,   (size_t) BinaryData::perc_blocc_wavSize,   sampleRate);
    s[KickU]    = decode (BinaryData::kick_u019c_wav,   (size_t) BinaryData::kick_u019c_wavSize,   sampleRate);
    core.prepare (sampleRate, std::move (s));
}

bool F1nzProcessor::isBusesLayoutSupported (const BusesLayout& layouts) const
{
    const auto out = layouts.getMainOutputChannelSet();
    return out == juce::AudioChannelSet::stereo() || out == juce::AudioChannelSet::mono();
}

void F1nzProcessor::handleMidi (juce::MidiBuffer& midi)
{
    for (const auto meta : midi)
    {
        const auto m = meta.getMessage();
        if (m.isNoteOn())
        {
            if (numHeld < (int) held.size()) held[(size_t) numHeld++] = m.getNoteNumber();
            if (! ccThrottle) midiThrottle = 0.4f + 0.6f * m.getFloatVelocity();
        }
        else if (m.isNoteOff())
        {
            for (int i = 0; i < numHeld; ++i)
                if (held[(size_t) i] == m.getNoteNumber())
                {
                    for (int j = i; j < numHeld - 1; ++j) held[(size_t) j] = held[(size_t) j + 1];
                    --numHeld; break;
                }
        }
        else if (m.isController() && m.getControllerNumber() == 1)
        {
            ccThrottle = true;
            midiThrottle = (float) m.getControllerValue() / 127.f;
        }
        else if (m.isAllNotesOff() || m.isAllSoundOff())
            numHeld = 0;
    }
}

void F1nzProcessor::processBlock (juce::AudioBuffer<float>& buffer, juce::MidiBuffer& midi)
{
    juce::ScopedNoDenormals noDenormals;
    handleMidi (midi);

    const int engine = (int) apvts.getRawParameterValue ("engine")->load();
    core.setEngine (engine);
    const auto& sp = engineSpecs()[juce::jlimit (0, kNumEngines - 1, engine)];

    float rpm  = apvts.getRawParameterValue ("rpm")->load();
    float thr  = apvts.getRawParameterValue ("throttle")->load();
    const bool ignition = apvts.getRawParameterValue ("ignition")->load() > 0.5f;
    const bool midiMode = apvts.getRawParameterValue ("midi")->load() > 0.5f;

    if (midiMode)
    {
        if (numHeld > 0)
        {
            const float t = juce::jlimit (0.f, 1.f, ((float) held[(size_t) numHeld - 1] - 36.f) / 48.f);
            rpm = sp.idle + (sp.redline - sp.idle) * t;
            thr = midiThrottle;
        }
        else { rpm = sp.idle; thr = ccThrottle ? midiThrottle : 0.15f; }
    }
    if (! ignition) rpm = 0.f;

    core.setControls (rpm, thr,
                      apvts.getRawParameterValue ("charger")->load(),
                      apvts.getRawParameterValue ("volume")->load(),
                      apvts.getRawParameterValue ("limiter")->load() > 0.5f);

    const int total = buffer.getNumSamples();
    float tmp[512];
    buffer.clear();
    for (int start = 0; start < total; start += 512)
    {
        const int n = std::min (512, total - start);
        core.process (tmp, n);
        for (int ch = 0; ch < buffer.getNumChannels(); ++ch)
            juce::FloatVectorOperations::copy (buffer.getWritePointer (ch) + start, tmp, n);
    }
    uiRpm.store (core.getRpm());
}

juce::AudioProcessorEditor* F1nzProcessor::createEditor() { return new F1nzEditor (*this); }

void F1nzProcessor::getStateInformation (juce::MemoryBlock& dest)
{
    auto state = apvts.copyState();
    if (auto xml = state.createXml()) copyXmlToBinary (*xml, dest);
}

void F1nzProcessor::setStateInformation (const void* data, int size)
{
    if (auto xml = getXmlFromBinary (data, size))
        if (xml->hasTagName (apvts.state.getType()))
            apvts.replaceState (juce::ValueTree::fromXml (*xml));
}

juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter() { return new F1nzProcessor(); }
