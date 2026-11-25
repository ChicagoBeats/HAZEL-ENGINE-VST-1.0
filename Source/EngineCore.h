// F1nz Engine - DSP core (no JUCE dependency; unit-tested on any platform).
//
// Piston engines: every cylinder firing triggers one pitched grain of a kick sample (+ rim/perc layer),
// firing_hz = rpm/60 * firesPerRev, plus a firing-locked tone, soft-clip, HP and a throttle-driven "muffler" LP.
//
// STAGE KIT (induction): supercharger whine (Roots / twin-screw / centrifugal, locked to RPM),
// turbo spool (whistle + airflow, delayed by turbine inertia), twin / quad turbo layering,
// blow-off valve / flutter on throttle lift, twin-charged (blower hands over to the turbo), electric motor whine.
//
// Jets / rockets / sci-fi engines are synthesised (noise + tone stacks) and use RPM as a 0..1 "power" control.
#pragma once
#include <algorithm>
#include <array>
#include <cmath>
#include <cstdint>
#include <vector>

namespace f1nz
{
enum Slot { KickAiry = 0, Kick444, Rim, Perc, KickU, NumSlots };

enum Induction { IndStock = 0, IndNA, IndRoots, IndTwinScrew, IndCentrifugal, IndTurbo, IndTwinTurbo,
                 IndQuadTurbo, IndTwinChargedT, IndTwinChargedTT, kNumInductions };

inline const char* inductionName (int i)
{
    static const char* n[kNumInductions] = { "Per engine (stock)", "Naturally aspirated", "Supercharger - Roots",
        "Supercharger - Twin-screw", "Supercharger - Centrifugal", "Turbo", "Twin-turbo", "Quad-turbo",
        "Twin-charged (SC + turbo)", "Twin-charged (SC + twin-turbo)" };
    return n[std::clamp (i, 0, (int) kNumInductions - 1)];
}

enum Kind { Piston = 0, Turbofan, Rocket, IonDrive, Plasma, WarpCore };
enum Group { GGeneric = 0, GAmerican, GModern, GMonster, GExotic, GJetSpace, GSciFi, kNumGroups };

inline const char* groupName (int g)
{
    static const char* n[kNumGroups] = { "Generic engines", "American muscle & big-blocks", "Modern performance V8s",
        "Monster machines", "Exotic & hypercar", "Jets, land speed & space", "Sci-fi" };
    return n[std::clamp (g, 0, (int) kNumGroups - 1)];
}

struct EngineSpec
{
    const char* name;
    int   group;
    int   kind;
    float firesPerRev;   // firings per crankshaft revolution (4-stroke: cylinders / 2)
    int   kick;          // sample slot for the body of each firing
    int   layer;         // sample slot for the top-end layer
    float layerGain;
    float lowpass;       // "muffler" cutoff (Hz) at full throttle
    float drive;         // soft-clip drive
    float idle;          // rpm
    float redline;       // rpm   (jets/sci-fi: 10000 = full power)
    float jitter;        // timing irregularity (fraction of a period)
    float imbalance;     // per-cylinder level spread (the "burble")
    int   defInd;        // default induction (Induction enum)
    float electric;      // 1 = hybrid electric motor whine
    float spool;         // response time multiplier (jets spool slowly)
    float reheat;        // afterburner amount (turbofan)
    float buzz;          // fan "buzz-saw" amount (turbofan)
};

// tuning values are sound-design settings inspired by the real engines, not official specifications
inline EngineSpec P (const char* n, int g, float fpr, int kick, int layer, float lg, float lp, float drv,
                     float idle, float red, float jit, float imb, int ind, float elec = 0.f)
{ return { n, g, Piston, fpr, kick, layer, lg, lp, drv, idle, red, jit, imb, ind, elec, 1.f, 0.f, 0.f }; }

inline EngineSpec S (const char* n, int g, int kind, float spool, float reheat, float buzz)
{ return { n, g, kind, 4.f, KickAiry, Rim, 0.f, 3000.f, 1.f, 800.f, 10000.f, 0.f, 0.f, IndNA, 0.f, spool, reheat, buzz }; }

inline const std::vector<EngineSpec>& engineList()
{
    static const std::vector<EngineSpec> v = {
        // ---- generic ----
        P ("Inline-4",          GGeneric, 2, Kick444, Rim,  .12f, 4200, 2.0f,  850,  7500, .015f, .10f, IndNA),
        P ("Flat-6",            GGeneric, 3, Kick444, Rim,  .10f, 3500, 2.0f,  900,  8000, .015f, .10f, IndNA),
        P ("V8",                GGeneric, 4, KickAiry,Rim,  .10f, 2200, 2.2f,  750,  6500, .020f, .12f, IndNA),
        P ("V10",               GGeneric, 5, KickAiry,Rim,  .14f, 3000, 2.0f,  850,  8500, .015f, .10f, IndNA),
        P ("V12",               GGeneric, 6, Kick444, Rim,  .18f, 3500, 1.6f,  900,  8500, .010f, .06f, IndNA),
        P ("W16",               GGeneric, 8, Kick444, Perc, .12f, 3000, 1.8f,  800,  7000, .010f, .06f, IndNA),
        P ("Rotary (3-rotor)",  GGeneric, 3, Kick444, Rim,  .30f, 5000, 3.0f, 1000,  9000, .005f, .03f, IndNA),
        P ("Diesel V8",         GGeneric, 4, KickU,   Perc, .08f, 1400, 2.6f,  600,  3500, .040f, .20f, IndTurbo),
        // ---- American muscle & big-blocks ----
        P ("Chevy 396/402 big-block",   GAmerican, 4, KickAiry, Rim,  .10f, 2000, 2.4f,  700, 6000, .030f, .14f, IndNA),
        P ("Chevy 427 big-block",       GAmerican, 4, KickAiry, Rim,  .12f, 2300, 2.4f,  800, 6800, .025f, .14f, IndNA),
        P ("Chevy 454 big-block",       GAmerican, 4, KickAiry, Rim,  .08f, 1900, 2.5f,  650, 5800, .030f, .15f, IndNA),
        P ("Chevy ZZ632 (1,004 hp)",    GAmerican, 4, KickAiry, Rim,  .10f, 2200, 2.6f,  800, 7000, .020f, .12f, IndNA),
        P ("Ford FE 427 side-oiler",    GAmerican, 4, KickAiry, Rim,  .12f, 2400, 2.3f,  800, 6800, .025f, .14f, IndNA),
        P ("Ford 460",                  GAmerican, 4, KickU,    Perc, .08f, 1800, 2.4f,  650, 5200, .035f, .16f, IndNA),
        P ("Chrysler 426 Hemi",         GAmerican, 4, KickAiry, Rim,  .14f, 2600, 2.5f,  850, 6600, .030f, .16f, IndNA),
        P ("Chrysler 440 Six Pack",     GAmerican, 4, KickAiry, Rim,  .10f, 2100, 2.4f,  750, 6000, .030f, .15f, IndNA),
        // ---- modern performance V8s ----
        P ("Dodge Hellcat 6.2 (blown)", GModern, 4, KickAiry, Rim,  .10f, 2600, 2.3f,  750, 6200, .020f, .10f, IndRoots),
        P ("Dodge Demon 170",           GModern, 4, KickAiry, Rim,  .12f, 2800, 2.4f,  800, 6500, .020f, .10f, IndRoots),
        P ("Ford Coyote 5.0",           GModern, 4, Kick444,  Rim,  .12f, 3200, 2.0f,  800, 7500, .018f, .08f, IndNA),
        P ("Ford Voodoo 5.2 flat-plane",GModern, 4, Kick444,  Rim,  .16f, 3800, 1.9f,  900, 8250, .008f, .04f, IndNA),
        P ("Ford Predator 5.2 (blown)", GModern, 4, KickAiry, Rim,  .12f, 3000, 2.2f,  800, 7500, .015f, .08f, IndRoots),
        P ("Ford Godzilla 7.3",         GModern, 4, KickAiry, Rim,  .08f, 2000, 2.4f,  650, 5800, .025f, .12f, IndNA),
        P ("Chevy LS3 6.2",             GModern, 4, KickAiry, Rim,  .10f, 2800, 2.2f,  700, 6600, .020f, .10f, IndNA),
        P ("Corvette LT6 flat-plane",   GModern, 4, Kick444,  Rim,  .16f, 3800, 1.9f,  900, 8600, .008f, .04f, IndNA),
        P ("Corvette LT7 twin-turbo",   GModern, 4, Kick444,  Rim,  .16f, 3600, 2.0f,  900, 8000, .008f, .04f, IndTwinTurbo),
        // ---- monster ----
        P ("Monster truck 565 Hemi (blown methanol)", GMonster, 4, KickU, Perc, .14f, 2400, 2.8f, 1200, 8200, .040f, .18f, IndRoots),
        // ---- exotic & hypercar ----
        P ("Lamborghini Murcielago LP670-4 SV V12", GExotic, 6, Kick444, Rim,  .18f, 3600, 1.7f,  900,  8000, .008f, .05f, IndNA),
        P ("Lamborghini Aventador SVJ V12",         GExotic, 6, Kick444, Rim,  .18f, 4000, 1.7f,  950,  8700, .008f, .05f, IndNA),
        P ("Lamborghini Revuelto V12 hybrid",       GExotic, 6, Kick444, Rim,  .16f, 4200, 1.7f, 1000,  9500, .008f, .04f, IndNA, 1.f),
        P ("Aston Martin Valkyrie V12",             GExotic, 6, Kick444, Rim,  .20f, 5000, 1.6f, 1200, 11100, .004f, .03f, IndNA),
        P ("Ferrari 12Cilindri V12",                GExotic, 6, Kick444, Rim,  .18f, 4600, 1.7f,  950,  9500, .006f, .04f, IndNA),
        P ("Lexus LFA V10",                         GExotic, 5, Kick444, Rim,  .20f, 5000, 1.8f,  900,  9000, .006f, .03f, IndNA),
        P ("Porsche Carrera GT V10",                GExotic, 5, Kick444, Rim,  .18f, 4200, 1.9f,  900,  8400, .008f, .04f, IndNA),
        P ("Audi R8 V10",                           GExotic, 5, Kick444, Rim,  .16f, 3800, 1.9f,  900,  8700, .008f, .04f, IndNA),
        P ("Koenigsegg Jesko twin-turbo V8",        GExotic, 4, Kick444, Rim,  .16f, 3800, 2.0f,  900,  8500, .008f, .04f, IndTwinTurbo),
        P ("Hennessey Venom F5 twin-turbo V8",      GExotic, 4, KickAiry,Rim,  .12f, 3000, 2.2f,  800,  8000, .015f, .08f, IndTwinTurbo),
        P ("Pagani Huayra twin-turbo V12",          GExotic, 6, Kick444, Rim,  .14f, 3600, 1.9f,  900,  7000, .010f, .05f, IndTwinTurbo),
        P ("Bugatti Chiron W16 quad-turbo",         GExotic, 8, Kick444, Perc, .12f, 3000, 1.8f,  800,  7000, .010f, .06f, IndQuadTurbo),
        P ("Bugatti Tourbillon V16 hybrid",         GExotic, 8, Kick444, Rim,  .20f, 4500, 1.7f, 1000,  9000, .006f, .03f, IndNA, 1.f),
        P ("Mercedes-AMG One V6 turbo hybrid",      GExotic, 3, Kick444, Rim,  .18f, 5000, 1.9f, 1200, 11000, .006f, .03f, IndTurbo, 1.f),
        // ---- jets, land speed, space ----
        S ("Airliner turbofan",              GJetSpace, Turbofan, 3.0f, 0.f, 0.0f),
        S ("Fighter jet (afterburner)",      GJetSpace, Turbofan, 2.5f, 1.f, 0.6f),
        S ("ThrustSSC twin-Spey (reheat)",   GJetSpace, Turbofan, 3.5f, 1.f, 0.3f),
        S ("Rocket launch (orbital)",        GJetSpace, Rocket,   3.0f, 0.f, 0.f),
        S ("Land-speed rocket motor",        GJetSpace, Rocket,   1.5f, 0.f, 0.f),
        // ---- sci-fi ----
        S ("Ion drive",                      GSciFi, IonDrive,  2.0f, 0.f, 0.f),
        S ("Plasma reactor",                 GSciFi, Plasma,    2.0f, 0.f, 0.f),
        S ("Warp core",                      GSciFi, WarpCore,  2.5f, 0.f, 0.f),
    };
    return v;
}
inline int engineCount() { return (int) engineList().size(); }
inline const EngineSpec& engineSpec (int i) { return engineList()[(size_t) std::clamp (i, 0, engineCount() - 1)]; }

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
        bovDecay_  = std::exp (-1.0f / (sr_ * 0.22f));
        flutDecay_ = std::exp (-1.0f / (sr_ * 0.50f));
        makeHighpass (35.0f);
        reset();
        setEngine (engine_);
    }

    void reset()
    {
        for (auto& v : voices_) v.active = false;
        phase_ = 0.f; limPhase_ = 0.f; fireIdx_ = 0;
        lp_ = {}; hp_.z1 = hp_.z2 = 0.f; rpm_ = 0.f; rpmTarget_ = 0.f; norm_ = normSm_ = 1.f;
        ts_ = 0.f; thrPeak_ = 0.f; bovEnv_ = flutEnv_ = bovCool_ = 0.f; flutPhase_ = 0.f;
        for (auto& p : ph_) p = 0.f;
        lfo_ = 0.f; thump_ = 0.f; abOn_ = false; crack_ = 0.f; sparkle_ = 0.f;
        sA_ = sR_ = sBH_ = sBB_ = sJ1_ = sJ2_ = sJ3_ = sAB_ = sK1_ = sK2_ = sSp_ = SVF {};
    }

    void setEngine (int idx)
    {
        idx = std::clamp (idx, 0, engineCount() - 1);
        const bool changed = (idx != engine_);
        engine_ = idx;
        const auto& sp = engineSpec (engine_);
        if (changed || ! gainsInit_)
        {
            for (int i = 0; i < 16; ++i) cylGain_[(size_t) i] = 1.0f + sp.imbalance * rnd();
            gainsInit_ = true;
        }
        if (changed) { for (auto& v : voices_) v.active = false; ts_ = 0.f; }
        resolveKit();
        updateFilters (sp);
    }

    void setInduction (int type, int stage, int valve)
    {
        indType_ = std::clamp (type, 0, (int) kNumInductions - 1);
        stage_ = std::clamp (stage, 0, 3);
        valve_ = std::clamp (valve, 0, 2);
        resolveKit();
    }

    void setControls (float rpmTarget, float throttle, float boostLevel, float volume, bool limiter)
    {
        const auto& sp = engineSpec (engine_);
        rpmTarget_ = std::clamp (rpmTarget, 0.f, sp.redline * 1.02f);
        thrTarget_ = std::clamp (throttle, 0.f, 1.f);
        chgTarget_ = std::clamp (boostLevel, 0.f, 1.f);
        volTarget_ = std::clamp (volume, 0.f, 1.f);
        limiter_ = limiter;
    }

    float getRpm() const { return rpm_; }
    float getBoost() const { return ts_; }
    int   getEngine() const { return engine_; }

    void process (float* out, int n)
    {
        const auto& sp = engineSpec (engine_);
        const bool piston = (sp.kind == Piston);

        for (int i = 0; i < n; ++i)
        {
            const float d = rpmTarget_ - rpm_;
            rpm_ += d * (d > 0.f ? coefUp_ : coefDown_) / sp.spool;
            thr_ += (thrTarget_ - thr_) * coefCtl_;
            chg_ += (chgTarget_ - chg_) * coefCtl_;
            vol_ += (volTarget_ - vol_) * coefCtl_;
            if (++filtCount_ >= 32) { filtCount_ = 0; updateFilters (sp); }

            const float n1 = rnd(), n2 = rnd();
            float y;

            if (piston)
            {
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

                float acc = 0.f;
                for (auto& v : voices_)
                {
                    if (! v.active) continue;
                    const int ip = (int) v.pos;
                    const float fr = v.pos - (float) ip;
                    const float s = (ip + 1 < v.len) ? v.data[ip] + (v.data[ip + 1] - v.data[ip]) * fr : v.data[ip];
                    const float t = (float) v.age / (float) v.maxAge;
                    acc += s * (1.f - t * t * (3.f - 2.f * t)) * v.gain;
                    v.pos += v.step;
                    if (++v.age >= v.maxAge || (int) v.pos >= v.len) v.active = false;
                }
                normSm_ += (norm_ - normSm_) * coefCtl_;

                if (rpm_ > 150.f) acc += kFiringTone * std::sin (kTwoPi * phase_);
                y = std::tanh (acc * normSm_ * sp.drive * (0.8f + 0.5f * thr_));
                y = hp_.run (y, hpC_);
                y = lp_.run (y, lpC_);
                y += inductionSample (sp, n1, n2) * chg_;                 // STAGE KIT
                y *= vol_ * 0.7f * (0.6f + 0.4f * thr_);
            }
            else
            {
                y = sciSample (sp, n1, n2);
                y *= vol_ * 0.8f;
            }

            const float fade = std::clamp ((rpm_ - 60.f) / 90.f, 0.f, 1.f);
            out[i] = std::clamp (y * fade, -1.f, 1.f);
        }
    }

private:
    static constexpr float kTwoPi = 6.28318530718f;
    static constexpr float kFiringTone = 0.45f;

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
        float run (float x, const std::array<float, 5>& c)
        {
            const float y = c[0] * x + z1;
            z1 = c[1] * x - c[3] * y + z2;
            z2 = c[2] * x - c[4] * y;
            return y;
        }
    };
    struct SVF   // TPT state-variable filter: lp / bp / hp outputs
    {
        float s1 = 0.f, s2 = 0.f, lp = 0.f, bp = 0.f, hp = 0.f;
        void run (float x, float g, float k)
        {
            const float a1 = 1.f / (1.f + g * (g + k)), a2 = g * a1, a3 = g * a2;
            const float v3 = x - s2, v1 = a1 * s1 + a2 * v3, v2 = s2 + a2 * s1 + a3 * v3;
            s1 = 2.f * v1 - s1; s2 = 2.f * v2 - s2;
            lp = v2; bp = v1; hp = x - k * v1 - v2;
        }
    };
    struct Kit { int scType = -1; int turbos = 0; };

    float rnd()  // [-1, 1]
    {
        seed_ ^= seed_ << 13; seed_ ^= seed_ >> 17; seed_ ^= seed_ << 5;
        return (float) (seed_ & 0xFFFFFF) / (float) 0x7FFFFF - 1.0f;
    }
    static float smooth01 (float x) { x = std::clamp (x, 0.f, 1.f); return x * x * (3.f - 2.f * x); }
    static float wrap1 (float p) { return p >= 1.f ? p - 1.f : p; }
    float tanG (float fc) const { return std::tan (3.14159265f * std::clamp (fc, 20.f, 0.45f * sr_) / sr_); }

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
        const float rpmN = std::min (1.f, rpm_ / sp.redline);
        const float fc = sp.lowpass * (0.5f + 0.5f * thr_) * (0.8f + 0.4f * rpmN);
        lpC_ = rbj (false, std::clamp (fc, 200.f, 0.45f * sr_), sr_);
        gAir_ = tanG (2000.f + 3500.f * ts_);  gRush_ = tanG (1200.f);
        gBH_ = tanG (2500.f);                  gBB_ = tanG (6500.f);
        const float p = rpmN;
        gJ1_ = tanG (250.f + 1600.f * p);      gJ2_ = tanG (140.f);   gJ3_ = tanG (3500.f);
        gAB_ = tanG (900.f);                   gK1_ = tanG (500.f + 3500.f * p);
        gK2_ = tanG (90.f);                    gSp_ = tanG (5000.f);
    }

    void resolveKit()
    {
        const int t = indType_ == IndStock ? engineSpec (engine_).defInd : indType_;
        kit_ = {};
        switch (t)
        {
            case IndRoots:          kit_.scType = 0; break;
            case IndTwinScrew:      kit_.scType = 1; break;
            case IndCentrifugal:    kit_.scType = 2; break;
            case IndTurbo:          kit_.turbos = 1; break;
            case IndTwinTurbo:      kit_.turbos = 2; break;
            case IndQuadTurbo:      kit_.turbos = 4; break;
            case IndTwinChargedT:   kit_.scType = 1; kit_.turbos = 1; break;
            case IndTwinChargedTT:  kit_.scType = 1; kit_.turbos = 2; break;
            default: break;
        }
        const float base = kit_.turbos <= 1 ? 0.9f : (kit_.turbos == 2 ? 0.6f : 0.45f);
        const float spoolSec = base / (0.75f + 0.15f * (float) stage_);          // bigger stage = faster spool
        tsUp_   = 1.f - std::exp (-1.f / (sr_ * spoolSec));
        tsDown_ = 1.f - std::exp (-1.f / (sr_ * 1.3f));
    }

    static float stageGain (int s) { static const float v[4] = { 0.6f, 0.85f, 1.1f, 1.4f }; return v[s]; }

    // ---- Stage Kit: supercharger + turbo + valve + electric whine ----
    float inductionSample (const EngineSpec& sp, float n1, float n2)
    {
        const bool anySC = kit_.scType >= 0, anyTB = kit_.turbos > 0;
        if (! anySC && ! anyTB && sp.electric <= 0.f) { ts_ = 0.f; return 0.f; }

        const float rpmN = std::clamp (rpm_ / sp.redline, 0.f, 1.f);
        const float sg = stageGain (stage_);
        float out = 0.f;

        sR_.run (n2, gRush_, 1.4f);

        if (anyTB)
        {
            float tgt = std::clamp ((rpmN - 0.18f) / 0.55f, 0.f, 1.f) * std::clamp ((thr_ - 0.12f) / 0.88f, 0.f, 1.f);
            tgt = std::min (1.f, tgt * (0.85f + 0.05f * (float) stage_));
            ts_ += (tgt - ts_) * (tgt > ts_ ? tsUp_ : tsDown_);
        }
        else ts_ = 0.f;

        if (anyTB)
        {
            const float s = ts_, s15 = s * std::sqrt (s);
            const float f1 = 1500.f + 6000.f * s * std::sqrt (std::sqrt (s));
            static const float det[4] = { 0.f, 0.027f, -0.018f, 0.045f };
            const int nT = kit_.turbos;

            // throttle lift -> blow-off valve / flutter
            thrPeak_ = std::max (thr_, thrPeak_ - 1.5f * invSr_);
            bovCool_ = std::max (0.f, bovCool_ - invSr_);
            if (thrPeak_ > 0.55f && thr_ < 0.22f && ts_ > 0.3f && bovCool_ <= 0.f)
            {
                bovCool_ = 1.0f; thrPeak_ = thr_;
                if (valve_ == 0)      bovEnv_ = 0.12f * ts_;                 // recirculating: faint hiss only
                else if (valve_ == 1) bovEnv_ = ts_;                         // vent-to-atmosphere BOV
                else                { flutEnv_ = ts_; bovEnv_ = 0.15f * ts_; } // compressor flutter
            }

            float gate = 1.f, flutNoise = 0.f;
            if (flutEnv_ > 0.001f)
            {
                flutPhase_ = wrap1 (flutPhase_ + 13.f * invSr_);
                const float gp = 0.5f + 0.5f * std::sin (kTwoPi * flutPhase_);
                const float gg = gp * gp;
                gate = 1.f - flutEnv_ * (1.f - gg);
                flutNoise = flutEnv_ * gg * 0.5f;
                flutEnv_ *= flutDecay_;
            }

            float whistle = 0.f;
            for (int k = 0; k < nT; ++k)
            {
                float& p = ph_[(size_t) k];
                p = wrap1 (p + f1 * (1.f + det[k]) * invSr_);
                const float a = kTwoPi * p;
                whistle += std::sin (a) + 0.35f * std::sin (2.f * a);
            }
            out += whistle * (0.055f * sg * s15 / std::sqrt ((float) nT)) * gate * (0.4f + 0.6f * thr_);

            sA_.run (n1, gAir_, 1.25f);
            out += sA_.bp * 0.10f * sg * s15 * (1.f + 0.2f * (float) (nT - 1)) * (0.4f + 0.6f * thr_);
            out += sR_.lp * 0.07f * sg * s * thr_;

            sBH_.run (n1, gBH_, 1.4f); sBB_.run (n2, gBB_, 0.9f);
            out += bovEnv_ * 0.35f * (sBH_.hp + 0.8f * sBB_.bp);
            bovEnv_ *= bovDecay_;
            out += flutNoise * (sBH_.hp + 0.5f * sBB_.bp) * 0.6f;
        }

        if (anySC)
        {
            const int t = kit_.scType;
            const float mul = t == 0 ? 14.f : (t == 1 ? 18.f : 38.f);
            float& p = ph_[4];
            p = wrap1 (p + rpm_ / 60.f * mul * invSr_);
            const float a = kTwoPi * p;
            float amp = sg * (0.25f + 0.75f * thr_) * smooth01 (rpmN) * (t == 0 ? 0.075f : (t == 1 ? 0.065f : 0.05f));
            if (anyTB) amp *= 1.f - 0.6f * ts_;                              // twin-charged: blower hands over to the turbo
            float w;
            if (t == 0)      w = std::sin (a) + 0.55f * std::sin (2.f * a) + 0.30f * std::sin (3.f * a);
            else if (t == 1) w = std::sin (a) + 0.70f * std::sin (2.f * a) + 0.50f * std::sin (3.f * a) + 0.25f * std::sin (5.f * a);
            else             w = std::sin (a) + 0.25f * std::sin (2.f * a);
            out += amp * w;
            out += sR_.lp * sg * thr_ * rpmN * 0.06f;                          // intake growl
            if (t == 2) { sA_.run (n1, tanG (rpm_ / 60.f * mul), 1.0f); out += sA_.bp * amp * 0.5f; }
        }

        if (sp.electric > 0.f)
        {
            const float pwr = thr_ * rpmN;
            float& p = ph_[5];
            p = wrap1 (p + (500.f + 3500.f * pwr) * invSr_);
            const float a = kTwoPi * p;
            out += 0.03f * (0.2f + 0.8f * pwr) * (std::sin (a) + 0.3f * std::sin (2.f * a));
        }
        return out;
    }

    // ---- jets, rockets, sci-fi ----
    float sciSample (const EngineSpec& sp, float n1, float n2)
    {
        const float p = std::clamp (rpm_ / sp.redline, 0.f, 1.f);
        const float sq = std::sqrt (p);
        float y = 0.f;
        lfo_ = wrap1 (lfo_ + 0.35f * invSr_);

        switch (sp.kind)
        {
            case Turbofan:
            {
                const float f = 300.f + 2200.f * p;
                ph_[0] = wrap1 (ph_[0] + f * invSr_);
                ph_[1] = wrap1 (ph_[1] + f * 0.5f * invSr_);
                const float a = kTwoPi * ph_[0], b = kTwoPi * ph_[1];
                float tone = std::sin (a) + 0.4f * std::sin (2.f * a) + 0.2f * std::sin (3.f * a);
                tone += sp.buzz * smooth01 ((p - 0.7f) / 0.3f) * (std::sin (b) + 0.6f * std::sin (b * 0.5f + 1.3f));
                sJ1_.run (n1, gJ1_, 1.2f); sJ2_.run (n2, gJ2_, 1.4f); sJ3_.run (n1, gJ3_, 1.0f);
                y += 0.07f * sq * tone;
                y += sJ1_.lp * 0.9f * std::pow (p, 1.2f);
                y += sJ2_.lp * 1.2f * p;
                y += sJ3_.bp * 0.15f * p;

                const float ab = sp.reheat * smooth01 ((thr_ - 0.8f) / 0.15f) * smooth01 ((p - 0.6f) / 0.3f);
                if (ab > 0.5f && ! abOn_) { abOn_ = true; thump_ = 1.f; }
                if (ab < 0.3f) abOn_ = false;
                sAB_.run (n2, gAB_, 1.4f);
                y += sAB_.lp * (0.5f + 0.8f * std::fabs (n1)) * 1.4f * ab;
                if (thump_ > 0.001f)
                {
                    ph_[2] = wrap1 (ph_[2] + 45.f * invSr_);
                    y += 0.6f * thump_ * std::sin (kTwoPi * ph_[2]);
                    thump_ *= std::exp (-1.f / (sr_ * 0.25f));
                }
                y *= 0.7f + 0.3f * thr_;
                break;
            }
            case Rocket:
            {
                sK1_.run (n1, gK1_, 1.2f); sK2_.run (n2, gK2_, 1.4f);
                ph_[0] = wrap1 (ph_[0] + (65.f + 30.f * p) * invSr_);
                if (std::fabs (n2) > 0.985f) crack_ = 1.f;
                crack_ *= 0.995f;
                y += sK1_.lp * 0.9f * p;
                y += sK2_.lp * 1.6f * p;
                y += 0.18f * p * std::sin (kTwoPi * ph_[0]);
                y += n1 * crack_ * 0.35f * p;
                y *= 0.6f + 0.4f * thr_;
                break;
            }
            case IonDrive:
            {
                const float f0 = (55.f + 85.f * p) * (1.f + 0.01f * std::sin (kTwoPi * lfo_));
                static const float ratio[5] = { 1.f, 2.f, 3.003f, 5.f, 0.5f };
                static const float amp[5]   = { 0.5f, 0.25f, 0.15f, 0.08f, 0.4f };
                for (int k = 0; k < 5; ++k)
                {
                    ph_[(size_t) k] = wrap1 (ph_[(size_t) k] + f0 * ratio[k] * invSr_);
                    y += amp[k] * std::sin (kTwoPi * ph_[(size_t) k]);
                }
                sSp_.run (n1, gSp_, 0.8f);
                y = y * 0.28f * (0.15f + 0.85f * sq) + sSp_.bp * 0.06f * p;
                break;
            }
            case Plasma:
            {
                const float f0 = 90.f + 160.f * p;
                static const float ratio[4] = { 1.f, 1.5f, 2.01f, 3.02f };
                static const float amp[4]   = { 0.5f, 0.3f, 0.2f, 0.1f };
                ph_[5] = wrap1 (ph_[5] + (6.f + 10.f * p) * invSr_);
                const float trem = 0.6f + 0.4f * std::sin (kTwoPi * ph_[5]);
                for (int k = 0; k < 4; ++k)
                {
                    ph_[(size_t) k] = wrap1 (ph_[(size_t) k] + f0 * ratio[k] * invSr_);
                    y += amp[k] * std::sin (kTwoPi * ph_[(size_t) k]);
                }
                if (std::fabs (n2) > 0.97f) sparkle_ = 1.f;
                sparkle_ *= 0.99f;
                sSp_.run (n1, gSp_, 0.8f);
                y = y * 0.30f * trem * (0.2f + 0.8f * sq) + sSp_.bp * sparkle_ * 0.4f * p;
                break;
            }
            case WarpCore:
            default:
            {
                const float f = 200.f + 3000.f * p * p;
                ph_[5] = wrap1 (ph_[5] + (1.3f + 5.f * p) * invSr_);
                const float pulse = 0.6f + 0.4f * std::sin (kTwoPi * ph_[5]);
                ph_[0] = wrap1 (ph_[0] + 38.f * invSr_);
                ph_[1] = wrap1 (ph_[1] + 76.f * invSr_);
                ph_[2] = wrap1 (ph_[2] + f * (1.f + 0.004f * std::sin (kTwoPi * lfo_ * 8.f)) * invSr_);
                ph_[3] = wrap1 (ph_[3] + f * 1.007f * invSr_);
                y = 0.35f * std::sin (kTwoPi * ph_[0]) + 0.18f * std::sin (kTwoPi * ph_[1])
                  + p * (0.18f * std::sin (kTwoPi * ph_[2]) + 0.18f * std::sin (kTwoPi * ph_[3]));
                y *= 0.5f * pulse * (0.3f + 0.7f * sq);
                break;
            }
        }
        return std::tanh (y * 1.2f);
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
        const float gn = cylGain_[(size_t) (fireIdx_++ % (uint32_t) cyl)];

        startGrain (sp.kick, r, period * 2.2f, gn);
        startGrain (sp.layer, r * 1.3f, period, gn * sp.layerGain);

        const float kickLen = std::min ((float) samples_[(size_t) sp.kick].size() / r, period * 2.2f);
        norm_ = 1.f / std::sqrt (std::max (1.f, kickLen / period));
    }

    float sr_ = 48000.f, invSr_ = 1.f / 48000.f;
    std::array<Sample, NumSlots> samples_;
    std::array<Voice, 40> voices_;
    std::array<float, 16> cylGain_ { };
    Biquad hp_, lp_;
    std::array<float, 5> hpC_ { 1, 0, 0, 0, 0 }, lpC_ { 1, 0, 0, 0, 0 };
    int engine_ = 2, filtCount_ = 0, indType_ = IndStock, stage_ = 1, valve_ = 1;
    Kit kit_;
    uint32_t seed_ = 0x1234567u, fireIdx_ = 0;
    float rpm_ = 0.f, rpmTarget_ = 0.f, thr_ = 0.6f, thrTarget_ = 0.6f;
    float chg_ = 1.f, chgTarget_ = 1.f, vol_ = 0.7f, volTarget_ = 0.7f;
    float phase_ = 0.f, limPhase_ = 0.f, norm_ = 1.f, normSm_ = 1.f;
    float coefUp_ = 0.f, coefDown_ = 0.f, coefCtl_ = 0.f;
    float tsUp_ = 0.f, tsDown_ = 0.f, bovDecay_ = 0.f, flutDecay_ = 0.f;
    float ts_ = 0.f, thrPeak_ = 0.f, bovEnv_ = 0.f, flutEnv_ = 0.f, bovCool_ = 0.f, flutPhase_ = 0.f;
    float lfo_ = 0.f, thump_ = 0.f, crack_ = 0.f, sparkle_ = 0.f;
    bool abOn_ = false, limiter_ = true, gainsInit_ = false;
    std::array<float, 8> ph_ { };
    float gAir_ = 0.1f, gRush_ = 0.1f, gBH_ = 0.1f, gBB_ = 0.1f, gJ1_ = 0.1f, gJ2_ = 0.1f, gJ3_ = 0.1f,
          gAB_ = 0.1f, gK1_ = 0.1f, gK2_ = 0.1f, gSp_ = 0.1f;
    SVF sA_, sR_, sBH_, sBB_, sJ1_, sJ2_, sJ3_, sAB_, sK1_, sK2_, sSp_;
};
} // namespace f1nz
