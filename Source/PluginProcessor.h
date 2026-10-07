#pragma once

#include <juce_audio_processors/juce_audio_processors.h>
#include "dsp/EchoEngine.h"
#include "dsp/Lfo.h"
#include "dsp/PlateEngine.h"
#include "dsp/Saturation.h"
#include "dsp/SpringEngine.h"

namespace ParamIDs
{
inline constexpr auto mode      = "mode";
inline constexpr auto time      = "time";
inline constexpr auto sync      = "sync";
inline constexpr auto division  = "division";
inline constexpr auto feedback  = "feedback";
inline constexpr auto preDelay  = "predelay";
inline constexpr auto decay     = "decay";
inline constexpr auto tone      = "tone";
inline constexpr auto age       = "age";
inline constexpr auto width     = "width";
inline constexpr auto duck      = "duck";
inline constexpr auto mix       = "mix";
inline constexpr auto drive     = "drive";
inline constexpr auto satType   = "sattype";
inline constexpr auto satPos    = "satpos";
inline constexpr auto satTone   = "sattone";
inline constexpr auto satBlend  = "satblend";
inline constexpr auto output    = "output";
inline constexpr auto lfoRate   = "lforate";
inline constexpr auto lfoSync   = "lfosync";
inline constexpr auto lfoDiv    = "lfodiv";
inline constexpr auto lfoShape  = "lfoshape";
inline constexpr const char* lfoTarget[] = { "lfotarget1", "lfotarget2", "lfotarget3" };
inline constexpr const char* lfoDepth[]  = { "lfodepth1", "lfodepth2", "lfodepth3" };
inline constexpr int numLfoSlots = 3;
} // namespace ParamIDs

/** Parameters the LFO can be routed to (index order matches the target choice parameter). */
enum class ModTarget { Off = 0, Time, Feedback, PreDelay, Decay, Tone, Age, Width, Mix, Drive, SatTone, SatBlend, Count };
inline constexpr int kNumModTargets = (int) ModTarget::Count;

enum class SpaceMode { Tape = 0, BBD, Spring, Plate };
enum class SatPosition { Pre = 0, Space, Post };

class PatinaProcessor final : public juce::AudioProcessor
{
public:
    PatinaProcessor();
    ~PatinaProcessor() override = default;

    void prepareToPlay (double sampleRate, int samplesPerBlock) override;
    void releaseResources() override {}
    bool isBusesLayoutSupported (const BusesLayout& layouts) const override;
    void processBlock (juce::AudioBuffer<float>&, juce::MidiBuffer&) override;
    using AudioProcessor::processBlock;

    juce::AudioProcessorEditor* createEditor() override;
    bool hasEditor() const override { return true; }

    const juce::String getName() const override { return JucePlugin_Name; }
    bool acceptsMidi() const override { return false; }
    bool producesMidi() const override { return false; }
    bool isMidiEffect() const override { return false; }
    double getTailLengthSeconds() const override { return 12.0; }

    int getNumPrograms() override;
    int getCurrentProgram() override { return currentProgram; }
    void setCurrentProgram (int index) override;
    const juce::String getProgramName (int index) override;
    void changeProgramName (int, const juce::String&) override {}

    void getStateInformation (juce::MemoryBlock& destData) override;
    void setStateInformation (const void* data, int sizeInBytes) override;

    juce::AudioProcessorValueTreeState apvts;

    static const juce::StringArray modeNames, satTypeNames, satPosNames, divisionNames,
                                   lfoShapeNames, lfoDivisionNames, lfoTargetNames;

    /** Parameter ID for each modulation target (nullptr for Off). */
    static const char* modTargetParamID (int target);

    // Read by the editor for metering and modulation display.
    std::atomic<float> lfoValueUI { 0.0f }, lfoPhaseUI { 0.0f }, outputLevelUI { 0.0f };
    std::atomic<float> outputPeakUI { 0.0f }; // max since the editor last reset it
    std::array<std::atomic<float>, kNumModTargets> modOffsetUI {};

private:
    struct Settings
    {
        int mode = 0, satType = 0, satPos = 0, division = 8;
        bool sync = false;
        float time = 320, feedback = 0.4f, preDelay = 12, decay = 2.2f, tone = 0.55f, age = 0.3f, width = 1,
              mix = 0.35f, duck = 0, drive = 6, satTone = 0.75f, satBlend = 1, output = 0;
    };

    static constexpr int kControlChunk = 32;

    static juce::AudioProcessorValueTreeState::ParameterLayout createLayout();
    Settings readSettings() const;
    float delayMsFor (const Settings&) const;
    float lfoValueForChunk (int chunkStart, int chunkLen, double ppqAtBlockStart, bool transportRunning);
    void applyModulation (Settings&, float& delayMs, float lfoValue);
    void applySettings (const Settings&, float delayMs);
    void processSpace (float* left, float* right, int numSamples, bool stereo, float duck);
    void runEngine (SpaceMode m, float inL, float inR, float& outL, float& outR);
    void resetEngine (SpaceMode m);

    patina::BlockSaturator saturator;
    patina::EchoEngine tapeEcho { patina::EchoEngine::Flavor::Tape };
    patina::EchoEngine bbdEcho { patina::EchoEngine::Flavor::BBD };
    patina::SpringEngine spring;
    patina::PlateEngine plate;
    patina::Lfo lfo;
    std::array<juce::RangedAudioParameter*, kNumModTargets> targetParams {};

    SpaceMode activeMode = SpaceMode::Tape, fadingMode = SpaceMode::Tape;
    int fadeRemaining = 0, fadeLength = 1;

    juce::SmoothedValue<float> mixSmoothed, widthSmoothed, outputSmoothed;
    float duckEnv = 0.0f, duckAttack = 0.0f, duckRelease = 0.0f;
    double sr = 44100.0;
    double lastBpm = 120.0;
    int currentProgram = 0;

    // Cached raw parameter pointers (audio-thread safe reads).
    std::atomic<float>* pMode {}; std::atomic<float>* pTime {}; std::atomic<float>* pSync {};
    std::atomic<float>* pDivision {}; std::atomic<float>* pFeedback {}; std::atomic<float>* pPreDelay {};
    std::atomic<float>* pDecay {}; std::atomic<float>* pTone {}; std::atomic<float>* pAge {};
    std::atomic<float>* pWidth {}; std::atomic<float>* pDuck {}; std::atomic<float>* pMix {};
    std::atomic<float>* pDrive {}; std::atomic<float>* pSatType {}; std::atomic<float>* pSatPos {};
    std::atomic<float>* pSatTone {}; std::atomic<float>* pSatBlend {}; std::atomic<float>* pOutput {};
    std::atomic<float>* pLfoRate {}; std::atomic<float>* pLfoSync {}; std::atomic<float>* pLfoDiv {};
    std::atomic<float>* pLfoShape {};
    std::array<std::atomic<float>*, ParamIDs::numLfoSlots> pLfoTarget {}, pLfoDepth {};

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (PatinaProcessor)
};
