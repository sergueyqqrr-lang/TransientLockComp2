#include "PluginEditor.h"

namespace
{
    constexpr int baseW = 960;
    constexpr int baseH = 700;
}

//==============================================================================
TLLookAndFeel::TLLookAndFeel()
{
    setColour (juce::Slider::textBoxTextColourId, juce::Colours::white.withAlpha (0.85f));
    setColour (juce::Slider::textBoxOutlineColourId, juce::Colours::transparentBlack);
    setColour (juce::Slider::textBoxBackgroundColourId, juce::Colours::transparentBlack);

    setColour (juce::ComboBox::backgroundColourId, panel);
    setColour (juce::ComboBox::textColourId, juce::Colours::white);
    setColour (juce::ComboBox::outlineColourId, panel.brighter (0.25f));
    setColour (juce::ComboBox::arrowColourId, hot);

    setColour (juce::PopupMenu::backgroundColourId, panel);
    setColour (juce::PopupMenu::textColourId, juce::Colours::white);
    setColour (juce::PopupMenu::highlightedBackgroundColourId, hot.withAlpha (0.7f));
    setColour (juce::PopupMenu::highlightedTextColourId, juce::Colours::black);
}

juce::Font TLLookAndFeel::getLabelFont (juce::Label& l)
{
    return juce::LookAndFeel_V4::getLabelFont (l).withHeight (juce::jmax (9.0f, 14.0f * uiScale));
}

void TLLookAndFeel::drawRotarySlider (juce::Graphics& g, int x, int y, int w, int h,
                                      float pos, float startAngle, float endAngle, juce::Slider& s)
{
    const bool hero = (bool) s.getProperties()["hero"];
    const bool tc   = (bool) s.getProperties()["tc"];
    const auto col  = hero ? hot : (tc ? juce::Colour (0xff7CFFB2) : accent);

    auto bounds = juce::Rectangle<float> ((float) x, (float) y, (float) w, (float) h).reduced (6.0f * uiScale);
    const float radius = juce::jmin (bounds.getWidth(), bounds.getHeight()) * 0.5f;
    const auto centre  = bounds.getCentre();
    const float angle  = startAngle + pos * (endAngle - startAngle);
    const float thick  = (hero ? 6.0f : 4.0f) * uiScale;
    const float arcR   = radius - thick;

    juce::Path track;
    track.addCentredArc (centre.x, centre.y, arcR, arcR, 0.0f, startAngle, endAngle, true);
    g.setColour (juce::Colour (0xff2a2f3a));
    g.strokePath (track, juce::PathStrokeType (thick, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));

    juce::Path value;
    value.addCentredArc (centre.x, centre.y, arcR, arcR, 0.0f, startAngle, angle, true);
    g.setColour (col);
    g.strokePath (value, juce::PathStrokeType (thick, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));

    const float bodyR = arcR - thick - 2.0f * uiScale;
    g.setColour (panel.brighter (0.15f));
    g.fillEllipse (centre.x - bodyR, centre.y - bodyR, bodyR * 2.0f, bodyR * 2.0f);

    juce::Path pointer;
    pointer.addRoundedRectangle (-1.5f * uiScale, -bodyR + 3.0f * uiScale,
                                 3.0f * uiScale, bodyR * 0.45f, 1.5f * uiScale);
    g.setColour (juce::Colours::white);
    g.fillPath (pointer, juce::AffineTransform::rotation (angle).translated (centre.x, centre.y));
}

void TLLookAndFeel::drawToggleButton (juce::Graphics& g, juce::ToggleButton& b, bool, bool)
{
    auto r = b.getLocalBounds().toFloat().reduced (1.0f);
    const bool on = b.getToggleState();
    g.setColour (on ? hot : panel.brighter (0.1f));
    g.fillRoundedRectangle (r, 6.0f * uiScale);
    g.setColour (on ? juce::Colours::black : juce::Colours::white.withAlpha (0.75f));
    g.setFont (juce::jmax (9.0f, r.getHeight() * 0.45f));
    g.drawText (b.getButtonText(), r.toNearestInt(), juce::Justification::centred);
}

//==============================================================================
MeterPanel::MeterPanel (TransientLockAudioProcessor& p) : proc (p)
{
    startTimerHz (30);
}

void MeterPanel::mouseDown (const juce::MouseEvent&)
{
    // clic en el medidor = reiniciar los picos retenidos
    for (int i = 0; i < 3; ++i) { hold[i] = 0.0f; holdFrames[i] = 0; }
}

void MeterPanel::timerCallback()
{
    auto follow = [] (float& disp, float target, float fall)
    {
        disp = target > disp ? target : std::max (target, disp - fall);
    };
    follow (body,    proc.meterBody.load(),      0.8f);
    follow (applied, proc.meterApplied.load(),   0.8f);
    follow (transGr, proc.meterTransGr.load(),   0.8f);
    follow (trans,   proc.meterTransient.load(), 0.06f);

    // Peak hold: se mantiene 1.5 s y luego cae suavemente
    const float vals[3] = { body, applied, transGr };
    for (int i = 0; i < 3; ++i)
    {
        if (vals[i] >= hold[i])        { hold[i] = vals[i]; holdFrames[i] = 45; }
        else if (holdFrames[i] > 0)    { --holdFrames[i]; }
        else                           { hold[i] = std::max (vals[i], hold[i] - 0.4f); }
    }
    repaint();
}

void MeterPanel::paint (juce::Graphics& g)
{
    const float s = (float) getWidth() / 190.0f;
    auto b = getLocalBounds().toFloat();
    g.setColour (TLLookAndFeel::panel);
    g.fillRoundedRectangle (b, 8.0f * s);

    auto inner       = b.reduced (8.0f * s);
    auto labelsArea  = inner.removeFromBottom (18.0f * s);
    auto readoutArea = inner.removeFromTop (18.0f * s);
    auto gutter      = inner.removeFromLeft (24.0f * s);
    readoutArea.removeFromLeft (24.0f * s);

    constexpr float maxDb = 24.0f;
    const float colW = inner.getWidth() / 4.0f;
    const char* names[]  = { "BODY", "TOTAL", "T-GR", "ACT" };
    const float values[] = { body, applied, transGr, trans };
    const juce::Colour colours[] = { TLLookAndFeel::accent, TLLookAndFeel::hot,
                                     juce::Colour (0xff7CFFB2), juce::Colour (0xff9AE6B4) };
    const float ticks[] = { 0.0f, 3.0f, 6.0f, 12.0f, 18.0f, 24.0f };
    const float smallFont = juce::jmax (8.0f, 10.0f * s);

    // Escala en dB (columna izquierda)
    g.setFont (juce::jmax (7.5f, 9.0f * s));
    for (float db : ticks)
    {
        const float y = inner.getY() + inner.getHeight() * (db / maxDb);
        g.setColour (juce::Colours::white.withAlpha (0.5f));
        const juce::String txt = db == 0.0f ? juce::String ("0") : "-" + juce::String ((int) db);
        g.drawText (txt, juce::Rectangle<float> (gutter.getX(), y - 6.0f * s,
                                                 gutter.getWidth() - 3.0f * s, 12.0f * s).toNearestInt(),
                    juce::Justification::centredRight);
    }
    g.setColour (juce::Colours::white.withAlpha (0.45f));
    g.setFont (smallFont);
    g.drawText ("dB", juce::Rectangle<float> (gutter.getX(), readoutArea.getY(),
                                              gutter.getWidth() - 3.0f * s, readoutArea.getHeight()).toNearestInt(),
                juce::Justification::centredRight);

    for (int i = 0; i < 4; ++i)
    {
        auto col = juce::Rectangle<float> (inner.getX() + (float) i * colW, inner.getY(),
                                           colW, inner.getHeight()).reduced (5.0f * s, 0.0f);
        g.setColour (juce::Colour (0xff0e1015));
        g.fillRoundedRectangle (col, 4.0f * s);

        if (i < 3)   // medidores de reduccion de ganancia: crecen desde arriba (0 dB) hacia abajo
        {
            const float frac = juce::jlimit (0.0f, 1.0f, values[i] / maxDb);
            g.setColour (colours[i]);
            g.fillRoundedRectangle (col.withHeight (juce::jmax (0.0f, col.getHeight() * frac)), 4.0f * s);
        }
        else         // actividad de transiente: crece desde abajo
        {
            const float frac = juce::jlimit (0.0f, 1.0f, values[i]);
            g.setColour (colours[i]);
            g.fillRoundedRectangle (col.withTrimmedTop (col.getHeight() * (1.0f - frac)), 4.0f * s);
        }

        if (i < 3)
        {
            // Lineas de la escala en dB sobre la barra
            g.setColour (juce::Colours::white.withAlpha (0.14f));
            for (float db : ticks)
            {
                const float y = col.getY() + col.getHeight() * (db / maxDb);
                g.fillRect (juce::Rectangle<float> (col.getX(), y - 0.5f, col.getWidth(), 1.0f));
            }

            // Marca de pico retenido
            const float hy = col.getY() + col.getHeight() * juce::jlimit (0.0f, 1.0f, hold[i] / maxDb);
            g.setColour (juce::Colours::white.withAlpha (0.9f));
            g.fillRect (juce::Rectangle<float> (col.getX(), hy - 1.0f * s, col.getWidth(), 2.0f * s));
        }

        // Lectura numerica (arriba)
        juce::String readout;
        if (i < 3) readout = hold[i] < 0.05f ? juce::String ("0.0") : "-" + juce::String (hold[i], 1);
        else       readout = juce::String (juce::roundToInt (trans * 100.0f)) + "%";

        g.setColour (colours[i]);
        g.setFont (smallFont);
        g.drawText (readout,
                    juce::Rectangle<float> (inner.getX() + (float) i * colW, readoutArea.getY(),
                                            colW, readoutArea.getHeight()).toNearestInt(),
                    juce::Justification::centred);

        // Nombre (abajo)
        g.setColour (juce::Colours::white.withAlpha (0.6f));
        g.setFont (juce::jmax (8.0f, 11.0f * s));
        g.drawText (names[i],
                    juce::Rectangle<float> (inner.getX() + (float) i * colW, labelsArea.getY(),
                                            colW, labelsArea.getHeight()).toNearestInt(),
                    juce::Justification::centred);
    }
}

//==============================================================================
TransientLockAudioProcessorEditor::TransientLockAudioProcessorEditor (TransientLockAudioProcessor& p)
    : AudioProcessorEditor (&p), proc (p), meters (p)
{
    setLookAndFeel (&laf);

    setupKnob (kThreshold,  ids::threshold,  "THRESHOLD");
    setupKnob (kRatio,      ids::ratio,      "RATIO");
    setupKnob (kProtection, ids::protection, "TRANSIENT PROTECTION", true);
    setupKnob (kBody,       ids::body,       "BODY COMPRESSION");
    setupKnob (kInput,      ids::input,      "INPUT");
    setupKnob (kAttack,     ids::attack,     "ATTACK");
    setupKnob (kRelease,    ids::release,    "RELEASE");
    setupKnob (kKnee,       ids::knee,       "KNEE");
    setupKnob (kHpf,        ids::hpf,        "SC HPF");
    setupKnob (kSens,       ids::sens,       "SENSITIVITY");
    setupKnob (kMakeup,     ids::makeup,     "MAKEUP");
    setupKnob (kMix,        ids::mix,        "MIX");
    setupKnob (kOutput,     ids::output,     "OUTPUT");
    setupKnob (kTcAmount,   ids::tAmount,    "AMOUNT",    false, true);
    setupKnob (kTcThr,      ids::tThreshold, "THRESHOLD", false, true);
    setupKnob (kTcRatio,    ids::tRatio,     "RATIO",     false, true);
    setupKnob (kTcAtt,      ids::tAttack,    "ATTACK",    false, true);
    setupKnob (kTcRel,      ids::tRelease,   "RELEASE",   false, true);

    knobs = { &kThreshold, &kRatio, &kProtection, &kBody,
              &kInput, &kAttack, &kRelease, &kKnee, &kHpf, &kSens, &kMakeup, &kMix, &kOutput,
              &kTcAmount, &kTcThr, &kTcRatio, &kTcAtt, &kTcRel };

    addAndMakeVisible (bypass);
    bypassAtt = std::make_unique<ButtonAtt> (proc.apvts, ids::bypass, bypass);

    // Presets
    presetBox.setTextWhenNothingSelected ("Presets...");
    for (int i = 0; i < proc.getNumPrograms(); ++i)
        presetBox.addItem (proc.getProgramName (i), i + 1);
    presetBox.onChange = [this]
    {
        const int id = presetBox.getSelectedId();
        if (id > 0) proc.setCurrentProgram (id - 1);
    };
    addAndMakeVisible (presetBox);

    // Oversampling
    osBox.addItemList (juce::StringArray { "Off", "2x", "4x" }, 1);
    addAndMakeVisible (osBox);
    osAtt = std::make_unique<ComboAtt> (proc.apvts, ids::os, osBox);

    addAndMakeVisible (meters);

    // Redimensionable con proporcion fija
    constrainer.setFixedAspectRatio ((double) baseW / (double) baseH);
    constrainer.setSizeLimits (720, 525, 1920, 1400);
    setConstrainer (&constrainer);
    setResizable (true, true);

    const int savedW = juce::jlimit (720, 1920, (int) proc.apvts.state.getProperty ("uiW", baseW));
    setSize (savedW, juce::roundToInt ((float) savedW * (float) baseH / (float) baseW));
}

TransientLockAudioProcessorEditor::~TransientLockAudioProcessorEditor()
{
    setLookAndFeel (nullptr);
}

void TransientLockAudioProcessorEditor::setupKnob (Knob& k, const juce::String& id,
                                                   const juce::String& name, bool hero, bool tc)
{
    k.name = name;
    k.hero = hero;
    k.tc   = tc;
    k.slider.setSliderStyle (juce::Slider::RotaryHorizontalVerticalDrag);
    k.slider.setTextBoxStyle (juce::Slider::TextBoxBelow, false, hero ? 100 : 72, 20);
    k.slider.setRotaryParameters (juce::MathConstants<float>::pi * 1.2f,
                                  juce::MathConstants<float>::pi * 2.8f, true);
    k.slider.getProperties().set ("hero", hero);
    k.slider.getProperties().set ("tc", tc);

    addAndMakeVisible (k.slider);
    k.att = std::make_unique<SliderAtt> (proc.apvts, id, k.slider);
}

void TransientLockAudioProcessorEditor::place (Knob& k, juce::Rectangle<int> r, float s)
{
    k.nameBounds = r.removeFromBottom (juce::roundToInt (18.0f * s));
    k.slider.setTextBoxStyle (juce::Slider::TextBoxBelow, false,
                              juce::roundToInt ((k.hero ? 100.0f : 72.0f) * s),
                              juce::roundToInt (20.0f * s));
    k.slider.setBounds (r);
}

void TransientLockAudioProcessorEditor::paint (juce::Graphics& g)
{
    const float s = (float) getWidth() / (float) baseW;

    g.fillAll (TLLookAndFeel::bg);

    // Titulo en dos colores
    g.setFont (22.0f * s);
    juce::AttributedString title;
    const auto f = g.getCurrentFont();
    title.append ("TRANSIENTLOCK ", f, juce::Colours::white);
    title.append ("COMP 2", f, TLLookAndFeel::hot);
    title.setJustification (juce::Justification::centredLeft);
    title.draw (g, titleBounds.toFloat());

    // Caption de oversampling
    g.setColour (juce::Colours::white.withAlpha (0.6f));
    g.setFont (11.0f * s);
    g.drawText ("OVERSAMPLING", osCaptionBounds, juce::Justification::centredRight);

    // Panel del control principal
    g.setColour (TLLookAndFeel::panel);
    g.fillRoundedRectangle (heroBounds.toFloat(), 10.0f * s);
    g.setColour (TLLookAndFeel::hot.withAlpha (0.55f));
    g.drawRoundedRectangle (heroBounds.toFloat().reduced (0.5f), 10.0f * s, 1.5f);

    // Panel del compresor de transientes
    const auto green = juce::Colour (0xff7CFFB2);
    g.setColour (TLLookAndFeel::panel);
    g.fillRoundedRectangle (tcBounds.toFloat(), 10.0f * s);
    g.setColour (green.withAlpha (0.4f));
    g.drawRoundedRectangle (tcBounds.toFloat().reduced (0.5f), 10.0f * s, 1.5f);
    g.setColour (green);
    g.setFont (12.0f * s);
    g.drawText ("TRANSIENT COMPRESSION  (independiente del cuerpo)",
                tcCaptionBounds.withTrimmedLeft (juce::roundToInt (12.0f * s)), juce::Justification::centredLeft);

    // Nombres de los knobs
    for (auto* k : knobs)
    {
        g.setColour (k->hero ? TLLookAndFeel::hot : (k->tc ? green.withAlpha (0.85f) : juce::Colours::white.withAlpha (0.6f)));
        g.setFont ((k->hero ? 13.0f : 11.0f) * s);
        g.drawText (k->name, k->nameBounds, juce::Justification::centred);
    }
}

void TransientLockAudioProcessorEditor::resized()
{
    const float s = (float) getWidth() / (float) baseW;
    laf.uiScale = s;
    proc.apvts.state.setProperty ("uiW", getWidth(), nullptr);

    auto R = [s] (float v) { return juce::roundToInt (v * s); };

    auto area = getLocalBounds().reduced (R (16.0f));

    // Cabecera
    auto header = area.removeFromTop (R (40.0f));
    bypass.setBounds (header.removeFromRight (R (90.0f)).reduced (0, R (5.0f)));
    header.removeFromRight (R (14.0f));
    presetBox.setBounds (header.removeFromRight (R (200.0f)).reduced (0, R (5.0f)));
    header.removeFromRight (R (14.0f));
    osBox.setBounds (header.removeFromRight (R (70.0f)).reduced (0, R (5.0f)));
    osCaptionBounds = header.removeFromRight (R (100.0f));
    titleBounds = header;
    area.removeFromTop (R (8.0f));

    // Medidores
    meters.setBounds (area.removeFromRight (R (190.0f)));
    area.removeFromRight (R (12.0f));

    const int totalH = area.getHeight();
    // Fila superior: Threshold | Ratio | [Transient Protection] | Body
    auto top = area.removeFromTop ((int) ((float) totalH * 0.38f));
    area.removeFromTop (R (8.0f));
    // Fila media: compresor de transientes
    auto mid = area.removeFromTop ((int) ((float) totalH * 0.31f));
    area.removeFromTop (R (8.0f));
    auto bottom = area;

    const int unit = top.getWidth() / 5;
    place (kThreshold, top.removeFromLeft (unit), s);
    place (kRatio,     top.removeFromLeft (unit), s);
    heroBounds = top.removeFromLeft (unit * 2);
    place (kProtection, heroBounds.reduced (R (8.0f)), s);
    place (kBody, top, s);

    // Compresor de transientes: 5 knobs
    tcBounds = mid;
    tcCaptionBounds = mid.removeFromTop (R (22.0f));
    mid = mid.reduced (R (6.0f), R (4.0f));
    const int tu = mid.getWidth() / 5;
    Knob* tcRow[] = { &kTcAmount, &kTcThr, &kTcRatio, &kTcAtt, &kTcRel };
    for (auto* k : tcRow)
        place (*k, mid.removeFromLeft (tu), s);

    // Fila inferior: 9 knobs
    const int u = bottom.getWidth() / 9;
    Knob* row[] = { &kInput, &kAttack, &kRelease, &kKnee, &kHpf, &kSens, &kMakeup, &kMix, &kOutput };
    for (auto* k : row)
        place (*k, bottom.removeFromLeft (u), s);
}
