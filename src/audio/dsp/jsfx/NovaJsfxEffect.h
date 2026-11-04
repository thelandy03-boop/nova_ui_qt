#pragma once

#include "audio/dsp/NovaDspEffect.h"
#include "audio/dsp/jsfx/NovaJsfxVm.h"
#include <QString>
#include <vector>
#include <memory>
#include <mutex>

/**
 * Efecto DSP que ejecuta scripts JSFX de Cockos REAPER.
 * Carga archivos de script JSFX (.jsfx / .txt), parsea sus sliders y evalúa
 * el código DSP (@init, @slider, @block, @sample) en tiempo real.
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
    bool parseScript(const QString& scriptContent, QString* outError = nullptr);

    QString m_id = QStringLiteral("jsfx:custom");
    QString m_name = QStringLiteral("JSFX Plugin");
    QString m_description;
    QString m_scriptContent;

    NovaJsfxVm m_vm;
    NovaJsfxVm::Program m_progInit;
    NovaJsfxVm::Program m_progSlider;
    NovaJsfxVm::Program m_progBlock;
    NovaJsfxVm::Program m_progSample;

    int m_varSpl0 = -1;
    int m_varSpl1 = -1;
    int m_varSrate = -1;
    int m_varSamplesBlock = -1;

    std::vector<SliderMeta> m_sliders;
    std::vector<int> m_sliderVarIndices;

    int m_lastSampleRate = 48000;
    mutable std::mutex m_mutex;
};
