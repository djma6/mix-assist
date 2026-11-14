#include "PluginEditor.h"

static juce::String noteName (float hz, int& cents)
{
    static const char* n[] { "C","C#","D","D#","E","F","F#","G","G#","A","A#","B" };
    const float m = 69.f + 12.f * std::log2 (hz / 440.f);
    const int mi = (int) std::round (m); cents = (int) std::round ((m - mi) * 100.f);
    return juce::String (n[((mi % 12) + 12) % 12]) + juce::String (mi / 12 - 1);
}

MixAssistantEditor::Knob& MixAssistantEditor::addKnob (const char* id, const char* en, const char* fa, juce::Slider::SliderStyle st)
{
    auto k = std::make_unique<Knob>(); k->en = en; k->fa = fa;
    k->s.setSliderStyle (st); k->s.setTextBoxStyle (juce::Slider::TextBoxBelow, false, 58, 16);
    k->l.setJustificationType (juce::Justification::centred);
    addAndMakeVisible (k->s); addAndMakeVisible (k->l);
    k->a = std::make_unique<SA> (proc.apvts, id, k->s);
    knobs.push_back (std::move (k));
    return *knobs.back();
}

MixAssistantEditor::MixAssistantEditor (MixAssistantProcessor& p) : AudioProcessorEditor (&p), proc (p)
{
    setSize (980, 800);

    for (int i = 0; i < mx::kBands; ++i)
    {
        auto& s = eq[(size_t) i]; s.setSliderStyle (juce::Slider::LinearVertical); s.setTextBoxStyle (juce::Slider::TextBoxBelow, false, 50, 16);
        eqLbl[(size_t) i].setText (juce::String ((int) mx::kBandHz[(size_t) i]) + " Hz", juce::dontSendNotification);
        eqLbl[(size_t) i].setJustificationType (juce::Justification::centred);
        addAndMakeVisible (s); addAndMakeVisible (eqLbl[(size_t) i]);
        eqAtt[(size_t) i] = std::make_unique<SA> (p.apvts, "eq" + juce::String (i), s);
    }
    addKnob ("hpf", "HPF", u8"HPF");
    addKnob ("thr", "Threshold", u8"آستانه");  addKnob ("ratio", "Ratio", u8"نسبت");
    addKnob ("atk", "Attack", u8"اتک");        addKnob ("rel", "Release", u8"ریلیز");
    addKnob ("makeup", "Makeup", u8"جبران گین");
    addKnob ("width", "Width %", u8"واید ٪");  addKnob ("pan", "Pan", u8"پن");
    addKnob ("bassMono", "Bass Mono Hz", u8"باس مونو (Hz)");
    addKnob ("limGain", "Limiter Gain", u8"گین لیمیتر"); addKnob ("limCeil", "Ceiling dB", u8"سقف (dB)");
    addKnob ("limRel", "Lim Release", u8"ریلیز لیمیتر");
    addKnob ("out", "Output", u8"خروجی");

    for (auto* c : { &modeBox, &instBox, &targetBox, &langBox }) addAndMakeVisible (c);
    targetBox.addItemList ({ "Spotify (-14 LUFS)", "Beatport / Club (-9 LUFS)" }, 1);
    langBox.addItem ("English", 1); langBox.addItem (juce::String::fromUTF8 (u8"فارسی"), 2);
    targetAtt = std::make_unique<CA> (p.apvts, "target", targetBox);
    langAtt   = std::make_unique<CA> (p.apvts, "lang", langBox);

    for (auto* b : { &eqOn, &compOn, &widthOn, &limOn }) addAndMakeVisible (b);
    eqOnA = std::make_unique<BA> (p.apvts, "eqOn", eqOn);        compOnA = std::make_unique<BA> (p.apvts, "compOn", compOn);
    widthOnA = std::make_unique<BA> (p.apvts, "widthOn", widthOn); limOnA = std::make_unique<BA> (p.apvts, "limOn", limOn);

    addAndMakeVisible (applyBtn); addAndMakeVisible (resetLufs);
    applyBtn.onClick = [this] { proc.applyGuidance(); };
    resetLufs.onClick = [this] { proc.loud.reset(); };
    for (auto* l : { &meters, &tuner }) { addAndMakeVisible (l); l->setFont (juce::FontOptions (14.f)); }
    tips.setMultiLine (true); tips.setReadOnly (true); tips.setFont (juce::FontOptions (15.f));
    addAndMakeVisible (tips);

    isFa = proc.flag ("lang");
    applyLanguage();
    startTimerHz (10);
}

void MixAssistantEditor::applyLanguage()
{
    // کمبوباکس‌ها: آیتم‌ها را دوباره بساز و اتچمنت را دوباره وصل کن
    modeAtt.reset(); instAtt.reset();
    modeBox.clear (juce::dontSendNotification); instBox.clear (juce::dontSendNotification);
    modeBox.addItem (T ("Track", u8"ترک"), 1); modeBox.addItem (T ("Master", u8"مستر"), 2);
    instBox.addItem (T ("Auto (AI detect)", u8"خودکار (تشخیص AI)"), 1);
    { int id = 2, i = 0; for (auto& pr : mx::profiles()) instBox.addItem (isFa ? juce::String::fromUTF8 (mx::profileNameFa (i++)) : juce::String (pr.name), id++); }
    modeAtt = std::make_unique<CA> (proc.apvts, "mode", modeBox);
    instAtt = std::make_unique<CA> (proc.apvts, "inst", instBox);

    for (auto& k : knobs) k->l.setText (T (k->en, k->fa), juce::dontSendNotification);
    eqOn.setButtonText (T ("Built-in EQ + HPF", u8"اکولایزر داخلی + HPF"));
    compOn.setButtonText (T ("Built-in Compressor", u8"کمپرسور داخلی"));
    widthOn.setButtonText (T ("Built-in Stereo tools", u8"ابزار استریو داخلی"));
    limOn.setButtonText (T ("Built-in Limiter", u8"لیمیتر داخلی"));
    applyBtn.setButtonText (T ("Apply AI guidance", u8"اعمال راهنمای AI"));
    resetLufs.setButtonText (T ("Reset LUFS", u8"ریست LUFS"));
    tips.setJustification (isFa ? juce::Justification::topRight : juce::Justification::topLeft);
    lastTips.clear();
}

void MixAssistantEditor::resized()
{
    auto r = getLocalBounds().reduced (10);
    auto top = r.removeFromTop (28);
    modeBox.setBounds (top.removeFromLeft (100)); top.removeFromLeft (6);
    instBox.setBounds (top.removeFromLeft (170)); top.removeFromLeft (6);
    targetBox.setBounds (top.removeFromLeft (190)); top.removeFromLeft (6);
    langBox.setBounds (top.removeFromLeft (90)); top.removeFromLeft (6);
    applyBtn.setBounds (top.removeFromLeft (160)); top.removeFromLeft (6);
    resetLufs.setBounds (top.removeFromLeft (100));

    r.removeFromTop (8);
    auto eqRow = r.removeFromTop (226);
    auto left = eqRow.removeFromLeft (620);
    eqOn.setBounds (left.removeFromTop (22));
    auto hpfArea = left.removeFromRight (108);
    const int w = left.getWidth() / mx::kBands;
    for (int i = 0; i < mx::kBands; ++i)
    {
        auto c = left.removeFromLeft (w);
        eqLbl[(size_t) i].setBounds (c.removeFromBottom (18)); eq[(size_t) i].setBounds (c);
    }
    knobs[0]->l.setBounds (hpfArea.removeFromTop (16)); knobs[0]->s.setBounds (hpfArea.removeFromTop (100));
    specArea = eqRow.removeFromLeft (170).reduced (4);
    scopeArea = eqRow.reduced (4);

    auto row = [&] (juce::ToggleButton& t, std::initializer_list<int> idx)
    {
        auto a = r.removeFromTop (104);
        auto tb = a.removeFromLeft (150); t.setBounds (tb.removeFromTop (24));
        const int kw = 82;
        for (int i : idx) { auto c = a.removeFromLeft (kw); knobs[(size_t) i]->l.setBounds (c.removeFromTop (16)); knobs[(size_t) i]->s.setBounds (c); }
        return a;
    };
    r.removeFromTop (4); row (compOn, { 1, 2, 3, 4, 5 });
    r.removeFromTop (2);
    {   // ردیف ۳: استریو + لیمیتر + خروجی (هر گروه: دکمه روشن/خاموش سمت چپ، ولوم‌ها کنارش)
        auto a = r.removeFromTop (104);
        auto place = [this] (juce::Rectangle<int>& area, int idx)
        { auto c = area.removeFromLeft (76); knobs[(size_t) idx]->l.setBounds (c.removeFromTop (16)); knobs[(size_t) idx]->s.setBounds (c); };
        auto group = [&] (juce::ToggleButton& t, int first)
        {
            auto g = a.removeFromLeft (150 + 76 * 3);
            t.setBounds (g.removeFromLeft (150).removeFromTop (24));
            for (int i = 0; i < 3; ++i) place (g, first + i);
            a.removeFromLeft (12);
        };
        group (widthOn, 6);
        group (limOn, 9);
        place (a, 12);
    }
    r.removeFromTop (6);
    meters.setBounds (r.removeFromTop (22)); tuner.setBounds (r.removeFromTop (22));
    tips.setBounds (r);
}

void MixAssistantEditor::paint (juce::Graphics& g)
{
    g.fillAll (juce::Colour (0xff15171c));
    g.setColour (juce::Colour (0xff22252c)); g.fillRect (specArea);
    g.setColour (juce::Colours::grey); g.drawHorizontalLine (specArea.getCentreY(), (float) specArea.getX(), (float) specArea.getRight());
    const float bw = specArea.getWidth() / (float) mx::kBands;
    for (int b = 0; b < mx::kBands; ++b)
    {
        const float d = juce::jlimit (-12.f, 12.f, proc.spectrum.dev[(size_t) b].load());
        const float h = d / 12.f * (specArea.getHeight() * 0.5f);
        g.setColour (std::abs (d) > 4.f ? juce::Colours::orangered : juce::Colours::limegreen);
        g.fillRect (specArea.getX() + b * bw + 3, h > 0 ? specArea.getCentreY() - h : (float) specArea.getCentreY(), bw - 6, std::abs (h));
    }
    g.setColour (juce::Colour (0xff22252c)); g.fillRect (scopeArea);
    const auto c = scopeArea.getCentre().toFloat(); const float R = juce::jmin (scopeArea.getWidth(), scopeArea.getHeight()) * 0.45f;
    g.setColour (juce::Colours::grey); g.drawLine (c.x - R, c.y, c.x + R, c.y); g.drawLine (c.x, c.y - R, c.x, c.y + R);
    g.setColour (juce::Colours::cyan.withAlpha (0.6f));
    for (int i = 0; i < mx::StereoMeter::scopeN; ++i)
    {
        const float l = proc.stereo.scope[(size_t) i * 2], rr = proc.stereo.scope[(size_t) i * 2 + 1];
        g.fillRect (c.x + (rr - l) * 0.7071f * R, c.y - (l + rr) * 0.7071f * R, 1.5f, 1.5f);
    }
}

juce::String MixAssistantEditor::buildGuidance()
{
    juce::String s;
    auto bandName = [] (int b) { return juce::String ((int) mx::kBandHz[(size_t) b]) + " Hz"; };
    auto where = [this] (const char* id) { return proc.flag (id) ? T ("built-in", u8"داخلی") : T ("your plugin", u8"پلاگین خودت"); };
    auto sgn = [] (float v) { return juce::String (v > 0 ? "+" : "") + juce::String (v, 1) + " dB"; };
    const auto rec = proc.recommend();

    if (proc.isMaster())
    {
        const float cur = proc.loud.integrated.load() > -69.f ? proc.loud.integrated.load() : proc.loud.shortTerm.load();
        const float diff = proc.targetLufs() - cur;
        s << T ("[MASTER] Target: ", u8"[مستر] هدف: ") << proc.targetLufs() << " LUFS\n";
        if (! rec.hasLoudness) s << T ("Play audio to start measuring.\n", u8"صدا پخش کن تا اندازه‌گیری شروع شود.\n");
        else if (diff > 1.f)  s << T ("You are ", u8"حدود ") << juce::String (diff, 1) << T (" dB below target. Raise the limiter input gain.\n", u8" dB کم‌صداتر از هدف هستی. گین ورودی لیمیتر را بالا ببر.\n");
        else if (diff < -1.f) s << T ("You are ", u8"حدود ") << juce::String (-diff, 1) << T (" dB above target; the platform will turn it down. Push the limiter less.\n", u8" dB بلندتر از هدف هستی؛ پلتفرم صدا را پایین می‌آورد. لیمیتر را کمتر فشار بده.\n");
        else s << T ("Loudness is close to target.\n", u8"لودنس نزدیک هدف است.\n");
        if (proc.loud.peakDb.load() > -1.f) s << T ("Peak is above -1 dB. Keep headroom (true-peak limiter, ceiling -1 dBTP).\n", u8"پیک از -1dB رد شده؛ هدروم بگذار (لیمیتر True Peak با سقف -1dBTP).\n");
        const float c = proc.stereo.corr.load();
        if (c < 0.f) s << T ("Negative correlation: phase problem, mono compatibility is broken.\n", u8"همبستگی منفی: مشکل فاز! مونو‌سازگاری خراب است.\n");
        else if (c < 0.3f) s << T ("Low correlation: make the lows mono and reduce layer width.\n", u8"همبستگی پایین: باس را مونو کن و واید لایه‌ها را کم کن.\n");

        s << T ("\nRecommended changes (relative to your current settings):\n", u8"\nتغییرات پیشنهادی (نسبت به تنظیمات فعلی):\n");
        bool any = false;
        s << "  EQ [" << where ("eqOn") << "]: ";
        for (int b = 0; b < mx::kBands; ++b) if (std::abs (rec.eq[(size_t) b]) >= 0.5f) { any = true; s << bandName (b) << " " << sgn (rec.eq[(size_t) b]) << "  "; }
        if (! any) s << T ("no change needed", u8"تغییر خاصی لازم نیست");
        s << "\n  " << T ("Glue comp", u8"کمپرسور گلو") << " [" << where ("compOn") << "]: " << T ("ratio ", u8"نسبت ") << rec.ratio << ":1, "
          << T ("attack ", u8"اتک ") << (int) rec.atk << " ms, " << T ("release ", u8"ریلیز ") << (int) rec.rel << " ms, "
          << T ("set threshold for 1-2 dB gain reduction", u8"آستانه را برای 1 تا 2 dB کاهش گین تنظیم کن") << "\n";
        s << "  " << T ("Bass mono below ", u8"باس مونو زیر ") << (int) rec.bassMono << " Hz [" << where ("widthOn") << "]\n";
        s << "  " << T ("Limiter", u8"لیمیتر") << " [" << where ("limOn") << "]: " << T ("ceiling ", u8"سقف ") << rec.limCeil << " dBTP, "
          << T ("release ~", u8"ریلیز حدود ") << (int) rec.limRel << " ms";
        if (rec.hasLoudness) s << ", " << T ("input gain ", u8"گین ورودی ") << sgn (rec.limGainDelta);
        s << "\n";
        for (int b = 0; b < mx::kBands; ++b)
        {
            const float d = proc.spectrum.dev[(size_t) b].load();
            if (d > 3.f)  s << T ("Band ", u8"باند ") << bandName (b) << T (" is high vs. target curve (+", u8" نسبت به منحنی هدف بلند است (+") << juce::String (d, 1) << " dB)\n";
            if (d < -3.f) s << T ("Band ", u8"باند ") << bandName (b) << T (" is low vs. target curve (", u8" کم است (") << juce::String (d, 1) << " dB)\n";
        }
        s << T ("\nSuggestions are sent to the Track instances.", u8"\nپیشنهادها به نمونه‌های «Track» ارسال می‌شود.");
    }
    else
    {
        const auto& P = mx::profiles()[(size_t) rec.profile];
        const bool autoDet = (int) proc.apvts.getRawParameterValue ("inst")->load() == 0;
        s << T ("[TRACK] Instrument: ", u8"[ترک] ساز: ") << (isFa ? juce::String::fromUTF8 (mx::profileNameFa (rec.profile)) : juce::String (P.name))
          << (autoDet ? T (" (auto-detected)", u8" (تشخیص خودکار)") : juce::String()) << "\n"
          << (isFa ? juce::String::fromUTF8 (P.tip) : juce::String (mx::profileTipEn (rec.profile))) << "\n";
        if (! proc.bus->alive()) s << T ("\n(No Master instance found. Put the plugin on the master bus and set it to Master mode.)", u8"\n(نمونه‌ای روی مستر پیدا نشد؛ پلاگین را روی مستر بگذار و حالت Master را انتخاب کن.)") << "\n";
        else if (proc.bus->corr.load() < 0.3f) s << T ("The mix is wide/out-of-phase; reduce this track's width.\n", u8"میکس واید/ناهمفاز است؛ واید این ترک را کم کن.\n");

        s << T ("\nRecommended values (use the built-in tools or set them in your own plugins):\n", u8"\nمقادیر پیشنهادی (با ابزار داخلی یا در پلاگین‌های خودت تنظیم کن):\n");
        s << "  EQ [" << where ("eqOn") << "]: HPF " << (int) rec.hpf << " Hz;  ";
        for (int b = 0; b < mx::kBands; ++b) if (std::abs (rec.eq[(size_t) b]) >= 0.5f) s << bandName (b) << " " << sgn (rec.eq[(size_t) b]) << "  ";
        s << "\n  " << T ("Compressor", u8"کمپرسور") << " [" << where ("compOn") << "]: " << T ("threshold ", u8"آستانه ") << rec.thr << " dB, "
          << T ("ratio ", u8"نسبت ") << rec.ratio << ":1, " << T ("attack ", u8"اتک ") << rec.atk << " ms, " << T ("release ", u8"ریلیز ") << (int) rec.rel << " ms ("
          << T ("aim for 2-4 dB gain reduction", u8"هدف: 2 تا 4 dB کاهش گین") << ")\n";
        s << "  " << T ("Stereo", u8"استریو") << " [" << where ("widthOn") << "]: " << T ("width ", u8"واید ") << (int) rec.width << "%, "
          << T ("bass mono below ", u8"باس مونو زیر ") << (int) rec.bassMono << " Hz\n";
        s << "  " << T ("Limiter: not needed on tracks; leave it for the master.", u8"لیمیتر: روی ترک‌ها لازم نیست؛ برای مستر بگذار.");
    }
    return s;
}

void MixAssistantEditor::timerCallback()
{
    if (proc.flag ("lang") != isFa) { isFa = proc.flag ("lang"); applyLanguage(); }

    meters.setText (T ("LUFS", u8"لودنس") + juce::String::formatted ("  M: %.1f  S: %.1f  I: %.1f  |  Peak: %.1f dB  |  ", proc.loud.momentary.load(), proc.loud.shortTerm.load(), proc.loud.integrated.load(), proc.loud.peakDb.load())
                    + T ("Corr", u8"همبستگی") + juce::String::formatted (": %.2f  |  ", proc.stereo.corr.load())
                    + T ("Width", u8"پهنا") + juce::String::formatted (": %.0f%%  |  ", proc.stereo.width.load() * 100.f)
                    + T ("Comp GR", u8"کاهش گین کمپرسور") + juce::String::formatted (": %.1f  |  ", proc.grDb.load())
                    + T ("Limiter GR", u8"کاهش گین لیمیتر") + juce::String::formatted (": %.1f dB", proc.limGrDb.load()), juce::dontSendNotification);

    float hz = 0.f;
    if (proc.pitch (hz)) { int cents; const auto n = noteName (hz, cents); tuner.setText (T ("Tuner: ", u8"تیونر: ") + n + juce::String::formatted ("  %.1f Hz  %+d cents", hz, cents), juce::dontSendNotification); }
    else tuner.setText (T ("Tuner: --", u8"تیونر: --"), juce::dontSendNotification);

    const auto g = buildGuidance();
    if (g != lastTips) { lastTips = g; tips.setText (g, false); }
    repaint (specArea.getUnion (scopeArea));
}
