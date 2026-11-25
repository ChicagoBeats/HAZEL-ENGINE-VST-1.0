// F1nz Engine - DSP core. No JUCE dependency so it can be unit-tested anywhere.
//
// Model (same idea as the FLP): every cylinder firing triggers one short grain of a
// kick sample (plus a rim/perc layer). firing_hz = rpm / 60 * firesPerRev.
// Grain pitch follows RPM, grains are cut to ~2.2 firing periods, then soft-clipped,
// high-passed, low-passed ("muffler") and an optional supercharger whine is added.
#pragma once
#include <algorithm>
#include <array>
#include <cmath>
#include <cstdint>
#include <vector>

namespace f1nz
{
enum Slot { KickAiry = 0, Kick444, Rim, Perc, KickU, NumSlots };

struct EngineSpec
{
    const char* name;
    float firesPerRev;   // firings per crankshaft revolution
    int   kick;          // sample slot for the body of each firing
    int   layer;         // sample slot for the top-end layer
    float layerGain;
    float lowpass;       // "muffler" cutoff (Hz) at full throttle
    float drive;         // soft-clip drive
    float idle;          // rpm
    float redline;       // rpm
    float jitter;        // timing irregularity (fraction of a period)
    float imbalance;     // per-cylinder level spread (the "burble")
};

constexpr int kNumEngines = 8;

inline const EngineSpec* engineSpecs()
{
    static const EngineSpec specs[kNumEngines] = {
        { "Inline-4",        2.f, Kick444, Rim,  0.12f, 4200.f, 2.0f,  850.f, 7500.f, 0.015f, 0.10f },
        { "Flat-6",          3.f, Kick444, Rim,  0.10f, 3500.f, 2.0f,  900.f, 8000.f, 0.015f, 0.10f },
        { "V8",              4.f, KickAiry,Rim,  0.10f, 2200.f, 2.2f,  750.f, 6500.f, 0.020f, 0.12f },
        { "V10",             5.f, KickAiry,Rim,  0.14f, 3000.f, 2.0f,  850.f, 8500.f, 0.015f, 0.10f },
        { "V12",             6.f, Kick444, Rim,  0.18f, 3500.f, 1.6f,  900.f, 8500.f, 0.010f, 0.06f },
        { "W16",             8.f, Kick444, Perc, 0.12f, 3000.f, 1.8f,  800.f, 7000.f, 0.010f, 0.06f },
        { "Rotary (3-rotor)",3.f, Kick444, Rim,  0.30f, 5000.f, 3.0f, 1000.f, 9000.f, 0.005f, 0.03f },
        { "Diesel V8",       4.f, KickU,   Perc, 0.08f, 1400.f, 2.6f,  600.f, 3500.f, 0.040f, 0.20f },
    };
    return specs;
}

class EngineCore
{
public:
    using Sample = std::vector<float>;

    void prepare (double sampleRate, std::array<Sample, NumSlots> samples)
    {
        sr_ = (float) sampleRate;
        invSr_ = 1.0f / sr_;
        samples_ = std::move (samples);
        coefUp_   = 1.0f - std::exp (-1.0f / (sr_ * 0.20f));
        coefDown_ = 1.0f - std::exp (-1.0f / (sr_ * 0.45f));
        coefCtl_  = 1.0f - std::exp (-1.0f / (sr_ * 0.05f));
        makeHighpass (35.0f);
        reset();
        setEngine (engine_);
    }

    void reset()
    {
        for (auto& v : voices_) v.active = false;
        phase_ = 0.f; whinePhase_ = 0.f; limPhase_ = 0.f; fireIdx_ = 0;
        lp_ = {}; hp_.z1 = hp_.z2 = 0.f; rpm_ = 0.f;
        rpmTarget_ = 0.f; norm_ = 1.f;
    }

    void setEngine (int idx)
    {
        idx = std::clamp (idx, 0, kNumEngines - 1);
        const bool changed = (idx != engine_);
        engine_ = idx;
        const auto& sp = engineSpecs()[engine_];
        for (int i = 0; i < 16; ++i)
            cylGain_[(size_t) i] = 1.0f + sp.imbalance * rnd();
        if (changed)
            for (auto& v : voices_) v.active = false;
        updateFilters (sp);
    }

    void setControls (float rpmTarget, float throttle, float charger, float volume, bool limiter)
    {
        const auto& sp = engineSpecs()[engine_];
        rpmTarget_ = std::clamp (rpmTarget, 0.f, sp.redline * 1.02f);
        thrTarget_ = std::clamp (throttle, 0.f, 1.f);
        chgTarget_ = std::clamp (charger, 0.f, 1.f);
        volTarget_ = std::clamp (volume, 0.f, 1.f);
        limiter_ = limiter;
    }

    float getRpm() const { return rpm_; }
    int   getEngine() const { return engine_; }

    void process (float* out, int n)
    {
        const auto& sp = engineSpecs()[engine_];
        constexpr float twoPi = 6.28318530718f;

        for (int i = 0; i < n; ++i)
        {
            rpm_ += (rpmTarget_ - rpm_) * (rpmTarget_ > rpm_ ? coefUp_ : coefDown_);
            thr_ += (thrTarget_ - thr_) * coefCtl_;
            chg_ += (chgTarget_ - chg_) * coefCtl_;
            vol_ += (volTarget_ - vol_) * coefCtl_;
            if (++filtCount_ >= 32) { filtCount_ = 0; updateFilters (sp); }

            // rev limiter: cut ignition in a ~14 Hz pattern at the redline
            bool cut = false;
            if (limiter_ && rpmTarget_ >= sp.redline * 0.99f && rpm_ > sp.redline * 0.97f)
            {
                limPhase_ += 14.f * invSr_;
                if (limPhase_ >= 1.f) limPhase_ -= 1.f;
                cut = limPhase_ < 0.45f;
                if (cut) rpm_ -= rpm_ * (2.5f * invSr_);
            }
            else limPhase_ = 0.f;

            // firing scheduler
            if (rpm_ > 150.f)
            {
                const float fireHz = rpm_ / 60.f * sp.firesPerRev;
                phase_ += fireHz * invSr_;
                if (phase_ >= 1.f)
                {
                    phase_ -= 1.f;
                    phase_ += sp.jitter * rnd() * 0.5f;
                    phase_ = std::clamp (phase_, 0.f, 0.99f);
                    if (! cut) trigger (sp, fireHz);
                }
            }

            // sum grains
            float acc = 0.f;
            for (auto& v : voices_)
            {
                if (! v.active) continue;
                const int ip = (int) v.pos;
                const float fr = v.pos - (float) ip;
                const float s = (ip + 1 < v.len) ? v.data[ip] + (v.data[ip + 1] - v.data[ip]) * fr : v.data[ip];
                const float t = (float) v.age / (float) v.maxAge;
                const float env = 1.f - t * t * (3.f - 2.f * t);
                acc += s * env * v.gain;
                v.pos += v.step;
                if (++v.age >= v.maxAge || (int) v.pos >= v.len) v.active = false;
            }
            normSm_ += (norm_ - normSm_) * coefCtl_;

            // firing-locked tone: anchors the perceived pitch at the firing frequency
            if (rpm_ > 150.f) acc += kFiringTone * std::sin (twoPi * phase_);

            float y = std::tanh (acc * normSm_ * sp.drive * (0.8f + 0.5f * thr_));
            y = hp_.run (y, hpC_);
            y = lp_.run (y, lpC_);

            if (rpm_ > 150.f && chg_ > 0.001f)
            {
                whinePhase_ += rpm_ / 60.f * 10.f * invSr_;
                if (whinePhase_ >= 1.f) whinePhase_ -= 1.f;
                const float w = std::sin (twoPi * whinePhase_) + 0.4f * std::sin (2.f * twoPi * whinePhase_);
                y += w * 0.08f * chg_ * (0.3f + 0.7f * thr_);
            }

            const float fade = std::clamp ((rpm_ - 60.f) / 90.f, 0.f, 1.f);
            y *= vol_ * 0.7f * (0.6f + 0.4f * thr_) * fade;
            out[i] = std::clamp (y, -1.f, 1.f);
        }
    }

private:
    struct Voice
    {
        const float* data = nullptr;
        int len = 0, age = 0, maxAge = 1;
        float pos = 0.f, step = 1.f, gain = 0.f;
        bool active = false;
    };
    struct Biquad
    {
        float z1 = 0.f, z2 = 0.f;
        float run (float x, const std::array<float, 5>& c)  // b0 b1 b2 a1 a2 (transposed DF-II)
        {
            const float y = c[0] * x + z1;
            z1 = c[1] * x - c[3] * y + z2;
            z2 = c[2] * x - c[4] * y;
            return y;
        }
    };

    float rnd()  // [-1, 1]
    {
        seed_ ^= seed_ << 13; seed_ ^= seed_ >> 17; seed_ ^= seed_ << 5;
        return (float) (seed_ & 0xFFFFFF) / (float) 0x7FFFFF - 1.0f;
    }

    static std::array<float, 5> rbj (bool high, float fc, float sr)
    {
        const float w0 = 6.28318530718f * fc / sr, c = std::cos (w0), a = std::sin (w0) / (2.f * 0.7071f);
        const float a0 = 1.f + a;
        float b0, b1, b2;
        if (high) { b0 = (1.f + c) / 2.f; b1 = -(1.f + c); b2 = b0; }
        else      { b0 = (1.f - c) / 2.f; b1 =  (1.f - c); b2 = b0; }
        return { b0 / a0, b1 / a0, b2 / a0, (-2.f * c) / a0, (1.f - a) / a0 };
    }
    void makeHighpass (float fc) { hpC_ = rbj (true, fc, sr_); }
    void updateFilters (const EngineSpec& sp)
    {
        const float fc = sp.lowpass * (0.5f + 0.5f * thr_) * (0.8f + 0.4f * std::min (1.f, rpm_ / sp.redline));
        lpC_ = rbj (false, std::clamp (fc, 200.f, 0.45f * sr_), sr_);
    }

    Voice* allocVoice()
    {
        Voice* oldest = &voices_[0];
        for (auto& v : voices_)
        {
            if (! v.active) return &v;
            if (v.age > oldest->age) oldest = &v;
        }
        return oldest;
    }

    void startGrain (int slot, float step, float maxLen, float gain)
    {
        const auto& s = samples_[(size_t) slot];
        if (s.empty()) return;
        Voice* v = allocVoice();
        v->data = s.data(); v->len = (int) s.size();
        v->pos = 0.f; v->step = step; v->age = 0;
        v->maxAge = std::max (8, (int) std::min ((float) s.size() / step, maxLen));
        v->gain = gain; v->active = true;
    }

    void trigger (const EngineSpec& sp, float fireHz)
    {
        const float period = sr_ / fireHz;
        const float span = std::max (1.f, sp.redline - sp.idle);
        const float r = std::clamp (0.8f + (rpm_ - sp.idle) / span * 0.9f, 0.7f, 1.7f);
        const int cyl = std::max (1, (int) std::lround (sp.firesPerRev * 2.f));
        const float g = cylGain_[(size_t) (fireIdx_++ % cyl)];

        startGrain (sp.kick, r, period * 2.2f, g);
        startGrain (sp.layer, r * 1.3f, period, g * sp.layerGain);

        const float kickLen = std::min ((float) samples_[(size_t) sp.kick].size() / r, period * 2.2f);
        norm_ = 1.f / std::sqrt (std::max (1.f, kickLen / period));
    }

    static constexpr float kFiringTone = 0.45f;
    float sr_ = 48000.f, invSr_ = 1.f / 48000.f;
    std::array<Sample, NumSlots> samples_;
    std::array<Voice, 40> voices_;
    std::array<float, 16> cylGain_ { };
    Biquad hp_, lp_;
    std::array<float, 5> hpC_ { 1, 0, 0, 0, 0 }, lpC_ { 1, 0, 0, 0, 0 };
    int engine_ = 2, filtCount_ = 0;
    uint32_t seed_ = 0x1234567u;
    uint32_t fireIdx_ = 0;
    float rpm_ = 0.f, rpmTarget_ = 0.f, thr_ = 0.6f, thrTarget_ = 0.6f;
    float chg_ = 0.f, chgTarget_ = 0.f, vol_ = 0.7f, volTarget_ = 0.7f;
    float phase_ = 0.f, whinePhase_ = 0.f, limPhase_ = 0.f;
    float norm_ = 1.f, normSm_ = 1.f;
    float coefUp_ = 0.f, coefDown_ = 0.f, coefCtl_ = 0.f;
    bool limiter_ = true;
};
} // namespace f1nz
