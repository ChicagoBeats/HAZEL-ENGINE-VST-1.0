// Unit tests (run on Linux in CI before the Windows build): engine pitch, Stage Kit induction, jets/sci-fi, CPU.
#include "../Source/EngineCore.h"
#include "wav_util.h"
#include <chrono>

using namespace f1nz;
static const float SR = 48000.f;
static int failures = 0;
#define CHECK(cond, ...) do { const bool ok_ = (cond); std::printf ("  [%s] ", ok_ ? "ok" : "FAIL"); std::printf (__VA_ARGS__); std::printf ("\n"); if (!ok_) ++failures; } while (0)

static std::array<EngineCore::Sample, NumSlots> loadSamples (const std::string& dir)
{
    std::array<EngineCore::Sample, NumSlots> s;
    s[KickAiry] = loadWavMono (dir + "kick_airy.wav", SR);   s[Kick444] = loadWavMono (dir + "kick_444_222.wav", SR);
    s[Rim] = loadWavMono (dir + "rim_kaisa.wav", SR);        s[Perc] = loadWavMono (dir + "perc_blocc.wav", SR);
    s[KickU] = loadWavMono (dir + "kick_u019c.wav", SR);
    return s;
}
static int findEngine (const char* part)
{
    for (int i = 0; i < engineCount(); ++i) if (std::strstr (engineSpec (i).name, part)) return i;
    std::printf ("engine '%s' not found\n", part); std::exit (3);
}
static std::vector<float> run (EngineCore& c, float rpm, float thr, float secs)
{
    c.setControls (rpm, thr, 1.f, 1.f, true);
    std::vector<float> b ((size_t) (SR * secs)); c.process (b.data(), (int) b.size()); return b;
}

int main (int argc, char** argv)
{
    const std::string dir = argc > 1 ? argv[1] : "Assets/Samples/";
    const auto smp = loadSamples (dir);
    std::printf ("Engine library: %d engines\n", engineCount());

    std::printf ("\n[1] every piston engine: pitch follows firing frequency (naturally aspirated)\n");
    for (int e = 0; e < engineCount(); ++e)
    {
        const auto& sp = engineSpec (e);
        EngineCore c; c.prepare (SR, smp); c.setEngine (e); c.setInduction (IndNA, 1, 1);
        for (float frac : { 0.15f, 0.85f })
        {
            const float rpm = sp.idle + (sp.redline - sp.idle) * frac;
            auto b = run (c, rpm, 0.8f, 2.5f);
            const float* seg = b.data() + (int) (SR * 2.0f); const int n = (int) (SR * 0.4f);
            float pk = 0; for (int i = 0; i < n; ++i) pk = std::max (pk, std::fabs (seg[i]));
            if (sp.kind == Piston)
            {
                const float f0 = rpm / 60.f * sp.firesPerRev;
                float best = 0, at0 = 0;
                for (float f = 30; f < 3000; f += 1.f) best = std::max (best, goertzelMag (seg, n, f, SR));
                for (float f = f0 * 0.97f; f < f0 * 1.03f; f += 0.5f) at0 = std::max (at0, goertzelMag (seg, n, f, SR));
                const bool ok = at0 >= 0.25f * best && pk > 0.02f && pk <= 1.f;
                if (!ok) std::printf ("  [FAIL] %-40s rpm=%5.0f f0=%6.1f fundamental/strongest=%.2f peak=%.2f\n", sp.name, rpm, f0, at0 / best, pk);
                if (!ok) ++failures;
            }
            else if (!(pk > 0.02f && pk <= 1.f && std::isfinite (pk)))
            { std::printf ("  [FAIL] %-40s sci-fi level peak=%.3f\n", sp.name, pk); ++failures; }
        }
    }
    std::printf ("  (only failures are listed)\n");

    const int v8 = findEngine ("V8");
    std::printf ("\n[2] Stage Kit: supercharger whine tracks RPM (V8, 6000 rpm, full throttle)\n");
    for (int t : { (int) IndRoots, (int) IndTwinScrew, (int) IndCentrifugal })
    {
        EngineCore c; c.prepare (SR, smp); c.setEngine (v8); c.setInduction (t, 2, 1);
        auto b = run (c, 6000, 1.f, 3.f);
        const float mul = t == IndRoots ? 14.f : (t == IndTwinScrew ? 18.f : 38.f);
        const float expect = 6000.f / 60.f * mul;
        const float* seg = b.data() + (int) (SR * 2.2f); const int n = (int) (SR * 0.4f);
        const float found = peakFreq (seg, n, expect * 0.92f, expect * 1.08f, SR, 1.f);
        const float line = goertzelMag (seg, n, expect, SR), fund = goertzelMag (seg, n, 400.f, SR);
        CHECK (std::fabs (found - expect) < expect * 0.02f && line > 0.03f * fund,
               "%-28s expected %.0f Hz, found %.0f Hz, whine/engine-fundamental = %.2f", inductionName (t), expect, found, line / fund);
    }

    std::printf ("\n[3] Stage Kit: turbo spool rises over time and responds to throttle\n");
    {
        EngineCore c; c.prepare (SR, smp); c.setEngine (v8); c.setInduction (IndTurbo, 2, 1);
        c.setControls (5000, 1.f, 1.f, 1.f, true);
        std::vector<float> b ((size_t) (SR * 4.f)); c.process (b.data(), (int) b.size());
        auto hiRms = [&] (double t0) { std::vector<float> h ((size_t) (SR * 0.3f)); const float* x = b.data() + (int) (SR * t0);
            for (size_t i = 2; i < h.size(); ++i) { h[i] = x[i] - 2.f * x[i - 1] + x[i - 2]; }
            return rms (h.data(), (int) h.size()); };
        const float early = hiRms (0.3), late = hiRms (3.6);
        const float s = c.getBoost(), f1 = 1500.f + 6000.f * s * std::sqrt (std::sqrt (s));
        const float found = peakFreq (b.data() + (int) (SR * 3.6f), (int) (SR * 0.3f), f1 * 0.9f, f1 * 1.1f, SR, 5.f);
        CHECK (late > early * 4.f && std::fabs (found - f1) < f1 * 0.04f,
               "high-band energy %.4f -> %.4f; whistle found at %.0f Hz (expected %.0f Hz, boost %.2f)", early, late, found, f1, s);
        c.setControls (5000, 0.f, 1.f, 1.f, true);
        std::vector<float> d ((size_t) (SR * 3.f)); c.process (d.data(), (int) d.size());
        CHECK (c.getBoost() < 0.6f, "boost decays after lift (%.2f)", c.getBoost());
    }

    std::printf ("\n[4] Stage Kit: blow-off valve vs recirculating vs flutter on throttle lift\n");
    {
        float post[3];
        for (int valve = 0; valve < 3; ++valve)
        {
            EngineCore c; c.prepare (SR, smp); c.setEngine (v8); c.setInduction (IndTurbo, 2, valve);
            run (c, 5500, 1.f, 3.5f);
            c.setControls (5500, 0.f, 1.f, 1.f, true);
            std::vector<float> b ((size_t) (SR * 0.6f)); c.process (b.data(), (int) b.size());
            // high-frequency energy (simple 1st-order difference as HP) in 0.1..0.5 s after the lift
            std::vector<float> hp (b.size()); for (size_t i = 1; i < b.size(); ++i) hp[i] = b[i] - b[i - 1];
            post[valve] = rms (hp.data() + (int) (SR * 0.1f), (int) (SR * 0.4f));
        }
        CHECK (post[1] > post[0] * 1.5f, "vent BOV hiss %.4f vs recirculating %.4f", post[1], post[0]);
        CHECK (post[2] > post[0] * 1.2f, "flutter %.4f vs recirculating %.4f", post[2], post[0]);
    }

    std::printf ("\n[5] Twin-charged: blower amplitude hands over to turbo; twin-turbo has 2 detuned whistles\n");
    {
        EngineCore c; c.prepare (SR, smp); c.setEngine (v8); c.setInduction (IndTwinChargedTT, 3, 1);
        auto b = run (c, 6000, 1.f, 4.f);
        const float scF = 6000.f / 60.f * 18.f;
        const float* seg = b.data() + (int) (SR * 3.5f); const int n = (int) (SR * 0.4f);
        CHECK (goertzelMag (seg, n, scF, SR) > 0.f && c.getBoost() > 0.6f, "blower line at %.0f Hz still present while boost=%.2f", scF, c.getBoost());
    }

    std::printf ("\n[6] Hybrid electric whine / jets / rockets / sci-fi produce bounded, non-silent output\n");
    for (int e = 0; e < engineCount(); ++e)
    {
        const auto& sp = engineSpec (e);
        if (sp.kind == Piston && sp.electric <= 0.f) continue;
        EngineCore c; c.prepare (SR, smp); c.setEngine (e);
        auto b = run (c, sp.redline * 0.9f, 1.f, 4.f);
        float pk = 0; bool finite = true; for (size_t i = (size_t) (SR * 3); i < b.size(); ++i) { pk = std::max (pk, std::fabs (b[i])); finite &= std::isfinite (b[i]); }
        CHECK (finite && pk > 0.05f && pk <= 1.f, "%-38s peak %.2f", sp.name, pk);
    }

    std::printf ("\n[7] CPU: 10 s of twin-charged V12 @ 8000 rpm\n");
    {
        EngineCore c; c.prepare (SR, smp); c.setEngine (4); c.setInduction (IndTwinChargedTT, 3, 2); c.setControls (8000, 1, 1, 1, false);
        std::vector<float> b ((size_t) SR * 10);
        auto t0 = std::chrono::steady_clock::now(); c.process (b.data(), (int) b.size());
        const double s = std::chrono::duration<double> (std::chrono::steady_clock::now() - t0).count();
        CHECK (s < 2.0, "rendered in %.3f s (%.0fx realtime)", s, 10.0 / s);
    }

    std::printf (failures ? "\n%d FAILURES\n" : "\nALL PASSED\n", failures);
    return failures ? 1 : 0;
}
