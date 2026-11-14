// Core.h - analysis engine, instrument profiles, master<->track bus
#pragma once
#include <juce_audio_processors/juce_audio_processors.h>
#include <juce_dsp/juce_dsp.h>
#include <array>
#include <atomic>
#include <cmath>
#include <vector>

namespace mx
{
constexpr int kBands = 8;
// مرکز باندهای اکولایزر ۸ باندی (باند ۱ = لو‌شلف، باند ۸ = های‌شلف)
constexpr std::array<float, kBands>     kBandHz { 35.f, 95.f, 245.f, 630.f, 1600.f, 3900.f, 8500.f, 15500.f };
constexpr std::array<float, kBands + 1> kEdgeHz { 20.f, 60.f, 150.f, 400.f, 1000.f, 2500.f, 6000.f, 12000.f, 20000.f };

//==============================================================================
// پروفایل هر ساز: EQ پیشنهادی، کمپرسور، واید، و راهنمای فارسی
struct Profile
{
    const char* name;
    std::array<float, kBands> eq;   // dB
    float hpf, thr, ratio, atk, rel, width; // Hz, dB, :1, ms, ms, %
    float centroid, lowShare;       // برای تشخیص خودکار
    const char* tip;
};

inline const std::vector<Profile>& profiles()
{
    static const std::vector<Profile> p {
    { "Kick",   { 2, 3,-3,-2, 0, 1.5f, 0, 0 },   30, -18, 4.f, 25, 80, 0,   120, 0.85f,
      u8"کیک: زیر ۳۰Hz را حذف کن، ۵۰–۹۰Hz بدنه، ۲۰۰–۴۰۰Hz گل‌آلودی را کم کن، ۳–۵kHz کلیک. مونو و وسط. اتک کند (۲۰–۳۰ms) تا ضربه بماند. با بیس سایدچین یا EQ مکمل بزن." },
    { "Bass",   { 2, 2,-2,-1, 0, 0,-3,-4 },       30, -20, 3.f, 30, 120, 0,  150, 0.80f,
      u8"بیس: HPF حدود ۳۰Hz، پایه ۴۰–۹۰Hz با کیک تداخل نکند، زیر ۱۲۰Hz کاملاً مونو، ۷۰۰–۱۵۰۰Hz برای شنیده شدن روی اسپیکر کوچک." },
    { "Snare",  { 0,-1, 1.5f,-1, 0, 2, 1, 1 },  100, -16, 3.f, 15, 90, 20,  2500, 0.20f,
      u8"اسنیر: زیر ۱۰۰Hz حذف، ۱۸۰–۲۵۰Hz بدنه، ۴–۵kHz ضربه، ۱۰kHz هوا. کمپرس ۳:۱ با اتک متوسط. ریورب کوتاه بده، پن وسط." },
    { "Clap",   { 0, 0,-2,-1, 1, 2.5f, 1.5f, 0 }, 150, -16, 3.f, 12, 80, 60,  2800, 0.10f,
      u8"کلپ: HPF حدود ۱۵۰Hz، ۱–۳kHz بدنه، ۸kHz درخشش. ریورب/واید جواب می‌دهد ولی لایه مونوی اصلی را نگه دار." },
    { "Hat/Perc", { 0, 0,-2,-1, 0, 1, 2, 2 },    350, -14, 2.f, 10, 60, 40,  8000, 0.00f,
      u8"هت/پرکاشن: HPF بالای ۳۰۰Hz، ۸–۱۲kHz روشنایی؛ اگر تیز بود ۶–۷kHz را کم کن. هر پرکاشن را پن متفاوت بده (۲۰–۵۰٪)، برای افروهاوس تکرار را با واید و ریورب متنوع کن." },
    { "Piano",  { 0, 0,-1.5f,-2, 0, 1, 1, 0.5f },  60, -20, 2.f, 20, 150, 80,  1500, 0.30f,
      u8"پیانو: HPF حدود ۶۰Hz، ۲۵۰–۵۰۰Hz را کمی کم کن تا شفاف شود، ۳–۵kHz برای حضور. کمپرس ملایم ۲:۱. واید متوسط." },
    { "Pluck",  { 0,-1,-1,-1, 0, 2, 1.5f, 1 },    120, -18, 2.5f, 8, 100, 70,  2200, 0.20f,
      u8"پلاک: HPF حدود ۱۲۰Hz، ۳–۶kHz برای حمله. اتک تند کمپرسور ضربه را له می‌کند؛ اتک کند‌تر بگذار. دیلی/ریورب کوتاه، واید ۶۰–۹۰٪." },
    { "Pad",    { 0, 0,-2,-2.5f, 0,-1, 1, 1.5f },  120, -22, 2.f, 40, 250, 150, 1200, 0.35f,
      u8"پد: HPF حدود ۱۲۰Hz، ۲۵۰–۶۰۰Hz را خلوت کن تا میکس گل نشود، های‌شلف ملایم. واید ۱۳۰–۱۷۰٪ ولی زیر ۱۵۰Hz مونو بماند. پشت لید جا باز کن (سایدچین)." },
    { "Guitar", { 0,-1,-1,-1.5f, 0, 2, 1, 0.5f },   90, -18, 3.f, 20, 120, 80,  1800, 0.25f,
      u8"گیتار: HPF حدود ۹۰Hz، ۲۰۰–۴۰۰Hz کم، ۳–۵kHz وضوح. اگر دو ترک است چپ/راست پن کن (±۵۰–۷۰٪)." },
    { "Strings",{ 0,-1,-1.5f,-2, 0, 1, 1.5f, 1 },    80, -22, 2.f, 40, 200, 130, 1500, 0.25f,
      u8"استرینگ: HPF حدود ۸۰Hz، ۲۵۰–۵۰۰Hz کم، ۶–۱۰kHz هوا. کمپرس خیلی ملایم، واید ۱۱۰–۱۵۰٪." },
    { "Lead Saw",   { 0,-1,-1,-1, 0, 2.5f, 1, 0 },  150, -18, 3.f, 15, 120, 60,  3000, 0.10f,
      u8"لید دندانه‌ای: HPF حدود ۱۵۰Hz، ۲–۴kHz را کنترل کن تا گوش را اذیت نکند، دی‌اِسِر در ۵–۷kHz. کمپرس ۳:۱." },
    { "Lead Soft",  { 0,-1, 0,-1, 0.5f, 1, 1, 1 },  130, -20, 2.5f, 20, 150, 50,  1500, 0.20f,
      u8"لید نرم: HPF حدود ۱۳۰Hz، ۱–۲kHz بدنه، های‌شلف ملایم. کمپرس ملایم و کمی ریورب پلیت." },
    { "Lead Bright",{ 0,-1,-1.5f,-1, 0,-1.5f, 0.5f, 1 }, 200, -18, 3.f, 10, 100, 40, 4000, 0.05f,
      u8"لید روشن: HPF حدود ۲۰۰Hz، ۳–۵kHz را کم کن اگر تیز است، ۱۲kHz+ برای هوا. دیلی بیشتر از ریورب، وسط بماند." },
    { "Vocal",  { 0,-1,-1,-1, 0, 1.5f, 1, 1 },    90, -20, 3.f, 15, 100, 20,  1800, 0.20f,
      u8"وکال: HPF حدود ۹۰Hz، ۲۰۰–۴۰۰Hz کم، ۳–۵kHz وضوح، ۱۰kHz هوا. کمپرس ۳:۱، وسط، ریورب/دیلی روی سند جدا." }
    };
    return p;
}

inline const char* profileNameFa (int i)
{
    static const char* n[] { u8"کیک", u8"بیس", u8"اسنیر", u8"کلپ", u8"هت/پرکاشن", u8"پیانو", u8"پلاک", u8"پد",
                             u8"گیتار", u8"استرینگ", u8"لید دندانه‌ای", u8"لید نرم", u8"لید روشن", u8"وکال" };
    return n[i];
}
inline const char* profileTipEn (int i)
{
    static const char* t[] {
      "Kick: cut below 30 Hz, 50-90 Hz for body, reduce 200-400 Hz mud, 3-5 kHz for click. Keep it mono and centered. A slow attack (20-30 ms) preserves the punch. Sidechain or complementary-EQ against the bass.",
      "Bass: HPF around 30 Hz, keep the fundamental (40-90 Hz) from fighting the kick, fully mono below 120 Hz, 700-1500 Hz so it translates on small speakers.",
      "Snare: cut below 100 Hz, 180-250 Hz for body, 4-5 kHz for crack, 10 kHz for air. Compress about 3:1 with a medium attack. Short reverb, pan center.",
      "Clap: HPF around 150 Hz, 1-3 kHz for body, 8 kHz for sparkle. Reverb and width work well, but keep a mono core layer.",
      "Hat/Perc: HPF above 300 Hz, 8-12 kHz for brightness; if harsh, reduce 6-7 kHz. Give each percussion a different pan (20-50%). For Afro House, vary repeating hits with width and reverb.",
      "Piano: HPF around 60 Hz, thin 250-500 Hz for clarity, 3-5 kHz for presence. Gentle 2:1 compression. Medium width.",
      "Pluck: HPF around 120 Hz, 3-6 kHz for the attack. A very fast compressor attack flattens the transient, so use a slower one. Short delay/reverb, width 60-90%.",
      "Pad: HPF around 120 Hz, clear out 250-600 Hz so the mix does not get muddy, gentle high shelf. Width 130-170% but keep it mono below 150 Hz. Duck it under the lead (sidechain).",
      "Guitar: HPF around 90 Hz, cut 200-400 Hz, 3-5 kHz for definition. If doubled, pan left/right (50-70%).",
      "Strings: HPF around 80 Hz, cut 250-500 Hz, 6-10 kHz for air. Very gentle compression, width 110-150%.",
      "Saw lead: HPF around 150 Hz, control 2-4 kHz so it is not fatiguing, de-ess around 5-7 kHz. Compress about 3:1.",
      "Soft lead: HPF around 130 Hz, 1-2 kHz for body, gentle high shelf. Light compression and a little plate reverb.",
      "Bright lead: HPF around 200 Hz, reduce 3-5 kHz if it is piercing, 12 kHz+ for air. More delay than reverb, keep it centered.",
      "Vocal: HPF around 90 Hz, cut 200-400 Hz, 3-5 kHz for clarity, 10 kHz for air. Compress about 3:1, keep it centered, put reverb/delay on a separate send." };
    return t[i];
}

//==============================================================================
// «هسته هوش مصنوعی»: رابط قابل تعویض. نسخه پیش‌فرض ویژگی‌محور است؛
// برای مدل واقعی (ONNX/RTNeural) کافیست IInstrumentClassifier را پیاده‌سازی کنی.
struct IInstrumentClassifier
{
    virtual ~IInstrumentClassifier() = default;
    virtual int classify (float centroidHz, float lowShare, float flatness) const = 0;
};

struct HeuristicClassifier : IInstrumentClassifier
{
    int classify (float c, float low, float flat) const override
    {
        int best = 0; float bestScore = 1e9f;
        const auto& P = profiles();
        for (int i = 0; i < (int) P.size(); ++i)
        {
            float s = std::abs (std::log2 (juce::jmax (c, 20.f) / P[(size_t) i].centroid))
                    + 2.f * std::abs (low - P[(size_t) i].lowShare)
                    + (P[(size_t) i].centroid > 6000.f ? 0.f : 0.5f * flat);
            if (s < bestScore) { bestScore = s; best = i; }
        }
        return best;
    }
};

//==============================================================================
// اسپکتروم ۸ باندی + ویژگی‌ها (سنتروید، فلتنس، سهم باس)
class SpectrumAnalyzer
{
public:
    static constexpr int order = 11, size = 1 << order;

    SpectrumAnalyzer() { for (auto& d : dev) d.store (0.f); }
    void prepare (double sr) { fs = sr; pos = 0; }

    void push (const float* mono, int n)
    {
        for (int i = 0; i < n; ++i)
        {
            fifo[(size_t) pos++] = mono[i];
            if (pos == size) { run(); pos = 0; }
        }
    }

    std::array<std::atomic<float>, kBands> dev;   // انحراف از منحنی هدف (dB، میانگین صفر)
    std::atomic<float> centroid { 0.f }, flatness { 0.f }, lowShare { 0.f };

private:
    void run()
    {
        std::copy (fifo.begin(), fifo.end(), data.begin());
        std::fill (data.begin() + size, data.end(), 0.f);
        window.multiplyWithWindowingTable (data.data(), (size_t) size);
        fft.performFrequencyOnlyForwardTransform (data.data());

        double bandPow[kBands] {}; int bandN[kBands] {};
        double num = 0, den = 0, low = 0, logSum = 0; int cnt = 0;
        for (int k = 1; k < size / 2; ++k)
        {
            const double f = k * fs / size, p = (double) data[(size_t) k] * data[(size_t) k] + 1e-20;
            num += f * p; den += p; if (f < 200.0) low += p;
            logSum += std::log (p); ++cnt;
            for (int b = 0; b < kBands; ++b)
                if (f >= kEdgeHz[(size_t) b] && f < kEdgeHz[(size_t) b + 1]) { bandPow[b] += p; ++bandN[b]; break; }
        }
        if (den < 1e-6) return; // سکوت

        float d[kBands]; float mean = 0.f;
        for (int b = 0; b < kBands; ++b)
        {
            const float dens   = 10.f * std::log10 ((float) (bandPow[b] / juce::jmax (1, bandN[b])) + 1e-12f);
            const float target = -4.5f * std::log2 (kBandHz[(size_t) b] / 1000.f); // شیب -4.5dB/oct
            d[b] = dens - target; mean += d[b] / kBands;
        }
        for (int b = 0; b < kBands; ++b)
            dev[(size_t) b].store (0.9f * dev[(size_t) b].load() + 0.1f * (d[b] - mean));

        auto sm = [] (std::atomic<float>& a, float v) { a.store (0.8f * a.load() + 0.2f * v); };
        sm (centroid, (float) (num / den));
        sm (lowShare, (float) (low / den));
        sm (flatness, (float) std::exp (logSum / cnt) / (float) (den / cnt));
    }

    double fs = 48000.0; int pos = 0;
    juce::dsp::FFT fft { order };
    juce::dsp::WindowingFunction<float> window { (size_t) size, juce::dsp::WindowingFunction<float>::hann };
    std::array<float, size> fifo {};
    std::array<float, 2 * size> data {};
};

//==============================================================================
// LUFS طبق ITU-R BS.1770 / EBU R128 (momentary, short-term, integrated)
class LoudnessMeter
{
public:
    void prepare (double sr)
    {
        fs = sr; hop = (int) (sr * 0.1);
        design (f[0][0], f[0][1]); design (f[1][0], f[1][1]);
        reset();
    }
    void reset()
    {
        for (auto& c : f) for (auto& b : c) b.z1 = b.z2 = 0;
        acc = 0; cnt = 0; hops.clear(); blocks.clear();
        momentary = shortTerm = integrated = -70.f; peakDb = -100.f;
    }
    void process (const float* l, const float* r, int n)
    {
        float pk = 0.f;
        for (int i = 0; i < n; ++i)
        {
            pk = juce::jmax (pk, std::abs (l[i]), std::abs (r[i]));
            const double a = f[0][1].p (f[0][0].p (l[i])), b = f[1][1].p (f[1][0].p (r[i]));
            acc += a * a + b * b;
            if (++cnt == hop) { push (acc / hop); acc = 0; cnt = 0; }
        }
        const float pdb = juce::Decibels::gainToDecibels (pk, -100.f);
        if (pdb > peakDb.load()) peakDb.store (pdb);
    }
    std::atomic<float> momentary { -70.f }, shortTerm { -70.f }, integrated { -70.f }, peakDb { -100.f };

private:
    struct Bq { double b0 = 1, b1 = 0, b2 = 0, a1 = 0, a2 = 0, z1 = 0, z2 = 0;
        double p (double x) { double y = b0 * x + z1; z1 = b1 * x - a1 * y + z2; z2 = b2 * x - a2 * y; return y; } };

    void design (Bq& shelf, Bq& hp) const
    {
        { const double f0 = 1681.974450955533, G = 3.999843853973347, Q = 0.7071752369554196;
          const double K = std::tan (juce::MathConstants<double>::pi * f0 / fs), Vh = std::pow (10.0, G / 20.0), Vb = std::pow (Vh, 0.4996667741545416);
          const double a0 = 1 + K / Q + K * K;
          shelf.b0 = (Vh + Vb * K / Q + K * K) / a0; shelf.b1 = 2 * (K * K - Vh) / a0; shelf.b2 = (Vh - Vb * K / Q + K * K) / a0;
          shelf.a1 = 2 * (K * K - 1) / a0; shelf.a2 = (1 - K / Q + K * K) / a0; }
        { const double f0 = 38.13547087602444, Q = 0.5003270373238773;
          const double K = std::tan (juce::MathConstants<double>::pi * f0 / fs), a0 = 1 + K / Q + K * K;
          hp.b0 = 1; hp.b1 = -2; hp.b2 = 1; hp.a1 = 2 * (K * K - 1) / a0; hp.a2 = (1 - K / Q + K * K) / a0; }
    }
    static double toLufs (double e) { return -0.691 + 10.0 * std::log10 (e + 1e-12); }

    void push (double e)
    {
        hops.push_back (e);
        auto meanLast = [this] (size_t n) { double s = 0; for (size_t i = hops.size() - n; i < hops.size(); ++i) s += hops[i]; return s / (double) n; };
        if (hops.size() >= 4)  { const double m = meanLast (4); momentary.store ((float) toLufs (m)); blocks.push_back (m); }
        if (hops.size() >= 30) shortTerm.store ((float) toLufs (meanLast (30)));
        if (hops.size() > 40) hops.erase (hops.begin(), hops.end() - 40);
        if (blocks.size() > 72000) blocks.erase (blocks.begin(), blocks.begin() + 36000); // ~۱ ساعت اخیر
        if (++sinceInt >= 10) { sinceInt = 0; computeIntegrated(); }
    }
    void computeIntegrated()
    {
        double s = 0; size_t n = 0;
        for (double b : blocks) if (toLufs (b) > -70.0) { s += b; ++n; }
        if (! n) return;
        const double rel = toLufs (s / (double) n) - 10.0;
        double s2 = 0; size_t n2 = 0;
        for (double b : blocks) if (toLufs (b) > rel) { s2 += b; ++n2; }
        if (n2) integrated.store ((float) toLufs (s2 / (double) n2));
    }

    double fs = 48000.0, acc = 0; int hop = 4800, cnt = 0, sinceInt = 0;
    Bq f[2][2];
    std::vector<double> hops, blocks;
};

//==============================================================================
// استریو: همبستگی فاز، پهنا (Side/Total) و داده وکتوراسکوپ (Stereo Image)
class StereoMeter
{
public:
    static constexpr int scopeN = 512;
    void process (const float* l, const float* r, int n)
    {
        double ll = 0, rr = 0, lr = 0, m = 0, s = 0;
        for (int i = 0; i < n; ++i)
        {
            ll += l[i] * l[i]; rr += r[i] * r[i]; lr += l[i] * r[i];
            const double M = 0.5 * (l[i] + r[i]), S = 0.5 * (l[i] - r[i]);
            m += M * M; s += S * S;
            const int w = wp.load(); scope[(size_t) w * 2] = l[i]; scope[(size_t) w * 2 + 1] = r[i]; wp.store ((w + 1) % scopeN);
        }
        if (ll + rr < 1e-9) return;
        const float c = (float) (lr / std::sqrt (ll * rr + 1e-20)), w = (float) (s / (m + s + 1e-20));
        corr.store (0.9f * corr.load() + 0.1f * c);
        width.store (0.9f * width.load() + 0.1f * w);
    }
    std::atomic<float> corr { 1.f }, width { 0.f };
    std::array<float, scopeN * 2> scope {};
    std::atomic<int> wp { 0 };
};

//==============================================================================
// تیونر لحظه‌ای: autocorrelation نرمال‌شده (NSDF)
inline bool detectPitch (const float* x, int n, double fs, float& hz)
{
    const int minLag = (int) (fs / 2000.0), maxLag = juce::jmin (n / 2, (int) (fs / 30.0));
    std::vector<float> nsdf ((size_t) maxLag + 1, 0.f);
    float best = 0.f;
    for (int t = minLag; t <= maxLag; ++t)
    {
        double r = 0, m = 0;
        for (int i = 0; i < n - t; ++i) { r += x[i] * x[i + t]; m += x[i] * x[i] + x[i + t] * x[i + t]; }
        nsdf[(size_t) t] = m > 1e-9 ? (float) (2 * r / m) : 0.f;
        best = juce::jmax (best, nsdf[(size_t) t]);
    }
    if (best < 0.8f) return false;
    for (int t = minLag + 1; t < maxLag; ++t)
        if (nsdf[(size_t) t] > 0.9f * best && nsdf[(size_t) t] >= nsdf[(size_t) t - 1] && nsdf[(size_t) t] >= nsdf[(size_t) t + 1])
        {
            const float a = nsdf[(size_t) t - 1], b = nsdf[(size_t) t], c = nsdf[(size_t) t + 1];
            const float d = (a - 2 * b + c), shift = d != 0.f ? 0.5f * (a - c) / d : 0.f;
            hz = (float) (fs / (t + shift)); return true;
        }
    return false;
}

//==============================================================================
// باس مشترک بین نمونه‌های پلاگین (مستر → ترک‌ها)
struct MasterBus
{
    MasterBus() { for (auto& d : dev) d.store (0.f); }
    std::array<std::atomic<float>, kBands> dev;
    std::atomic<float> lufs { -70.f }, corr { 1.f }, width { 0.f };
    std::atomic<juce::int64> lastUpdate { 0 };
    bool alive() const { return juce::Time::currentTimeMillis() - lastUpdate.load() < 2000; }
};
} // namespace mx
