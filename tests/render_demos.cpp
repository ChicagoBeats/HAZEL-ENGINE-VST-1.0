// Renders Stage Kit demo WAVs: a Revheadz-style drive (idle -> floor it through the gears with auto shifts -> lift -> brake).
// Usage: render_demos <Assets/Samples/> <output-dir>
#include "../Source/EngineCore.h"
#include "../Source/Drivetrain.h"
#include "wav_util.h"
#include <cstdio>

using namespace f1nz;
static const float SR = 48000.f;

struct Demo { const char* file; const char* engine; int induction; int stage; int valve; };

static int findEngine (const char* name)
{
    for (int i = 0; i < engineCount(); ++i) if (std::string (engineSpec (i).name) == name) return i;
    for (int i = 0; i < engineCount(); ++i) if (std::strstr (engineSpec (i).name, name)) return i;
    std::printf ("engine '%s' not found\n", name); std::exit (3);
}

int main (int argc, char** argv)
{
    if (argc < 3) { std::puts ("usage: render_demos <samples-dir> <out-dir>"); return 1; }
    const std::string dir = argv[1], out = argv[2];
    std::array<EngineCore::Sample, NumSlots> smp;
    smp[KickAiry] = loadWavMono (dir + "kick_airy.wav", SR);   smp[Kick444] = loadWavMono (dir + "kick_444_222.wav", SR);
    smp[Rim] = loadWavMono (dir + "rim_kaisa.wav", SR);        smp[Perc] = loadWavMono (dir + "perc_blocc.wav", SR);
    smp[KickU] = loadWavMono (dir + "kick_u019c.wav", SR);

    const Demo demos[] = {
        { "01_V8_naturally_aspirated",       "V8",                         IndNA,           1, 1 },
        { "02_Hellcat_Roots_supercharger",   "Dodge Hellcat",              IndStock,        2, 1 },
        { "03_V8_turbo_BOV_vent",            "V8",                         IndTurbo,        2, 1 },
        { "04_V8_turbo_flutter_stage3",      "V8",                         IndTurbo,        3, 2 },
        { "05_V8_centrifugal_supercharger",  "V8",                         IndCentrifugal,  2, 1 },
        { "06_V8_twin_charged_stage3",       "V8",                         IndTwinChargedTT,3, 1 },
        { "07_Koenigsegg_Jesko_twin_turbo",  "Jesko",                      IndStock,        3, 1 },
        { "08_Bugatti_Chiron_W16_quad_turbo","Chiron",                     IndStock,        3, 1 },
        { "09_Aventador_SVJ_V12",            "Aventador",                  IndStock,        1, 1 },
        { "10_Valkyrie_V12_11100rpm",        "Valkyrie",                   IndStock,        1, 1 },
        { "11_Revuelto_hybrid_V12",          "Revuelto",                   IndStock,        1, 1 },
        { "12_Fighter_jet_afterburner",      "Fighter jet",                IndStock,        1, 1 },
        { "13_ThrustSSC_twin_Spey",          "ThrustSSC",                  IndStock,        1, 1 },
        { "14_Rocket_launch",                "Rocket launch",              IndStock,        1, 1 },
        { "15_Ion_drive_scifi",              "Ion drive",                  IndStock,        1, 1 },
        { "16_Warp_core_scifi",              "Warp core",                  IndStock,        1, 1 },
    };

    for (const auto& d : demos)
    {
        const int e = findEngine (d.engine);
        const auto& sp = engineSpec (e);
        EngineCore core; core.prepare (SR, smp); core.setEngine (e); core.setInduction (d.induction, d.stage, d.valve);
        Drivetrain dt; dt.configure (sp.idle, sp.redline); dt.reset();
        Drivetrain::Input in; in.ignition = true; in.autoShift = true;
        const bool piston = sp.kind == Piston;
        const float total = 16.f; const int block = 256;
        std::vector<float> audio; audio.reserve ((size_t) (SR * total));
        float tmp[256];
        for (int i = 0; i < (int) (SR * total); i += block)
        {
            const float t = (float) i / SR;
            // timeline: idle 1.5 s, floor it, lift at 12 s, brake at 13 s
            in.pedal = (t < 1.5f) ? 0.f : (t < 12.f ? 1.f : 0.f);
            in.brake = t > 13.f;
            in.gearRequest = piston ? (t >= 1.5f ? 1 : 0) : 0;
            if (! piston) in.pedal = (t < 1.0f) ? 0.f : (t < 11.f ? std::min (1.f, (t - 1.f) / 6.f) : 0.f);
            dt.update ((float) block / SR, in);
            core.setControls (dt.rpm(), dt.throttleForSound (in.pedal), 1.f, 0.9f, true);
            core.process (tmp, block);
            audio.insert (audio.end(), tmp, tmp + block);
        }
        writeWav16 (out + "/" + d.file + ".wav", audio, SR);
        std::printf ("%-40s %-38s final speed %3.0f km/h gear %d\n", d.file, sp.name, dt.speedKmh(), dt.gear());
    }
    return 0;
}
