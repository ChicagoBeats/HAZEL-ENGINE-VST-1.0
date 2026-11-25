// Layer sampler (JUCE-free, unit-tested). 16 slots = 4 tabs x 4 engines, Krotos-Studio style.
// Each slot holds one user sound and plays it in one of four modes:
//   Loop      - fixed-pitch bed (wind, road, tyres...), optional level link to rpm / speed / throttle
//   RPM loop  - engine loop recorded at refRpm: pitch follows rpm, level window lets you crossfade several loops
//   Granular  - grain cloud, density + pitch follow rpm
//   Event     - one-shot on gear shift / throttle lift (BOV) / limiter hit / ignition
#pragma once
#include <algorithm>
#include <array>
#include <atomic>
#include <cmath>
#include <cstdint>
#include <memory>
#include <mutex>
#include <string>
#include <utility>
#include <vector>

namespace f1nz
{
struct Clip
{
    std::vector<float> l, r;      // r empty = mono
    double sr = 48000.0;
    float  normGain = 1.f;        // gain that brings the peak to -1 dB
    std::string name;
    void finalize()
    {
        float pk = 1e-9f;
        for (float v : l) pk = std::max (pk, std::fabs (v));
        for (float v : r) pk = std::max (pk, std::fabs (v));
        normGain = 0.891f / pk;
    }
    size_t size() const { return l.size(); }
};
using ClipPtr = std::shared_ptr<const Clip>;

enum SlotMode { ModeLoop = 0, ModeRpmLoop, ModeGranular, ModeEvent, kNumModes };
enum LevelLink { LinkNone = 0, LinkRpm, LinkSpeed, LinkThrottle, kNumLinks };
enum LoadMode { LoadAlways = 0, LoadOnThrottle, LoadOffThrottle, kNumLoads };
enum Event : uint32_t { EvShift = 1, EvLift = 2, EvLimiter = 4, EvIgnition = 8 };

inline const char* modeName (int m) { static const char* n[kNumModes] = { "Loop", "RPM loop", "Granular", "Event one-shot" }; return n[std::clamp (m, 0, kNumModes - 1)]; }

struct SlotParams
{
    std::atomic<int>   mode { ModeRpmLoop }, link { LinkNone }, load { LoadAlways };
    std::atomic<bool>  mute { false }, solo { false }, loop { true }, normalize { true };
    std::atomic<float> levelDb { 0.f }, pan { 0.f }, pitchSemis { 0.f };
    std::atomic<float> refRpm { 3000.f }, rpmLo { 0.f }, rpmHi { 20000.f }, pitchDepth { 1.f };
    std::atomic<uint32_t> eventMask { EvShift };
};

struct Control { float rpm = 0.f, redline = 7000.f, throttle = 0.f, speedKmh = 0.f; uint32_t events = 0; };

class SamplerSlot
{
public:
    SlotParams p;
    void setClip (ClipPtr c) { std::atomic_store (&clip_, std::move (c)); }
    ClipPtr getClip() const { return std::atomic_load (&clip_); }

    void render (float* outL, float* outR, int n, const Control& c, double hostSr, bool audible, uint32_t& rngState)
    {
        const ClipPtr cl = std::atomic_load (&clip_);
        if (! cl || cl->l.size() < 4) return;
        const int mode = p.mode.load();

        // one-shot events must be tracked even when muted so they don't pile up
        if (mode == ModeEvent && (c.events & p.eventMask.load()) != 0) startShot (*cl);
        if (! audible) { if (mode == ModeEvent) for (auto& s : shots_) s.active = false; return; }

        float gain = std::pow (10.f, p.levelDb.load() / 20.f) * (p.normalize.load() ? cl->normGain : 1.f);
        const float rpmN = std::clamp (c.rpm / std::max (1.f, c.redline), 0.f, 1.f);
        switch (p.link.load())
        {
            case LinkRpm:      gain *= rpmN; break;
            case LinkSpeed:    gain *= std::clamp (c.speedKmh / 200.f, 0.f, 1.f); break;
            case LinkThrottle: gain *= c.throttle; break;
            default: break;
        }
        const int load = p.load.load();
        if (load == LoadOnThrottle) gain *= c.throttle;
        else if (load == LoadOffThrottle) gain *= (1.f - c.throttle);

        const float panv = std::clamp (p.pan.load(), -1.f, 1.f);
        const float a = (panv + 1.f) * 0.78539816f;
        const float gl = std::cos (a) * 1.41421356f, gr = std::sin (a) * 1.41421356f;
        const double srRatio = cl->sr / hostSr;
        const float semis = std::pow (2.f, p.pitchSemis.load() / 12.f);

        if (mode == ModeEvent) { renderShots (outL, outR, n, *cl, gain * gl, gain * gr, srRatio * semis); return; }

        if (mode == ModeRpmLoop || mode == ModeGranular)
        {
            const float ref = std::max (50.f, p.refRpm.load());
            const float lo = p.rpmLo.load(), hi = p.rpmHi.load();
            float w = 1.f;                                   // crossfade window around refRpm
            if (c.rpm < ref && lo > 0.f) w = smooth ((c.rpm - lo) / std::max (1.f, ref - lo));
            else if (c.rpm > ref && hi < 19999.f) w = smooth ((hi - c.rpm) / std::max (1.f, hi - ref));
            gain *= w;
            const float depth = p.pitchDepth.load();
            const float ratioTarget = std::clamp (std::pow (std::max (c.rpm, 1.f) / ref, depth), 0.2f, 5.f) * semis;
            if (mode == ModeRpmLoop) renderLoop (outL, outR, n, *cl, gain * gl, gain * gr, srRatio, ratioTarget);
            else renderGrains (outL, outR, n, *cl, gain * gl, gain * gr, srRatio, ratioTarget, rpmN, rngState, hostSr);
        }
        else renderLoop (outL, outR, n, *cl, gain * gl, gain * gr, srRatio, semis);   // ModeLoop
    }

private:
    struct Shot { double pos = 0; bool active = false; int age = 0; };
    struct Grain { double pos = 0, step = 1; int age = 0, len = 0; float g = 0; bool active = false; };
    static float smooth (float x) { x = std::clamp (x, 0.f, 1.f); return x * x * (3.f - 2.f * x); }
    static float rnd01 (uint32_t& s) { s ^= s << 13; s ^= s >> 17; s ^= s << 5; return (float) (s & 0xFFFFFF) / (float) 0x1000000; }

    static void fetch (const Clip& c, double pos, float& l, float& r)
    {
        const size_t n = c.l.size(); size_t i = (size_t) pos; const float f = (float) (pos - (double) i);
        const size_t j = std::min (i + 1, n - 1); i = std::min (i, n - 1);
        l = c.l[i] + (c.l[j] - c.l[i]) * f;
        if (c.r.empty()) r = l; else r = c.r[i] + (c.r[j] - c.r[i]) * f;
    }

    void renderLoop (float* oL, float* oR, int n, const Clip& c, float gl, float gr, double srRatio, float ratioTarget)
    {
        const bool looping = p.loop.load() || p.mode.load() == ModeRpmLoop;
        const double len = (double) c.l.size();
        if (ratio_ <= 0.f) ratio_ = ratioTarget;
        const float r0 = ratio_;
        for (int i = 0; i < n; ++i)
        {
            const float ratio = r0 + (ratioTarget - r0) * (float) (i + 1) / (float) n;
            float l, r; fetch (c, loopPos_, l, r);
            oL[i] += l * gl; oR[i] += r * gr;
            loopPos_ += srRatio * ratio;
            if (loopPos_ >= len) { if (looping) loopPos_ = std::fmod (loopPos_, len); else loopPos_ = len - 1.0; }
        }
        ratio_ = ratioTarget;
    }

    void renderGrains (float* oL, float* oR, int n, const Clip& c, float gl, float gr, double srRatio, float ratio,
                       float rpmN, uint32_t& rng, double hostSr)
    {
        const double rate = 15.0 + 55.0 * rpmN;                      // grains per second
        const int grainLen = (int) (0.08 * hostSr);
        const double len = (double) c.l.size();
        for (int i = 0; i < n; ++i)
        {
            grainTimer_ -= 1.0;
            if (grainTimer_ <= 0.0)
            {
                grainTimer_ += hostSr / rate;
                Grain* g = &grains_[0];
                for (auto& x : grains_) { if (! x.active) { g = &x; break; } if (x.age > g->age) g = &x; }
                g->pos = rnd01 (rng) * std::max (0.0, len - 2.0 - grainLen * srRatio * ratio);
                g->step = srRatio * ratio; g->age = 0; g->len = std::max (64, grainLen); g->g = 1.f; g->active = true;
            }
            float accL = 0.f, accR = 0.f;
            for (auto& g : grains_)
            {
                if (! g.active) continue;
                const float w = 0.5f - 0.5f * std::cos (6.28318530718f * (float) g.age / (float) g.len);
                float l, r; fetch (c, g.pos, l, r);
                accL += l * w * g.g; accR += r * w * g.g;
                g.pos += g.step;
                if (++g.age >= g.len || g.pos >= len - 1.0) g.active = false;
            }
            oL[i] += accL * gl * 0.5f; oR[i] += accR * gr * 0.5f;
        }
    }

    void startShot (const Clip&)
    {
        Shot* s = &shots_[0];
        for (auto& x : shots_) { if (! x.active) { s = &x; break; } if (x.age > s->age) s = &x; }
        s->pos = 0; s->age = 0; s->active = true;
    }
    void renderShots (float* oL, float* oR, int n, const Clip& c, float gl, float gr, double step)
    {
        const double len = (double) c.l.size();
        for (auto& s : shots_)
        {
            if (! s.active) continue;
            for (int i = 0; i < n && s.active; ++i)
            {
                float l, r; fetch (c, s.pos, l, r);
                oL[i] += l * gl; oR[i] += r * gr;
                s.pos += step; ++s.age;
                if (s.pos >= len - 1.0) s.active = false;
            }
        }
    }

    ClipPtr clip_;
    double loopPos_ = 0.0, grainTimer_ = 0.0;
    float ratio_ = 0.f;
    std::array<Shot, 4> shots_;
    std::array<Grain, 16> grains_;
};

class SamplerEngine
{
public:
    static constexpr int kSlots = 16;
    std::array<SamplerSlot, kSlots> slots;

    void setClip (int slot, ClipPtr c)
    {
        if (slot < 0 || slot >= kSlots) return;
        auto old = slots[(size_t) slot].getClip();
        slots[(size_t) slot].setClip (std::move (c));
        if (old) { std::lock_guard<std::mutex> g (m_); retired_.push_back ({ old, counter_ }); }   // message thread only
    }
    void purgeRetired()   // message thread: free clips the audio thread can no longer be using
    {
        std::lock_guard<std::mutex> g (m_);
        ++counter_;
        retired_.erase (std::remove_if (retired_.begin(), retired_.end(), [this] (auto& e) { return counter_ - e.second > 3; }), retired_.end());
    }

    void process (float* outL, float* outR, int n, const Control& c, double hostSr)
    {
        bool anySolo = false;
        for (auto& s : slots) anySolo |= s.p.solo.load();
        for (size_t i = 0; i < slots.size(); ++i)
        {
            auto& s = slots[i];
            const bool audible = ! s.p.mute.load() && (! anySolo || s.p.solo.load());
            s.render (outL, outR, n, c, hostSr, audible, rng_);
        }
    }

    // audition a clip from the browser (one-shot, mixed on top)
    void preview (ClipPtr c) { std::atomic_store (&prev_, std::move (c)); prevRestart_.store (true); }
    void renderPreview (float* outL, float* outR, int n, double hostSr)
    {
        const ClipPtr c = std::atomic_load (&prev_);
        if (! c || c->l.size() < 4) return;
        if (prevRestart_.exchange (false)) prevPos_ = 0.0;
        const double step = c->sr / hostSr, len = (double) c->l.size();
        for (int i = 0; i < n && prevPos_ < len - 1.0; ++i)
        {
            const size_t k = (size_t) prevPos_; const float f = (float) (prevPos_ - (double) k);
            const float l = c->l[k] + (c->l[k + 1] - c->l[k]) * f;
            const float r = c->r.empty() ? l : c->r[k] + (c->r[k + 1] - c->r[k]) * f;
            outL[i] += l * 0.7f * c->normGain; outR[i] += r * 0.7f * c->normGain;
            prevPos_ += step;
        }
    }

private:
    uint32_t rng_ = 0x9E3779B9u;
    std::mutex m_;
    std::vector<std::pair<ClipPtr, int>> retired_;
    int counter_ = 0;
    ClipPtr prev_;
    std::atomic<bool> prevRestart_ { false };
    double prevPos_ = 1e12;
};
} // namespace f1nz
