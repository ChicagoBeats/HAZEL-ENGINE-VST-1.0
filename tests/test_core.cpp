// Unit test for EngineCore: runs on any platform (CI runs it on Linux before the Windows build).
#include "../Source/EngineCore.h"
#include <chrono>
#include <cstdio>
#include <cstring>
#include <fstream>
#include <string>

using namespace f1nz;
static const float SR = 48000.f;

static std::vector<float> loadWav (const std::string& path)
{
    std::ifstream f (path, std::ios::binary);
    if (!f) { std::printf ("cannot open %s\n", path.c_str()); std::exit (2); }
    std::vector<char> b ((std::istreambuf_iterator<char> (f)), {});
    auto u16 = [&] (size_t o) { uint16_t v; std::memcpy (&v, &b[o], 2); return v; };
    auto u32 = [&] (size_t o) { uint32_t v; std::memcpy (&v, &b[o], 4); return v; };
    int tag = 0, ch = 1, bits = 16; uint32_t sr = 48000; size_t dOff = 0, dLen = 0;
    for (size_t p = 12; p + 8 <= b.size();)
    {
        uint32_t len = u32 (p + 4);
        if (!std::memcmp (&b[p], "fmt ", 4)) { tag = u16 (p + 8); ch = u16 (p + 10); sr = u32 (p + 12); bits = u16 (p + 22); }
        if (!std::memcmp (&b[p], "data", 4)) { dOff = p + 8; dLen = std::min<size_t> (len, b.size() - dOff); break; }
        p += 8 + len + (len & 1);
    }
    const int bps = bits / 8; const size_t frames = dLen / (size_t) (bps * ch);
    std::vector<float> mono (frames);
    for (size_t i = 0; i < frames; ++i)
    {
        float acc = 0;
        for (int c = 0; c < ch; ++c)
        {
            size_t o = dOff + (i * (size_t) ch + (size_t) c) * (size_t) bps; float v = 0;
            if (tag == 3 && bits == 32) std::memcpy (&v, &b[o], 4);
            else if (bits == 16) { int16_t s; std::memcpy (&s, &b[o], 2); v = s / 32768.f; }
            acc += v;
        }
        mono[i] = acc / (float) ch;
    }
    // linear resample to SR, normalise to peak 1
    std::vector<float> out ((size_t) ((double) frames * SR / sr));
    for (size_t i = 0; i < out.size(); ++i)
    {
        double p = (double) i * sr / SR; size_t k = (size_t) p; double fr = p - (double) k;
        float a = mono[std::min (k, frames - 1)], c2 = mono[std::min (k + 1, frames - 1)];
        out[i] = (float) (a + (c2 - a) * fr);
    }
    float pk = 1e-9f; for (float v : out) pk = std::max (pk, std::fabs (v));
    for (float& v : out) v /= pk;
    return out;
}

static float goertzel (const float* x, int n, float f)
{
    const double w = 2.0 * M_PI * f / SR, c = 2.0 * std::cos (w); double s1 = 0, s2 = 0;
    for (int i = 0; i < n; ++i) { double h = 0.5 - 0.5 * std::cos (2.0 * M_PI * i / (n - 1)); double s = x[i] * h + c * s1 - s2; s2 = s1; s1 = s; }
    return (float) std::sqrt (s1 * s1 + s2 * s2 - c * s1 * s2);
}

static void writeWav (const char* path, const std::vector<float>& d)
{
    std::ofstream f (path, std::ios::binary); uint32_t n = (uint32_t) d.size() * 2, sr = (uint32_t) SR, br = sr * 2, v16 = 16;
    uint16_t one = 1, bits = 16, ba = 2; uint32_t riff = 36 + n;
    f.write ("RIFF", 4); f.write ((char*) &riff, 4); f.write ("WAVEfmt ", 8); f.write ((char*) &v16, 4);
    f.write ((char*) &one, 2); f.write ((char*) &one, 2); f.write ((char*) &sr, 4); f.write ((char*) &br, 4);
    f.write ((char*) &ba, 2); f.write ((char*) &bits, 2); f.write ("data", 4); f.write ((char*) &n, 4);
    for (float x : d) { int16_t s = (int16_t) (std::max (-1.f, std::min (1.f, x)) * 32767.f); f.write ((char*) &s, 2); }
}

int main (int argc, char** argv)
{
    const std::string dir = argc > 1 ? argv[1] : "Assets/Samples/";
    const bool dump = argc > 2;
    std::array<EngineCore::Sample, NumSlots> smp;
    smp[KickAiry] = loadWav (dir + "kick_airy.wav");   smp[Kick444] = loadWav (dir + "kick_444_222.wav");
    smp[Rim] = loadWav (dir + "rim_kaisa.wav");        smp[Perc] = loadWav (dir + "perc_blocc.wav");
    smp[KickU] = loadWav (dir + "kick_u019c.wav");

    int failures = 0;
    for (int e = 0; e < kNumEngines; ++e)
    {
        const auto& sp = engineSpecs()[e];
        EngineCore core; core.prepare (SR, smp); core.setEngine (e);
        std::vector<float> all;
        for (float frac : { 0.15f, 0.5f, 0.85f })
        {
            const float rpm = sp.idle + (sp.redline - sp.idle) * frac, f0 = rpm / 60.f * sp.firesPerRev;
            core.setControls (rpm, 0.8f, 0.f, 1.f, true);
            std::vector<float> buf ((size_t) (SR * 2.5f));
            core.process (buf.data(), (int) buf.size());
            all.insert (all.end(), buf.begin(), buf.end());
            const float* seg = buf.data() + (int) (SR * 2.0f); const int n = (int) (SR * 0.4f);
            float best = 0, bestF = 0, at0 = 0, pk = 0;
            for (int i = 0; i < n; ++i) pk = std::max (pk, std::fabs (seg[i]));
            for (float f = 30; f < 3000; f += 1.f) { float m = goertzel (seg, n, f); if (m > best) { best = m; bestF = f; } }
            for (float f = f0 * 0.97f; f < f0 * 1.03f; f += 0.5f) at0 = std::max (at0, goertzel (seg, n, f));
            const bool ok = at0 >= 0.25f * best && pk > 0.02f && pk <= 1.0f && std::isfinite (pk);
            std::printf ("%-17s rpm=%6.0f firing=%6.1fHz strongest=%6.1fHz fundamental/strongest=%.2f peak=%.2f  %s\n",
                         sp.name, rpm, f0, bestF, at0 / best, pk, ok ? "OK" : "FAIL");
            if (!ok) ++failures;
        }
        if (dump) { std::string p = std::string ("demo_") + std::to_string (e) + ".wav"; writeWav (p.c_str(), all); }
    }
    // CPU: 10 s of V12 at redline
    EngineCore core; core.prepare (SR, smp); core.setEngine (4); core.setControls (8000, 1, 1, 1, false);
    std::vector<float> buf ((size_t) SR * 10);
    auto t0 = std::chrono::steady_clock::now(); core.process (buf.data(), (int) buf.size());
    double s = std::chrono::duration<double> (std::chrono::steady_clock::now() - t0).count();
    std::printf ("CPU: 10 s of V12 @ 8000 rpm rendered in %.3f s (%.0fx realtime)\n", s, 10.0 / s);
    if (s > 5.0) { std::puts ("FAIL: too slow"); ++failures; }
    std::printf (failures ? "\n%d FAILURES\n" : "\nALL PASSED\n", failures);
    return failures ? 1 : 0;
}
