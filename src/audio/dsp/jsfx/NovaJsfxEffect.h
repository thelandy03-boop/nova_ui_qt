#pragma once

#include "audio/dsp/NovaDspEffect.h"
#include "audio/dsp/jsfx/NovaJsfxYsfxEngine.h"
#include <QByteArray>
#include <QString>
#include <vector>
#include <memory>
#include <mutex>
#include <atomic>

/**
 * Efecto DSP que ejecuta scripts JSFX de Cockos REAPER.
 * Carga archivos de script JSFX (.jsfx / .txt), parsea sus sliders y evalúa
 * el código DSP (@init, @slider, @block, @sample) en tiempo real y guarda el
 * estado interno de @serialize.
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
    void setTransport(double playPositionSeconds, double bpm, bool playing) override;
    void reset() override;

    // Carga y recompilación en caliente de scripts
    bool loadScript(const QString& scriptContent, QString* outError = nullptr);
    bool loadScriptFile(const QString& filePath, QString* outError = nullptr);
    bool recompile(const QString& newSource, QString* outError = nullptr);

    QString scriptSource() const { return m_scriptContent; }
    const std::vector<SliderMeta>& sliders() const { return m_sliders; }

    /** true si el script declara una seccion @serialize con codigo. */
    bool hasSerialize() const { return m_serializePresente.load(std::memory_order_acquire); }

    /**
     * Ejecuta @serialize en modo escritura y devuelve el estado interno.
     * No incluye los sliders: esos se guardan aparte, como en REAPER.
     * Devuelve un QByteArray vacio si el script no tiene @serialize.
     */
    QByteArray saveState();

    /**
     * Restaura el estado interno ejecutando @serialize en modo lectura.
     * Orden: @init (con la RAM intacta) -> @serialize (lectura) -> @slider.
     * Los sliders deben haberse restaurado antes de llamar a este metodo.
     */
    bool loadState(const QByteArray& state);

private:
    struct ParsedScript {
        QString name;
        std::vector<SliderMeta> sliders;
        QString initSrc;
        QString sliderSrc;
        QString blockSrc;
        QString sampleSrc;
        QString serializeSrc;
    };

    static ParsedScript parseSource(const QString& scriptContent, const QString& baseName);
    bool parseScript(const QString& scriptContent, QString* outError = nullptr);
    void resetSinBloqueo();
    // Debe llamarse con m_mutex tomado: escribe en la VM del motor las
    // variables de transporte que veran @block/@sample.
    void aplicarTransporteAlMotor();

    QString m_id = QStringLiteral("jsfx:custom");
    QString m_name = QStringLiteral("JSFX Plugin");
    QString m_description;
    QString m_scriptContent;

    std::unique_ptr<NovaJsfxYsfxEngine> m_engine;

    std::vector<SliderMeta> m_sliders;
    std::vector<void*> m_sliderVars;
    std::atomic<bool> m_serializePresente{false};

    double m_playPosition = 0.0;
    // Transporte de la sesion. Se escribe desde el hilo de la UI y se lee en
    // el callback de audio; los atomics evitan bloquear el procesado.
    std::atomic<bool> m_transporteActivo{false};
    std::atomic<double> m_transporteSegundos{0.0};
    std::atomic<double> m_transporteBpm{120.0};
    std::atomic<bool> m_transporteReproduciendo{false};
    int m_lastSampleRate = 48000;
    int m_lastSamplesBlock = 512;
    mutable std::mutex m_mutex;
};
