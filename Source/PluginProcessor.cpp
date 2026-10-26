#include "PluginProcessor.h"
#include "PluginEditor.h"

LosslessCheckProcessor::LosslessCheckProcessor()
    : AudioProcessor (BusesProperties()
                          .withInput  ("Input",  juce::AudioChannelSet::stereo(), true)
                          .withOutput ("Output", juce::AudioChannelSet::stereo(), true))
{
}

void LosslessCheckProcessor::prepareToPlay (double sampleRate, int)
{
    currentSampleRate.store (sampleRate);
}

bool LosslessCheckProcessor::isBusesLayoutSupported (const BusesLayout& layouts) const
{
    const auto out = layouts.getMainOutputChannelSet();
    if (out != juce::AudioChannelSet::mono() && out != juce::AudioChannelSet::stereo())
        return false;
    return layouts.getMainInputChannelSet() == out;
}

// El audio pasa sin tocarse: solo se copia a la cola para analizarlo.
void LosslessCheckProcessor::processBlock (juce::AudioBuffer<float>& buffer, juce::MidiBuffer&)
{
    juce::ScopedNoDenormals noDenormals;
    const int numIn = getTotalNumInputChannels();
    for (int ch = numIn; ch < getTotalNumOutputChannels(); ++ch)
        buffer.clear (ch, 0, buffer.getNumSamples());

    // Avisar a la ventana cuando Logic arranca a reproducir (para empezar un análisis nuevo).
    if (auto* head = getPlayHead())
        if (auto position = head->getPosition())
        {
            const bool playing = position->getIsPlaying();
            if (playing && ! wasPlaying)
                playStarts.fetch_add (1);
            wasPlaying = playing;
        }

    if (numIn == 0) return;
    const float* l = buffer.getReadPointer (0);
    const float* r = numIn > 1 ? buffer.getReadPointer (1) : l;
    fifo.push (l, r, buffer.getNumSamples());
}

juce::AudioProcessorEditor* LosslessCheckProcessor::createEditor()
{
    return new LosslessCheckEditor (*this);
}

juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter()
{
    return new LosslessCheckProcessor();
}
