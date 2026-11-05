#pragma once

#include "audio/dsp/NovaDspEffect.h"
#include "audio/dsp/jsfx/NovaJsfxWdlEngine.h"
#include <QString>
#include <vector>
#include <memory>
#include <mutex>

/**
 * Efecto DSP que ejecuta scripts JSFX de Cockos REAPER.
 * Carga archivos de script JSFX (.jsfx / .txt), parsea sus sliders y evalúa
 * el código DSP (@init, @slider, @block, @sample) en tiempo real.
 *
 * La fachada y el parser de cabecera son propios. El lenguaje EEL2 lo
 * ejecuta el motor real de Cockos (WDL) a través de NovaJsfxWdlEngine.
 */
class NovaJsfxEffect : public NovaDspEffect {
public:
    struct SliderMeta {
        int index = 0;
        int sliderNum = 1; // 1 a 64
        QString name;
        float defaultValue = 0.0f;
        float minValue = 0.0f;
        float maxValue = 1.0f;
        float stepValue = 0.01f;
        QString unit;
    };

    struct Metadata {
        QString name;
        std::vector<SliderMeta> sliders;
    };

    /**
     * Lee solo la cabecera (desc: y sliderN:), sin crear ni compilar una VM.
     * La usa la UI para mostrar los controles mientras se escribe, aunque el
     * cuerpo del script todavia no compile.
     */
    static Metadata parseMetadata(const QString& scriptContent);

    NovaJsfxEffect();
    explicit NovaJsfxEffect(const QString& scriptContent, const QString& customName = QString());
    ~NovaJsfxEffect() override = default;

    QString id() const override { return m_id; }
    QString name() const override { return m_name; }
    QString category() const override { return QStringLiteral("JSFX"); }

    void process(float* interleavedBuffer, int numFrames, int channels, int sampleRate) override;
    void setParameter(int paramIndex, float value) override;
    float getParameter(int paramIndex) const override;
    void reset() override;

    // Carga y recompilación en caliente de scripts
    bool loadScript(const QString& scriptContent, QString* outError = nullptr);
    bool loadScriptFile(const QString& filePath, QString* outError = nullptr);
    bool recompile(const QString& newSource, QString* outError = nullptr);

    QString scriptSource() const { return m_scriptContent; }
    const std::vector<SliderMeta>& sliders() const { return m_sliders; }

private:
    struct ParsedScript {
        QString name;
        std::vector<SliderMeta> sliders;
        QString initSrc;
        QString sliderSrc;
        QString blockSrc;
        QString sampleSrc;
    };

    static ParsedScript parseSource(const QString& scriptContent, const QString& baseName);
    bool parseScript(const QString& scriptContent, QString* outError = nullptr);
    void resetSinBloqueo();

    QString m_id = QStringLiteral("jsfx:custom");
    QString m_name = QStringLiteral("JSFX Plugin");
    QString m_description;
    QString m_scriptContent;

    std::unique_ptr<NovaJsfxWdlEngine> m_engine;

    std::vector<SliderMeta> m_sliders;
    std::vector<void*> m_sliderVars;

    double m_playPosition = 0.0;
    int m_lastSampleRate = 48000;
    int m_lastSamplesBlock = 512;
    mutable std::mutex m_mutex;
};
