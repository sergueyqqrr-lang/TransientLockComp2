#include "PluginProcessor.h"
#include "PluginEditor.h"

//==============================================================================
// Presets de fabrica (valores en las unidades de cada parametro)
namespace
{
struct Preset
{
    const char* name;
    float input, threshold, ratio, attack, release, knee, makeup,
          mix, output, protection, body, hpf, sens,
          tAmount, tThreshold, tRatio, tAttack, tRelease;
    bool tOn;   // compresor de picos activado en este preset
};

const Preset kPresets[] =
{
    { "Default", 0.0f, -18.0f, 4.0f, 10.0f, 150.0f, 6.0f, 0.0f, 100.0f, 0.0f, 60.0f, 100.0f, 20.0f, 50.0f, 50.0f, -12.0f, 4.0f, 1.0f, 40.0f, false },
    { "Vocal - Gentle", 0.0f, -20.0f, 2.5f, 12.0f, 140.0f, 10.0f, 2.0f, 100.0f, 0.0f, 45.0f, 100.0f, 80.0f, 50.0f, 50.0f, -12.0f, 4.0f, 1.0f, 40.0f, false },
    { "Vocal - Present", 0.0f, -24.0f, 4.0f, 8.0f, 110.0f, 6.0f, 4.0f, 100.0f, 0.0f, 65.0f, 100.0f, 90.0f, 55.0f, 50.0f, -12.0f, 4.0f, 1.0f, 40.0f, false },
    { "Vocal - Peak Control", 0.0f, -22.0f, 3.0f, 12.0f, 130.0f, 8.0f, 2.0f, 100.0f, 0.0f, 100.0f, 100.0f, 100.0f, 60.0f, 60.0f, -16.0f, 5.0f, 0.3f, 30.0f, true },
    { "Drum Bus - Glue", 0.0f, -16.0f, 3.0f, 15.0f, 120.0f, 8.0f, 2.0f, 100.0f, 0.0f, 75.0f, 100.0f, 60.0f, 50.0f, 50.0f, -12.0f, 4.0f, 1.0f, 40.0f, false },
    { "Drums - Tame Peaks", 0.0f, -20.0f, 3.0f, 10.0f, 120.0f, 6.0f, 2.0f, 100.0f, 0.0f, 100.0f, 60.0f, 60.0f, 55.0f, 70.0f, -14.0f, 6.0f, 0.5f, 40.0f, true },
    { "Drums - Punch Smash", 0.0f, -30.0f, 8.0f, 5.0f, 90.0f, 4.0f, 8.0f, 65.0f, 0.0f, 95.0f, 100.0f, 40.0f, 60.0f, 50.0f, -12.0f, 4.0f, 1.0f, 40.0f, false },
    { "Snare - Snap", 0.0f, -22.0f, 5.0f, 6.0f, 100.0f, 3.0f, 4.0f, 100.0f, 0.0f, 90.0f, 100.0f, 100.0f, 65.0f, 50.0f, -12.0f, 4.0f, 1.0f, 40.0f, false },
    { "Bass - Control", 0.0f, -22.0f, 4.0f, 15.0f, 200.0f, 8.0f, 3.0f, 100.0f, 0.0f, 50.0f, 100.0f, 60.0f, 45.0f, 50.0f, -12.0f, 4.0f, 1.0f, 40.0f, false },
    { "Guitar - Tighten", 0.0f, -20.0f, 3.5f, 10.0f, 130.0f, 6.0f, 3.0f, 100.0f, 0.0f, 60.0f, 100.0f, 100.0f, 50.0f, 50.0f, -12.0f, 4.0f, 1.0f, 40.0f, false },
    { "Mix Bus - Glue", 0.0f, -14.0f, 2.0f, 30.0f, 200.0f, 12.0f, 1.5f, 100.0f, 0.0f, 70.0f, 60.0f, 80.0f, 45.0f, 50.0f, -12.0f, 4.0f, 1.0f, 40.0f, false },
    { "Parallel - Heavy", 0.0f, -34.0f, 10.0f, 3.0f, 120.0f, 6.0f, 10.0f, 40.0f, 0.0f, 100.0f, 100.0f, 50.0f, 60.0f, 50.0f, -12.0f, 4.0f, 1.0f, 40.0f, false },
};
constexpr int kNumPresets = (int) (sizeof (kPresets) / sizeof (kPresets[0]));
}

//==============================================================================
juce::AudioProcessorValueTreeState::ParameterLayout TransientLockAudioProcessor::createLayout()
{
    using APF   = juce::AudioParameterFloat;
    using Attr  = juce::AudioParameterFloatAttributes;
    using PID   = juce::ParameterID;
    using Range = juce::NormalisableRange<float>;

    juce::AudioProcessorValueTreeState::ParameterLayout layout;

    auto add = [&] (const char* id, const char* name, Range r, float def, const char* unit)
    {
        layout.add (std::make_unique<APF> (PID { id, 1 }, name, r, def, Attr().withLabel (unit)));
    };

    add (ids::input,      "Input",                Range (-24.0f, 24.0f, 0.1f),        0.0f,   "dB");
    add (ids::threshold,  "Threshold",            Range (-60.0f, 0.0f, 0.1f),         -18.0f, "dB");
    layout.add (std::make_unique<APF> (PID { ids::ratio, 1 }, "Ratio",
                Range (1.0f, 20.0f, 0.1f, 0.5f), 4.0f,
                Attr().withStringFromValueFunction ([] (float v, int)
                { return juce::String (v, 1) + ":1"; })));
    add (ids::attack,     "Attack",               Range (0.1f, 100.0f, 0.01f, 0.35f), 10.0f,  "ms");
    add (ids::release,    "Release",              Range (10.0f, 1000.0f, 0.1f, 0.4f), 150.0f, "ms");
    add (ids::knee,       "Knee",                 Range (0.0f, 24.0f, 0.1f),          6.0f,   "dB");
    add (ids::makeup,     "Makeup",               Range (0.0f, 24.0f, 0.1f),          0.0f,   "dB");
    add (ids::mix,        "Mix",                  Range (0.0f, 100.0f, 0.1f),         100.0f, "%");
    add (ids::output,     "Output",               Range (-24.0f, 24.0f, 0.1f),        0.0f,   "dB");
    add (ids::protection, "Transient Protection", Range (0.0f, 100.0f, 0.1f),         60.0f,  "%");
    add (ids::body,       "Body Compression",     Range (0.0f, 100.0f, 0.1f),         100.0f, "%");
    add (ids::hpf,        "Sidechain HPF",        Range (20.0f, 500.0f, 1.0f, 0.4f),  20.0f,  "Hz");
    add (ids::sens,       "Detector Sensitivity", Range (0.0f, 100.0f, 0.1f),         50.0f,  "%");

    // Compresor de transientes (independiente del cuerpo)
    add (ids::tAmount,    "Transient Comp Amount",    Range (0.0f, 100.0f, 0.1f),         50.0f,  "%");
    add (ids::tThreshold, "Transient Comp Threshold", Range (-60.0f, 0.0f, 0.1f),         -12.0f, "dB");
    layout.add (std::make_unique<APF> (PID { ids::tRatio, 1 }, "Transient Comp Ratio",
                Range (1.0f, 20.0f, 0.1f, 0.5f), 4.0f,
                Attr().withStringFromValueFunction ([] (float v, int)
                { return juce::String (v, 1) + ":1"; })));
    add (ids::tAttack,    "Transient Comp Attack",    Range (0.05f, 30.0f, 0.01f, 0.4f),  1.0f,   "ms");
    add (ids::tRelease,   "Transient Comp Release",   Range (5.0f, 300.0f, 0.1f, 0.5f),   40.0f,  "ms");
    layout.add (std::make_unique<juce::AudioParameterBool> (PID { ids::tcOn, 1 },   "Peak Compressor On", false));
    layout.add (std::make_unique<juce::AudioParameterBool> (PID { ids::bodyOn, 1 }, "Body Compressor On", true));

    layout.add (std::make_unique<juce::AudioParameterChoice> (PID { ids::os, 1 }, "Oversampling",
                juce::StringArray { "Off", "2x", "4x" }, 0));
    layout.add (std::make_unique<juce::AudioParameterBool> (PID { ids::bypass, 1 }, "Bypass", false));
    return layout;
}

//==============================================================================
TransientLockAudioProcessor::TransientLockAudioProcessor()
    : AudioProcessor (BusesProperties()
                        .withInput  ("Input",  juce::AudioChannelSet::stereo(), true)
                        .withOutput ("Output", juce::AudioChannelSet::stereo(), true)),
      apvts (*this, nullptr, "PARAMS", createLayout())
{
}

void TransientLockAudioProcessor::prepareToPlay (double sampleRate, int samplesPerBlock)
{
    maxBlock = juce::jmax (1, samplesPerBlock);
    const auto nch = (size_t) juce::jmax (1, getTotalNumOutputChannels());

    engines[0].prepare (sampleRate);
    engines[1].prepare (sampleRate * 2.0);
    engines[2].prepare (sampleRate * 4.0);

    os2 = std::make_unique<Oversampler> (nch, (size_t) 1,
            Oversampler::filterHalfBandFIREquiripple, true, true);
    os4 = std::make_unique<Oversampler> (nch, (size_t) 2,
            Oversampler::filterHalfBandFIREquiripple, true, true);
    os2->initProcessing ((size_t) maxBlock);
    os4->initProcessing ((size_t) maxBlock);

    currentOs = juce::jlimit (0, 2, (int) std::lround (get (ids::os)));
    updateLatency();
}

void TransientLockAudioProcessor::updateLatency()
{
    float lat = 0.0f;
    if (currentOs == 1 && os2) lat = os2->getLatencyInSamples();
    if (currentOs == 2 && os4) lat = os4->getLatencyInSamples();

    const int latInt = juce::roundToInt (lat);
    if (latInt != getLatencySamples())
        setLatencySamples (latInt);
}

bool TransientLockAudioProcessor::isBusesLayoutSupported (const BusesLayout& layouts) const
{
    const auto& out = layouts.getMainOutputChannelSet();
    if (out != juce::AudioChannelSet::mono() && out != juce::AudioChannelSet::stereo())
        return false;
    return out == layouts.getMainInputChannelSet();
}

void TransientLockAudioProcessor::processBlock (juce::AudioBuffer<float>& buffer, juce::MidiBuffer&)
{
    juce::ScopedNoDenormals noDenormals;

    const int numSamples = buffer.getNumSamples();
    for (int c = getTotalNumInputChannels(); c < getTotalNumOutputChannels(); ++c)
        buffer.clear (c, 0, numSamples);

    tl::Params p;
    p.inputDb        = get (ids::input);
    p.thresholdDb    = get (ids::threshold);
    p.ratio          = get (ids::ratio);
    p.attackMs       = get (ids::attack);
    p.releaseMs      = get (ids::release);
    p.kneeDb         = get (ids::knee);
    p.makeupDb       = get (ids::makeup);
    p.mix            = get (ids::mix) * 0.01f;
    p.outputDb       = get (ids::output);
    p.protection     = get (ids::protection) * 0.01f;
    p.bodyAmount     = get (ids::body) * 0.01f;
    p.sidechainHpfHz = get (ids::hpf);
    p.sensitivity    = get (ids::sens) * 0.01f;
    p.transAmount      = get (ids::tAmount) * 0.01f;
    p.transThresholdDb = get (ids::tThreshold);
    p.transRatio       = get (ids::tRatio);
    p.transAttackMs    = get (ids::tAttack);
    p.transReleaseMs   = get (ids::tRelease);
    p.transEnabled     = get (ids::tcOn) > 0.5f;
    p.bodyEnabled      = get (ids::bodyOn) > 0.5f;
    p.bypass         = get (ids::bypass) > 0.5f;

    // Cambio de oversampling: reiniciar estados y actualizar latencia
    const int osIdx = juce::jlimit (0, 2, (int) std::lround (get (ids::os)));
    if (osIdx != currentOs)
    {
        currentOs = osIdx;
        engines[(size_t) osIdx].reset();
        if (os2) os2->reset();
        if (os4) os4->reset();
        updateLatency();
    }

    const int nch = juce::jmin (buffer.getNumChannels(), tl::maxChannels);
    tl::Meters m;

    if (osIdx == 0 || ! os2 || ! os4)
    {
        m = engines[0].process (buffer.getArrayOfWritePointers(), nch, numSamples, p);
    }
    else
    {
        auto& os = (osIdx == 1) ? *os2 : *os4;
        auto block = juce::dsp::AudioBlock<float> (buffer).getSubsetChannelBlock (0, (size_t) nch);

        for (int pos = 0; pos < numSamples; pos += maxBlock)
        {
            const int len = juce::jmin (maxBlock, numSamples - pos);
            auto sub = block.getSubBlock ((size_t) pos, (size_t) len);

            auto osBlock = os.processSamplesUp (sub);

            float* chans[tl::maxChannels];
            const int onch = juce::jmin ((int) osBlock.getNumChannels(), tl::maxChannels);
            for (int c = 0; c < onch; ++c)
                chans[c] = osBlock.getChannelPointer ((size_t) c);

            m.merge (engines[(size_t) osIdx].process (chans, onch, (int) osBlock.getNumSamples(), p));

            os.processSamplesDown (sub);
        }
    }

    meterBody.store (m.bodyGrDb);
    meterApplied.store (m.appliedGrDb);
    meterTransient.store (m.transient);
    meterTransGr.store (m.transGrDb);
}

//==============================================================================
int TransientLockAudioProcessor::getNumPrograms() { return kNumPresets; }

const juce::String TransientLockAudioProcessor::getProgramName (int index)
{
    return (index >= 0 && index < kNumPresets) ? juce::String (kPresets[index].name) : juce::String();
}

void TransientLockAudioProcessor::setCurrentProgram (int index)
{
    if (index < 0 || index >= kNumPresets) return;
    currentProgram = index;
    const auto& pr = kPresets[index];

    auto set = [this] (const char* id, float v)
    {
        if (auto* prm = apvts.getParameter (id))
            prm->setValueNotifyingHost (prm->convertTo0to1 (v));
    };

    set (ids::input, pr.input);        set (ids::threshold, pr.threshold);
    set (ids::ratio, pr.ratio);        set (ids::attack, pr.attack);
    set (ids::release, pr.release);    set (ids::knee, pr.knee);
    set (ids::makeup, pr.makeup);      set (ids::mix, pr.mix);
    set (ids::output, pr.output);      set (ids::protection, pr.protection);
    set (ids::body, pr.body);          set (ids::hpf, pr.hpf);
    set (ids::sens, pr.sens);
    set (ids::tAmount, pr.tAmount);        set (ids::tThreshold, pr.tThreshold);
    set (ids::tRatio, pr.tRatio);          set (ids::tAttack, pr.tAttack);
    set (ids::tRelease, pr.tRelease);
    set (ids::tcOn, pr.tOn ? 1.0f : 0.0f);
    set (ids::bodyOn, 1.0f);
}

//==============================================================================
juce::AudioProcessorEditor* TransientLockAudioProcessor::createEditor()
{
    return new TransientLockAudioProcessorEditor (*this);
}

void TransientLockAudioProcessor::getStateInformation (juce::MemoryBlock& destData)
{
    if (auto xml = apvts.copyState().createXml())
        copyXmlToBinary (*xml, destData);
}

void TransientLockAudioProcessor::setStateInformation (const void* data, int sizeInBytes)
{
    if (auto xml = getXmlFromBinary (data, sizeInBytes))
        if (xml->hasTagName (apvts.state.getType()))
            apvts.replaceState (juce::ValueTree::fromXml (*xml));
}

juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter()
{
    return new TransientLockAudioProcessor();
}
