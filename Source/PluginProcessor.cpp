#include "PluginProcessor.h"
#include "PluginEditor.h"
#include "BinaryData.h"

using namespace f1nz;

juce::AudioProcessorValueTreeState::ParameterLayout F1nzProcessor::createLayout()
{
    juce::StringArray engines;
    for (int i = 0; i < engineCount(); ++i) engines.add (engineSpec (i).name);
    juce::StringArray inductions;
    for (int i = 0; i < kNumInductions; ++i) inductions.add (inductionName (i));

    using F = juce::AudioParameterFloat;
    using NR = juce::NormalisableRange<float>;
    std::vector<std::unique_ptr<juce::RangedAudioParameter>> p;
    p.push_back (std::make_unique<juce::AudioParameterChoice> ("engine", "Engine", engines, 2));
    p.push_back (std::make_unique<juce::AudioParameterChoice> ("induction", "Induction (Stage Kit)", inductions, 0));
    p.push_back (std::make_unique<juce::AudioParameterChoice> ("stage", "Stage", juce::StringArray { "Stock", "Stage 1", "Stage 2", "Stage 3" }, 1));
    p.push_back (std::make_unique<juce::AudioParameterChoice> ("valve", "Turbo valve", juce::StringArray { "Recirculating", "Vent BOV", "Flutter" }, 1));
    p.push_back (std::make_unique<juce::AudioParameterChoice> ("control", "Control mode",
                    juce::StringArray { "Pedal & gearbox", "RPM knob", "MIDI notes" }, 0));
    p.push_back (std::make_unique<F> ("rpm", "RPM", NR (0.f, 12000.f, 1.f), 900.f));
    p.push_back (std::make_unique<F> ("throttle", "Throttle (RPM mode)", NR (0.f, 1.f, 0.001f), 0.6f));
    p.push_back (std::make_unique<F> ("pedal", "Pedal", NR (0.f, 1.f, 0.001f), 0.f));
    p.push_back (std::make_unique<juce::AudioParameterBool> ("brake", "Brake", false));
    p.push_back (std::make_unique<juce::AudioParameterInt> ("gear", "Gear (0 = N)", 0, 6, 0));
    p.push_back (std::make_unique<juce::AudioParameterBool> ("auto", "Auto shift", false));
    p.push_back (std::make_unique<juce::AudioParameterBool> ("ignition", "Ignition", true));
    p.push_back (std::make_unique<juce::AudioParameterBool> ("limiter", "Rev limiter", true));
    p.push_back (std::make_unique<F> ("pedalramp", "Pedal feel (s)", NR (0.2f, 3.f, 0.01f), 0.9f));
    p.push_back (std::make_unique<F> ("charger", "Induction level", NR (0.f, 1.f, 0.001f), 0.9f));
    p.push_back (std::make_unique<F> ("volume", "Master volume", NR (0.f, 1.f, 0.001f), 0.7f));
    p.push_back (std::make_unique<F> ("enginelvl", "Synth engine level", NR (0.f, 1.f, 0.001f), 1.f));
    p.push_back (std::make_unique<F> ("samplerlvl", "Sampler level", NR (0.f, 1.f, 0.001f), 1.f));
    p.push_back (std::make_unique<juce::AudioParameterChoice> ("fxmode", "Quick FX", juce::StringArray { "Off", "Garage", "Tunnel", "Street" }, 0));
    p.push_back (std::make_unique<F> ("fxmix", "FX mix", NR (0.f, 1.f, 0.001f), 0.3f));
    p.push_back (std::make_unique<F> ("fxlp", "FX low-pass (Hz)", NR (500.f, 20000.f, 1.f, 0.35f), 20000.f));
    p.push_back (std::make_unique<juce::AudioParameterChoice> ("view", "View", juce::StringArray { "Rim", "Car" }, 0));
    p.push_back (std::make_unique<juce::AudioParameterChoice> ("scene", "Scene", juce::StringArray { "Dyno", "Highway Night", "Highway Day" }, 0));
    p.push_back (std::make_unique<juce::AudioParameterChoice> ("layout", "Layout", juce::StringArray { "Cockpit", "Showroom", "Studio" }, 0));
    return { p.begin(), p.end() };
}

F1nzProcessor::F1nzProcessor()
    : AudioProcessor (BusesProperties().withOutput ("Output", juce::AudioChannelSet::stereo(), true)),
      apvts (*this, nullptr, "STATE", createLayout())
{
    formats.registerBasicFormats();
    auto get = [this] (const char* id) { return apvts.getRawParameterValue (id); };
    pEngine = get ("engine"); pInduction = get ("induction"); pStage = get ("stage"); pValve = get ("valve");
    pControl = get ("control"); pRpm = get ("rpm"); pThrottle = get ("throttle"); pPedal = get ("pedal");
    pBrake = get ("brake"); pGear = get ("gear"); pAuto = get ("auto"); pIgnition = get ("ignition");
    pLimiter = get ("limiter"); pBoostSnd = get ("charger"); pVolume = get ("volume");
    pEngineLvl = get ("enginelvl"); pSamplerLvl = get ("samplerlvl");
    pFxMode = get ("fxmode"); pFxMix = get ("fxmix"); pFxLP = get ("fxlp");
    sampleFolder = defaultSampleFolder().getFullPathName();
}

juce::File F1nzProcessor::defaultSampleFolder()
{
    auto d = juce::File::getSpecialLocation (juce::File::userDocumentsDirectory).getChildFile ("F1nz Engine").getChildFile ("Sounds");
    d.createDirectory();
    return d;
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
    sr = sampleRate;
    std::array<EngineCore::Sample, NumSlots> s;
    s[KickAiry] = decode (BinaryData::kick_airy_wav,    (size_t) BinaryData::kick_airy_wavSize,    sampleRate);
    s[Kick444]  = decode (BinaryData::kick_444_222_wav, (size_t) BinaryData::kick_444_222_wavSize, sampleRate);
    s[Rim]      = decode (BinaryData::rim_kaisa_wav,    (size_t) BinaryData::rim_kaisa_wavSize,    sampleRate);
    s[Perc]     = decode (BinaryData::perc_blocc_wav,   (size_t) BinaryData::perc_blocc_wavSize,   sampleRate);
    s[KickU]    = decode (BinaryData::kick_u019c_wav,   (size_t) BinaryData::kick_u019c_wavSize,   sampleRate);
    core.prepare (sampleRate, std::move (s));
    reverb.setSampleRate (sampleRate);
    reverb.reset();
    drive.reset();
    lastEngine = lastInd = lastStage = lastValve = -1;
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
            noteVel = m.getFloatVelocity(); aftertouch = -1.f;                       // velocity = how hard you hit the key
        }
        else if (m.isNoteOff())
        {
            for (int i = 0; i < numHeld; ++i)
                if (held[(size_t) i] == m.getNoteNumber())
                {
                    for (int j = i; j < numHeld - 1; ++j) held[(size_t) j] = held[(size_t) j + 1];
                    --numHeld; break;
                }
            if (numHeld == 0) { noteVel = 0.f; aftertouch = -1.f; }
        }
        else if (m.isChannelPressure())  aftertouch = (float) m.getChannelPressureValue() / 127.f;   // pressure after the hit
        else if (m.isAftertouch())       aftertouch = (float) m.getAfterTouchValue() / 127.f;
        else if (m.isController() && m.getControllerNumber() == 1) modWheel = (float) m.getControllerValue() / 127.f;
        else if (m.isAllNotesOff() || m.isAllSoundOff()) { numHeld = 0; noteVel = 0.f; aftertouch = -1.f; }
    }
}

void F1nzProcessor::processBlock (juce::AudioBuffer<float>& buffer, juce::MidiBuffer& midi)
{
    juce::ScopedNoDenormals noDenormals;
    handleMidi (midi);

    // ---- engine / stage kit changes (only when they change) ----
    const int engine = juce::jlimit (0, engineCount() - 1, (int) pEngine->load());
    const int ind = (int) pInduction->load(), stage = (int) pStage->load(), valve = (int) pValve->load();
    const auto& sp = engineSpec (engine);
    if (engine != lastEngine) { core.setEngine (engine); drive.configure (sp.idle, sp.redline); lastEngine = engine; lastInd = -1; }
    if (ind != lastInd || stage != lastStage || valve != lastValve) { core.setInduction (ind, stage, valve); lastInd = ind; lastStage = stage; lastValve = valve; }

    const int mode = (int) pControl->load();
    const bool ignition = pIgnition->load() > 0.5f, autoShift = pAuto->load() > 0.5f, limiter = pLimiter->load() > 0.5f;
    const float indLevel = pBoostSnd->load();
    const float master = pVolume->load() / 0.7f;
    const float engLvl = pEngineLvl->load(), smpLvl = pSamplerLvl->load();

    // manual gear: follow the gear parameter whenever it changes; on leaving auto keep the current gear
    const int gearParam = (int) pGear->load();
    if (gearParam != lastGearParam) { manualGear = gearParam; lastGearParam = gearParam; }
    if (prevAuto && ! autoShift) manualGear = drive.gear();
    prevAuto = autoShift;

    const int total = buffer.getNumSamples();
    buffer.clear();
    float engBuf[128], sL[128], sR[128];
    const int channels = buffer.getNumChannels();

    for (int start = 0; start < total; start += 128)
    {
        const int n = std::min (128, total - start);
        const float dtSec = (float) n / (float) sr;
        float rpmT = 0.f, thr = 0.f, speed = 0.f;
        float pedal = std::max (pPedal->load(), std::max (noteVel, modWheel));
        if (aftertouch >= 0.f && numHeld > 0) pedal = std::max (pedal, aftertouch);

        if (mode == 0)            // ---- Revheadz-style: pedal + gearbox ----
        {
            Drivetrain::Input in; in.pedal = pedal; in.brake = pBrake->load() > 0.5f; in.ignition = ignition;
            in.autoShift = autoShift; in.gearRequest = manualGear;
            drive.update (dtSec, in);
            rpmT = drive.rpm(); thr = drive.throttleForSound (in.pedal); speed = drive.speedKmh();
        }
        else if (mode == 1)       // ---- plain RPM knob ----
        {
            rpmT = ignition ? pRpm->load() : 0.f;
            thr = std::max (pThrottle->load(), pPedal->load());
            speed = rpmT / sp.redline * 135.f;
        }
        else                      // ---- MIDI notes = RPM ----
        {
            if (numHeld > 0)
            {
                const float t = juce::jlimit (0.f, 1.f, ((float) held[(size_t) numHeld - 1] - 36.f) / 48.f);
                rpmT = sp.idle + (sp.redline - sp.idle) * t;
                thr = aftertouch >= 0.f ? aftertouch : std::max (0.4f, noteVel);
            }
            else { rpmT = sp.idle; thr = std::max (0.15f, modWheel); }
            if (! ignition) rpmT = 0.f;
            speed = rpmT / sp.redline * 135.f;
        }

        core.setControls (rpmT, thr, indLevel, 1.f, limiter);
        core.process (engBuf, n);

        // ---- events for the sampler ----
        uint32_t ev = 0;
        const bool shiftNow = mode == 0 && drive.shifting();
        if (shiftNow && ! prevShift) ev |= EvShift;
        prevShift = shiftNow;
        const bool limNow = mode == 0 && drive.limiterLamp();
        if (limNow && ! prevLimiter) ev |= EvLimiter;
        prevLimiter = limNow;
        if (ignition && ! prevIgnition) ev |= EvIgnition;
        prevIgnition = ignition;
        liftCooldown = std::max (0.f, liftCooldown - dtSec);
        if (prevPedal > 0.5f && pedal < 0.1f && core.getRpm() > 0.3f * sp.redline && liftCooldown <= 0.f) { ev |= EvLift; liftCooldown = 0.8f; }
        prevPedal = pedal;

        Control c; c.rpm = core.getRpm(); c.redline = sp.redline; c.throttle = thr; c.speedKmh = speed; c.events = ev;
        for (int i = 0; i < n; ++i) { sL[i] = 0.f; sR[i] = 0.f; }
        sampler.process (sL, sR, n, c, sr);
        sampler.renderPreview (sL, sR, n, sr);

        for (int i = 0; i < n; ++i)
        {
            const float l = (engBuf[i] * engLvl + sL[i] * smpLvl) * master;
            const float r = (engBuf[i] * engLvl + sR[i] * smpLvl) * master;
            if (channels == 1) buffer.getWritePointer (0)[start + i] = 0.5f * (l + r);
            else { buffer.getWritePointer (0)[start + i] = l; buffer.getWritePointer (1)[start + i] = r; }
        }

        if (start + n >= total)
        {
            uiRpm.store (core.getRpm()); uiSpeed.store (speed); uiBoost.store (core.getBoost());
            uiGear.store (mode == 0 ? drive.gear() : 0);
            uiShiftLamp.store (core.getRpm() > 0.84f * sp.redline); uiLimiterLamp.store (limNow);
        }
    }

    // ---- Quick FX: algorithmic reverb (Garage / Tunnel / Street) + low-pass ----
    const int fx = (int) pFxMode->load();
    if (fx > 0 && channels >= 2)
    {
        juce::Reverb::Parameters rp;
        const float mix = pFxMix->load();
        rp.roomSize = fx == 1 ? 0.35f : (fx == 2 ? 0.88f : 0.55f);
        rp.damping  = fx == 1 ? 0.5f  : (fx == 2 ? 0.3f  : 0.7f);
        rp.width = 1.f; rp.freezeMode = 0.f;
        rp.wetLevel = mix * 0.6f; rp.dryLevel = 1.f - 0.5f * mix;
        reverb.setParameters (rp);
        reverb.processStereo (buffer.getWritePointer (0), buffer.getWritePointer (1), total);
    }
    const float lpHz = pFxLP->load();
    if (lpHz < 19500.f)
    {
        const float a = 1.f - std::exp (-6.2831853f * lpHz / (float) sr);
        for (int i = 0; i < total; ++i)
        {
            float* l = buffer.getWritePointer (0);
            lpStateL += a * (l[i] - lpStateL); l[i] = lpStateL;
            if (channels > 1) { float* r = buffer.getWritePointer (1); lpStateR += a * (r[i] - lpStateR); r[i] = lpStateR; }
        }
    }
}

// ------------------------------------------------------------------ sampler management (message thread)
f1nz::ClipPtr F1nzProcessor::loadClip (const juce::File& file)
{
    std::unique_ptr<juce::AudioFormatReader> reader (formats.createReaderFor (file));
    if (reader == nullptr || reader->lengthInSamples <= 0) return nullptr;
    const int len = (int) juce::jmin ((juce::int64) reader->lengthInSamples, (juce::int64) (reader->sampleRate * 180.0));
    const int ch = reader->numChannels > 1 ? 2 : 1;
    juce::AudioBuffer<float> buf (ch, len);
    reader->read (&buf, 0, len, 0, true, true);
    auto clip = std::make_shared<f1nz::Clip>();
    clip->sr = reader->sampleRate;
    clip->l.assign (buf.getReadPointer (0), buf.getReadPointer (0) + len);
    if (ch > 1) clip->r.assign (buf.getReadPointer (1), buf.getReadPointer (1) + len);
    clip->name = file.getFileNameWithoutExtension().toStdString();
    clip->finalize();
    return clip;
}

bool F1nzProcessor::loadSlotFile (int slot, const juce::File& file)
{
    if (slot < 0 || slot >= SamplerEngine::kSlots) return false;
    auto clip = loadClip (file);
    if (clip == nullptr) return false;
    sampler.setClip (slot, clip);
    slotPaths[(size_t) slot] = file.getFullPathName();
    sampler.purgeRetired();
    return true;
}

void F1nzProcessor::clearSlot (int slot)
{
    if (slot < 0 || slot >= SamplerEngine::kSlots) return;
    sampler.setClip (slot, nullptr);
    slotPaths[(size_t) slot] = {};
}

void F1nzProcessor::previewFile (const juce::File& file)
{
    if (auto c = loadClip (file)) sampler.preview (c);
}

juce::XmlElement* F1nzProcessor::samplerToXml() const
{
    auto* x = new juce::XmlElement ("Sampler");
    x->setAttribute ("folder", sampleFolder);
    for (int i = 0; i < SamplerEngine::kSlots; ++i)
    {
        auto& s = sampler.slots[(size_t) i];
        auto* e = x->createNewChildElement ("Slot");
        e->setAttribute ("index", i);
        e->setAttribute ("path", slotPaths[(size_t) i]);
        e->setAttribute ("mode", s.p.mode.load());       e->setAttribute ("link", s.p.link.load());
        e->setAttribute ("load", s.p.load.load());       e->setAttribute ("mute", s.p.mute.load());
        e->setAttribute ("solo", s.p.solo.load());       e->setAttribute ("loop", s.p.loop.load());
        e->setAttribute ("norm", s.p.normalize.load());  e->setAttribute ("level", (double) s.p.levelDb.load());
        e->setAttribute ("pan", (double) s.p.pan.load()); e->setAttribute ("pitch", (double) s.p.pitchSemis.load());
        e->setAttribute ("ref", (double) s.p.refRpm.load()); e->setAttribute ("lo", (double) s.p.rpmLo.load());
        e->setAttribute ("hi", (double) s.p.rpmHi.load());   e->setAttribute ("depth", (double) s.p.pitchDepth.load());
        e->setAttribute ("events", (int) s.p.eventMask.load());
    }
    return x;
}

void F1nzProcessor::samplerFromXml (const juce::XmlElement& x)
{
    if (x.hasAttribute ("folder")) sampleFolder = x.getStringAttribute ("folder");
    for (auto* e : x.getChildWithTagNameIterator ("Slot"))
    {
        const int i = e->getIntAttribute ("index", -1);
        if (i < 0 || i >= SamplerEngine::kSlots) continue;
        auto& s = sampler.slots[(size_t) i];
        s.p.mode = e->getIntAttribute ("mode", ModeRpmLoop);  s.p.link = e->getIntAttribute ("link", 0);
        s.p.load = e->getIntAttribute ("load", 0);            s.p.mute = e->getBoolAttribute ("mute", false);
        s.p.solo = e->getBoolAttribute ("solo", false);       s.p.loop = e->getBoolAttribute ("loop", true);
        s.p.normalize = e->getBoolAttribute ("norm", true);   s.p.levelDb = (float) e->getDoubleAttribute ("level", 0.0);
        s.p.pan = (float) e->getDoubleAttribute ("pan", 0.0); s.p.pitchSemis = (float) e->getDoubleAttribute ("pitch", 0.0);
        s.p.refRpm = (float) e->getDoubleAttribute ("ref", 3000.0); s.p.rpmLo = (float) e->getDoubleAttribute ("lo", 0.0);
        s.p.rpmHi = (float) e->getDoubleAttribute ("hi", 20000.0);  s.p.pitchDepth = (float) e->getDoubleAttribute ("depth", 1.0);
        s.p.eventMask = (uint32_t) e->getIntAttribute ("events", (int) EvShift);
        const juce::String path = e->getStringAttribute ("path");
        if (path.isNotEmpty() && juce::File (path).existsAsFile()) loadSlotFile (i, juce::File (path));
        else clearSlot (i);
    }
}

juce::AudioProcessorEditor* F1nzProcessor::createEditor() { return new F1nzEditor (*this); }

void F1nzProcessor::getStateInformation (juce::MemoryBlock& dest)
{
    auto state = apvts.copyState();
    if (auto xml = state.createXml())
    {
        xml->addChildElement (samplerToXml());
        copyXmlToBinary (*xml, dest);
    }
}

void F1nzProcessor::setStateInformation (const void* data, int size)
{
    if (auto xml = getXmlFromBinary (data, size))
    {
        if (! xml->hasTagName (apvts.state.getType())) return;
        if (auto* smp = xml->getChildByName ("Sampler"))
        {
            samplerFromXml (*smp);
            xml->removeChildElement (smp, true);
        }
        apvts.replaceState (juce::ValueTree::fromXml (*xml));
    }
}

juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter() { return new F1nzProcessor(); }
