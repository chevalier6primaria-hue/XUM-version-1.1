#pragma once
#include "Parameters.h"
#include "Engine/DspUtil.h"
#include <algorithm>
#include <array>
#include <cmath>
#include <memory>
#include <vector>

namespace xum
{
// Parámetros de un oscilador para un sub-bloque, ya con la modulación aplicada.
struct OscFrame
{
    float baseFreq = 440.f;     // Hz de la nota MIDI
    float pitchSemis = 0.f;     // octava + semitonos + modulación
    float pan = 0.f;
    int   voices = 1;           // unísono (1..16)
    float detune = 0.f, blend = 1.f, spread = 0.f;
    float pos = 0.f, warp1 = 0.f, warp2 = 0.f;
    float grainSize = 80.f, grainDensity = 32.f, grainPos = 0.f;
};

// Interfaz común de los 5 motores. render() SUMA su salida en L/R.
class IOscEngine
{
public:
    virtual ~IOscEngine() = default;
    virtual void prepare (double sampleRate) = 0;
    virtual void noteOn() = 0;
    virtual void render (float* L, float* R, int numSamples, const OscFrame& f) = 0;
};

// ---------------------------------------------------------------- Unísono
struct UnisonLayout
{
    int count = 1;
    std::array<float, kMaxUnison> ratio{}, gainL{}, gainR{};

    void compute (int voices, float detune, float blend, float spread, float pan)
    {
        count = std::clamp (voices, 1, kMaxUnison);
        const float maxCents = detune * detune * 100.f;
        std::array<float, kMaxUnison> w{}, pans{};
        float sumSq = 0.f;
        for (int i = 0; i < count; ++i)
        {
            const float o = count > 1 ? 2.f * (float) i / (float) (count - 1) - 1.f : 0.f;
            ratio[(size_t) i] = std::exp2 (o * maxCents / 1200.f);
            w[(size_t) i]     = 1.f - (1.f - blend) * std::abs (o);
            pans[(size_t) i]  = std::clamp (pan + o * spread, -1.f, 1.f);
            sumSq += w[(size_t) i] * w[(size_t) i];
        }
        const float norm = 1.f / std::sqrt (std::max (sumSq, 1e-6f));
        for (int i = 0; i < count; ++i)
        {
            const float ang = (pans[(size_t) i] + 1.f) * 0.25f * kPi;
            gainL[(size_t) i] = std::cos (ang) * 1.41421356f * w[(size_t) i] * norm;
            gainR[(size_t) i] = std::sin (ang) * 1.41421356f * w[(size_t) i] * norm;
        }
    }
};

// ---------------------------------------------------------------- Banco de wavetables
// 32 frames x 2048 muestras x 10 niveles mipmap (limitados en banda por octavas).
// Frame 0 = seno -> sierra -> cuadrada -> espectro brillante con formante.
class WavetableBank
{
public:
    static constexpr int tableSize = 2048, numFrames = 32, numMips = 10, stride = tableSize + 1;

    // Llamar una vez fuera del hilo de audio (se hace en el constructor del procesador).
    static const WavetableBank& instance() { static WavetableBank bank; return bank; }

    int mipFor (float inc) const
    {
        const float allowed = 0.5f / std::max (inc, 1e-5f);
        int m = 0;
        while (m < numMips - 1 && (float) (tableSize / 2 >> m) > allowed) ++m;
        return m;
    }

    // Interpolación lineal en fase y entre frames ("Smooth Interpolation")
    float read (float phase, float framePos, int mip) const
    {
        const float fp = std::clamp (framePos, 0.f, 1.f) * (float) (numFrames - 1);
        const int   f0 = std::min ((int) fp, numFrames - 1), f1 = std::min (f0 + 1, numFrames - 1);
        const float ft = fp - (float) f0;
        const float ph = phase * (float) tableSize;
        const int   i0 = std::min ((int) ph, tableSize - 1);
        const float t  = ph - (float) i0;
        const float* a = frame (mip, f0);
        const float* b = frame (mip, f1);
        const float va = a[i0] + t * (a[i0 + 1] - a[i0]);
        const float vb = b[i0] + t * (b[i0 + 1] - b[i0]);
        return va + ft * (vb - va);
    }

private:
    WavetableBank()
    {
        data.assign ((size_t) numMips * numFrames * stride, 0.f);
        std::vector<float> sinT ((size_t) tableSize);
        for (int i = 0; i < tableSize; ++i) sinT[(size_t) i] = std::sin (kTwoPi * (float) i / (float) tableSize);

        for (int f = 0; f < numFrames; ++f)
        {
            const float morph = (float) f / (float) (numFrames - 1);
            float gain = 1.f;
            for (int m = 0; m < numMips; ++m)
            {
                float* d = frame (m, f);
                const int maxH = (tableSize / 2) >> m;
                for (int h = 1; h <= maxH; ++h)
                {
                    const float a = harmonicAmp (h, morph);
                    if (a < 1e-5f) continue;
                    int idx = 0;
                    for (int n = 0; n < tableSize; ++n, idx = (idx + h) & (tableSize - 1))
                        d[n] += a * sinT[(size_t) idx];
                }
                if (m == 0)
                {
                    float pk = 1e-6f;
                    for (int n = 0; n < tableSize; ++n) pk = std::max (pk, std::abs (d[n]));
                    gain = 1.f / pk;
                }
                for (int n = 0; n < tableSize; ++n) d[n] *= gain;
                d[tableSize] = d[0];
            }
        }
    }

    static float harmonicAmp (int h, float m)
    {
        const float seg  = m * 3.f;
        const float sine = (h == 1) ? 1.f : 0.f;
        const float saw  = 1.f / (float) h;
        const float sq   = (h & 1) ? saw : 0.f;
        if (seg < 1.f) return sine + (saw - sine) * seg;
        if (seg < 2.f) return saw + (sq - saw) * (seg - 1.f);
        const float t = seg - 2.f;
        const float dh = (float) h - 9.f;
        return sq * (1.f + 6.f * t * std::exp (-0.5f * dh * dh / 9.f));
    }

    float*       frame (int mip, int f)       { return data.data() + ((size_t) mip * numFrames + (size_t) f) * stride; }
    const float* frame (int mip, int f) const { return data.data() + ((size_t) mip * numFrames + (size_t) f) * stride; }

    std::vector<float> data;
};

// ---------------------------------------------------------------- Motor Wavetable
class WavetableEngine : public IOscEngine
{
public:
    void prepare (double sr) override { sampleRate = sr; }

    void noteOn() override
    {
        for (auto& p : phase) p = rng.next01();     // fase inicial aleatoria por voz de unísono
        firstBlock = true;
    }

    void render (float* L, float* R, int n, const OscFrame& f) override
    {
        const auto& bank = WavetableBank::instance();
        unison.compute (f.voices, f.detune, f.blend, f.spread, f.pan);

        const float freq = f.baseFreq * std::exp2 (f.pitchSemis / 12.f);
        const float pos1 = std::clamp (f.pos, 0.f, 1.f);
        const float pos0 = firstBlock ? pos1 : lastPos;
        const float posStep = (pos1 - pos0) / (float) n;     // rampa suave de la posición
        lastPos = pos1; firstBlock = false;

        for (int v = 0; v < unison.count; ++v)
        {
            const float inc = std::min (0.49f, freq * unison.ratio[(size_t) v] / (float) sampleRate);
            const int   mip = bank.mipFor (inc);
            const float gl = unison.gainL[(size_t) v], gr = unison.gainR[(size_t) v];
            float ph = phase[(size_t) v], pos = pos0;

            for (int s = 0; s < n; ++s)
            {
                pos += posStep;
                float p = warpSync (warpBend (ph, f.warp1), f.warp2);   // warps duales
                const float y = bank.read (p, pos, mip);
                L[s] += y * gl;
                R[s] += y * gr;
                ph += inc;
                if (ph >= 1.f) ph -= 1.f;
            }
            phase[(size_t) v] = ph;
        }
    }

private:
    // Warp 1: "bend" (distorsión de fase por tramos). Warp 2: "sync" (hard-sync de fase).
    static float warpBend (float p, float a)
    {
        if (a < 1e-3f) return p;
        const float c = 0.5f + 0.45f * a;
        return p < c ? 0.5f * p / c : 0.5f + 0.5f * (p - c) / (1.f - c);
    }
    static float warpSync (float p, float a)
    {
        if (a < 1e-3f) return p;
        const float q = p * (1.f + 7.f * a);
        return q - std::floor (q);
    }

    double sampleRate = 44100.0;
    std::array<float, kMaxUnison> phase{};
    UnisonLayout unison;
    FastRand rng;
    float lastPos = 0.f;
    bool  firstBlock = true;
};

// ---------------------------------------------------------------- Motores pendientes
// Misma interfaz y ya enrutados en la voz: rellenar render() y listo.
class UnimplementedEngine : public IOscEngine
{
public:
    void prepare (double) override {}
    void noteOn() override {}
    void render (float*, float*, int, const OscFrame&) override {}
};

// TODO: reproducción por capas de velocidad / mapeo de teclas, parser SFZ.
class MultisampleEngine : public UnimplementedEngine {};
// TODO: un sample con "snap loop" (loop automático en cruces por cero) y tape-stop (rampa de velocidad).
class SampleEngine : public UnimplementedEngine {};
// TODO: pool de kMaxGrains (256) granos con ventana; usa f.grainSize / f.grainDensity / f.grainPos.
class GranularEngine : public UnimplementedEngine {};
// TODO: resíntesis FFT / aditiva por parciales en tiempo real.
class SpectralEngine : public UnimplementedEngine {};

inline std::unique_ptr<IOscEngine> makeEngine (int type)
{
    switch ((OscType) type)
    {
        case OscType::Wavetable:   return std::make_unique<WavetableEngine>();
        case OscType::Multisample: return std::make_unique<MultisampleEngine>();
        case OscType::Sample:      return std::make_unique<SampleEngine>();
        case OscType::Granular:    return std::make_unique<GranularEngine>();
        case OscType::Spectral:    return std::make_unique<SpectralEngine>();
        default:                   return std::make_unique<UnimplementedEngine>();
    }
}
}
