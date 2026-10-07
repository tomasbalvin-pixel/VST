// Loads the built Patina VST3, renders impulse / burst signals through every
// mode and factory preset, and checks for NaN, runaway level, silence and
// sensible tails. Writes WAV renders next to the binary for listening.

#include <juce_audio_formats/juce_audio_formats.h>
#include <juce_audio_processors/juce_audio_processors.h>
#include <iostream>

using namespace juce;

namespace
{
constexpr double kRate = 48000.0;
constexpr int kBlock = 256;

int failures = 0;

void check (bool ok, const String& what)
{
    std::cout << (ok ? "  [ok]   " : "  [FAIL] ") << what << "\n";
    if (! ok) ++failures;
}

void setParam (AudioPluginInstance& p, const String& name, float plainValue)
{
    for (auto* param : p.getParameters())
        if (param->getName (64) == name)
        {
            // Map plain -> normalised using the parameter's own text parser.
            param->setValueNotifyingHost (param->getValueForText (String (plainValue)));
            return;
        }
    check (false, "parameter not found: " + name);
}

void setParamNormalised (AudioPluginInstance& p, const String& name, float v)
{
    for (auto* param : p.getParameters())
        if (param->getName (64) == name)
        {
            param->setValueNotifyingHost (v);
            return;
        }
    check (false, "parameter not found: " + name);
}

struct Stats { float peak = 0, tailRms = 0, earlyRms = 0; bool finite = true; double cpuRatio = 0; };

Stats render (AudioPluginInstance& p, int seconds, const File& wav, bool burst)
{
    p.reset();
    const int total = (int) (seconds * kRate);
    AudioBuffer<float> out (2, total);
    AudioBuffer<float> block (2, kBlock);
    MidiBuffer midi;
    Random rng (1);

    const auto t0 = Time::getHighResolutionTicks();
    for (int start = 0; start < total; start += kBlock)
    {
        const int n = std::min (kBlock, total - start);
        block.setSize (2, n, false, false, true);
        for (int i = 0; i < n; ++i)
        {
            const int s = start + i;
            float x = 0.0f;
            if (burst)
                x = s < (int) (0.25 * kRate) ? 0.5f * std::sin (2.0 * MathConstants<double>::pi * 220.0 * s / kRate)
                                                   + 0.1f * (rng.nextFloat() * 2.0f - 1.0f)
                                             : 0.0f;
            else
                x = (s == 0) ? 0.9f : 0.0f;
            block.setSample (0, i, x);
            block.setSample (1, i, x);
        }
        p.processBlock (block, midi);
        for (int ch = 0; ch < 2; ++ch)
            out.copyFrom (ch, start, block, ch, 0, n);
    }
    const double elapsed = Time::highResolutionTicksToSeconds (Time::getHighResolutionTicks() - t0);

    Stats st;
    st.cpuRatio = elapsed / seconds;
    for (int ch = 0; ch < 2; ++ch)
        for (int i = 0; i < total; ++i)
        {
            const float v = out.getSample (ch, i);
            if (! std::isfinite (v)) st.finite = false;
            st.peak = std::max (st.peak, std::abs (v));
        }
    const int early0 = (int) (0.3 * kRate), early1 = (int) (1.3 * kRate);
    const int tail0 = total - (int) (1.0 * kRate);
    st.earlyRms = 0.5f * (out.getRMSLevel (0, early0, early1 - early0) + out.getRMSLevel (1, early0, early1 - early0));
    st.tailRms = 0.5f * (out.getRMSLevel (0, tail0, total - tail0) + out.getRMSLevel (1, tail0, total - tail0));

    WavAudioFormat fmt;
    wav.deleteFile();
    if (auto os = std::unique_ptr<FileOutputStream> (wav.createOutputStream()))
        if (auto w = std::unique_ptr<AudioFormatWriter> (fmt.createWriterFor (os.get(), kRate, 2, 24, {}, 0)))
        {
            os.release();
            w->writeFromAudioSampleBuffer (out, 0, total);
        }
    return st;
}
} // namespace

int main (int argc, char** argv)
{
    ScopedJuceInitialiser_GUI juce;

    File pluginFile (argc > 1 ? String (argv[1]) : String (PATINA_VST3_PATH));
    if (! pluginFile.getFullPathName().endsWith (".vst3"))
        pluginFile = pluginFile.getChildFile ("VST3/Patina.vst3");
    File outDir = File::getCurrentWorkingDirectory().getChildFile ("renders");
    outDir.createDirectory();

    AudioPluginFormatManager fm;
    fm.addFormat (new VST3PluginFormat());
    OwnedArray<PluginDescription> descs;
    VST3PluginFormat vst3;
    vst3.findAllTypesForFile (descs, pluginFile.getFullPathName());
    if (descs.isEmpty()) { std::cout << "No plugin found at " << pluginFile.getFullPathName() << "\n"; return 2; }

    String err;
    auto plugin = fm.createPluginInstance (*descs[0], kRate, kBlock, err);
    if (plugin == nullptr) { std::cout << "Load failed: " << err << "\n"; return 2; }

    std::cout << "Loaded " << plugin->getName() << ", params: " << plugin->getParameters().size()
              << ", latency: " << plugin->getLatencySamples() << " samples\n";
    plugin->setPlayConfigDetails (2, 2, kRate, kBlock);
    plugin->prepareToPlay (kRate, kBlock);
    std::cout << "Latency after prepare: " << plugin->getLatencySamples() << " samples\n";

    // --- Each mode, impulse response, mix 100%
    const char* modes[] = { "Tape", "BBD", "Spring", "Plate" };
    for (int m = 0; m < 4; ++m)
    {
        plugin->setCurrentProgram (0);
        setParamNormalised (*plugin, "Mode", (float) m / 3.0f);
        setParamNormalised (*plugin, "Mix", 1.0f);
        setParam (*plugin, "Drive", 0.0f);
        setParam (*plugin, "Decay", 3.0f);
        setParam (*plugin, "Feedback", 50.0f);
        std::cout << "Mode " << modes[m] << " (impulse)\n";
        auto st = render (*plugin, 5, outDir.getChildFile (String ("ir_") + modes[m] + ".wav"), false);
        check (st.finite, "finite output");
        check (st.peak > 0.005f && st.peak < 2.0f, "peak in range: " + String (st.peak, 4));
        check (st.earlyRms > 1.0e-4f, "audible early wet: rms " + String (st.earlyRms, 6));
        check (st.tailRms < st.earlyRms, "tail decays: " + String (st.tailRms, 6));
        std::cout << "         cpu " << String (st.cpuRatio * 100.0, 2) << "% of realtime\n";
    }

    // --- Wet level vs input on sustained pink-ish noise (mix 100%, drive 0)
    for (int m = 0; m < 4; ++m)
    {
        plugin->setCurrentProgram (0);
        setParamNormalised (*plugin, "Mode", (float) m / 3.0f);
        setParamNormalised (*plugin, "Mix", 1.0f);
        setParam (*plugin, "Drive", 0.0f);
        setParam (*plugin, "Feedback", 40.0f);
        plugin->reset();
        Random rng (7);
        AudioBuffer<float> block (2, kBlock);
        MidiBuffer midi;
        double inSq = 0, outSq = 0;
        float lp = 0;
        for (int b = 0; b < (int) (4 * kRate / kBlock); ++b)
        {
            for (int i = 0; i < kBlock; ++i)
            {
                lp += 0.2f * ((rng.nextFloat() * 2.0f - 1.0f) * 0.3f - lp);
                block.setSample (0, i, lp);
                block.setSample (1, i, lp);
                if (b > kRate / kBlock) inSq += (double) lp * lp;
            }
            plugin->processBlock (block, midi);
            if (b > kRate / kBlock)
                for (int i = 0; i < kBlock; ++i)
                    outSq += 0.5 * (block.getSample (0, i) * block.getSample (0, i) + block.getSample (1, i) * block.getSample (1, i));
        }
        const double ratioDb = 10.0 * std::log10 (outSq / inSq);
        check (ratioDb > -12.0 && ratioDb < 6.0, String ("wet level ") + modes[m] + ": " + String (ratioDb, 1) + " dB re input");
    }

    // --- Factory presets, burst signal
    for (int i = 0; i < plugin->getNumPrograms(); ++i)
    {
        plugin->setCurrentProgram (i);
        std::cout << "Preset " << i << ": " << plugin->getProgramName (i) << "\n";
        auto st = render (*plugin, 8, outDir.getChildFile ("preset_" + String (i) + ".wav"), true);
        check (st.finite, "finite output");
        check (st.peak > 0.01f && st.peak < 4.0f, "peak in range: " + String (st.peak, 3));
        std::cout << "         tail rms " << String (st.tailRms, 6) << ", cpu "
                  << String (st.cpuRatio * 100.0, 2) << "% of realtime\n";
    }

    // --- Stress: max feedback, max drive, every saturation type in the loop
    const char* sats[] = { "Tape", "Tube", "Fuzz", "Crush" };
    for (int m = 0; m < 4; ++m)
        for (int s = 0; s < 4; ++s)
        {
            plugin->setCurrentProgram (0);
            setParamNormalised (*plugin, "Mode", (float) m / 3.0f);
            setParamNormalised (*plugin, "Saturation", (float) s / 3.0f);
            setParamNormalised (*plugin, "Sat Position", 0.5f);
            setParamNormalised (*plugin, "Feedback", 1.0f);
            setParamNormalised (*plugin, "Decay", 1.0f);
            setParamNormalised (*plugin, "Drive", 1.0f);
            setParamNormalised (*plugin, "Age", 1.0f);
            setParamNormalised (*plugin, "Mix", 1.0f);
            auto st = render (*plugin, 6, outDir.getChildFile ("stress.wav"), true);
            check (st.finite && st.peak < 4.0f,
                   String ("stress ") + modes[m] + " + " + sats[s] + ": peak " + String (st.peak, 3));
        }

    // --- State round trip
    {
        plugin->setCurrentProgram (4);
        MemoryBlock state;
        plugin->getStateInformation (state);
        plugin->setCurrentProgram (0);
        plugin->setStateInformation (state.getData(), (int) state.getSize());
        MemoryBlock state2;
        plugin->getStateInformation (state2);
        check (state == state2, "state round trip");
    }

    // --- Optional editor snapshot (needs a display; run under xvfb-run on CI/Linux)
    if (argc > 2)
    {
        for (int m : { 0, 2 })
        {
            plugin->setCurrentProgram (m == 0 ? 1 : 4);
            std::unique_ptr<AudioProcessorEditor> editor (plugin->createEditorIfNeeded());
            check (editor != nullptr, "editor created");
            if (editor == nullptr) break;
            editor->setVisible (true);
            auto img = editor->createComponentSnapshot (editor->getLocalBounds(), true, 1.0f);
            File png (String (argv[2]) + (m == 0 ? "_echo.png" : "_spring.png"));
            png.deleteFile();
            FileOutputStream os (png);
            PNGImageFormat().writeImageToStream (img, os);
            std::cout << "Saved " << png.getFullPathName() << " (" << img.getWidth() << "x" << img.getHeight() << ")\n";
        }
    }

    plugin->releaseResources();
    std::cout << (failures == 0 ? "ALL PASSED\n" : String (failures) + " FAILURES\n");
    return failures == 0 ? 0 : 1;
}
