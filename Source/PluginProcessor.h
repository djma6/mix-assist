#pragma once
#include "Core.h"

class MixAssistantProcessor : public juce::AudioProcessor
{
public:
    MixAssistantProcessor();
    void prepareToPlay (double sr, int maxBlock) override;
    void releaseResources() override {}
    bool isBusesLayoutSupported (const BusesLayout&) const override;
    void processBlock (juce::AudioBuffer<float>&, juce::MidiBuffer&) override;

    juce::AudioProcessorEditor* createEditor() override;
    bool hasEditor() const override { return true; }
    const juce::String getName() const override { return "Mix Assistant"; }
    bool acceptsMidi() const override { return false; }
    bool producesMidi() const override { return false; }
    double getTailLengthSeconds() const override { return 0.0; }
    int getNumPrograms() override { return 1; }
    int getCurrentProgram() override { return 0; }
    void setCurrentProgram (int) override {}
    const juce::String getProgramName (int) override { return {}; }
    void changeProgramName (int, const juce::String&) override {}
    void getStateInformation (juce::MemoryBlock&) override;
    void setStateInformation (const void*, int) override;

    // --- راهنما / هوش مصنوعی ---
    struct Rec   // مقادیر پیشنهادی (برای ابزار داخلی یا پلاگین دلخواه کاربر)
    {
        std::array<float, mx::kBands> eq {};
        float hpf = 20, thr = 0, ratio = 1, atk = 20, rel = 120, makeup = 0, width = 100, bassMono = 0,
              limGainDelta = 0, limCeil = -1, limRel = 100;
        bool relative = false;   // مستر: EQ و گین لیمیتر «تغییر نسبی» هستند
        int profile = 0;
        bool hasLoudness = false;
    };
    Rec recommend() const;
    bool isMaster() const { return flag ("mode"); }
    int  effectiveProfile() const;
    std::array<float, mx::kBands> suggestionFor (int profile) const;
    void applyGuidance();            // فقط روی ابزارهای داخلیِ روشن اعمال می‌شود
    bool pitch (float& hz) const;
    float targetLufs() const { return flag ("target") ? -9.f : -14.f; }
    bool flag (const char* id) const { return apvts.getRawParameterValue (id)->load() > 0.5f; }

    juce::AudioProcessorValueTreeState apvts;
    mx::SpectrumAnalyzer spectrum;
    mx::LoudnessMeter loud;
    mx::StereoMeter stereo;
    std::atomic<float> grDb { 0.f }, limGrDb { 0.f };
    juce::SharedResourcePointer<mx::MasterBus> bus;

private:
    static juce::AudioProcessorValueTreeState::ParameterLayout createLayout();
    void setParam (const juce::String& id, float v);
    float getParam (const char* id) const { return apvts.getRawParameterValue (id)->load(); }
    void updateCoeffs();
    void analyse (const juce::AudioBuffer<float>&);

    using Filt = juce::dsp::ProcessorDuplicator<juce::dsp::IIR::Filter<float>, juce::dsp::IIR::Coefficients<float>>;
    std::array<Filt, mx::kBands + 1> filt;
    std::array<float, mx::kBands + 1> cached {};

    struct Comp { float env = 0.f;
        void process (float* l, float* r, int n, float thr, float ratio, float atk, float rel, float mk, double fs, std::atomic<float>& gr); } comp;

    // لیمیتر Lookahead با تخمین True Peak (۴ برابر، درون‌یابی Catmull-Rom)
    struct Limiter
    {
        int la = 96, pos = 0; double sum = 0; float gcur = 1.f;
        std::vector<float> dl[2], req, hold;
        float h[2][3] {};
        void prepare (double fs);
        void process (float* l, float* r, int n, bool active, float gainLin, float ceilDb, float relMs, double fs, std::atomic<float>& gr);
    } limiter;

    juce::dsp::IIR::Filter<float> bassMonoFilter;
    float bassMonoCached = -1.f;
    std::vector<float> mono, side;
    std::array<float, 4096> pitchRing {};
    std::atomic<int> pitchPos { 0 };
    mx::HeuristicClassifier classifier;
    double sampleRate = 48000.0;
};
