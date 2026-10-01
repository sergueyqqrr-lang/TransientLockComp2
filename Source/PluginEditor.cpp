#include "PluginEditor.h"

using TL = TLLookAndFeel;

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
    const juce::Colour col ((juce::uint32) (juce::int64) s.getProperties()["col"]);

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

    if (b.getProperties().contains ("led"))
    {
        // Boton de encendido de etapa: pastilla con LED + ON/OFF
        const juce::Colour led ((juce::uint32) (juce::int64) b.getProperties()["led"]);
        const float rad = r.getHeight() * 0.5f;

        g.setColour (on ? led.withAlpha (0.16f) : juce::Colour (0xff23272f));
        g.fillRoundedRectangle (r, rad);
        g.setColour (on ? led : offGrey);
        g.drawRoundedRectangle (r, rad, 1.2f);

        const float d = r.getHeight() * 0.42f;
        g.setColour (on ? led : offGrey.darker());
        g.fillEllipse (juce::Rectangle<float> (d, d).withCentre ({ r.getX() + rad, r.getCentreY() }));

        g.setColour (on ? led : offGrey);
        g.setFont (juce::jmax (8.0f, r.getHeight() * 0.52f));
        g.drawText (on ? "ON" : "OFF", r.withTrimmedLeft (r.getHeight()).toNearestInt(),
                    juce::Justification::centred);
        return;
    }

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

    const float vals[3] = { transGr, body, applied };
    for (int i = 0; i < 3; ++i)
    {
        if (vals[i] >= hold[i])      { hold[i] = vals[i]; holdFrames[i] = 45; }   // 1.5 s
        else if (holdFrames[i] > 0)  { --holdFrames[i]; }
        else                         { hold[i] = std::max (vals[i], hold[i] - 0.4f); }
    }
    repaint();
}

void MeterPanel::paint (juce::Graphics& g)
{
    const float s = (float) getWidth() / 190.0f;
    auto b = getLocalBounds().toFloat();
    g.setColour (TL::panel);
    g.fillRoundedRectangle (b, 8.0f * s);

    auto inner       = b.reduced (8.0f * s);
    auto labelsArea  = inner.removeFromBottom (18.0f * s);
    auto readoutArea = inner.removeFromTop (18.0f * s);
    auto gutter      = inner.removeFromLeft (24.0f * s);
    readoutArea.removeFromLeft (24.0f * s);

    constexpr float maxDb = 24.0f;
    const float colW = inner.getWidth() / 4.0f;
    // Mismo orden que la cadena de senal: deteccion -> picos -> cuerpo -> total
    const char* names[]  = { "ACT", "PEAK", "BODY", "TOTAL" };
    const float values[] = { trans, transGr, body, applied };
    const juce::Colour colours[] = { juce::Colour (0xff9AE6B4), TL::peakCol, TL::bodyCol, TL::hot };
    const float ticks[] = { 0.0f, 3.0f, 6.0f, 12.0f, 18.0f, 24.0f };
    const float smallFont = juce::jmax (8.0f, 10.0f * s);

    // Escala en dB
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

        juce::String readout;
        g.setColour (colours[i]);

        if (i == 0)   // actividad del detector: crece desde abajo
        {
            const float frac = juce::jlimit (0.0f, 1.0f, values[i]);
            g.fillRoundedRectangle (col.withTrimmedTop (col.getHeight() * (1.0f - frac)), 4.0f * s);
            readout = juce::String (juce::roundToInt (trans * 100.0f)) + "%";
        }
        else          // reduccion de ganancia: crece desde arriba (0 dB)
        {
            const float frac = juce::jlimit (0.0f, 1.0f, values[i] / maxDb);
            g.fillRoundedRectangle (col.withHeight (juce::jmax (0.0f, col.getHeight() * frac)), 4.0f * s);

            g.setColour (juce::Colours::white.withAlpha (0.14f));
            for (float db : ticks)
            {
                const float y = col.getY() + col.getHeight() * (db / maxDb);
                g.fillRect (juce::Rectangle<float> (col.getX(), y - 0.5f, col.getWidth(), 1.0f));
            }

            const float h = hold[i - 1];
            const float hy = col.getY() + col.getHeight() * juce::jlimit (0.0f, 1.0f, h / maxDb);
            g.setColour (juce::Colours::white.withAlpha (0.9f));
            g.fillRect (juce::Rectangle<float> (col.getX(), hy - 1.0f * s, col.getWidth(), 2.0f * s));

            readout = h < 0.05f ? juce::String ("0.0") : "-" + juce::String (h, 1);
        }

        g.setColour (colours[i]);
        g.setFont (smallFont);
        g.drawText (readout,
                    juce::Rectangle<float> (inner.getX() + (float) i * colW, readoutArea.getY(),
                                            colW, readoutArea.getHeight()).toNearestInt(),
                    juce::Justification::centred);

        g.setColour (juce::Colours::white.withAlpha (0.6f));
        g.setFont (juce::jmax (8.0f, 10.5f * s));
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

    const auto white = juce::Colour (0xffc9d1dc);

    // Etapa 0: entrada / detector
    setupKnob (kInput,  ids::input, "INPUT",       white, 0);
    setupKnob (kHpf,    ids::hpf,   "SC HPF",      white, 0);
    setupKnob (kSens,   ids::sens,  "SENSITIVITY", white, 0);

    // Etapa 1: compresor de picos
    setupKnob (kTcAmount, ids::tAmount,    "AMOUNT",    TL::peakCol, 1);
    setupKnob (kTcThr,    ids::tThreshold, "THRESHOLD", TL::peakCol, 1);
    setupKnob (kTcRatio,  ids::tRatio,     "RATIO",     TL::peakCol, 1);
    setupKnob (kTcAtt,    ids::tAttack,    "ATTACK",    TL::peakCol, 1);
    setupKnob (kTcRel,    ids::tRelease,   "RELEASE",   TL::peakCol, 1);

    // Etapa 2: compresor de cuerpo
    setupKnob (kThreshold,  ids::threshold,  "THRESHOLD",   TL::bodyCol, 2);
    setupKnob (kRatio,      ids::ratio,      "RATIO",       TL::bodyCol, 2);
    setupKnob (kAttack,     ids::attack,     "ATTACK",      TL::bodyCol, 2);
    setupKnob (kRelease,    ids::release,    "RELEASE",     TL::bodyCol, 2);
    setupKnob (kKnee,       ids::knee,       "KNEE",        TL::bodyCol, 2);
    setupKnob (kBody,       ids::body,       "BODY AMOUNT", TL::bodyCol, 2);
    setupKnob (kProtection, ids::protection, "TRANSIENT PROTECTION", TL::hot, 2, true);

    // Etapa 3: salida
    setupKnob (kMakeup, ids::makeup, "MAKEUP", white, 0);
    setupKnob (kMix,    ids::mix,    "MIX",    white, 0);
    setupKnob (kOutput, ids::output, "OUTPUT", white, 0);

    knobs = { &kInput, &kHpf, &kSens,
              &kTcAmount, &kTcThr, &kTcRatio, &kTcAtt, &kTcRel,
              &kThreshold, &kRatio, &kAttack, &kRelease, &kKnee, &kBody, &kProtection,
              &kMakeup, &kMix, &kOutput };
    peakKnobs = { &kTcAmount, &kTcThr, &kTcRatio, &kTcAtt, &kTcRel };
    bodyKnobs = { &kThreshold, &kRatio, &kAttack, &kRelease, &kKnee, &kBody, &kProtection };

    // Botones
    addAndMakeVisible (bypass);
    bypassAtt = std::make_unique<ButtonAtt> (proc.apvts, ids::bypass, bypass);
    setupPowerButton (peakBtn, peakAtt, ids::tcOn,   TL::peakCol);
    setupPowerButton (bodyBtn, bodyAtt, ids::bodyOn, TL::bodyCol);

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

    constrainer.setFixedAspectRatio ((double) baseW / (double) baseH);
    constrainer.setSizeLimits (750, 600, 2000, 1600);
    setConstrainer (&constrainer);
    setResizable (true, true);

    const int savedW = juce::jlimit (750, 2000, (int) proc.apvts.state.getProperty ("uiW", baseW));
    setSize (savedW, juce::roundToInt ((float) savedW * (float) baseH / (float) baseW));

    lastPeakOn = isOn (ids::tcOn);
    lastBodyOn = isOn (ids::bodyOn);
    updateStageAlpha();
    startTimerHz (15);
}

TransientLockAudioProcessorEditor::~TransientLockAudioProcessorEditor()
{
    setLookAndFeel (nullptr);
}

bool TransientLockAudioProcessorEditor::isOn (const char* paramId) const
{
    return proc.apvts.getRawParameterValue (paramId)->load() > 0.5f;
}

void TransientLockAudioProcessorEditor::setupKnob (Knob& k, const juce::String& id, const juce::String& name,
                                                   juce::Colour colour, int stage, bool hero)
{
    k.name  = name;
    k.hero  = hero;
    k.stage = stage;
    k.slider.setSliderStyle (juce::Slider::RotaryHorizontalVerticalDrag);
    k.slider.setTextBoxStyle (juce::Slider::TextBoxBelow, false, hero ? 100 : 72, 20);
    k.slider.setRotaryParameters (juce::MathConstants<float>::pi * 1.2f,
                                  juce::MathConstants<float>::pi * 2.8f, true);
    k.slider.getProperties().set ("hero", hero);
    k.slider.getProperties().set ("col", (juce::int64) colour.getARGB());

    addAndMakeVisible (k.slider);
    k.att = std::make_unique<SliderAtt> (proc.apvts, id, k.slider);
}

void TransientLockAudioProcessorEditor::setupPowerButton (juce::ToggleButton& b, std::unique_ptr<ButtonAtt>& att,
                                                          const char* paramId, juce::Colour led)
{
    b.getProperties().set ("led", (juce::int64) led.getARGB());
    addAndMakeVisible (b);
    att = std::make_unique<ButtonAtt> (proc.apvts, paramId, b);
}

void TransientLockAudioProcessorEditor::place (Knob& k, juce::Rectangle<int> r, float s)
{
    k.nameBounds = r.removeFromBottom (juce::roundToInt (18.0f * s));
    k.slider.setTextBoxStyle (juce::Slider::TextBoxBelow, false,
                              juce::roundToInt ((k.hero ? 100.0f : 72.0f) * s),
                              juce::roundToInt (20.0f * s));
    k.slider.setBounds (r);
}

void TransientLockAudioProcessorEditor::updateStageAlpha()
{
    const bool p = isOn (ids::tcOn), b = isOn (ids::bodyOn);
    for (auto* k : peakKnobs) k->slider.setAlpha (p ? 1.0f : 0.38f);
    for (auto* k : bodyKnobs) k->slider.setAlpha (b ? 1.0f : 0.38f);
}

void TransientLockAudioProcessorEditor::timerCallback()
{
    const bool p = isOn (ids::tcOn), b = isOn (ids::bodyOn);
    if (p != lastPeakOn || b != lastBodyOn)
    {
        lastPeakOn = p;
        lastBodyOn = b;
        updateStageAlpha();
        repaint();
    }
}

//==============================================================================
void TransientLockAudioProcessorEditor::drawPanel (juce::Graphics& g, juce::Rectangle<int> r,
                                                   const juce::String& title, const juce::String& sub,
                                                   juce::Colour c, bool on, float s)
{
    auto rf = r.toFloat();
    g.setColour (TL::panel);
    g.fillRoundedRectangle (rf, 10.0f * s);
    g.setColour ((on ? c : TL::offGrey).withAlpha (on ? 0.55f : 0.35f));
    g.drawRoundedRectangle (rf.reduced (0.5f), 10.0f * s, 1.3f);

    g.setColour (on ? c : TL::offGrey);
    g.fillRoundedRectangle (rf.getX() + 10.0f * s, rf.getY() + 7.0f * s, 4.0f * s, 14.0f * s, 2.0f * s);

    auto head = r.withHeight (juce::roundToInt (28.0f * s));
    head.removeFromLeft (juce::roundToInt (22.0f * s));
    g.setColour (on ? c : TL::offGrey);
    g.setFont (12.5f * s);
    g.drawText (title, head.removeFromLeft (juce::roundToInt (190.0f * s)), juce::Justification::centredLeft);
    g.setColour (juce::Colours::white.withAlpha (on ? 0.45f : 0.28f));
    g.setFont (10.5f * s);
    g.drawText (sub, head, juce::Justification::centredLeft);
}

void TransientLockAudioProcessorEditor::drawFlow (juce::Graphics& g, juce::Rectangle<int> r, float s,
                                                  bool peakOn, bool bodyOnState)
{
    const char* names[5] = { "INPUT", "DETECT", "PEAK", "BODY", "OUTPUT" };
    const juce::Colour base = juce::Colours::white.withAlpha (0.7f);
    const juce::Colour cols[5] = { base, base,
                                   peakOn ? TL::peakCol : TL::offGrey,
                                   bodyOnState ? TL::bodyCol : TL::offGrey,
                                   base };

    const float aw = 16.0f * s;
    auto rf = r.toFloat();
    const float nodeW = (rf.getWidth() - 4.0f * aw) / 5.0f;
    const float nodeH = 28.0f * s;
    const float y = rf.getCentreY() - nodeH * 0.5f - 6.0f * s;

    g.setFont (10.5f * s);
    for (int i = 0; i < 5; ++i)
    {
        const float x = rf.getX() + (float) i * (nodeW + aw);
        juce::Rectangle<float> n (x, y, nodeW, nodeH);
        g.setColour (cols[i].withAlpha (0.14f));
        g.fillRoundedRectangle (n, 6.0f * s);
        g.setColour (cols[i]);
        g.drawRoundedRectangle (n, 6.0f * s, 1.2f);
        g.drawText (names[i], n.toNearestInt(), juce::Justification::centred);

        if (i < 4)
        {
            const float x0 = x + nodeW + 2.0f * s, x1 = x + nodeW + aw - 2.0f * s, cy = n.getCentreY();
            g.setColour (juce::Colours::white.withAlpha (0.45f));
            g.drawLine (x0, cy, x1 - 4.0f * s, cy, 1.4f);
            juce::Path tri;
            tri.addTriangle (x1, cy, x1 - 6.0f * s, cy - 3.5f * s, x1 - 6.0f * s, cy + 3.5f * s);
            g.fillPath (tri);
        }
    }

    g.setColour (juce::Colours::white.withAlpha (0.38f));
    g.setFont (10.0f * s);
    g.drawText ("signal flow: top to bottom",
                juce::Rectangle<float> (rf.getX(), y + nodeH + 4.0f * s, rf.getWidth(), 14.0f * s).toNearestInt(),
                juce::Justification::centred);
}

void TransientLockAudioProcessorEditor::paint (juce::Graphics& g)
{
    const float s = scale();
    const bool peakOn = isOn (ids::tcOn), bodyOnState = isOn (ids::bodyOn);

    g.fillAll (TL::bg);

    // Titulo en dos colores
    g.setFont (22.0f * s);
    juce::AttributedString title;
    const auto f = g.getCurrentFont();
    title.append ("TRANSIENTLOCK ", f, juce::Colours::white);
    title.append ("COMP 2", f, TL::hot);
    title.setJustification (juce::Justification::centredLeft);
    title.draw (g, titleBounds.toFloat());

    // Paneles de etapa (de arriba a abajo = orden de la senal)
    drawPanel (g, pIn,   "INPUT / DETECTOR", "Level in and transient detection", juce::Colour (0xffc9d1dc), true, s);
    drawPanel (g, pPeak, "1  PEAK COMPRESSOR", "Acts first - transients only", TL::peakCol, peakOn, s);
    drawPanel (g, pBody, "2  BODY COMPRESSOR", "Sees the signal after the peak stage", TL::bodyCol, bodyOnState, s);
    drawPanel (g, pOut,  "OUTPUT", "Gain, parallel mix and quality", juce::Colour (0xffc9d1dc), true, s);

    // Flechas de conexion entre etapas
    auto arrow = [&] (juce::Rectangle<int> upper, juce::Rectangle<int> lower, juce::Colour c)
    {
        const float cx = (float) upper.getCentreX();
        const float y0 = (float) upper.getBottom() + 1.0f * s, y1 = (float) lower.getY() - 1.0f * s;
        juce::Path t;
        t.addTriangle (cx - 7.0f * s, y0, cx + 7.0f * s, y0, cx, y1);
        g.setColour (c.withAlpha (0.85f));
        g.fillPath (t);
    };
    arrow (pIn,   pPeak, peakOn ? TL::peakCol : TL::offGrey);
    arrow (pPeak, pBody, bodyOnState ? TL::bodyCol : TL::offGrey);
    arrow (pBody, pOut,  juce::Colour (0xffc9d1dc));

    drawFlow (g, flowBounds, s, peakOn, bodyOnState);

    // Marco del control principal
    g.setColour (TL::hot.withAlpha (bodyOnState ? 0.55f : 0.25f));
    g.drawRoundedRectangle (heroBounds.toFloat(), 8.0f * s, 1.4f);

    // Caption de oversampling
    g.setColour (juce::Colours::white.withAlpha (0.6f));
    g.setFont (11.0f * s);
    g.drawText ("OVERSAMPLING", osCaptionBounds, juce::Justification::centredRight);

    // Nombres de los knobs
    for (auto* k : knobs)
    {
        const bool dim = (k->stage == 1 && ! peakOn) || (k->stage == 2 && ! bodyOnState);
        g.setColour ((k->hero ? TL::hot : juce::Colours::white).withAlpha (dim ? 0.28f : (k->hero ? 0.95f : 0.6f)));
        g.setFont ((k->hero ? 12.0f : 10.5f) * s);
        g.drawText (k->name, k->nameBounds, juce::Justification::centred);
    }
}

void TransientLockAudioProcessorEditor::resized()
{
    const float s = scale();
    laf.uiScale = s;
    proc.apvts.state.setProperty ("uiW", getWidth(), nullptr);

    auto R = [s] (float v) { return juce::roundToInt (v * s); };

    auto area = getLocalBounds().reduced (R (16.0f));

    // Cabecera
    auto header = area.removeFromTop (R (40.0f));
    bypass.setBounds (header.removeFromRight (R (90.0f)).reduced (0, R (5.0f)));
    header.removeFromRight (R (14.0f));
    presetBox.setBounds (header.removeFromRight (R (220.0f)).reduced (0, R (5.0f)));
    titleBounds = header;
    area.removeFromTop (R (8.0f));

    // Medidores a la derecha
    meters.setBounds (area.removeFromRight (R (190.0f)));
    area.removeFromRight (R (12.0f));

    // Cuatro etapas apiladas = orden de la senal
    const int gap = R (10.0f);
    const int H = area.getHeight() - 3 * gap;
    pIn   = area.removeFromTop ((int) ((float) H * 0.20f));  area.removeFromTop (gap);
    pPeak = area.removeFromTop ((int) ((float) H * 0.28f));  area.removeFromTop (gap);
    pBody = area.removeFromTop ((int) ((float) H * 0.32f));  area.removeFromTop (gap);
    pOut  = area;

    const int head = R (28.0f);
    auto contentOf = [&] (juce::Rectangle<int> p) { return p.withTrimmedTop (head).reduced (R (6.0f), R (2.0f)); };

    // --- Etapa 0: INPUT / DETECTOR (3 knobs + diagrama de flujo)
    {
        auto c = contentOf (pIn);
        const int cw = R (100.0f);
        place (kInput, c.removeFromLeft (cw), s);
        place (kHpf,   c.removeFromLeft (cw), s);
        place (kSens,  c.removeFromLeft (cw), s);
        flowBounds = c.reduced (R (16.0f), 0);
    }

    // --- Etapa 1: PEAK COMPRESSOR (5 knobs + boton ON/OFF)
    {
        peakBtn.setBounds (pPeak.getRight() - R (80.0f), pPeak.getY() + R (5.0f), R (68.0f), R (19.0f));
        auto c = contentOf (pPeak);
        const int cw = c.getWidth() / 5;
        Knob* row[] = { &kTcAmount, &kTcThr, &kTcRatio, &kTcAtt, &kTcRel };
        for (auto* k : row) place (*k, c.removeFromLeft (cw), s);
    }

    // --- Etapa 2: BODY COMPRESSOR (6 knobs + Transient Protection destacado)
    {
        bodyBtn.setBounds (pBody.getRight() - R (80.0f), pBody.getY() + R (5.0f), R (68.0f), R (19.0f));
        auto c = contentOf (pBody);
        const int unit = (int) ((float) c.getWidth() / 7.7f);
        Knob* row[] = { &kThreshold, &kRatio, &kAttack, &kRelease, &kKnee, &kBody };
        for (auto* k : row) place (*k, c.removeFromLeft (unit), s);
        heroBounds = c.reduced (R (6.0f), R (2.0f));
        place (kProtection, heroBounds.reduced (R (4.0f)), s);
    }

    // --- Etapa 3: OUTPUT (3 knobs + oversampling)
    {
        auto c = contentOf (pOut);
        const int cw = R (100.0f);
        place (kMakeup, c.removeFromLeft (cw), s);
        place (kMix,    c.removeFromLeft (cw), s);
        place (kOutput, c.removeFromLeft (cw), s);

        auto r = c.reduced (R (10.0f), 0);
        osBox.setBounds (r.removeFromRight (R (90.0f)).withSizeKeepingCentre (R (90.0f), R (26.0f)));
        osCaptionBounds = r.removeFromRight (R (120.0f));
    }
}
