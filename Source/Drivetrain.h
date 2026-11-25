// Revheadz-style drivetrain: pedal + gearbox + speed. JUCE-free so it is unit-tested.
//   pedal 0..1  -> throttle        gear 0 = N, 1..6        brake 0/1        ignition on/off
// Engine RPM is physically coupled to road speed through the selected gear (clutch dip on every shift),
// free-revs in neutral, and the fuel cut at the redline drives the limiter lamp + limiter sound.
#pragma once
#include <algorithm>
#include <cmath>

namespace f1nz
{
// Road speed (km/h) reached at redline in each gear
inline const float* gearTopSpeeds() { static const float v[7] = { 0.f, 40.f, 75.f, 105.f, 135.f, 165.f, 200.f }; return v; }

class Drivetrain
{
public:
    struct Input { float pedal = 0.f; bool brake = false; bool ignition = true; bool autoShift = false; int gearRequest = 0; };

    void configure (float idleRpm, float redlineRpm) { idle_ = idleRpm; red_ = redlineRpm; }
    void reset() { rpm_ = 0.f; speed_ = 0.f; gear_ = 0; shiftT_ = 0.f; shifting_ = false; cut_ = false; }

    void update (float dt, const Input& in)
    {
        dt = std::clamp (dt, 0.f, 0.1f);
        const float pedal = in.ignition ? std::clamp (in.pedal, 0.f, 1.f) : 0.f;

        // ---------- gear requests (manual or automatic) ----------
        int want = std::clamp (in.gearRequest, 0, 6);
        if (in.autoShift && gear_ >= 1 && ! shifting_)
        {
            want = gear_;
            if (rpm_ > 0.93f * red_ && gear_ < 6 && pedal > 0.3f) want = gear_ + 1;
            else if (rpm_ < 0.33f * red_ && gear_ > 1 && speed_ > 5.f) want = gear_ - 1;
            else if (pedal > 0.9f && rpm_ < 0.5f * red_ && gear_ > 1) want = gear_ - 1;    // kick-down
        }
        else if (in.autoShift && gear_ == 0) want = in.gearRequest;

        if (! shifting_ && want != gear_)
        {
            // refuse a downshift that would over-rev the engine
            const bool overRev = want >= 1 && speed_ / gearTopSpeeds()[want] * red_ > 1.05f * red_;
            if (! overRev) { pendingGear_ = want; shifting_ = true; shiftT_ = kShiftTime; rpmAtShift_ = rpm_; }
        }

        // ---------- shift (clutch dip) ----------
        bool engaged = (gear_ >= 1) && ! shifting_;
        if (shifting_)
        {
            shiftT_ -= dt;
            if (shiftT_ <= kShiftTime * 0.5f && gear_ != pendingGear_) gear_ = pendingGear_;
            if (shiftT_ <= 0.f) { shifting_ = false; engaged = gear_ >= 1; }
        }

        // ---------- speed ----------
        const float rpmN = std::clamp (rpm_ / red_, 0.f, 1.2f);
        float accel = 0.f;
        if (engaged && in.ignition)
        {
            const float top = gearTopSpeeds()[gear_];
            const float torque = 0.55f + 1.2f * rpmN * (1.1f - rpmN);
            accel += cut_ ? 0.f : pedal * torque * 1500.f / top;
            accel -= (1.f - pedal) * (0.5f + 2.0f * rpmN) * 350.f / top;               // engine braking
        }
        accel -= 1.0f + 0.00006f * speed_ * speed_;                                    // rolling + air drag
        if (in.brake) accel -= 25.f;
        speed_ = std::max(0.f, speed_ + accel * dt);

        // ---------- rpm ----------
        const float freeTarget = in.ignition ? idle_ + pedal * (red_ - idle_) : 0.f;
        float target;
        float tau;
        if (engaged)
        {
            const float coupled = speed_ / gearTopSpeeds()[gear_] * red_;
            const float launchRpm = idle_ + pedal * 0.45f * (red_ - idle_);            // clutch slip off the line
            target = in.ignition ? std::max (coupled, std::min (launchRpm, 0.55f * red_ > coupled ? launchRpm : coupled)) : coupled * 0.f;
            tau = 0.03f;
        }
        else if (shifting_ && gear_ >= 1)
        {
            const float coupled = speed_ / gearTopSpeeds()[gear_] * red_;
            target = in.ignition ? std::max (coupled, idle_) : 0.f;                       // rpm falls/blips to match the new gear
            tau = 0.10f;
        }
        else if (shifting_)                                                             // first half of the dip, still old gear
        {
            const float coupledOld = gear_ >= 1 ? speed_ / gearTopSpeeds()[gear_] * red_ : freeTarget;
            target = in.ignition ? std::max (idle_, std::min (coupledOld, rpmAtShift_)) : 0.f;
            tau = 0.10f;
        }
        else { target = freeTarget; tau = (target > rpm_) ? 0.20f : 0.45f; }           // neutral: free revving

        rpm_ += (target - rpm_) * (1.f - std::exp (-dt / tau));
        if (! in.ignition && rpm_ < 30.f) rpm_ = 0.f;

        // ---------- limiter ----------
        cut_ = in.ignition && rpm_ >= 0.985f * red_ && pedal > 0.5f;
        if (rpm_ > red_) rpm_ = red_;
    }

    float rpm() const { return rpm_; }
    float speedKmh() const { return speed_; }
    int   gear() const { return gear_; }
    bool  shifting() const { return shifting_; }
    bool  shiftLamp() const { return rpm_ > 0.84f * red_; }
    bool  limiterLamp() const { return cut_; }
    bool  limiterActive() const { return cut_; }
    float throttleForSound (float pedal) const { return shifting_ ? 0.f : pedal; }   // throttle cut during a shift

private:
    static constexpr float kShiftTime = 0.28f;
    float idle_ = 800.f, red_ = 7000.f, rpm_ = 0.f, speed_ = 0.f, shiftT_ = 0.f, rpmAtShift_ = 0.f;
    int gear_ = 0, pendingGear_ = 0;
    bool shifting_ = false, cut_ = false;
};

// ---------------------------------------------------------------------------------------------
// Pedal pressure model. A mouse or keyboard has no pressure sensor, so "how hard you push" is modelled as:
//   * mouse   : WHERE on the pedal you press (lower = harder) and how long you hold it
//   * keyboard: hold time - a quick tap is a small blip, holding builds up to full throttle
//   * pen/touch with pressure support: real pressure
//   * MIDI    : velocity / aftertouch (handled in the processor)
class PedalInput
{
public:
    enum Source { None, Mouse, Keyboard, Pressure };

    void setRampSeconds (float s) { ramp_ = std::max (0.05f, s); }

    void press (Source s, float initialDepth = 0.f) { src_ = s; held_ = true; holdT_ = 0.f; initial_ = std::clamp (initialDepth, 0.f, 1.f); depth_ = initial_; }
    void setDepth (float d) { initial_ = std::clamp (d, 0.f, 1.f); }              // live change while held (mouse drag / real pressure)
    void release() { held_ = false; }
    bool isHeld() const { return held_; }

    // returns the pedal position 0..1; call at a fixed rate (e.g. 60 Hz timer or per audio block)
    float update (float dt)
    {
        if (held_)
        {
            holdT_ += dt;
            float target = initial_;
            if (src_ == Keyboard) target = std::max (initial_, std::min (1.f, holdT_ / ramp_));          // build-up
            else if (src_ == Mouse) target = std::max (initial_, std::min (1.f, initial_ + holdT_ / (ramp_ * 3.f)));
            else if (src_ == Pressure) target = initial_;
            const float k = 1.f - std::exp (-dt / 0.03f);
            depth_ += (target - depth_) * k;
        }
        else depth_ += (0.f - depth_) * (1.f - std::exp (-dt / 0.12f));                // foot comes off the pedal
        if (depth_ < 0.002f) depth_ = 0.f;
        return depth_;
    }
    float depth() const { return depth_; }

private:
    Source src_ = None;
    bool held_ = false;
    float holdT_ = 0.f, initial_ = 0.f, depth_ = 0.f, ramp_ = 0.9f;
};
} // namespace f1nz
