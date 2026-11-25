// Tiny WAV reader/writer used by the tests and the demo renderer (no dependencies).
#pragma once
#include <algorithm>
#include <cmath>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <fstream>
#include <iterator>
#include <string>
#include <vector>

inline std::vector<float> loadWavMono (const std::string& path, float targetSr)
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
    std::vector<float> out ((size_t) ((double) frames * targetSr / sr));
    for (size_t i = 0; i < out.size(); ++i)
    {
        double p = (double) i * sr / targetSr; size_t k = (size_t) p; double fr = p - (double) k;
        float a = mono[std::min (k, frames - 1)], c2 = mono[std::min (k + 1, frames - 1)];
        out[i] = (float) (a + (c2 - a) * fr);
    }
    float pk = 1e-9f; for (float v : out) pk = std::max (pk, std::fabs (v));
    for (float& v : out) v /= pk;
    return out;
}

inline void writeWav16 (const std::string& path, const std::vector<float>& d, float sr)
{
    std::ofstream f (path, std::ios::binary); uint32_t n = (uint32_t) d.size() * 2, s = (uint32_t) sr, br = s * 2, v16 = 16;
    uint16_t one = 1, bits = 16, ba = 2; uint32_t riff = 36 + n;
    f.write ("RIFF", 4); f.write ((char*) &riff, 4); f.write ("WAVEfmt ", 8); f.write ((char*) &v16, 4);
    f.write ((char*) &one, 2); f.write ((char*) &one, 2); f.write ((char*) &s, 4); f.write ((char*) &br, 4);
    f.write ((char*) &ba, 2); f.write ((char*) &bits, 2); f.write ("data", 4); f.write ((char*) &n, 4);
    for (float x : d) { int16_t q = (int16_t) (std::max (-1.f, std::min (1.f, x)) * 32767.f); f.write ((char*) &q, 2); }
}

inline float goertzelMag (const float* x, int n, float f, float sr)
{
    const double w = 2.0 * M_PI * f / sr, c = 2.0 * std::cos (w); double s1 = 0, s2 = 0;
    for (int i = 0; i < n; ++i) { double h = 0.5 - 0.5 * std::cos (2.0 * M_PI * i / (n - 1)); double s = x[i] * h + c * s1 - s2; s2 = s1; s1 = s; }
    return (float) std::sqrt (s1 * s1 + s2 * s2 - c * s1 * s2);
}
inline float peakFreq (const float* x, int n, float lo, float hi, float sr, float step = 1.f)
{
    float best = 0, bf = 0;
    for (float f = lo; f < hi; f += step) { float m = goertzelMag (x, n, f, sr); if (m > best) { best = m; bf = f; } }
    return bf;
}
inline float rms (const float* x, int n) { double s = 0; for (int i = 0; i < n; ++i) s += (double) x[i] * x[i]; return (float) std::sqrt (s / std::max (1, n)); }
