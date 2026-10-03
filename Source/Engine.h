#pragma once
#include <juce_dsp/juce_dsp.h>
#include <array>
#include <atomic>
#include <cmath>
#include <vector>
#include "Params.h"

namespace shz
{
constexpr double kPi = 3.14159265358979323846;

inline float dB2g (float db) noexcept { return std::pow (10.0f, db * 0.05f); }
inline float lerpf (float a, float b, float t) noexcept { return a + (b - a) * t; }
inline float expLerp (float a, float b, float t) noexcept { return a * std::pow (b / a, t); }   // a,b > 0

// ------------------------------------------------------------------------------------------
// RT-safe RBJ biquad (transposed direct form II). No allocation when coefficients change.
struct Bq
{
    float b0 = 1, b1 = 0, b2 = 0, a1 = 0, a2 = 0, z1 = 0, z2 = 0;

    inline float process (float x) noexcept
    {
        const float y = b0 * x + z1;
        z1 = b1 * x - a1 * y + z2;
        z2 = b2 * x - a2 * y;
        return y;
    }
    void reset() noexcept { z1 = z2 = 0; }

    void raw (double B0, double B1, double B2, double A0, double A1, double A2) noexcept
    {
        b0 = (float) (B0 / A0); b1 = (float) (B1 / A0); b2 = (float) (B2 / A0);
        a1 = (float) (A1 / A0); a2 = (float) (A2 / A0);
    }

    static double cf (double f, double sr) noexcept { return juce::jlimit (8.0, sr * 0.45, f); }

    void lowpass (double sr, double f, double q = 0.7071) noexcept
    {
        const double w = 2 * kPi * cf (f, sr) / sr, c = std::cos (w), al = std::sin (w) / (2 * q);
        raw ((1 - c) / 2, 1 - c, (1 - c) / 2, 1 + al, -2 * c, 1 - al);
    }
    void highpass (double sr, double f, double q = 0.7071) noexcept
    {
        const double w = 2 * kPi * cf (f, sr) / sr, c = std::cos (w), al = std::sin (w) / (2 * q);
        raw ((1 + c) / 2, -(1 + c), (1 + c) / 2, 1 + al, -2 * c, 1 - al);
    }
    void peak (double sr, double f, double q, double dB) noexcept
    {
        const double A = std::pow (10.0, dB / 40.0), w = 2 * kPi * cf (f, sr) / sr;
        const double c = std::cos (w), al = std::sin (w) / (2 * q);
        raw (1 + al * A, -2 * c, 1 - al * A, 1 + al / A, -2 * c, 1 - al / A);
    }
    void lowShelf (double sr, double f, double q, double dB) noexcept
    {
        const double A = std::pow (10.0, dB / 40.0), w = 2 * kPi * cf (f, sr) / sr;
        const double c = std::cos (w), al = std::sin (w) / (2 * q), be = 2 * std::sqrt (A) * al;
        raw (A * ((A + 1) - (A - 1) * c + be), 2 * A * ((A - 1) - (A + 1) * c), A * ((A + 1) - (A - 1) * c - be),
             (A + 1) + (A - 1) * c + be, -2 * ((A - 1) + (A + 1) * c), (A + 1) + (A - 1) * c - be);
    }
    void highShelf (double sr, double f, double q, double dB) noexcept
    {
        const double A = std::pow (10.0, dB / 40.0), w = 2 * kPi * cf (f, sr) / sr;
        const double c = std::cos (w), al = std::sin (w) / (2 * q), be = 2 * std::sqrt (A) * al;
        raw (A * ((A + 1) + (A - 1) * c + be), -2 * A * ((A - 1) + (A + 1) * c), A * ((A + 1) + (A - 1) * c - be),
             (A + 1) - (A - 1) * c + be, 2 * ((A - 1) - (A + 1) * c), (A + 1) - (A - 1) * c - be);
    }
};

// One-pole parameter smoother
struct Sm
{
    float v = 0, t = 0, k = 0.002f; bool init = false;
    void prepare (double sr, double ms = 12.0) { k = 1.0f - std::exp (-1.0f / (float) (ms * 0.001 * sr)); }
    void set (float x) { t = x; if (! init) { v = x; init = true; } }
    inline float next() noexcept { v += (t - v) * k; return v; }
};

// ------------------------------------------------------------------------------------------
// Noise gate (peak envelope, hysteresis, smoothed gain)
struct Gate
{
    float env = 0, g = 0, dec = 0.999f, atk = 0.01f, rel = 0.001f; bool open = false;
    void prepare (double sr)
    {
        dec = std::exp (-1.0f / (float) (0.030 * sr));
        atk = 1.0f - std::exp (-1.0f / (float) (0.002 * sr));
        rel = 1.0f - std::exp (-1.0f / (float) (0.060 * sr));
    }
    inline float process (float x, float thr, bool on) noexcept
    {
        if (! on) { g += (1.0f - g) * atk; open = true; return x * g; }
        const float a = std::abs (x);
        env = a > env ? a : env * dec;
        if (! open && env > thr) open = true;
        else if (open && env < thr * 0.5f) open = false;
        g += ((open ? 1.0f : 0.0f) - g) * (open ? atk : rel);
        return x * g;
    }
};

// ------------------------------------------------------------------------------------------
// Two-tap crossfaded delay-line pitch shifter (guitar-friendly, ~45 ms window)
struct PitchShifter
{
    std::vector<float> buf; int mask = 0, w = 0; float W = 1, phase = 0;

    void prepare (double sr, float ms = 45.0f)
    {
        W = (float) (ms * 0.001 * sr);
        int n = 1; while (n < (int) (W * 2 + 16)) n <<= 1;
        buf.assign ((size_t) n, 0.0f); mask = n - 1; w = 0; phase = 0;
    }
    void reset() { std::fill (buf.begin(), buf.end(), 0.0f); phase = 0; w = 0; }

    inline float tap (float delay) const noexcept
    {
        const float pos = (float) w - delay;
        const int i = (int) std::floor (pos);
        const float f = pos - (float) i;
        const float xm1 = buf[(size_t) ((i - 1) & mask)], x0 = buf[(size_t) (i & mask)],
                    x1  = buf[(size_t) ((i + 1) & mask)], x2 = buf[(size_t) ((i + 2) & mask)];
        const float c1 = 0.5f * (x1 - xm1), c2 = xm1 - 2.5f * x0 + 2.0f * x1 - 0.5f * x2,
                    c3 = 0.5f * (x2 - xm1) + 1.5f * (x0 - x1);
        return ((c3 * f + c2) * f + c1) * f + x0;
    }

    // ratio = 2^(semitones/12), wet = 0..1
    inline float process (float x, float ratio, float wet) noexcept
    {
        buf[(size_t) w] = x;
        float p2 = phase + 0.5f; if (p2 >= 1.0f) p2 -= 1.0f;
        const float w1 = 0.5f - 0.5f * std::cos (2.0f * (float) kPi * phase);
        const float wetSig = tap (3.0f + phase * W) * w1 + tap (3.0f + p2 * W) * (1.0f - w1);
        const float drySig = tap (3.0f + W * 0.5f);
        phase += (1.0f - ratio) / W;
        if (phase >= 1.0f) phase -= 1.0f; else if (phase < 0.0f) phase += 1.0f;
        w = (w + 1) & mask;
        return drySig + (wetSig - drySig) * wet;
    }
};

// ------------------------------------------------------------------------------------------
// Sympathetic string (damped feedback comb, fractional delay)
struct SympString
{
    std::vector<float> buf; int mask = 0, w = 0; float delay = 100, lp = 0;

    void prepare (double sr, double freq)
    {
        delay = (float) (sr / freq) - 1.0f;
        int n = 1; while (n < (int) delay + 8) n <<= 1;
        buf.assign ((size_t) n, 0.0f); mask = n - 1; w = 0; lp = 0;
    }
    inline float process (float x, float fb, float damp) noexcept
    {
        const float pos = (float) w - delay;
        const int i = (int) std::floor (pos);
        const float f = pos - (float) i;
        const float y = buf[(size_t) (i & mask)] * (1.0f - f) + buf[(size_t) ((i + 1) & mask)] * f;
        lp += damp * (y - lp);
        buf[(size_t) w] = x * (1.0f - fb) + fb * lp;
        w = (w + 1) & mask;
        return y;
    }
};

struct SitarPedal
{
    static constexpr int N = 8;
    // Sa Pa Sa' + Bhairav-ish scale tones (C): C3 G3 C4 Db4 E4 F4 G4 Ab4
    static constexpr double freqs[N] = { 130.81, 196.00, 261.63, 277.18, 329.63, 349.23, 392.00, 415.30 };
    std::array<SympString, N> strings; Bq hp, presence, tone; double sr = 44100;

    void prepare (double s) { sr = s; for (int i = 0; i < N; ++i) strings[(size_t) i].prepare (s, freqs[i]); hp.reset(); presence.reset(); tone.reset(); }
    void update (float buzzK, float toneKnob)
    {
        juce::ignoreUnused (buzzK);
        hp.highpass (sr, 120.0);
        presence.peak (sr, 3200.0, 1.0, 6.0);
        tone.lowpass (sr, expLerp (1500.0f, 9000.0f, toneKnob * 0.1f));
    }
    inline float process (float x, float buzz, float res, float mix) noexcept
    {
        // jawari buzz: bright pre-emphasis into a soft wave-folder
        const float k = 1.0f + buzz * 0.35f;
        const float emph = presence.process (hp.process (x));
        float buzzed = std::sin (1.3f * std::tanh (k * emph));
        buzzed = tone.process (buzzed);
        const float b = buzz * 0.1f;
        const float main = x + b * (buzzed - x);

        const float fb = 0.90f + 0.0985f * (res * 0.1f);   // 0.90 .. 0.9985
        float sym = 0;
        for (auto& s : strings) sym += s.process (x, fb, 0.55f);
        sym *= 1.6f * (res * 0.1f);

        return x + (main + sym - x) * (mix * 0.1f);
    }
};

// ------------------------------------------------------------------------------------------
struct Boost
{
    Bq hp, tilt; double sr = 44100;
    void prepare (double s) { sr = s; hp.reset(); tilt.reset(); }
    void update (float tone) { hp.highpass (sr, 70.0); tilt.highShelf (sr, 1800.0, 0.7, (tone - 5.0f) * 1.4f); }
    inline float process (float x, float gDb, float lvlLin) noexcept
    {
        float y = hp.process (x) * dB2g (gDb);
        y = std::tanh (0.7f * y) / 0.7f;
        return tilt.process (y) * lvlLin;
    }
};

// Overdrive pedal, runs inside the oversampled block
struct DrivePedal
{
    Bq hp, lp; double sr = 44100;
    void prepare (double s) { sr = s; hp.reset(); lp.reset(); }
    void update (float tight, float tone)
    {
        hp.highpass (sr, expLerp (60.0f, 720.0f, tight * 0.1f));
        lp.lowpass (sr, expLerp (1500.0f, 9000.0f, tone * 0.1f));
    }
    inline float process (float x, float gain, float lvl) noexcept
    {
        float y = hp.process (x) * gain;
        y = std::tanh (0.8f * y + 0.1f) - std::tanh (0.1f);
        return lp.process (y) * lvl;
    }
};

// ------------------------------------------------------------------------------------------
// Amp: 5 voicings (Clean, Crunch, Modern, Lead, Sitar), runs inside the oversampled block
struct Voice
{
    int n; float g0b, g0s, g1b, g1s, g2b, g2s; float bias, hard; float tMin, tMax;
    float lp0, lp1, lp2; float midF, midQ; float pdrive; float presF, lowF;
};
inline const Voice kVoices[5] =
{
    // n   g0         g1          g2         bias  hard  tMin tMax   interstage LPFs        mid        pwr  pres  low
    { 1,  6, 1.6f,   0, 0,       0, 0,       0.00f, 0.00f,  30, 120,  9000, 9000, 9000,  600, 0.8f, 1.2f, 4500, 100 }, // Clean
    { 2, 10, 2.0f,   4, 0.8f,    0, 0,       0.15f, 0.10f,  40, 200,  7500, 6500, 6500,  700, 0.9f, 1.8f, 4200, 100 }, // Crunch
    { 3, 12, 2.3f,   3, 0.9f,    0, 0.5f,    0.12f, 0.35f,  60, 420,  6200, 5200, 4800,  800, 0.8f, 2.4f, 4000,  90 }, // Modern
    { 3, 11, 2.0f,   4, 0.8f,    2, 0.5f,    0.20f, 0.15f,  50, 260,  6800, 5600, 5200, 1000, 0.7f, 2.0f, 3800,  95 }, // Lead
    { 2,  8, 1.8f,   2, 0.7f,    0, 0,       0.25f, 0.00f,  40, 160, 11000, 9000, 9000, 1400, 0.7f, 1.4f, 5000, 100 }, // Sitar
};

struct Amp
{
    double sr = 44100; int mode = 2; int n = 3; float bias = 0, hard = 0, pdrive = 1.5f, pdNorm = 1;
    Bq tightHp, coup[3], lp[3], bassF, midF, trebF, presF, depthF;
    Sm g[3], master;

    void prepare (double osSr)
    {
        sr = osSr;
        for (auto& s : g) s.prepare (sr, 15.0);
        master.prepare (sr, 15.0);
        tightHp.reset(); bassF.reset(); midF.reset(); trebF.reset(); presF.reset(); depthF.reset();
        for (auto& b : coup) b.reset();
        for (auto& b : lp) b.reset();
    }

    void update (int m, float gainK, float tightK, float bassK, float midK, float trebK, float presK, float depthK, float masterK)
    {
        mode = juce::jlimit (0, 4, m);
        const Voice& v = kVoices[mode];
        n = v.n; bias = v.bias; hard = v.hard; pdrive = v.pdrive; pdNorm = 1.0f / std::tanh (pdrive);

        g[0].set (dB2g (v.g0b + v.g0s * gainK));
        g[1].set (dB2g (v.g1b + v.g1s * gainK));
        g[2].set (dB2g (v.g2b + v.g2s * gainK));

        const double tHz = expLerp (v.tMin, v.tMax, tightK * 0.1f);
        tightHp.highpass (sr, tHz);
        coup[0].highpass (sr, 25.0);
        coup[1].highpass (sr, juce::jmax (30.0, tHz * 0.5));
        coup[2].highpass (sr, juce::jmax (30.0, tHz * 0.5));
        lp[0].lowpass (sr, v.lp0); lp[1].lowpass (sr, v.lp1); lp[2].lowpass (sr, v.lp2);

        bassF.lowShelf  (sr, 110.0, 0.7, (bassK - 5.0f) * 2.2);
        midF.peak       (sr, v.midF, v.midQ, (midK - 5.0f) * 2.0);
        trebF.highShelf (sr, 3000.0, 0.7, (trebK - 5.0f) * 2.0);
        presF.highShelf (sr, v.presF, 0.7, presK * 0.9);
        depthF.lowShelf (sr, v.lowF, 0.8, depthK * 0.9);

        master.set (std::pow (masterK * 0.1f, 1.5f) * 1.0f);
    }

    inline float shape (float x) const noexcept
    {
        const float t = std::tanh (x + bias) - std::tanh (bias);
        return t + hard * (juce::jlimit (-1.0f, 1.0f, x) - t);
    }

    inline float process (float x) noexcept
    {
        x = tightHp.process (x);
        for (int s = 0; s < n; ++s)
        {
            x *= g[s].next();
            x = shape (x);
            x = coup[s].process (x);
            x = lp[s].process (x);
        }
        for (int s = n; s < 3; ++s) g[s].next();   // keep unused smoothers moving

        x = bassF.process (x);
        x = midF.process (x);
        x = trebF.process (x);
        x = std::tanh (x * pdrive) * pdNorm;      // power stage
        x = presF.process (x);
        x = depthF.process (x);
        return x * master.next();
    }
};

// ------------------------------------------------------------------------------------------
// Cabinet + microphone model (IIR). Replaced by the convolution engine when a WAV IR is loaded.
struct CabModel
{
    Bq hp, ls, pk1, pk2, nt, lp1, lp2, mic1, mic2, posHs, angHs, distLs, distHs;
    double sr = 44100;

    void prepare (double s) { sr = s; for (Bq* b : { &hp, &ls, &pk1, &pk2, &nt, &lp1, &lp2, &mic1, &mic2, &posHs, &angHs, &distLs, &distHs }) b->reset(); }

    void update (int cab, int mic, float dist, float pos, float angle)
    {
        double hpF = 85, lsF = 120, lsG = 1.5, p1F = 2400, p1G = 4.5, p1Q = 1.1, p2F = 800, p2G = -1.5, ntF = 3800, ntG = 0, lpF = 5200;
        switch (cab)
        {
            case 0: break;                                                                                  // 1x12
            case 1: hpF = 75; lsF = 110; lsG = 3.0; p1F = 2000; p1G = 3.5; p2F = 700; p2G = -2.0; lpF = 5400; break; // 2x12
            default: hpF = 68; lsF = 100; lsG = 4.5; p1F = 1800; p1G = 3.0; p2F = 500; p2G = -2.0; ntG = -2.5; lpF = 5000; break; // 4x12
        }

        double micLpScale = 1.0;
        switch (mic)
        {
            case 0: mic1.peak (sr, 5500.0, 1.4, 3.5);  mic2.peak (sr, 120.0, 0.7, 0.0); break;          // dynamic
            case 1: mic1.highShelf (sr, 7000.0, 0.7, 4.0); mic2.highpass (sr, 40.0); micLpScale = 1.35; break; // condenser
            default: mic1.highShelf (sr, 3500.0, 0.7, -4.5); mic2.lowShelf (sr, 200.0, 0.7, 3.0); break; // ribbon
        }

        const double d = dist, p = pos, a = angle;
        hp.highpass (sr, hpF, 0.8);
        ls.lowShelf (sr, lsF, 0.7, lsG);
        pk1.peak (sr, p1F, p1Q, p1G);
        pk2.peak (sr, p2F, 1.0, p2G);
        nt.peak (sr, ntF, 3.0, ntG);
        lp1.lowpass (sr, lpF * micLpScale, 0.5412);
        lp2.lowpass (sr, lpF * micLpScale, 1.3065);
        posHs.highShelf (sr, 3200.0, 0.7, 2.0 - 0.8 * p);
        angHs.highShelf (sr, 5000.0, 0.7, -0.6 * a);
        distLs.lowShelf (sr, 180.0, 0.7, 4.0 - 0.6 * d);
        distHs.highShelf (sr, 6000.0, 0.7, -0.35 * d);
    }

    inline float process (float x) noexcept
    {
        x = hp.process (x);   x = ls.process (x);  x = pk1.process (x); x = pk2.process (x); x = nt.process (x);
        x = lp1.process (x);  x = lp2.process (x);
        x = mic1.process (x); x = mic2.process (x);
        x = posHs.process (x); x = angHs.process (x); x = distLs.process (x); x = distHs.process (x);
        return x;
    }
};

// ------------------------------------------------------------------------------------------
struct EQ
{
    Bq hp, lp, b[5]; double sr = 44100;
    void prepare (double s) { sr = s; hp.reset(); lp.reset(); for (auto& x : b) x.reset(); }
    void update (const float* P)
    {
        hp.highpass (sr, P[Id::hpf]);
        lp.lowpass  (sr, P[Id::lpf]);
        b[0].lowShelf  (sr, P[Id::f1], 0.7, P[Id::e1]);
        b[1].peak      (sr, P[Id::f2], 1.1, P[Id::e2]);
        b[2].peak      (sr, P[Id::f3], 1.1, P[Id::e3]);
        b[3].peak      (sr, P[Id::f4], 1.1, P[Id::e4]);
        b[4].highShelf (sr, P[Id::f5], 0.7, P[Id::e5]);
    }
    inline float process (float x) noexcept
    {
        x = hp.process (x);
        for (auto& f : b) x = f.process (x);
        return lp.process (x);
    }
};

// ------------------------------------------------------------------------------------------
class Engine
{
public:
    std::atomic<float> inPeak { 0.0f }, outPeak { 0.0f };
    std::atomic<bool> irLoaded { false };

    int prepare (double sampleRate, int maxBlock)
    {
        sr = sampleRate; maxN = juce::jmax (32, maxBlock);
        osSr = sr * 4.0;

        mono.setSize (1, maxN, false, true, false);
        cabBuf.setSize (1, maxN, false, true, false);
        mono.clear(); cabBuf.clear();

        os.initProcessing ((size_t) maxN);
        os.reset();

        inGain.prepare (sr); outGain.prepare (sr);
        gate.prepare (sr);
        trans.prepare (sr); drop.prepare (sr);
        sitar.prepare (sr);
        boost.prepare (sr);
        drive.prepare (osSr);
        amp.prepare (osSr);
        driveGain.prepare (osSr, 15.0); driveLvl.prepare (osSr, 15.0);
        cabModel.prepare (sr);
        eq.prepare (sr);

        juce::dsp::ProcessSpec spec { sr, (juce::uint32) maxN, 1 };
        comp.prepare (spec);
        comp.reset();
        conv.prepare (spec);
        conv.reset();

        reverb.setSampleRate (sr);
        reverb.reset();

        inGain.init = outGain.init = false;
        return (int) std::lround (os.getLatencyInSamples());
    }

    void reset()
    {
        os.reset(); comp.reset(); conv.reset(); reverb.reset();
        trans.reset(); drop.reset();
    }

    void loadIR (const juce::File& f)
    {
        conv.loadImpulseResponse (f, juce::dsp::Convolution::Stereo::no, juce::dsp::Convolution::Trim::yes, 0,
                                  juce::dsp::Convolution::Normalise::yes);
        irLoaded.store (true);
    }
    void clearIR() { irLoaded.store (false); }

    void process (float* L, float* R, int total, int numIn, const float* P)
    {
        for (int off = 0; off < total; off += maxN)
        {
            const int n = juce::jmin (maxN, total - off);
            processChunk (L + off, R + off, n, numIn, P);
        }
    }

private:
    void processChunk (float* L, float* R, int n, int numIn, const float* P)
    {
        float* m = mono.getWritePointer (0);

        // ---- block-rate parameter setup ----
        inGain.set (dB2g (P[Id::ingain]));
        outGain.set (dB2g (P[Id::out]));

        const float gateThr = dB2g (P[Id::gate]);
        const bool  gateOn  = P[Id::gateon] > 0.5f;

        const bool  transOn = std::abs (P[Id::transpose]) > 0.01f;
        const float transRatio = std::pow (2.0f, std::round (P[Id::transpose]) / 12.0f);

        const bool  dropOn  = P[Id::tunon] > 0.5f;
        const float dropRatio = std::pow (2.0f, std::round (P[Id::t_pitch]) / 12.0f);
        const float dropWet = P[Id::t_blend] * 0.1f;

        const bool  sitarOn = P[Id::stron] > 0.5f;
        sitar.update (P[Id::s_buzz], P[Id::s_tone]);
        const float sBuzz = P[Id::s_buzz], sRes = P[Id::s_res], sMix = P[Id::s_mix];

        const bool  compOn = P[Id::cmpon] > 0.5f;
        {
            const float sus = P[Id::c_sus] * 0.1f, att = P[Id::c_att] * 0.1f;
            comp.setThreshold (lerpf (-8.0f, -38.0f, sus));
            comp.setRatio (lerpf (3.0f, 10.0f, sus));
            comp.setAttack (lerpf (1.0f, 40.0f, att));
            comp.setRelease (180.0f);
            compMakeup = dB2g (lerpf (1.0f, 12.0f, sus) + (P[Id::c_level] - 5.0f) * 2.4f);
        }

        const bool  boostOn = P[Id::bston] > 0.5f;
        boost.update (P[Id::b_tone]);
        const float bGainDb = P[Id::b_gain] * 2.4f;
        const float bLvl = dB2g ((P[Id::b_level] - 5.0f) * 2.4f);

        const bool  drvOn = P[Id::drvon] > 0.5f;
        drive.update (P[Id::d_tight], P[Id::d_tone]);
        driveGain.set (dB2g (P[Id::d_drive] * 3.8f));
        driveLvl.set (dB2g ((P[Id::d_level] - 5.0f) * 2.4f));

        amp.update ((int) std::lround (P[Id::mode]), P[Id::gain], P[Id::tight], P[Id::bass], P[Id::mid],
                    P[Id::treble], P[Id::pres], P[Id::depth], P[Id::master]);

        cabModel.update ((int) std::lround (P[Id::cab]), (int) std::lround (P[Id::mic]),
                         P[Id::dist], P[Id::pos], P[Id::angle]);
        eq.update (P);

        // ---- 1x-rate front end ----
        float pk = 0.0f;
        for (int i = 0; i < n; ++i)
        {
            float x = numIn > 1 ? 0.5f * (L[i] + R[i]) : (numIn == 1 ? L[i] : 0.0f);
            pk = juce::jmax (pk, std::abs (x));
            x *= inGain.next();
            x = gate.process (x, gateThr, gateOn);
            if (transOn) x = trans.process (x, transRatio, 1.0f);
            if (dropOn)  x = drop.process (x, dropRatio, dropWet);
            if (sitarOn) x = sitar.process (x, sBuzz, sRes, sMix);
            if (compOn)  x = comp.processSample (0, x) * compMakeup;
            if (boostOn) x = boost.process (x, bGainDb, bLvl);
            m[i] = x;
        }
        inPeak.store (juce::jmax (pk, inPeak.load() * 0.85f));

        // ---- 4x oversampled: drive pedal + amp ----
        {
            juce::dsp::AudioBlock<float> blk (mono.getArrayOfWritePointers(), 1, (size_t) n);
            auto up = os.processSamplesUp (blk);
            float* u = up.getChannelPointer (0);
            const int un = (int) up.getNumSamples();
            for (int i = 0; i < un; ++i)
            {
                float x = u[i];
                const float dg = driveGain.next(), dl = driveLvl.next();
                if (drvOn) x = drive.process (x, dg, dl);
                u[i] = amp.process (x);
            }
            os.processSamplesDown (blk);
        }

        // ---- cabinet: built-in IIR model or user IR ----
        float* c = cabBuf.getWritePointer (0);
        if (irLoaded.load())
        {
            std::copy (m, m + n, c);
            juce::dsp::AudioBlock<float> cb (cabBuf.getArrayOfWritePointers(), 1, (size_t) n);
            juce::dsp::ProcessContextReplacing<float> ctx (cb);
            conv.process (ctx);
        }
        else
        {
            for (int i = 0; i < n; ++i) c[i] = cabModel.process (m[i]);
        }

        const float mix = P[Id::cabmix] * 0.01f;
        for (int i = 0; i < n; ++i)
        {
            float y = m[i] + (c[i] - m[i]) * mix;
            y = eq.process (y);
            L[i] = R[i] = y;
        }

        // ---- room ----
        {
            juce::Reverb::Parameters rp;
            rp.roomSize = 0.30f + 0.05f * P[Id::room];
            rp.damping = 0.55f;
            rp.wetLevel = juce::jmin (0.5f, 0.025f * P[Id::room] + 0.012f * P[Id::dist]);
            rp.dryLevel = 0.5f;   // juce::Reverb doubles this -> unity dry
            rp.width = 1.0f;
            rp.freezeMode = 0.0f;
            reverb.setParameters (rp);
            reverb.processStereo (L, R, n);
        }

        // ---- output ----
        float op = 0.0f;
        for (int i = 0; i < n; ++i)
        {
            const float og = outGain.next();
            float l = L[i] * og, r = R[i] * og;
            if (! std::isfinite (l)) l = 0.0f;
            if (! std::isfinite (r)) r = 0.0f;
            l = juce::jlimit (-8.0f, 8.0f, l); r = juce::jlimit (-8.0f, 8.0f, r);
            L[i] = l; R[i] = r;
            op = juce::jmax (op, std::abs (l), std::abs (r));
        }
        outPeak.store (juce::jmax (op, outPeak.load() * 0.85f));
    }

    double sr = 44100, osSr = 176400; int maxN = 512;
    juce::AudioBuffer<float> mono, cabBuf;
    juce::dsp::Oversampling<float> os { 1, 2, juce::dsp::Oversampling<float>::filterHalfBandPolyphaseIIR, true, true };

    Sm inGain, outGain, driveGain, driveLvl;
    Gate gate;
    PitchShifter trans, drop;
    SitarPedal sitar;
    juce::dsp::Compressor<float> comp; float compMakeup = 1.0f;
    Boost boost;
    DrivePedal drive;
    Amp amp;
    CabModel cabModel;
    juce::dsp::Convolution conv;
    EQ eq;
    juce::Reverb reverb;
};

} // namespace shz
