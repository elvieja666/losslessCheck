#include "PluginEditor.h"

namespace
{
    const auto T = [] (const char* s) { return juce::String::fromUTF8 (s); };

    constexpr int fftOrder = 13;          // 8192: buen detalle en frecuencia para medir el corte
    constexpr double hopSeconds = 0.010;
    constexpr int width = 480, heightBase = 262, detailsExtra = 34;
}

LosslessCheckEditor::LosslessCheckEditor (LosslessCheckProcessor& p)
    : AudioProcessorEditor (p), proc (p)
{
    setLookAndFeel (&lnf);

    restartButton.setTooltip (T ("Empieza un análisis nuevo. También se reinicia solo al darle Play, "
                                 "después de 2 segundos de silencio o cuando cambia el audio."));
    restartButton.onClick = [this] { restart (T ("Análisis nuevo.")); };
    addAndMakeVisible (restartButton);

    detailsButton.setClickingTogglesState (true);
    detailsButton.setTooltip (T ("Muestra los datos técnicos de la medición."));
    detailsButton.onClick = [this] { setSize (width, heightBase + (detailsButton.getToggleState() ? detailsExtra : 0)); };
    addAndMakeVisible (detailsButton);

    bufL.resize (8192);
    bufR.resize (8192);
    mono.resize (8192);

    setSize (width, heightBase);
    proc.fifo.clear();
    seenPlayStarts = proc.playStarts.load();
    configure();
    startTimerHz (30);
}

LosslessCheckEditor::~LosslessCheckEditor()
{
    stopTimer();
    setLookAndFeel (nullptr);
}

void LosslessCheckEditor::configure()
{
    configuredRate = proc.currentSampleRate.load();
    analyzer.prepare (configuredRate, fftOrder, hopSeconds);
    detector.reset (analyzer.numBins(), analyzer.binHz(), configuredRate);
    verdict = sc::Judge::decide (detector.analyze());
}

void LosslessCheckEditor::restart (const juce::String& note)
{
    detector.reset (analyzer.numBins(), analyzer.binHz(), analyzer.getSampleRate());
    detector.setResetNote (note);
    verdict = sc::Judge::decide (detector.analyze());
    repaint();
}

void LosslessCheckEditor::timerCallback()
{
    if (std::abs (proc.currentSampleRate.load() - configuredRate) > 0.5)
        configure();

    // Le dieron Play en Logic: cada reproducción es un análisis nuevo.
    if (const int starts = proc.playStarts.load(); starts != seenPlayStarts)
    {
        seenPlayStarts = starts;
        restart (T ("Le diste Play: análisis nuevo."));
    }

    for (;;)
    {
        const int n = proc.fifo.pop (bufL.data(), bufR.data(), (int) bufL.size());
        if (n <= 0) break;
        detector.pushStereo (bufL.data(), bufR.data(), n);
        for (int i = 0; i < n; ++i)
            mono[(size_t) i] = 0.5f * (bufL[(size_t) i] + bufR[(size_t) i]);
        analyzer.push (mono.data(), n, [this] (const float* db, int bins, float rms)
        {
            detector.addFrame (db, bins, rms, analyzer.frameSeconds());
        });
    }

    if (++ticks % 6 == 0)   // 5 veces por segundo
    {
        verdict = sc::Judge::decide (detector.analyze());
        repaint();
    }
    else if (verdict.state == sc::Verdict::State::analyzing)
    {
        spin += 0.12f;
        repaint (card.withWidth (110));
    }
}

juce::Colour LosslessCheckEditor::colourFor (sc::Verdict::State s)
{
    using S = sc::Verdict::State;
    switch (s)
    {
        case S::original:   return Theme::good;
        case S::lossy:
        case S::inflated:   return Theme::codec;
        case S::doubtful:   return Theme::doubt;
        case S::cannotTell: return Theme::accent;
        case S::analyzing:  return Theme::accent;
        case S::waiting:    break;
    }
    return Theme::neutral;
}

// Ícono grande del veredicto: anillo girando mientras analiza; tilde, signo de
// exclamación o de pregunta cuando ya hay veredicto.
void LosslessCheckEditor::paintIcon (juce::Graphics& g, juce::Rectangle<float> r, juce::Colour c) const
{
    using S = sc::Verdict::State;
    const auto centre = r.getCentre();
    const float radius = r.getWidth() * 0.5f;

    if (verdict.state == S::analyzing || verdict.state == S::waiting)
    {
        g.setColour (Theme::line);
        g.drawEllipse (r.reduced (3.0f), 4.0f);
        if (verdict.state == S::analyzing)
        {
            juce::Path arc;
            arc.addCentredArc (centre.x, centre.y, radius - 3.0f, radius - 3.0f, 0.0f, spin, spin + 1.6f, true);
            g.setColour (c);
            g.strokePath (arc, juce::PathStrokeType (4.0f, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));
        }
        else
        {
            juce::Path play;
            play.addTriangle (centre.x - 7.0f, centre.y - 10.0f, centre.x - 7.0f, centre.y + 10.0f, centre.x + 11.0f, centre.y);
            g.setColour (Theme::dim);
            g.fillPath (play);
        }
        return;
    }

    g.setColour (c.withAlpha (0.16f));
    g.fillEllipse (r);
    g.setColour (c);
    g.drawEllipse (r.reduced (1.5f), 3.0f);

    const juce::PathStrokeType stroke (5.0f, juce::PathStrokeType::curved, juce::PathStrokeType::rounded);
    juce::Path p;
    switch (verdict.state)
    {
        case S::original:
            p.startNewSubPath (centre.x - radius * 0.38f, centre.y + radius * 0.02f);
            p.lineTo (centre.x - radius * 0.10f, centre.y + radius * 0.30f);
            p.lineTo (centre.x + radius * 0.40f, centre.y - radius * 0.28f);
            g.strokePath (p, stroke);
            break;
        case S::lossy:
        case S::inflated:
            p.startNewSubPath (centre.x, centre.y - radius * 0.40f);
            p.lineTo (centre.x, centre.y + radius * 0.10f);
            g.strokePath (p, stroke);
            g.fillEllipse (centre.x - 3.5f, centre.y + radius * 0.30f, 7.0f, 7.0f);
            break;
        case S::doubtful:
            g.setFont (Theme::font (radius * 1.15f, 600));
            g.drawText ("?", r, juce::Justification::centred);
            break;
        case S::cannotTell:
            p.startNewSubPath (centre.x - radius * 0.36f, centre.y);
            p.lineTo (centre.x + radius * 0.36f, centre.y);
            g.strokePath (p, stroke);
            break;
        case S::analyzing:
        case S::waiting:
            break;
    }
}

void LosslessCheckEditor::paint (juce::Graphics& g)
{
    g.fillAll (Theme::bg);

    // encabezado
    g.setColour (Theme::text);
    g.setFont (Theme::font (16.0f, 600, -0.01f));
    g.drawText ("Lossless Check", 20, 14, 160, 22, juce::Justification::centredLeft);
    g.setColour (Theme::faint);
    g.setFont (Theme::font (11.0f, 500));
    g.drawText (juce::String ("v") + JucePlugin_VersionString, 146, 18, 60, 16, juce::Justification::centredLeft);

    // tarjeta del veredicto
    const auto c = colourFor (verdict.state);
    g.setColour (Theme::bar);
    g.fillRoundedRectangle (card.toFloat(), 10.0f);
    g.setColour (verdict.state == sc::Verdict::State::analyzing || verdict.state == sc::Verdict::State::waiting
                     ? Theme::line : c.withAlpha (0.45f));
    g.drawRoundedRectangle (card.toFloat().reduced (0.5f), 10.0f, 1.0f);

    auto inner = card.reduced (22, 20);
    auto iconArea = inner.removeFromLeft (64).withSizeKeepingCentre (64, 64).withY (inner.getY() + 4).toFloat();
    paintIcon (g, iconArea, c);
    inner.removeFromLeft (20);

    g.setColour (Theme::text);
    g.setFont (Theme::font (28.0f, 600, -0.01f));
    g.drawText (verdict.title, inner.removeFromTop (36), juce::Justification::centredLeft, true);

    g.setColour (verdict.state == sc::Verdict::State::waiting ? Theme::dim : c);
    g.setFont (Theme::font (15.0f, 500));
    g.drawFittedText (verdict.subtitle, inner.removeFromTop (22), juce::Justification::centredLeft, 1, 0.85f);
    inner.removeFromTop (10);

    if (verdict.showProgress)
    {
        auto bar = inner.removeFromTop (6).toFloat();
        g.setColour (Theme::control);
        g.fillRoundedRectangle (bar, 3.0f);
        g.setColour (c);
        g.fillRoundedRectangle (bar.withWidth (juce::jmax (6.0f, bar.getWidth() * verdict.progress)), 3.0f);
        inner.removeFromTop (12);
    }

    g.setColour (Theme::dim);
    g.setFont (Theme::font (13.0f));
    g.drawFittedText (verdict.detail, inner, juce::Justification::topLeft, 4, 0.9f);

    // pie
    auto foot = getLocalBounds().removeFromBottom (44 + (detailsButton.getToggleState() ? detailsExtra : 0)).reduced (20, 0);
    if (detailsButton.getToggleState())
    {
        auto techRow = foot.removeFromTop (detailsExtra);
        g.setColour (Theme::dim);
        g.setFont (Theme::font (12.0f));
        g.drawFittedText (verdict.techText.isNotEmpty() ? verdict.techText : T ("Todavía no hay datos."),
                          techRow, juce::Justification::centredLeft, 1, 0.8f);
    }
    juce::String heard = T ("Escuchado: ") + juce::String (juce::roundToInt (verdict.seconds)) + " s  "
                         + T ("·") + "  " + juce::String (analyzer.getSampleRate() / 1000.0, 1) + " kHz";
    g.setColour (Theme::faint);
    g.setFont (Theme::font (12.0f));
    g.drawText (heard, foot.withTrimmedRight (200), juce::Justification::centredLeft);
}

void LosslessCheckEditor::resized()
{
    auto r = getLocalBounds();
    auto header = r.removeFromTop (50);
    restartButton.setBounds (header.getRight() - 20 - 90, 13, 90, 26);

    auto foot = r.removeFromBottom (44);
    if (detailsButton.getToggleState()) r.removeFromBottom (detailsExtra);
    detailsButton.setBounds (foot.getRight() - 20 - 86, foot.getCentreY() - 13, 86, 26);

    card = r.reduced (20, 0);
}
