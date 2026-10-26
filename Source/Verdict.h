#pragma once
#include "Analyzer.h"

// ---------------------------------------------------------------------------
//  El "juez": mira lo que mide el motor (corte, huecos en los agudos, líneas
//  fijas) y decide si ya hay información suficiente para dar un veredicto.
//  Mientras no la hay, dice "Analizando..." con el progreso.
//
//  Reglas (calibradas con un master 24/48 y sus versiones MP3 de Logic, LAME
//  y AAC):
//   - Original: los agudos llegan arriba y casi no tienen huecos durante 30 s.
//   - Comprimido: si la huella es muy clara se dice enseguida (10 s), si no a
//     los 20 s. La calidad se muestra recién a los 45 s, cuando ya es estable.
//   - Dudoso: algunos huecos, pocos para asegurar, después de 45 s.
//   - No se puede analizar: la música no llega a los agudos (30 s).
// ---------------------------------------------------------------------------
namespace sc
{
    struct Verdict
    {
        enum class State { waiting, analyzing, original, lossy, inflated, doubtful, cannotTell };
        State state = State::waiting;
        juce::String title, subtitle, detail;
        float progress = 0.0f;      // 0..1 mientras analiza (o mientras calcula la calidad)
        bool showProgress = false;
        float seconds = 0.0f;
        juce::String techText;      // datos técnicos para "Detalles"
    };

    class Judge
    {
    public:
        static constexpr float originalSeconds = 30.0f;
        static constexpr float lossyClearSeconds = 10.0f;
        static constexpr float lossySeconds = 20.0f;
        static constexpr float qualitySeconds = 45.0f;
        static constexpr float doubtfulSeconds = 45.0f;
        static constexpr float clearHolesPct = 20.0f;     // huella "muy clara"
        static constexpr float originalMaxHolesPct = 1.0f; // el WAV probado nunca pasó de 0.6% en 10 s

        static Verdict decide (const CutoffResult& r)
        {
            using K = CutoffResult::Kind;
            using S = Verdict::State;
            Verdict v;
            v.seconds = r.seconds;
            v.techText = r.techText;
            const float sec = r.seconds;
            const auto kHz = [] (double hz) { return juce::String (hz / 1000.0, 1) + " kHz"; };

            if (r.kind == K::waiting)
            {
                v.state = sec <= 0.0f && r.title.startsWith ("Esperando") ? S::waiting : S::analyzing;
                v.title = v.state == S::waiting ? txt ("Dale Play") : txt ("Analizando...");
                v.subtitle = v.state == S::waiting ? txt ("Esperando audio") : txt ("Escuchando el audio");
                v.detail = r.detail;
                v.showProgress = v.state == S::analyzing;
                return v;
            }

            const float score = r.holesMeasured ? juce::jmax (r.holesMid, r.holesSide) : -1.0f;
            const bool holesLossy = r.holesMeasured && score >= CutoffDetector::holesStrongPct;
            const bool cutLossy = r.kind == K::codec && ! holesLossy;   // corte abrupto típico de códec
            bool anyLineAbove = false;
            for (const auto& l : r.lines) anyLineAbove = anyLineAbove || l.aboveCutoff;

            // --- alta resolución inflada (88.2/96 kHz que corta en ~22 kHz) ---
            if (r.kind == K::resampled && sec >= lossyClearSeconds)
            {
                v.state = S::inflated;
                v.title = txt ("Alta resolución inflada");
                v.subtitle = txt ("Viene de un archivo de 44.1/48 kHz");
                v.detail = txt ("Corta en ") + kHz (r.edgeHz) + txt (" aunque el archivo es de mayor calidad: lo agrandaron desde 44.1/48 kHz o desde un MP3/AAC.");
                return v;
            }

            // --- comprimido ---
            const bool lossyNow = (cutLossy && sec >= lossyClearSeconds)
                               || (holesLossy && score >= clearHolesPct && sec >= lossyClearSeconds)
                               || (holesLossy && sec >= lossySeconds);
            if (lossyNow)
            {
                v.state = S::lossy;
                v.title = txt ("Comprimido");
                if (sec >= qualitySeconds)
                    v.subtitle = txt ("MP3/AAC · posible ") + CutoffDetector::estimateQuality (r);
                else
                {
                    v.subtitle = txt ("MP3/AAC · calculando la calidad...");
                    v.showProgress = true;
                    v.progress = sec / qualitySeconds;
                }
                v.detail = txt ("Tiene la huella de un MP3 o AAC. Si el archivo es WAV o AIFF, es un falso WAV.");
                if (anyLineAbove)
                    v.detail += txt (" Además tiene un tono fijo por encima del corte, como un relleno para disimularlo.");
                return v;
            }

            // --- original ---
            if (r.holesMeasured && score < originalMaxHolesPct && (r.kind == K::fullBand || r.kind == K::gradual))
            {
                if (sec >= originalSeconds)
                {
                    v.state = S::original;
                    v.title = txt ("Original");
                    v.subtitle = txt ("Sin huella de MP3/AAC");
                    v.detail = txt ("Llega hasta ") + kHz (r.edgeHz) + txt (" y los agudos son continuos, como en un WAV original.");
                    return v;
                }
                return analyzing (v, sec / originalSeconds, txt ("Por ahora no aparece ninguna huella de MP3/AAC."));
            }

            // --- dudoso ---
            if (r.holesMeasured && score >= originalMaxHolesPct)
            {
                if (sec >= doubtfulSeconds)
                {
                    v.state = S::doubtful;
                    v.title = txt ("Dudoso");
                    v.subtitle = txt ("Podría ser un MP3/AAC de muy alta calidad");
                    v.detail = txt ("Tiene muy pocas huellas para asegurarlo. Dejalo sonar más o probá con otra parte del tema.");
                    return v;
                }
                return analyzing (v, sec / doubtfulSeconds, txt ("Aparecen algunas huellas: hace falta escuchar más para decidir."));
            }

            // --- no llega a los agudos ---
            if (! r.holesMeasured && r.edgeHz > 0.0f && r.edgeHz < CutoffDetector::holeMinEdgeHz && ! cutLossy)
            {
                if (sec >= originalSeconds)
                {
                    v.state = S::cannotTell;
                    v.title = txt ("No se puede analizar");
                    v.subtitle = txt ("La música no llega a los agudos");
                    v.detail = txt ("Se apaga cerca de ") + kHz (r.edgeHz)
                               + txt (". La huella de un MP3 se ve en los agudos y este tema casi no tiene. Probá con una parte más brillante.");
                    return v;
                }
                return analyzing (v, sec / originalSeconds, txt ("Por ahora la música casi no llega a los agudos."));
            }

            return analyzing (v, sec / originalSeconds, {});
        }

    private:
        static Verdict analyzing (Verdict v, float progress, const juce::String& note)
        {
            v.state = Verdict::State::analyzing;
            v.title = txt ("Analizando...");
            v.subtitle = txt ("Escuchando el audio");
            v.detail = note;
            v.showProgress = true;
            v.progress = juce::jlimit (0.0f, 1.0f, progress);
            return v;
        }
    };
}
