#include "PluginProcessor.h"
#include "PluginEditor.h"

using namespace patina;

const juce::StringArray PatinaProcessor::modeNames { "Tape", "BBD", "Spring", "Plate" };
const juce::StringArray PatinaProcessor::satTypeNames { "Tape", "Tube", "Fuzz", "Crush" };
const juce::StringArray PatinaProcessor::satPosNames { "Pre", "Space", "Post" };
const juce::StringArray PatinaProcessor::divisionNames { "1/32", "1/16T", "1/16", "1/8T", "1/16D", "1/8", "1/4T",
                                                         "1/8D", "1/4",  "1/2T",  "1/4D", "1/2", "1/1" };
const juce::StringArray PatinaProcessor::lfoShapeNames { "Sine", "Triangle", "Square", "Saw", "Sample & Hold", "Drift" };
const juce::StringArray PatinaProcessor::lfoDivisionNames { "8 bars", "4 bars", "2 bars", "1 bar", "1/2", "1/4D", "1/4",
                                                            "1/4T",   "1/8D",   "1/8",    "1/8T",  "1/16", "1/32" };
const juce::StringArray PatinaProcessor::lfoTargetNames { "Off",  "Time", "Feedback", "Pre-Delay", "Decay",    "Tone",
                                                          "Age",  "Width", "Mix",     "Drive",     "Sat Tone", "Sat Blend" };

const char* PatinaProcessor::modTargetParamID (int target)
{
    static const char* ids[] = { nullptr,        ParamIDs::time, ParamIDs::feedback, ParamIDs::preDelay,
                                 ParamIDs::decay, ParamIDs::tone, ParamIDs::age,      ParamIDs::width,
                                 ParamIDs::mix,   ParamIDs::drive, ParamIDs::satTone, ParamIDs::satBlend };
    return juce::isPositiveAndBelow (target, kNumModTargets) ? ids[target] : nullptr;
}

namespace
{
constexpr float kDivisionBeats[] = { 0.125f, 1.0f / 6.0f, 0.25f, 1.0f / 3.0f, 0.375f, 0.5f, 2.0f / 3.0f,
                                     0.75f,  1.0f,        4.0f / 3.0f, 1.5f,  2.0f, 4.0f };
constexpr float kLfoDivisionBeats[] = { 32.0f, 16.0f, 8.0f, 4.0f, 2.0f, 1.5f, 1.0f, 2.0f / 3.0f,
                                        0.75f, 0.5f,  1.0f / 3.0f, 0.25f, 0.125f };
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
    { "Seasick Tape",         { { "mode", 0 }, { "time", 380 }, { "feedback", 0.55f }, { "tone", 0.5f }, { "age", 0.5f },
                                { "mix", 0.35f }, { "drive", 8 }, { "satpos", 1 },
                                { "lfoshape", 5 }, { "lforate", 0.35f }, { "lfotarget1", 1 }, { "lfodepth1", 0.12f },
                                { "lfotarget2", 5 }, { "lfodepth2", -0.3f } } },
    { "Breathing Plate",      { { "mode", 3 }, { "predelay", 30 }, { "decay", 4.0f }, { "tone", 0.55f }, { "mix", 0.35f },
                                { "drive", 4 }, { "lfoshape", 0 }, { "lfosync", 1 }, { "lfodiv", 2 },
                                { "lfotarget1", 4 }, { "lfodepth1", 0.35f }, { "lfotarget2", 5 }, { "lfodepth2", 0.3f },
                                { "lfotarget3", 7 }, { "lfodepth3", 0.5f } } },
    { "Stuttering Spring",    { { "mode", 2 }, { "decay", 3.0f }, { "tone", 0.65f }, { "age", 0.6f }, { "mix", 0.4f },
                                { "drive", 14 }, { "sattype", 1 }, { "satpos", 1 },
                                { "lfoshape", 4 }, { "lfosync", 1 }, { "lfodiv", 9 },
                                { "lfotarget1", 9 }, { "lfodepth1", 0.6f }, { "lfotarget2", 8 }, { "lfodepth2", 0.4f } } },
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
    pLfoRate = raw (ParamIDs::lfoRate);   pLfoSync = raw (ParamIDs::lfoSync);   pLfoDiv = raw (ParamIDs::lfoDiv);
    pLfoShape = raw (ParamIDs::lfoShape);
    for (int i = 0; i < ParamIDs::numLfoSlots; ++i)
    {
        pLfoTarget[(size_t) i] = raw (ParamIDs::lfoTarget[i]);
        pLfoDepth[(size_t) i] = raw (ParamIDs::lfoDepth[i]);
    }
    for (int t = 0; t < kNumModTargets; ++t)
        if (auto* id = modTargetParamID (t))
            targetParams[(size_t) t] = apvts.getParameter (id);
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

    NormalisableRange<float> rateRange (0.02f, 20.0f);
    rateRange.setSkewForCentre (1.0f);
    floatParam (ParamIDs::lfoRate, "LFO Rate", rateRange, 0.5f,
                [] (float v, int) { return String (v, v < 1.0f ? 2 : 1) + " Hz"; }, plainFromText);
    layout.add (std::make_unique<AudioParameterChoice> (ParameterID { ParamIDs::lfoSync, 1 }, "LFO Sync", StringArray { "Off", "On" }, 0));
    layout.add (std::make_unique<AudioParameterChoice> (ParameterID { ParamIDs::lfoDiv, 1 }, "LFO Division", lfoDivisionNames, 3));
    layout.add (std::make_unique<AudioParameterChoice> (ParameterID { ParamIDs::lfoShape, 1 }, "LFO Shape", lfoShapeNames, 0));
    for (int i = 0; i < ParamIDs::numLfoSlots; ++i)
    {
        const String n (i + 1);
        layout.add (std::make_unique<AudioParameterChoice> (ParameterID { ParamIDs::lfoTarget[i], 1 }, "LFO Target " + n,
                                                            lfoTargetNames, 0));
        floatParam (ParamIDs::lfoDepth[i], ("LFO Depth " + n).toRawUTF8(), { -1.0f, 1.0f }, 0.0f,
                    [] (float v, int) { return (v > 0.0f ? "+" : "") + String (roundToInt (v * 100.0f)) + "%"; },
                    pctFromText);
    }

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

    lfo.reset();
    outputLevelUI = 0.0f;

    duckEnv = 0.0f;
    duckAttack = 1.0f - std::exp (-1.0f / (0.004f * (float) sampleRate));
    duckRelease = 1.0f - std::exp (-1.0f / (0.25f * (float) sampleRate));
}

PatinaProcessor::Settings PatinaProcessor::readSettings() const
{
    Settings st;
    st.mode = (int) pMode->load();
    st.satType = (int) pSatType->load();
    st.satPos = (int) pSatPos->load();
    st.division = juce::jlimit (0, (int) std::size (kDivisionBeats) - 1, (int) pDivision->load());
    st.sync = pSync->load() > 0.5f;
    st.time = pTime->load();
    st.feedback = pFeedback->load();
    st.preDelay = pPreDelay->load();
    st.decay = pDecay->load();
    st.tone = pTone->load();
    st.age = pAge->load();
    st.width = pWidth->load();
    st.mix = pMix->load();
    st.duck = pDuck->load();
    st.drive = pDrive->load();
    st.satTone = pSatTone->load();
    st.satBlend = pSatBlend->load();
    st.output = pOutput->load();
    return st;
}

float PatinaProcessor::delayMsFor (const Settings& st) const
{
    if (! st.sync)
        return st.time;
    return juce::jmin (kMaxDelayMs - 50.0f, (float) (kDivisionBeats[st.division] * 60000.0 / lastBpm));
}

float PatinaProcessor::lfoValueForChunk (int chunkStart, int chunkLen, double ppqAtBlockStart, bool transportRunning)
{
    const bool synced = pLfoSync->load() > 0.5f;
    const int div = juce::jlimit (0, (int) std::size (kLfoDivisionBeats) - 1, (int) pLfoDiv->load());
    const double cycleBeats = kLfoDivisionBeats[div];
    const auto shape = (Lfo::Shape) (int) pLfoShape->load();

    if (synced && transportRunning)
    {
        // Phase-locked to the song position, so the LFO lands identically on every playback.
        const double ppq = ppqAtBlockStart + (double) chunkStart / sr * lastBpm / 60.0;
        lfo.setPhase (ppq / cycleBeats);
        return lfo.value (shape);
    }

    const float v = lfo.value (shape);
    const double rateHz = synced ? lastBpm / 60.0 / cycleBeats : (double) pLfoRate->load();
    lfo.advance ((double) chunkLen / sr, rateHz);
    return v;
}

void PatinaProcessor::applyModulation (Settings& st, float& delayMs, float lfoValue)
{
    std::array<float, kNumModTargets> offset {};
    for (int i = 0; i < ParamIDs::numLfoSlots; ++i)
    {
        const int t = (int) pLfoTarget[(size_t) i]->load();
        if (t > 0 && t < kNumModTargets)
            offset[(size_t) t] += 0.5f * pLfoDepth[(size_t) i]->load() * lfoValue;
    }

    for (int t = 1; t < kNumModTargets; ++t)
        modOffsetUI[(size_t) t].store (offset[(size_t) t], std::memory_order_relaxed);

    // Offsets are applied in normalised parameter space, so a given depth
    // feels the same on every target regardless of its range or skew.
    auto mod = [&] (ModTarget target, float plain)
    {
        const float off = offset[(size_t) target];
        if (juce::exactlyEqual (off, 0.0f))
            return plain;
        auto* p = targetParams[(size_t) target];
        const auto& range = p->getNormalisableRange();
        const float norm = range.convertTo0to1 (juce::jlimit (range.start, range.end, plain));
        return range.convertFrom0to1 (juce::jlimit (0.0f, 1.0f, norm + off));
    };

    delayMs = mod (ModTarget::Time, delayMs);
    st.feedback = mod (ModTarget::Feedback, st.feedback);
    st.preDelay = mod (ModTarget::PreDelay, st.preDelay);
    st.decay = mod (ModTarget::Decay, st.decay);
    st.tone = mod (ModTarget::Tone, st.tone);
    st.age = mod (ModTarget::Age, st.age);
    st.width = mod (ModTarget::Width, st.width);
    st.mix = mod (ModTarget::Mix, st.mix);
    st.drive = mod (ModTarget::Drive, st.drive);
    st.satTone = mod (ModTarget::SatTone, st.satTone);
    st.satBlend = mod (ModTarget::SatBlend, st.satBlend);
}

void PatinaProcessor::applySettings (const Settings& st, float delayMs)
{
    saturator.setParameters ((SatType) st.satType, st.drive, st.satTone, st.satBlend);

    SpaceSat sat;
    sat.enabled = (SatPosition) st.satPos == SatPosition::Space;
    sat.type = (SatType) st.satType;
    sat.gain = dbToGain (st.drive);
    sat.amount = st.drive / 36.0f;
    sat.blend = st.satBlend;

    EchoEngine::Params ep;
    ep.delayMs = delayMs;
    ep.feedback = st.feedback;
    ep.tone = st.tone;
    ep.age = st.age;
    ep.sat = sat;
    tapeEcho.setParams (ep);
    bbdEcho.setParams (ep);

    SpringEngine::Params sp;
    sp.preDelayMs = st.preDelay;
    sp.decaySec = st.decay;
    sp.tone = st.tone;
    sp.age = st.age;
    sp.sat = sat;
    spring.setParams (sp);

    PlateEngine::Params pp;
    pp.preDelayMs = st.preDelay;
    pp.decaySec = st.decay;
    pp.tone = st.tone;
    pp.age = st.age;
    pp.sat = sat;
    plate.setParams (pp);

    mixSmoothed.setTargetValue (st.mix);
    widthSmoothed.setTargetValue (st.width);
    outputSmoothed.setTargetValue (dbToGain (st.output));
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

void PatinaProcessor::processSpace (float* left, float* right, int numSamples, bool stereo, float duck)
{
    const auto requested = (SpaceMode) (int) pMode->load();
    if (requested != activeMode && fadeRemaining == 0)
    {
        fadingMode = activeMode;
        activeMode = requested;
        fadeRemaining = fadeLength;
    }

    for (int i = 0; i < numSamples; ++i)
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

        if (stereo)
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
    const int numSamples = buffer.getNumSamples();
    for (int ch = numIn; ch < numOut; ++ch)
        buffer.clear (ch, 0, numSamples);

    const int numCh = std::min (numOut, buffer.getNumChannels());
    if (numCh == 0 || numSamples == 0)
        return;

    double ppq = 0.0;
    bool transportRunning = false;
    if (auto* ph = getPlayHead())
        if (auto pos = ph->getPosition())
        {
            if (auto bpm = pos->getBpm(); bpm.hasValue() && *bpm > 1.0)
                lastBpm = *bpm;
            if (auto p = pos->getPpqPosition(); p.hasValue() && pos->getIsPlaying())
            {
                ppq = *p;
                transportRunning = true;
            }
        }

    const Settings base = readSettings();
    const auto position = (SatPosition) base.satPos;
    const bool stereo = numCh > 1;
    juce::dsp::AudioBlock<float> fullBlock (buffer.getArrayOfWritePointers(), (size_t) numCh, (size_t) numSamples);

    // Control-rate loop: the LFO and all parameter-derived coefficients update every 32 samples.
    for (int start = 0; start < numSamples; start += kControlChunk)
    {
        const int len = std::min (kControlChunk, numSamples - start);

        Settings st = base;
        float delayMs = delayMsFor (st);
        const float lfoValue = lfoValueForChunk (start, len, ppq, transportRunning);
        applyModulation (st, delayMs, lfoValue);
        applySettings (st, delayMs);

        auto chunk = fullBlock.getSubBlock ((size_t) start, (size_t) len);

        // The oversampler runs exactly once per chunk (Pre slot, or Post slot when
        // Post is selected) so the reported latency is constant across positions.
        if (position != SatPosition::Post)
            saturator.process (chunk, position == SatPosition::Pre);

        float* left = buffer.getWritePointer (0, start);
        float* right = stereo ? buffer.getWritePointer (1, start) : left;
        processSpace (left, right, len, stereo, st.duck);

        if (position == SatPosition::Post)
            saturator.process (chunk, true);

        for (int i = 0; i < len; ++i)
        {
            const float g = outputSmoothed.getNextValue();
            left[i] *= g;
            if (stereo)
                right[i] *= g;
        }

        lfoValueUI.store (lfoValue, std::memory_order_relaxed);
    }

    lfoPhaseUI.store (lfo.getPhase(), std::memory_order_relaxed);

    float level = 0.0f, peak = 0.0f;
    for (int ch = 0; ch < numCh; ++ch)
    {
        level = std::max (level, buffer.getRMSLevel (ch, 0, numSamples));
        peak = std::max (peak, buffer.getMagnitude (ch, 0, numSamples));
    }
    outputLevelUI.store (level, std::memory_order_relaxed);
    outputPeakUI.store (std::max (peak, outputPeakUI.load (std::memory_order_relaxed)), std::memory_order_relaxed);
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
