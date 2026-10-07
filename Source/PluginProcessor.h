#pragma once

#include <juce_audio_processors/juce_audio_processors.h>
#include "dsp/EchoEngine.h"
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
} // namespace ParamIDs

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

    static const juce::StringArray modeNames, satTypeNames, satPosNames, divisionNames;

private:
    static juce::AudioProcessorValueTreeState::ParameterLayout createLayout();
    float currentDelayMs();
    void updateEngineParams (float delayMs);
    void processSpace (juce::AudioBuffer<float>& buffer, int numChannels);
    void runEngine (SpaceMode m, float inL, float inR, float& outL, float& outR);
    void resetEngine (SpaceMode m);

    patina::BlockSaturator saturator;
    patina::EchoEngine tapeEcho { patina::EchoEngine::Flavor::Tape };
    patina::EchoEngine bbdEcho { patina::EchoEngine::Flavor::BBD };
    patina::SpringEngine spring;
    patina::PlateEngine plate;

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

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (PatinaProcessor)
};
