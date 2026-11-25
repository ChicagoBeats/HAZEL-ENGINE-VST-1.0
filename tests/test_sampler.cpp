// Sampler tests: rpm-linked pitch, crossfade window, mute/solo, pan, events, granular, throttle layers, sample-rate handling.
#include "../Source/Sampler.h"
#include "wav_util.h"

using namespace f1nz;
static int failures = 0;
#define CHECK(cond, ...) do { const bool ok_ = (cond); std::printf ("  [%s] ", ok_ ? "ok" : "FAIL"); std::printf (__VA_ARGS__); std::printf ("\n"); if (!ok_) ++failures; } while (0)
static const double SR = 48000.0;

static ClipPtr sine (double freq, double clipSr, double secs, float amp = 0.5f, bool stereoRight = false)
{
    auto c = std::make_shared<Clip>(); c->sr = clipSr;
    const size_t n = (size_t) (clipSr * secs); c->l.resize (n);
    for (size_t i = 0; i < n; ++i) c->l[i] = amp * (float) std::sin (2.0 * M_PI * freq * (double) i / clipSr);
    if (stereoRight) c->r = c->l;
    c->finalize(); return c;
}
static std::vector<float> render (SamplerEngine& e, const Control& c, double secs, std::vector<float>* right = nullptr)
{
    const int n = (int) (SR * secs); std::vector<float> L ((size_t) n, 0.f), R ((size_t) n, 0.f);
    for (int i = 0; i < n; i += 256) { const int k = std::min (256, n - i); e.process (L.data() + i, R.data() + i, k, c, SR); }
    if (right) { *right = R; }
    return L;
}

int main()
{
    Control c; c.rpm = 2000; c.redline = 7000; c.throttle = 1.f;
    std::printf ("[1] RPM loop: pitch follows rpm (loop recorded at 2000 rpm = 220 Hz)\n");
    for (float rpm : { 1000.f, 2000.f, 4000.f })
    {
        SamplerEngine e; e.slots[0].setClip (sine (220, SR, 1.0)); e.slots[0].p.refRpm = 2000.f; e.slots[0].p.normalize = false;
        c.rpm = rpm; auto b = render (e, c, 1.5);
        const float f = peakFreq (b.data() + (int) (SR * 0.5), (int) (SR * 0.5), 60, 1000, (float) SR, 1.f);
        CHECK (std::fabs (f - 220.f * rpm / 2000.f) < 3.f, "rpm %.0f -> %.0f Hz (expected %.0f)", rpm, f, 220.f * rpm / 2000.f);
    }
    std::printf ("\n[2] 44.1 kHz clip on a 48 kHz host keeps its pitch\n");
    {
        SamplerEngine e; e.slots[0].setClip (sine (220, 44100.0, 1.0)); e.slots[0].p.refRpm = 2000.f; c.rpm = 2000;
        auto b = render (e, c, 1.0); const float f = peakFreq (b.data() + 4800, 24000, 100, 400, (float) SR, 0.5f);
        CHECK (std::fabs (f - 220.f) < 2.f, "found %.1f Hz", f);
    }
    std::printf ("\n[3] crossfade window (loop A fades out above its range, loop B fades in)\n");
    {
        SamplerEngine e;
        e.slots[0].setClip (sine (200, SR, 1.0)); e.slots[0].p.refRpm = 2000.f; e.slots[0].p.rpmLo = 500.f;  e.slots[0].p.rpmHi = 3000.f; e.slots[0].p.pitchDepth = 0.f;
        e.slots[1].setClip (sine (800, SR, 1.0)); e.slots[1].p.refRpm = 5000.f; e.slots[1].p.rpmLo = 3000.f; e.slots[1].p.rpmHi = 7000.f; e.slots[1].p.pitchDepth = 0.f;
        float aLow, bLow, aHigh, bHigh;
        c.rpm = 2000; auto x = render (e, c, 0.6); aLow = goertzelMag (x.data() + 4800, 24000, 200, (float) SR); bLow = goertzelMag (x.data() + 4800, 24000, 800, (float) SR);
        c.rpm = 5000; x = render (e, c, 0.6);       aHigh = goertzelMag (x.data() + 4800, 24000, 200, (float) SR); bHigh = goertzelMag (x.data() + 4800, 24000, 800, (float) SR);
        CHECK (aLow > 50.f * bLow && bHigh > 50.f * aHigh, "at 2000 rpm A=%.1f B=%.2f | at 5000 rpm A=%.2f B=%.1f", aLow, bLow, aHigh, bHigh);
    }
    std::printf ("\n[4] mute / solo\n");
    {
        SamplerEngine e; e.slots[0].setClip (sine (200, SR, 1.0)); e.slots[1].setClip (sine (600, SR, 1.0));
        for (int i = 0; i < 2; ++i) { e.slots[(size_t) i].p.refRpm = 2000.f; e.slots[(size_t) i].p.pitchDepth = 0.f; }
        c.rpm = 2000; e.slots[0].p.mute = true; auto x = render (e, c, 0.5);
        CHECK (goertzelMag (x.data() + 4800, 12000, 200, (float) SR) < 1.f && goertzelMag (x.data() + 4800, 12000, 600, (float) SR) > 50.f, "mute slot 1 -> only the 600 Hz layer plays");
        e.slots[0].p.mute = false; e.slots[0].p.solo = true; x = render (e, c, 0.5);
        CHECK (goertzelMag (x.data() + 4800, 12000, 600, (float) SR) < 1.f && goertzelMag (x.data() + 4800, 12000, 200, (float) SR) > 50.f, "solo slot 1 -> only the 200 Hz layer plays");
    }
    std::printf ("\n[5] pan (hard left)\n");
    {
        SamplerEngine e; e.slots[0].setClip (sine (300, SR, 1.0)); e.slots[0].p.pan = -1.f; e.slots[0].p.pitchDepth = 0.f;
        std::vector<float> R; auto L = render (e, c, 0.5, &R);
        CHECK (rms (L.data(), (int) L.size()) > 20.f * rms (R.data(), (int) R.size()), "L rms %.3f, R rms %.4f", rms (L.data(), (int) L.size()), rms (R.data(), (int) R.size()));
    }
    std::printf ("\n[6] event one-shot: silent until a gear shift, then plays once\n");
    {
        SamplerEngine e; auto clip = sine (500, SR, 0.2); e.slots[0].setClip (clip); e.slots[0].p.mode = ModeEvent; e.slots[0].p.eventMask = EvShift; e.slots[0].p.normalize = false;
        c.events = 0; auto a = render (e, c, 0.3);
        c.events = EvShift; std::vector<float> L (4800, 0.f), R (4800, 0.f); e.process (L.data(), R.data(), 256, c, SR); c.events = 0;
        for (int i = 256; i < 4800; i += 256) e.process (L.data() + i, R.data() + i, std::min (256, 4800 - i), c, SR);
        auto after = render (e, c, 0.5);
        CHECK (rms (a.data(), (int) a.size()) == 0.f && rms (L.data(), 4800) > 0.1f && rms (after.data() + 7200, (int) after.size() - 7200) < 0.001f,
               "before %.4f | during %.3f | after %.5f", rms (a.data(), (int) a.size()), rms (L.data(), 4800), rms (after.data() + 7200, (int) after.size() - 7200));
        c.events = 0;
    }
    std::printf ("\n[7] granular: audible, bounded, denser at high rpm\n");
    {
        float lvl[2]; int k = 0;
        for (float rpm : { 800.f, 6500.f })
        {
            SamplerEngine e; e.slots[0].setClip (sine (200, SR, 1.0, 0.5f)); e.slots[0].p.mode = ModeGranular; e.slots[0].p.refRpm = 2000.f; e.slots[0].p.pitchDepth = 0.f;
            c.rpm = rpm; auto x = render (e, c, 2.0); float pk = 0; for (float v : x) pk = std::max (pk, std::fabs (v));
            lvl[k++] = rms (x.data() + 48000, 48000); if (pk > 1.5f) ++failures;
        }
        CHECK (lvl[0] > 0.01f && lvl[1] > lvl[0] * 1.5f, "rms at 800 rpm %.3f, at 6500 rpm %.3f", lvl[0], lvl[1]);
    }
    std::printf ("\n[8] on-throttle / off-throttle layers\n");
    {
        SamplerEngine e; e.slots[0].setClip (sine (300, SR, 1.0)); e.slots[0].p.pitchDepth = 0.f; e.slots[0].p.load = LoadOnThrottle;
        e.slots[1].setClip (sine (700, SR, 1.0)); e.slots[1].p.pitchDepth = 0.f; e.slots[1].p.load = LoadOffThrottle;
        for (float thr : { 1.f, 0.f })
        {
            c.throttle = thr; c.rpm = 2000; auto x = render (e, c, 0.5);
            const float on = goertzelMag (x.data() + 4800, 12000, 300, (float) SR), off = goertzelMag (x.data() + 4800, 12000, 700, (float) SR);
            CHECK (thr > 0.5f ? (on > 20.f * off) : (off > 20.f * on), "throttle %.0f: on-layer %.1f off-layer %.1f", thr, on, off);
        }
    }
    std::printf ("\n[9] normalize brings a quiet clip up to ~-1 dB\n");
    {
        SamplerEngine e; e.slots[0].setClip (sine (300, SR, 1.0, 0.05f)); e.slots[0].p.pitchDepth = 0.f; e.slots[0].p.normalize = true; c.rpm = 3000; c.throttle = 1;
        auto x = render (e, c, 0.5); float pk = 0; for (float v : x) pk = std::max (pk, std::fabs (v));
        CHECK (pk > 0.8f && pk < 1.4f, "peak %.2f (input peak 0.05)", pk);
    }
    std::printf (failures ? "\n%d FAILURES\n" : "\nALL PASSED\n", failures);
    return failures ? 1 : 0;
}
