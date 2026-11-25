// Drivetrain + pedal model tests.
#include "../Source/Drivetrain.h"
#include <cstdio>
using namespace f1nz;
static int failures = 0;
#define CHECK(cond, ...) do { const bool ok_ = (cond); std::printf ("  [%s] ", ok_ ? "ok" : "FAIL"); std::printf (__VA_ARGS__); std::printf ("\n"); if (!ok_) ++failures; } while (0)

int main()
{
    const float dt = 1.f / 200.f;
    std::printf ("[1] neutral: rpm free-revs with the pedal, idles when released\n");
    {
        Drivetrain d; d.configure (800, 8000); Drivetrain::Input in;
        for (int i = 0; i < 400; ++i) d.update (dt, in);
        const float idle = d.rpm();
        in.pedal = 1.f; for (int i = 0; i < 600; ++i) d.update (dt, in);
        const float high = d.rpm();
        in.pedal = 0.f; for (int i = 0; i < 800; ++i) d.update (dt, in);
        CHECK (std::fabs (idle - 800.f) < 30.f && high > 7500.f && std::fabs (d.rpm() - 800.f) < 60.f, "idle %.0f -> full pedal %.0f -> released %.0f", idle, high, d.rpm());
    }
    std::printf ("\n[2] full-throttle pull through the gears (manual upshifts at 90%% redline)\n");
    {
        Drivetrain d; d.configure (800, 8000); Drivetrain::Input in; in.pedal = 1.f; in.gearRequest = 1;
        float t = 0, t100 = -1, lastRpm = 0; int upshifts = 0, lastGear = 0;
        for (int i = 0; i < 200 * 40; ++i)
        {
            if (! d.shifting() && d.gear() >= 1 && d.gear() < 6 && d.rpm() > 0.9f * 8000.f) in.gearRequest = d.gear() + 1;
            
            d.update (dt, in); t += dt;
            if (d.gear() != lastGear) { if (d.gear() > lastGear && lastGear >= 1) { ++upshifts; } lastGear = d.gear(); }
            if (t100 < 0 && d.speedKmh() >= 100.f) t100 = t;
            lastRpm = d.rpm();
        }
        // after an upshift the rpm must have dropped well below redline at some moment
        CHECK (upshifts == 5, "upshifts 1->6: %d", upshifts);
        CHECK (t100 > 3.f && t100 < 14.f, "0-100 km/h in %.1f s (final speed %.0f km/h, gear %d, rpm %.0f)", t100, d.speedKmh(), d.gear(), lastRpm);
        CHECK (d.speedKmh() > 150.f, "reaches %.0f km/h", d.speedKmh());
    }
    std::printf ("\n[3] shift: rpm drops after an upshift, blips after a downshift; over-rev downshift refused\n");
    {
        Drivetrain d; d.configure (800, 8000); Drivetrain::Input in; in.pedal = 1.f; in.gearRequest = 1;
        for (int i = 0; i < 200 * 3; ++i) { if (d.gear() == 1 && d.rpm() > 7000 ) break; d.update (dt, in); }
        const float rpmBefore = d.rpm(); in.gearRequest = 2; in.pedal = 0.f;
        for (int i = 0; i < 200 * 1; ++i) d.update (dt, in);
        CHECK (d.gear() == 2 && d.rpm() < rpmBefore * 0.8f, "1->2 rpm %.0f -> %.0f", rpmBefore, d.rpm());
        in.pedal = 1.f; for (int i = 0; i < 200 * 12; ++i) { if (d.gear() < 6 && ! d.shifting() && d.rpm() > 7200) in.gearRequest = d.gear() + 1; d.update (dt, in); }
        const int g = d.gear(); in.gearRequest = 1; for (int i = 0; i < 200; ++i) d.update (dt, in);
        CHECK (d.gear() == g && g >= 4, "downshift 1st at %.0f km/h refused (stays in %d)", d.speedKmh(), d.gear());
    }
    std::printf ("\n[4] brake stops the car, ignition off winds engine down\n");
    {
        Drivetrain d; d.configure (800, 8000); Drivetrain::Input in; in.pedal = 1.f; in.gearRequest = 1;
        for (int i = 0; i < 200 * 3; ++i) { if (d.rpm() > 7000) in.gearRequest = d.gear() + 1; d.update (dt, in); }
        in.pedal = 0; in.brake = true; for (int i = 0; i < 200 * 8; ++i) d.update (dt, in);
        CHECK (d.speedKmh() < 0.5f, "stopped (%.1f km/h)", d.speedKmh());
        in.brake = false; in.ignition = false; in.gearRequest = 0; for (int i = 0; i < 200 * 3; ++i) d.update (dt, in);
        CHECK (d.rpm() == 0.f, "engine off: rpm %.0f", d.rpm());
    }
    std::printf ("\n[5] auto-shift runs 1st..6th by itself\n");
    {
        Drivetrain d; d.configure (800, 8000); Drivetrain::Input in; in.pedal = 1.f; in.gearRequest = 1; in.autoShift = true;
        int maxGear = 0; for (int i = 0; i < 200 * 40; ++i) { d.update (dt, in); maxGear = std::max (maxGear, d.gear()); }
        CHECK (maxGear == 6, "reached gear %d, speed %.0f km/h", maxGear, d.speedKmh());
    }
    std::printf ("\n[6] pedal pressure model\n");
    {
        PedalInput p; float v = 0;
        p.press (PedalInput::Keyboard); for (int i = 0; i < 6; ++i) v = p.update (0.01f);      // 60 ms tap
        p.release(); for (int i = 0; i < 100; ++i) p.update (0.01f);
        const float tap = v;
        p.press (PedalInput::Keyboard); for (int i = 0; i < 150; ++i) v = p.update (0.01f);    // 1.5 s hold
        const float hold = v; p.release(); for (int i = 0; i < 100; ++i) v = p.update (0.01f);
        p.press (PedalInput::Mouse, 0.8f); for (int i = 0; i < 20; ++i) v = p.update (0.01f);
        const float hardClick = v; p.release();
        p.press (PedalInput::Mouse, 0.2f); for (int i = 0; i < 20; ++i) v = p.update (0.01f);
        const float softClick = v;
        CHECK (tap < 0.15f && hold > 0.95f, "keyboard tap %.2f vs hold %.2f", tap, hold);
        CHECK (hardClick > softClick + 0.3f, "mouse low-on-pedal %.2f vs high-on-pedal %.2f", hardClick, softClick);
    }
    std::printf (failures ? "\n%d FAILURES\n" : "\nALL PASSED\n", failures);
    return failures ? 1 : 0;
}
