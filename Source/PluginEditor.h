#pragma once

#include "PluginProcessor.h"

//==============================================================================
class TLLookAndFeel : public juce::LookAndFeel_V4
{
public:
    TLLookAndFeel();
    void drawRotarySlider (juce::Graphics&, int x, int y, int w, int h,
                           float pos, float startAngle, float endAngle, juce::Slider&) override;
    void drawToggleButton (juce::Graphics&, juce::ToggleButton&, bool highlighted, bool down) override;
    juce::Font getLabelFont (juce::Label&) override;

    float uiScale = 1.0f;

    static inline const juce::Colour peakCol { 0xff7CFFB2 };   // etapa 1: compresor de picos
    static inline const juce::Colour bodyCol { 0xff4fc3f7 };   // etapa 2: compresor de cuerpo
    static inline const juce::Colour hot     { 0xffff8a3d };   // Transient Protection / total
    static inline const juce::Colour offGrey { 0xff59606d };
    static inline const juce::Colour bg      { 0xff12151b };
    static inline const juce::Colour panel   { 0xff1b1f27 };
};

//==============================================================================
class MeterPanel : public juce::Component, private juce::Timer
{
public:
    explicit MeterPanel (TransientLockAudioProcessor& p);
    void paint (juce::Graphics&) override;
    void mouseDown (const juce::MouseEvent&) override;   // reinicia los picos retenidos

private:
    void timerCallback() override;

    TransientLockAudioProcessor& proc;
    float body = 0.0f, applied = 0.0f, transGr = 0.0f, trans = 0.0f;
    float hold[3] = { 0.0f, 0.0f, 0.0f };      // PEAK, BODY, TOTAL
    int holdFrames[3] = { 0, 0, 0 };
};

//==============================================================================
class TransientLockAudioProcessorEditor : public juce::AudioProcessorEditor,
                                          private juce::Timer
{
public:
    explicit TransientLockAudioProcessorEditor (TransientLockAudioProcessor&);
    ~TransientLockAudioProcessorEditor() override;

    void paint (juce::Graphics&) override;
    void resized() override;

    static constexpr int baseW = 1000;
    static constexpr int baseH = 800;

private:
    using SliderAtt = juce::AudioProcessorValueTreeState::SliderAttachment;
    using ButtonAtt = juce::AudioProcessorValueTreeState::ButtonAttachment;
    using ComboAtt  = juce::AudioProcessorValueTreeState::ComboBoxAttachment;

    struct Knob
    {
        juce::Slider slider;
        juce::String name;
        juce::Rectangle<int> nameBounds;
        bool hero  = false;
        int  stage = 0;      // 0 = general, 1 = picos, 2 = cuerpo
        std::unique_ptr<SliderAtt> att;
    };

    void setupKnob (Knob&, const juce::String& id, const juce::String& name,
                    juce::Colour colour, int stage, bool hero = false);
    void setupPowerButton (juce::ToggleButton&, std::unique_ptr<ButtonAtt>&, const char* paramId, juce::Colour led);
    void place (Knob&, juce::Rectangle<int>, float scale);
    void drawPanel (juce::Graphics&, juce::Rectangle<int>, const juce::String& title,
                    const juce::String& subtitle, juce::Colour c, bool on, float s);
    void drawFlow (juce::Graphics&, juce::Rectangle<int>, float s, bool peakOn, bool bodyOnState);
    void updateStageAlpha();
    bool isOn (const char* paramId) const;
    float scale() const { return (float) getWidth() / (float) baseW; }
    void timerCallback() override;

    TransientLockAudioProcessor& proc;
    TLLookAndFeel laf;

    // Etapa 0: entrada / detector
    Knob kInput, kHpf, kSens;
    // Etapa 1: compresor de picos
    Knob kTcAmount, kTcThr, kTcRatio, kTcAtt, kTcRel;
    // Etapa 2: compresor de cuerpo
    Knob kThreshold, kRatio, kAttack, kRelease, kKnee, kBody, kProtection;
    // Etapa 3: salida
    Knob kMakeup, kMix, kOutput;

    std::vector<Knob*> knobs, peakKnobs, bodyKnobs;

    juce::ToggleButton bypass { "BYPASS" }, peakBtn { "PEAK" }, bodyBtn { "BODY" };
    std::unique_ptr<ButtonAtt> bypassAtt, peakAtt, bodyAtt;

    juce::ComboBox presetBox, osBox;
    std::unique_ptr<ComboAtt> osAtt;

    MeterPanel meters;
    juce::ComponentBoundsConstrainer constrainer;

    juce::Rectangle<int> titleBounds, pIn, pPeak, pBody, pOut,
                         flowBounds, heroBounds, osCaptionBounds;
    bool lastPeakOn = false, lastBodyOn = true;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (TransientLockAudioProcessorEditor)
};
