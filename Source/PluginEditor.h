#pragma once
#include "PluginProcessor.h"
#include "Verdict.h"
#include "Theme.h"

class LosslessCheckEditor : public juce::AudioProcessorEditor,
                            private juce::Timer
{
public:
    explicit LosslessCheckEditor (LosslessCheckProcessor&);
    ~LosslessCheckEditor() override;

    void paint (juce::Graphics&) override;
    void resized() override;

private:
    void timerCallback() override;
    void configure();
    void restart (const juce::String& note);
    void paintIcon (juce::Graphics&, juce::Rectangle<float>, juce::Colour) const;
    static juce::Colour colourFor (sc::Verdict::State);

    LosslessCheckProcessor& proc;
    Theme::LookAndFeel lnf;
    juce::TextButton restartButton { "Reiniciar" }, detailsButton { "Detalles" };

    sc::SpectrumAnalyzer analyzer;
    sc::CutoffDetector detector;
    sc::Verdict verdict;
    double configuredRate = 0.0;
    int seenPlayStarts = 0, ticks = 0;
    float spin = 0.0f;               // animación del "Analizando..."
    std::vector<float> bufL, bufR, mono;
    juce::Rectangle<int> card;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (LosslessCheckEditor)
};
