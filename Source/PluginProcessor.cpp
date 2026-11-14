#include "PluginProcessor.h"
#include "PluginEditor.h"

using APVTS = juce::AudioProcessorValueTreeState;

APVTS::ParameterLayout MixAssistantProcessor::createLayout()
{
    std::vector<std::unique_ptr<juce::RangedAudioParameter>> p;
    using F = juce::AudioParameterFloat; using C = juce::AudioParameterChoice; using B = juce::AudioParameterBool;
    using R = juce::NormalisableRange<float>;
    auto pid = [] (const juce::String& s) { return juce::ParameterID { s, 1 }; };

    p.push_back (std::make_unique<C> (pid ("mode"), "Mode", juce::StringArray { "Track", "Master" }, 0));
    p.push_back (std::make_unique<C> (pid ("lang"), "Language", juce::StringArray { "English", juce::String::fromUTF8 (u8"فارسی") }, 0)); // پیش‌فرض: انگلیسی
    juce::StringArray names { "Auto" };
    for (auto& pr : mx::profiles()) names.add (pr.name);
    p.push_back (std::make_unique<C> (pid ("inst"), "Instrument", names, 0));
    p.push_back (std::make_unique<C> (pid ("target"), "Target", juce::StringArray { "Spotify (-14 LUFS)", "Beatport / Club (-9 LUFS)" }, 0));

    // کلید روشن/خاموش هر ابزار داخلی (خاموش = از پلاگین دلخواه خودت استفاده کن)
    p.push_back (std::make_unique<B> (pid ("eqOn"), "EQ On", true));
    p.push_back (std::make_unique<B> (pid ("compOn"), "Comp On", true));
    p.push_back (std::make_unique<B> (pid ("widthOn"), "Stereo On", true));
    p.push_back (std::make_unique<B> (pid ("limOn"), "Limiter On", false));

    for (int i = 0; i < mx::kBands; ++i)
        p.push_back (std::make_unique<F> (pid ("eq" + juce::String (i)), "EQ " + juce::String (i + 1), R (-12.f, 12.f, 0.1f), 0.f));
    p.push_back (std::make_unique<F> (pid ("hpf"),    "HPF",       R (20.f, 400.f, 1.f, 0.5f), 20.f));
    p.push_back (std::make_unique<F> (pid ("thr"),    "Threshold", R (-40.f, 0.f, 0.1f), 0.f));
    p.push_back (std::make_unique<F> (pid ("ratio"),  "Ratio",     R (1.f, 10.f, 0.1f), 1.f));
    p.push_back (std::make_unique<F> (pid ("atk"),    "Attack",    R (1.f, 100.f, 0.1f, 0.5f), 20.f));
    p.push_back (std::make_unique<F> (pid ("rel"),    "Release",   R (20.f, 500.f, 1.f, 0.5f), 120.f));
    p.push_back (std::make_unique<F> (pid ("makeup"), "Makeup",    R (0.f, 12.f, 0.1f), 0.f));
    p.push_back (std::make_unique<F> (pid ("width"),  "Width %",   R (0.f, 200.f, 1.f), 100.f));
    p.push_back (std::make_unique<F> (pid ("pan"),    "Pan",       R (-1.f, 1.f, 0.01f), 0.f));
    p.push_back (std::make_unique<F> (pid ("bassMono"), "Bass Mono", R (0.f, 300.f, 1.f), 0.f));   // <40 = خاموش
    p.push_back (std::make_unique<F> (pid ("limGain"), "Limiter Gain", R (0.f, 18.f, 0.1f), 0.f));
    p.push_back (std::make_unique<F> (pid ("limCeil"), "Ceiling",    R (-6.f, 0.f, 0.1f), -1.f));
    p.push_back (std::make_unique<F> (pid ("limRel"),  "Lim Release", R (10.f, 500.f, 1.f, 0.5f), 100.f));
    p.push_back (std::make_unique<F> (pid ("out"),    "Output",    R (-12.f, 12.f, 0.1f), 0.f));
    return { p.begin(), p.end() };
}

MixAssistantProcessor::MixAssistantProcessor()
    : AudioProcessor (BusesProperties().withInput ("Input", juce::AudioChannelSet::stereo(), true)
                                       .withOutput ("Output", juce::AudioChannelSet::stereo(), true)),
      apvts (*this, nullptr, "STATE", createLayout())
{
    cached.fill (-999.f);
}

bool MixAssistantProcessor::isBusesLayoutSupported (const BusesLayout& l) const
{
    return l.getMainOutputChannelSet() == l.getMainInputChannelSet()
        && (l.getMainOutputChannelSet() == juce::AudioChannelSet::stereo() || l.getMainOutputChannelSet() == juce::AudioChannelSet::mono());
}

void MixAssistantProcessor::prepareToPlay (double sr, int maxBlock)
{
    sampleRate = sr;
    juce::dsp::ProcessSpec spec { sr, (juce::uint32) maxBlock, 2 };
    for (auto& f : filt) { f.prepare (spec); f.reset(); }
    cached.fill (-999.f); bassMonoCached = -1.f; bassMonoFilter.reset();
    spectrum.prepare (sr); loud.prepare (sr); comp.env = 0.f;
    limiter.prepare (sr);
    setLatencySamples (limiter.la);   // تأخیر ثابت ~۲ms (PDC خودکار در DAW)
    mono.assign ((size_t) maxBlock, 0.f); side.assign ((size_t) maxBlock, 0.f);
}

//==============================================================================
void MixAssistantProcessor::Limiter::prepare (double fs)
{
    la = juce::jmax (8, (int) std::ceil (fs * 0.002));
    const size_t N = (size_t) la + 2;
    for (auto& d : dl) d.assign (N, 0.f);
    req.assign (N, 1.f); hold.assign (N, 1.f);
    pos = 0; sum = la + 1; gcur = 1.f;
    for (auto& a : h) for (auto& v : a) v = 0.f;
}

void MixAssistantProcessor::Limiter::process (float* l, float* r, int n, bool active, float gainLin, float ceilDb, float relMs, double fs, std::atomic<float>& grOut)
{
    const int N = la + 2;
    const float ceil = juce::Decibels::decibelsToGain (ceilDb), gin = active ? gainLin : 1.f;
    const float rc = std::exp (-1.f / (relMs * 0.001f * (float) fs));
    float minG = 1.f;
    auto cr = [] (float p0, float p1, float p2, float p3, float t)
    { return 0.5f * ((2 * p1) + (-p0 + p2) * t + (2 * p0 - 5 * p1 + 4 * p2 - p3) * t * t + (-p0 + 3 * p1 - 3 * p2 + p3) * t * t * t); };

    for (int i = 0; i < n; ++i)
    {
        const float x[2] { l[i] * gin, (r ? r[i] : l[i]) * gin };
        for (int c = 0; c < 2; ++c) dl[c][(size_t) pos] = x[c];

        if (active)
        {
            float pk = juce::jmax (std::abs (x[0]), std::abs (x[1]));
            for (int c = 0; c < 2; ++c)   // inter-sample peak بین x[n-2] و x[n-1]
                for (float t : { 0.25f, 0.5f, 0.75f })
                    pk = juce::jmax (pk, std::abs (cr (h[c][2], h[c][1], h[c][0], x[c], t)));
            req[(size_t) pos] = pk > ceil ? ceil / pk : 1.f;
        }
        else req[(size_t) pos] = 1.f;
        for (int c = 0; c < 2; ++c) { h[c][2] = h[c][1]; h[c][1] = h[c][0]; h[c][0] = x[c]; }

        float m = 1.f;                                        // min روی la+1 نمونه اخیر (lookahead)
        for (int k = 0; k <= la; ++k) m = juce::jmin (m, req[(size_t) ((pos - k + N) % N)]);
        const size_t leaving = (size_t) ((pos + 1) % N);
        sum += (double) m - (double) hold[leaving]; hold[(size_t) pos] = m;
        const float box = (float) (sum / (la + 1));           // میانگین ≤ نیاز هر نمونه ⇒ سقف تضمین می‌شود
        gcur = box < gcur ? box : gcur + (box - gcur) * (1.f - rc);   // ریلیز نرم
        minG = juce::jmin (minG, gcur);

        const size_t rd = (size_t) ((pos - la + N) % N);
        float o0 = dl[0][rd] * gcur, o1 = dl[1][rd] * gcur;
        if (active) { o0 = juce::jlimit (-ceil, ceil, o0); o1 = juce::jlimit (-ceil, ceil, o1); } // ایمنی نهایی
        l[i] = o0; if (r) r[i] = o1;
        pos = (pos + 1) % N;
    }
    grOut.store (-juce::Decibels::gainToDecibels (minG, -60.f));
}

//==============================================================================
void MixAssistantProcessor::updateCoeffs()
{
    using Co = juce::dsp::IIR::Coefficients<float>;
    for (int b = 0; b < mx::kBands; ++b)
    {
        const float db = getParam (("eq" + juce::String (b)).toRawUTF8());
        if (db == cached[(size_t) b]) continue;
        cached[(size_t) b] = db;
        const float gain = juce::Decibels::decibelsToGain (db), f = mx::kBandHz[(size_t) b];
        auto c = b == 0 ? Co::makeLowShelf (sampleRate, f, 0.7f, gain)
               : b == mx::kBands - 1 ? Co::makeHighShelf (sampleRate, f, 0.7f, gain)
               : Co::makePeakFilter (sampleRate, f, 1.0f, gain);
        *filt[(size_t) b].state = *c;
    }
    const float hp = getParam ("hpf");
    if (hp != cached[mx::kBands]) { cached[mx::kBands] = hp; *filt[mx::kBands].state = *Co::makeHighPass (sampleRate, juce::jmax (20.f, hp), 0.707f); }
}

void MixAssistantProcessor::Comp::process (float* l, float* r, int n, float thr, float ratio, float atk, float rel, float mk, double fs, std::atomic<float>& gr)
{
    const float ac = std::exp (-1.f / (atk * 0.001f * (float) fs)), rc = std::exp (-1.f / (rel * 0.001f * (float) fs));
    const float slope = 1.f - 1.f / ratio, W = 6.f, makeup = juce::Decibels::decibelsToGain (mk);
    for (int i = 0; i < n; ++i)
    {
        const float lvl = juce::jmax (std::abs (l[i]), r ? std::abs (r[i]) : 0.f);
        const float over = juce::Decibels::gainToDecibels (lvl, -100.f) - thr;
        float target = 0.f;
        if (2.f * over > W) target = slope * over;
        else if (2.f * std::abs (over) <= W) target = slope * (over + W / 2.f) * (over + W / 2.f) / (2.f * W);
        const float c = target > env ? ac : rc;
        env = c * env + (1.f - c) * target;
        const float gain = juce::Decibels::decibelsToGain (-env) * makeup;
        l[i] *= gain; if (r) r[i] *= gain;
    }
    gr.store (env);
}

void MixAssistantProcessor::analyse (const juce::AudioBuffer<float>& b)
{
    const int n = b.getNumSamples();
    if ((int) mono.size() < n) mono.resize ((size_t) n);
    const float* l = b.getReadPointer (0); const float* r = b.getNumChannels() > 1 ? b.getReadPointer (1) : l;
    for (int i = 0; i < n; ++i)
    {
        mono[(size_t) i] = 0.5f * (l[i] + r[i]);
        const int w = pitchPos.load(); pitchRing[(size_t) w] = mono[(size_t) i]; pitchPos.store ((w + 1) % (int) pitchRing.size());
    }
    spectrum.push (mono.data(), n);
}

void MixAssistantProcessor::processBlock (juce::AudioBuffer<float>& buf, juce::MidiBuffer&)
{
    juce::ScopedNoDenormals nd;
    const int n = buf.getNumSamples(), nch = juce::jmin (2, buf.getNumChannels());
    const bool master = isMaster();

    if (! master) analyse (buf);                    // ترک: ورودی خام برای تشخیص ساز

    if (flag ("eqOn"))
    {
        updateCoeffs();
        juce::dsp::AudioBlock<float> blk (buf);
        auto sub = blk.getSubsetChannelBlock (0, (size_t) nch);
        for (auto& f : filt) f.process (juce::dsp::ProcessContextReplacing<float> (sub));
    }

    float* l = buf.getWritePointer (0); float* r = nch > 1 ? buf.getWritePointer (1) : nullptr;

    if (flag ("compOn")) comp.process (l, r, n, getParam ("thr"), getParam ("ratio"), getParam ("atk"), getParam ("rel"), getParam ("makeup"), sampleRate, grDb);
    else { grDb.store (0.f); comp.env = 0.f; }

    if (r && flag ("widthOn"))
    {
        if ((int) side.size() < n) side.resize ((size_t) n);
        for (int i = 0; i < n; ++i) side[(size_t) i] = 0.5f * (l[i] - r[i]);
        const float bm = getParam ("bassMono");
        if (bm >= 40.f)                              // مونو کردن باس: Side را بالاگذر کن
        {
            if (bm != bassMonoCached) { bassMonoCached = bm; bassMonoFilter.coefficients = juce::dsp::IIR::Coefficients<float>::makeHighPass (sampleRate, bm, 0.707f); }
            float* chans[] { side.data() };
            juce::dsp::AudioBlock<float> sb (chans, 1, (size_t) n);
            bassMonoFilter.process (juce::dsp::ProcessContextReplacing<float> (sb));
        }
        const float w = getParam ("width") / 100.f, pan = getParam ("pan");
        const float ang = (pan + 1.f) * juce::MathConstants<float>::pi * 0.25f;
        const float gl = std::cos (ang) * juce::MathConstants<float>::sqrt2, gr2 = std::sin (ang) * juce::MathConstants<float>::sqrt2;
        for (int i = 0; i < n; ++i)
        {
            const float M = 0.5f * (l[i] + r[i]), S = side[(size_t) i] * w;
            l[i] = (M + S) * gl; r[i] = (M - S) * gr2;
        }
    }
    buf.applyGain (juce::Decibels::decibelsToGain (getParam ("out")));

    limiter.process (l, r, n, flag ("limOn"), juce::Decibels::decibelsToGain (getParam ("limGain")), getParam ("limCeil"), getParam ("limRel"), sampleRate, limGrDb);

    if (master) analyse (buf);                      // مستر: خروجی نهایی (بعد از لیمیتر)
    loud.process (l, r ? r : l, n);
    if (r) stereo.process (l, r, n);

    if (master)
    {
        for (int b = 0; b < mx::kBands; ++b) bus->dev[(size_t) b].store (spectrum.dev[(size_t) b].load());
        bus->lufs.store (loud.shortTerm.load()); bus->corr.store (stereo.corr.load()); bus->width.store (stereo.width.load());
        bus->lastUpdate.store (juce::Time::currentTimeMillis());
    }
}

//==============================================================================
int MixAssistantProcessor::effectiveProfile() const
{
    const int sel = (int) getParam ("inst");
    if (sel > 0) return sel - 1;
    return classifier.classify (spectrum.centroid.load(), spectrum.lowShare.load(), spectrum.flatness.load());
}

std::array<float, mx::kBands> MixAssistantProcessor::suggestionFor (int pi) const
{
    std::array<float, mx::kBands> s {};
    if (! bus->alive() || isMaster()) return s;
    const auto& P = mx::profiles()[(size_t) pi];
    for (int b = 0; b < mx::kBands; ++b)
    {
        const float d = std::log2 (mx::kBandHz[(size_t) b] / P.centroid) / 1.2f, w = std::exp (-0.5f * d * d);
        s[(size_t) b] = -juce::jlimit (-3.f, 3.f, bus->dev[(size_t) b].load() * w * 0.6f);
    }
    return s;
}

MixAssistantProcessor::Rec MixAssistantProcessor::recommend() const
{
    Rec r;
    if (! isMaster())
    {
        r.profile = effectiveProfile();
        const auto& P = mx::profiles()[(size_t) r.profile];
        const auto sg = suggestionFor (r.profile);
        for (int b = 0; b < mx::kBands; ++b) r.eq[(size_t) b] = juce::jlimit (-12.f, 12.f, P.eq[(size_t) b] + sg[(size_t) b]);
        r.hpf = P.hpf; r.thr = P.thr; r.ratio = P.ratio; r.atk = P.atk; r.rel = P.rel;
        r.width = P.width; r.bassMono = P.width > 0.f ? 120.f : 0.f;
    }
    else
    {
        r.relative = true;
        for (int b = 0; b < mx::kBands; ++b)       // اصلاح ملایم مستر: نصف انحراف، حداکثر ±۲.۵dB
        {
            const float d = spectrum.dev[(size_t) b].load();
            r.eq[(size_t) b] = std::abs (d) < 1.5f ? 0.f : juce::jlimit (-2.5f, 2.5f, -d * 0.5f);
        }
        r.hpf = 25; r.thr = -16; r.ratio = 1.5f; r.atk = 30; r.rel = 150; r.width = 100; r.bassMono = 120;
        r.limCeil = -1.f; r.limRel = 100.f;
        const float cur = loud.integrated.load() > -69.f ? loud.integrated.load() : loud.shortTerm.load();
        r.hasLoudness = cur > -69.f;
        r.limGainDelta = r.hasLoudness ? targetLufs() - cur : 0.f;
    }
    return r;
}

void MixAssistantProcessor::setParam (const juce::String& id, float v)
{
    if (auto* p = apvts.getParameter (id)) p->setValueNotifyingHost (p->convertTo0to1 (v));
}

void MixAssistantProcessor::applyGuidance()
{
    const auto r = recommend();
    if (flag ("eqOn"))
    {
        for (int b = 0; b < mx::kBands; ++b)
        {
            const juce::String id = "eq" + juce::String (b);
            setParam (id, juce::jlimit (-12.f, 12.f, (r.relative ? getParam (id.toRawUTF8()) : 0.f) + r.eq[(size_t) b]));
        }
        setParam ("hpf", r.hpf);
    }
    if (flag ("compOn")) { setParam ("thr", r.thr); setParam ("ratio", r.ratio); setParam ("atk", r.atk); setParam ("rel", r.rel); }
    if (flag ("widthOn")) { setParam ("width", r.width); setParam ("bassMono", r.bassMono); }
    if (flag ("limOn") && r.relative)
    {
        setParam ("limGain", juce::jlimit (0.f, 18.f, getParam ("limGain") + r.limGainDelta));
        setParam ("limCeil", r.limCeil); setParam ("limRel", r.limRel);
    }
}

bool MixAssistantProcessor::pitch (float& hz) const
{
    std::vector<float> x (pitchRing.size());
    const int w = pitchPos.load();
    for (size_t i = 0; i < x.size(); ++i) x[i] = pitchRing[(size_t) ((w + (int) i) % (int) x.size())];
    return mx::detectPitch (x.data(), (int) x.size(), sampleRate, hz);
}

void MixAssistantProcessor::getStateInformation (juce::MemoryBlock& d)
{
    if (auto xml = apvts.copyState().createXml()) copyXmlToBinary (*xml, d);
}
void MixAssistantProcessor::setStateInformation (const void* data, int size)
{
    if (auto xml = getXmlFromBinary (data, size)) apvts.replaceState (juce::ValueTree::fromXml (*xml));
}

juce::AudioProcessorEditor* MixAssistantProcessor::createEditor() { return new MixAssistantEditor (*this); }
juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter() { return new MixAssistantProcessor(); }
