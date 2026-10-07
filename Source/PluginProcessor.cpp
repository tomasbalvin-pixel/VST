#include "PluginProcessor.h"
#include "PluginEditor.h"

using namespace patina;

const juce::StringArray PatinaProcessor::modeNames { "Tape", "BBD", "Spring", "Plate" };
const juce::StringArray PatinaProcessor::satTypeNames { "Tape", "Tube", "Fuzz", "Crush" };
const juce::StringArray PatinaProcessor::satPosNames { "Pre", "Space", "Post" };
const juce::StringArray PatinaProcessor::divisionNames { "1/32", "1/16T", "1/16", "1/8T", "1/16D", "1/8", "1/4T",
                                                         "1/8D", "1/4",  "1/2T",  "1/4D", "1/2", "1/1" };

namespace
{
constexpr float kDivisionBeats[] = { 0.125f, 1.0f / 6.0f, 0.25f, 1.0f / 3.0f, 0.375f, 0.5f, 2.0f / 3.0f,
                                     0.75f,  1.0f,        4.0f / 3.0f, 1.5f,  2.0f, 4.0f };
constexpr float kMaxDelayMs = 5000.0f;

juce::String pct (float v, int) { return juce::String (juce::roundToInt (v * 100.0f)) + "%"; }
float pctFromText (const juce::String& t) { return t.getFloatValue() / 100.0f; }
float msFromText (const juce::String& t)
{
    const auto s = t.trim().toLowerCase();
    return (s.endsWith ("s") && ! s.endsWith ("ms")) ? s.getFloatValue() * 1000.0f : s.getFloatValue();
}
float plainFromText (const juce::String& t) { return t.getFloatValue(); }

struct Preset
{
    const char* name;
    std::initializer_list<std::pair<const char*, float>> values;
};

// Values are in natural parameter units (choices by index). Unlisted parameters keep their defaults.
const Preset kPresets[] = {
    { "Slapback Tape",        { { "mode", 0 }, { "time", 110 }, { "feedback", 0.18f }, { "tone", 0.55f }, { "age", 0.25f },
                                { "mix", 0.3f }, { "drive", 6 }, { "sattype", 0 }, { "satpos", 0 } } },
    { "Dub Tape Siren",       { { "mode", 0 }, { "sync", 1 }, { "division", 10 }, { "feedback", 0.88f }, { "tone", 0.4f },
                                { "age", 0.6f }, { "mix", 0.4f }, { "drive", 14 }, { "sattype", 0 }, { "satpos", 1 },
                                { "duck", 0.4f } } },
    { "Bucket Brigade",       { { "mode", 1 }, { "time", 340 }, { "feedback", 0.45f }, { "tone", 0.5f }, { "age", 0.5f },
                                { "mix", 0.35f }, { "drive", 4 }, { "sattype", 1 }, { "satpos", 1 } } },
    { "Fuzzed BBD Runaway",   { { "mode", 1 }, { "sync", 1 }, { "division", 7 }, { "feedback", 1.02f }, { "tone", 0.45f },
                                { "age", 0.4f }, { "mix", 0.4f }, { "drive", 20 }, { "sattype", 2 }, { "satpos", 1 } } },
    { "Surf Spring",          { { "mode", 2 }, { "predelay", 0 }, { "decay", 2.6f }, { "tone", 0.6f }, { "age", 0.45f },
                                { "mix", 0.35f }, { "drive", 9 }, { "sattype", 1 }, { "satpos", 1 } } },
    { "Garage Drip",          { { "mode", 2 }, { "decay", 3.5f }, { "tone", 0.7f }, { "age", 0.8f }, { "mix", 0.45f },
                                { "drive", 18 }, { "sattype", 2 }, { "satpos", 0 }, { "satblend", 0.7f } } },
    { "Studio Plate",         { { "mode", 3 }, { "predelay", 22 }, { "decay", 2.4f }, { "tone", 0.62f }, { "age", 0.2f },
                                { "mix", 0.25f }, { "drive", 3 }, { "sattype", 0 }, { "satpos", 0 } } },
    { "Crushed Plate Wash",   { { "mode", 3 }, { "predelay", 60 }, { "decay", 8.0f }, { "tone", 0.45f }, { "age", 0.6f },
                                { "mix", 0.5f }, { "drive", 22 }, { "sattype", 3 }, { "satpos", 2 }, { "satblend", 0.5f } } },
};
} // namespace

PatinaProcessor::PatinaProcessor()
    : AudioProcessor (BusesProperties()
                          .withInput ("Input", juce::AudioChannelSet::stereo(), true)
                          .withOutput ("Output", juce::AudioChannelSet::stereo(), true)),
      apvts (*this, nullptr, "PATINA", createLayout())
{
    auto raw = [this] (const char* id) { return apvts.getRawParameterValue (id); };
    pMode = raw (ParamIDs::mode);         pTime = raw (ParamIDs::time);         pSync = raw (ParamIDs::sync);
    pDivision = raw (ParamIDs::division); pFeedback = raw (ParamIDs::feedback); pPreDelay = raw (ParamIDs::preDelay);
    pDecay = raw (ParamIDs::decay);       pTone = raw (ParamIDs::tone);         pAge = raw (ParamIDs::age);
    pWidth = raw (ParamIDs::width);       pDuck = raw (ParamIDs::duck);         pMix = raw (ParamIDs::mix);
    pDrive = raw (ParamIDs::drive);       pSatType = raw (ParamIDs::satType);   pSatPos = raw (ParamIDs::satPos);
    pSatTone = raw (ParamIDs::satTone);   pSatBlend = raw (ParamIDs::satBlend); pOutput = raw (ParamIDs::output);
}

juce::AudioProcessorValueTreeState::ParameterLayout PatinaProcessor::createLayout()
{
    using namespace juce;
    AudioProcessorValueTreeState::ParameterLayout layout;

    auto ms = [] (float v, int) { return v < 1000.0f ? String (roundToInt (v)) + " ms" : String (v / 1000.0f, 2) + " s"; };
    auto sec = [] (float v, int) { return String (v, v < 10.0f ? 2 : 1) + " s"; };
    auto db = [] (float v, int) { return (v > 0.0f ? "+" : "") + String (v, 1) + " dB"; };

    auto floatParam = [&] (const char* id, const char* name, NormalisableRange<float> range, float def,
                           std::function<String (float, int)> toText, std::function<float (const String&)> fromText)
    {
        layout.add (std::make_unique<AudioParameterFloat> (ParameterID { id, 1 }, name, range, def,
                                                           AudioParameterFloatAttributes()
                                                               .withStringFromValueFunction (std::move (toText))
                                                               .withValueFromStringFunction (std::move (fromText))));
    };

    layout.add (std::make_unique<AudioParameterChoice> (ParameterID { ParamIDs::mode, 1 }, "Mode", modeNames, 0));

    NormalisableRange<float> timeRange (10.0f, 2000.0f);
    timeRange.setSkewForCentre (300.0f);
    floatParam (ParamIDs::time, "Time", timeRange, 320.0f, ms, msFromText);
    // A two-state choice rather than AudioParameterBool: choices snap their stored
    // value, so hosts read back exactly what a saved state restores.
    layout.add (std::make_unique<AudioParameterChoice> (ParameterID { ParamIDs::sync, 1 }, "Sync", StringArray { "Off", "On" }, 0));
    layout.add (std::make_unique<AudioParameterChoice> (ParameterID { ParamIDs::division, 1 }, "Division", divisionNames, 8));
    floatParam (ParamIDs::feedback, "Feedback", { 0.0f, 1.1f }, 0.4f, pct, pctFromText);

    NormalisableRange<float> preRange (0.0f, 300.0f);
    preRange.setSkewForCentre (60.0f);
    floatParam (ParamIDs::preDelay, "Pre-Delay", preRange, 12.0f, ms, msFromText);
    NormalisableRange<float> decayRange (0.3f, 12.0f);
    decayRange.setSkewForCentre (2.5f);
    floatParam (ParamIDs::decay, "Decay", decayRange, 2.2f, sec, plainFromText);

    floatParam (ParamIDs::tone, "Tone", { 0.0f, 1.0f }, 0.55f, pct, pctFromText);
    floatParam (ParamIDs::age, "Age", { 0.0f, 1.0f }, 0.3f, pct, pctFromText);
    floatParam (ParamIDs::width, "Width", { 0.0f, 1.5f }, 1.0f, pct, pctFromText);
    floatParam (ParamIDs::duck, "Duck", { 0.0f, 1.0f }, 0.0f, pct, pctFromText);
    floatParam (ParamIDs::mix, "Mix", { 0.0f, 1.0f }, 0.35f, pct, pctFromText);

    floatParam (ParamIDs::drive, "Drive", { 0.0f, 36.0f }, 6.0f, db, plainFromText);
    layout.add (std::make_unique<AudioParameterChoice> (ParameterID { ParamIDs::satType, 1 }, "Saturation", satTypeNames, 0));
    layout.add (std::make_unique<AudioParameterChoice> (ParameterID { ParamIDs::satPos, 1 }, "Sat Position", satPosNames, 0));
    floatParam (ParamIDs::satTone, "Sat Tone", { 0.0f, 1.0f }, 0.75f, pct, pctFromText);
    floatParam (ParamIDs::satBlend, "Sat Blend", { 0.0f, 1.0f }, 1.0f, pct, pctFromText);
    floatParam (ParamIDs::output, "Output", { -24.0f, 12.0f }, 0.0f, db, plainFromText);

    return layout;
}

bool PatinaProcessor::isBusesLayoutSupported (const BusesLayout& layouts) const
{
    const auto out = layouts.getMainOutputChannelSet();
    if (out != juce::AudioChannelSet::stereo() && out != juce::AudioChannelSet::mono())
        return false;
    return layouts.getMainInputChannelSet() == out;
}

void PatinaProcessor::prepareToPlay (double sampleRate, int samplesPerBlock)
{
    sr = sampleRate;
    const int numCh = std::max (1, getTotalNumOutputChannels());

    saturator.prepare (sampleRate, samplesPerBlock, numCh);
    setLatencySamples (saturator.getLatencySamples());

    tapeEcho.prepare (sampleRate, kMaxDelayMs);
    bbdEcho.prepare (sampleRate, kMaxDelayMs);
    spring.prepare (sampleRate);
    plate.prepare (sampleRate);

    activeMode = fadingMode = (SpaceMode) (int) pMode->load();
    fadeRemaining = 0;
    fadeLength = (int) (0.04 * sampleRate);

    mixSmoothed.reset (sampleRate, 0.03);
    widthSmoothed.reset (sampleRate, 0.05);
    outputSmoothed.reset (sampleRate, 0.03);
    mixSmoothed.setCurrentAndTargetValue (pMix->load());
    widthSmoothed.setCurrentAndTargetValue (pWidth->load());
    outputSmoothed.setCurrentAndTargetValue (dbToGain (pOutput->load()));

    duckEnv = 0.0f;
    duckAttack = 1.0f - std::exp (-1.0f / (0.004f * (float) sampleRate));
    duckRelease = 1.0f - std::exp (-1.0f / (0.25f * (float) sampleRate));
}

float PatinaProcessor::currentDelayMs()
{
    if (pSync->load() < 0.5f)
        return pTime->load();

    if (auto* ph = getPlayHead())
        if (auto pos = ph->getPosition())
            if (auto bpm = pos->getBpm(); bpm.hasValue() && *bpm > 1.0)
                lastBpm = *bpm;

    const int div = juce::jlimit (0, (int) std::size (kDivisionBeats) - 1, (int) pDivision->load());
    return juce::jmin (kMaxDelayMs - 50.0f, (float) (kDivisionBeats[div] * 60000.0 / lastBpm));
}

void PatinaProcessor::updateEngineParams (float delayMs)
{
    SpaceSat sat;
    sat.enabled = (SatPosition) (int) pSatPos->load() == SatPosition::Space;
    sat.type = (SatType) (int) pSatType->load();
    sat.gain = dbToGain (pDrive->load());
    sat.amount = pDrive->load() / 36.0f;
    sat.blend = pSatBlend->load();

    const float tone = pTone->load(), age = pAge->load();

    EchoEngine::Params ep;
    ep.delayMs = delayMs;
    ep.feedback = pFeedback->load();
    ep.tone = tone;
    ep.age = age;
    ep.sat = sat;
    tapeEcho.setParams (ep);
    bbdEcho.setParams (ep);

    SpringEngine::Params sp;
    sp.preDelayMs = pPreDelay->load();
    sp.decaySec = pDecay->load();
    sp.tone = tone;
    sp.age = age;
    sp.sat = sat;
    spring.setParams (sp);

    PlateEngine::Params pp;
    pp.preDelayMs = pPreDelay->load();
    pp.decaySec = pDecay->load();
    pp.tone = tone;
    pp.age = age;
    pp.sat = sat;
    plate.setParams (pp);
}

void PatinaProcessor::runEngine (SpaceMode m, float inL, float inR, float& outL, float& outR)
{
    switch (m)
    {
        case SpaceMode::Tape:   tapeEcho.process (inL, inR, outL, outR); break;
        case SpaceMode::BBD:    bbdEcho.process (inL, inR, outL, outR); break;
        case SpaceMode::Spring: spring.process (inL, inR, outL, outR); break;
        case SpaceMode::Plate:  plate.process (inL, inR, outL, outR); break;
    }
}

void PatinaProcessor::resetEngine (SpaceMode m)
{
    switch (m)
    {
        case SpaceMode::Tape:   tapeEcho.reset(); break;
        case SpaceMode::BBD:    bbdEcho.reset(); break;
        case SpaceMode::Spring: spring.reset(); break;
        case SpaceMode::Plate:  plate.reset(); break;
    }
}

void PatinaProcessor::processSpace (juce::AudioBuffer<float>& buffer, int numChannels)
{
    const auto requested = (SpaceMode) (int) pMode->load();
    if (requested != activeMode && fadeRemaining == 0)
    {
        fadingMode = activeMode;
        activeMode = requested;
        fadeRemaining = fadeLength;
    }

    mixSmoothed.setTargetValue (pMix->load());
    widthSmoothed.setTargetValue (pWidth->load());
    const float duck = pDuck->load();

    auto* left = buffer.getWritePointer (0);
    auto* right = numChannels > 1 ? buffer.getWritePointer (1) : left;

    for (int i = 0; i < buffer.getNumSamples(); ++i)
    {
        const float inL = left[i], inR = right[i];

        float wetL, wetR;
        runEngine (activeMode, inL, inR, wetL, wetR);

        if (fadeRemaining > 0)
        {
            float oldL, oldR;
            runEngine (fadingMode, inL, inR, oldL, oldR);
            const float t = (float) fadeRemaining / (float) fadeLength; // 1 -> 0
            wetL = wetL * (1.0f - t) + oldL * t;
            wetR = wetR * (1.0f - t) + oldR * t;
            if (--fadeRemaining == 0)
                resetEngine (fadingMode);
        }

        // Stereo width on the wet signal (mid/side).
        const float w = widthSmoothed.getNextValue();
        const float mid = 0.5f * (wetL + wetR);
        const float side = 0.5f * (wetL - wetR) * w;
        wetL = mid + side;
        wetR = mid - side;

        // Ducking: the wet signal backs off while the input is busy.
        const float level = std::max (std::abs (inL), std::abs (inR));
        duckEnv += (level > duckEnv ? duckAttack : duckRelease) * (level - duckEnv);
        const float duckGain = 1.0f - duck * juce::jlimit (0.0f, 1.0f, duckEnv * 4.0f);

        // Equal-power dry/wet.
        const float m = mixSmoothed.getNextValue();
        const float dryGain = std::cos (m * 0.5f * kPi);
        const float wetGain = std::sin (m * 0.5f * kPi) * duckGain;

        if (numChannels > 1)
        {
            left[i] = inL * dryGain + wetL * wetGain;
            right[i] = inR * dryGain + wetR * wetGain;
        }
        else
        {
            left[i] = inL * dryGain + 0.5f * (wetL + wetR) * wetGain;
        }
    }
}

void PatinaProcessor::processBlock (juce::AudioBuffer<float>& buffer, juce::MidiBuffer&)
{
    juce::ScopedNoDenormals noDenormals;

    const int numIn = getTotalNumInputChannels();
    const int numOut = getTotalNumOutputChannels();
    for (int ch = numIn; ch < numOut; ++ch)
        buffer.clear (ch, 0, buffer.getNumSamples());

    const int numCh = std::min (numOut, buffer.getNumChannels());
    if (numCh == 0 || buffer.getNumSamples() == 0)
        return;

    const auto position = (SatPosition) (int) pSatPos->load();
    saturator.setParameters ((SatType) (int) pSatType->load(), pDrive->load(), pSatTone->load(), pSatBlend->load());
    updateEngineParams (currentDelayMs());

    // The oversampler runs exactly once per block (Pre slot, or Post slot when
    // Post is selected) so the reported latency is constant across positions.
    if (position != SatPosition::Post)
        saturator.process (buffer, position == SatPosition::Pre);

    processSpace (buffer, numCh);

    if (position == SatPosition::Post)
        saturator.process (buffer, true);

    outputSmoothed.setTargetValue (dbToGain (pOutput->load()));
    if (outputSmoothed.isSmoothing())
    {
        for (int i = 0; i < buffer.getNumSamples(); ++i)
        {
            const float g = outputSmoothed.getNextValue();
            for (int ch = 0; ch < numCh; ++ch)
                buffer.getWritePointer (ch)[i] *= g;
        }
    }
    else
    {
        buffer.applyGain (0, 0, buffer.getNumSamples(), outputSmoothed.getTargetValue());
        if (numCh > 1)
            buffer.applyGain (1, 0, buffer.getNumSamples(), outputSmoothed.getTargetValue());
    }
}

int PatinaProcessor::getNumPrograms() { return (int) std::size (kPresets); }

const juce::String PatinaProcessor::getProgramName (int index)
{
    return juce::isPositiveAndBelow (index, getNumPrograms()) ? kPresets[index].name : "";
}

void PatinaProcessor::setCurrentProgram (int index)
{
    if (! juce::isPositiveAndBelow (index, getNumPrograms()))
        return;
    currentProgram = index;

    // Reset everything to defaults first so presets are complete snapshots.
    for (auto* p : getParameters())
        if (auto* rp = dynamic_cast<juce::RangedAudioParameter*> (p))
            rp->setValueNotifyingHost (rp->getDefaultValue());

    for (const auto& [id, value] : kPresets[index].values)
        if (auto* rp = apvts.getParameter (id))
            rp->setValueNotifyingHost (rp->convertTo0to1 (value));
}

void PatinaProcessor::getStateInformation (juce::MemoryBlock& destData)
{
    auto state = apvts.copyState();
    state.setProperty ("program", currentProgram, nullptr);
    if (auto xml = state.createXml())
        copyXmlToBinary (*xml, destData);
}

void PatinaProcessor::setStateInformation (const void* data, int sizeInBytes)
{
    if (auto xml = getXmlFromBinary (data, sizeInBytes))
        if (xml->hasTagName (apvts.state.getType()))
        {
            auto state = juce::ValueTree::fromXml (*xml);
            currentProgram = state.getProperty ("program", 0);
            apvts.replaceState (state);
        }
}

juce::AudioProcessorEditor* PatinaProcessor::createEditor() { return new PatinaEditor (*this); }

juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter() { return new PatinaProcessor(); }
