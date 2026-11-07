#include "audio/dsp/jsfx/NovaJsfxEffect.h"
#include <QFile>
#include <QFileInfo>
#include <QTextStream>
#include <QRegularExpression>
#include <algorithm>
#include <string>

NovaJsfxEffect::NovaJsfxEffect()
    : m_engine(std::make_unique<NovaJsfxYsfxEngine>())
{
}

NovaJsfxEffect::NovaJsfxEffect(const QString& scriptContent, const QString& customName)
    : NovaJsfxEffect()
{
    if (!customName.isEmpty()) {
        m_name = customName;
    }
    loadScript(scriptContent);
}

bool NovaJsfxEffect::loadScriptFile(const QString& filePath, QString* outError)
{
    QFile file(filePath);
    if (!file.open(QIODevice::ReadOnly | QIODevice::Text)) {
        if (outError) *outError = QStringLiteral("No se pudo abrir el archivo JSFX: %1").arg(filePath);
        return false;
    }
    QTextStream in(&file);
    QString content = in.readAll();
    file.close();

    m_id = QStringLiteral("jsfx:%1").arg(QFileInfo(filePath).baseName());
    return loadScript(content, outError);
}

bool NovaJsfxEffect::recompile(const QString& newSource, QString* outError)
{
    return loadScript(newSource, outError);
}

bool NovaJsfxEffect::loadScript(const QString& scriptContent, QString* outError)
{
    std::lock_guard<std::mutex> lock(m_mutex);
    return parseScript(scriptContent, outError);
}

NovaJsfxEffect::ParsedScript NovaJsfxEffect::parseSource(const QString& scriptContent,
                                                         const QString& baseName)
{
    ParsedScript parsed;
    parsed.name = baseName;

    QString currentSection = QStringLiteral("header");

    const QStringList lines = scriptContent.split(QRegularExpression(QStringLiteral("[\r\n]+")));

    // Regex para sliders: slider1:0<-24,24,0.1>Nombre (dB)
    QRegularExpression sliderRegex(QStringLiteral("^slider(\\d+)\\s*:\\s*([-\\d.]+)\\s*<\\s*([-\\d.]+)\\s*,\\s*([-\\d.]+)(?:\\s*,\\s*([-\\d.]+))?[^>]*>\\s*(.*)$"));

    for (const QString& line : lines) {
        QString trimmed = line.trimmed();
        if (trimmed.isEmpty()) continue;

        if (trimmed.startsWith(QStringLiteral("@init"), Qt::CaseInsensitive)) {
            currentSection = QStringLiteral("init");
            continue;
        } else if (trimmed.startsWith(QStringLiteral("@slider"), Qt::CaseInsensitive)) {
            currentSection = QStringLiteral("slider");
            continue;
        } else if (trimmed.startsWith(QStringLiteral("@block"), Qt::CaseInsensitive)) {
            currentSection = QStringLiteral("block");
            continue;
        } else if (trimmed.startsWith(QStringLiteral("@sample"), Qt::CaseInsensitive)) {
            currentSection = QStringLiteral("sample");
            continue;
        } else if (trimmed.startsWith(QStringLiteral("@serialize"), Qt::CaseInsensitive)) {
            currentSection = QStringLiteral("serialize");
            continue;
        } else if (trimmed.startsWith(QStringLiteral("@"), Qt::CaseInsensitive)) {
            // @gfx y otras secciones quedan fuera de esta fase.
            currentSection = QStringLiteral("other");
            continue;
        }

        if (currentSection == QStringLiteral("header")) {
            if (trimmed.startsWith(QStringLiteral("desc:"), Qt::CaseInsensitive)) {
                parsed.name = trimmed.mid(5).trimmed();
            } else {
                auto match = sliderRegex.match(trimmed);
                if (match.hasMatch()) {
                    SliderMeta meta;
                    meta.index = static_cast<int>(parsed.sliders.size());
                    meta.sliderNum = match.captured(1).toInt();
                    meta.defaultValue = match.captured(2).toFloat();
                    meta.minValue = match.captured(3).toFloat();
                    meta.maxValue = match.captured(4).toFloat();
                    meta.stepValue = match.captured(5).isEmpty() ? 0.01f : match.captured(5).toFloat();
                    meta.name = match.captured(6).trimmed();
                    if (meta.name.isEmpty()) {
                        meta.name = QStringLiteral("Control %1").arg(meta.sliderNum);
                    }

                    // Deducir unidad de la etiqueta (ej. "Gain (dB)" -> unit "dB")
                    if (meta.name.contains(QLatin1Char('(')) && meta.name.endsWith(QLatin1Char(')'))) {
                        int pOpen = meta.name.lastIndexOf(QLatin1Char('('));
                        meta.unit = meta.name.mid(pOpen + 1, meta.name.length() - pOpen - 2).trimmed();
                    }

                    parsed.sliders.push_back(meta);
                }
            }
        } else if (currentSection == QStringLiteral("init")) {
            parsed.initSrc += trimmed + QLatin1Char('\n');
        } else if (currentSection == QStringLiteral("slider")) {
            parsed.sliderSrc += trimmed + QLatin1Char('\n');
        } else if (currentSection == QStringLiteral("block")) {
            parsed.blockSrc += trimmed + QLatin1Char('\n');
        } else if (currentSection == QStringLiteral("sample")) {
            parsed.sampleSrc += trimmed + QLatin1Char('\n');
        } else if (currentSection == QStringLiteral("serialize")) {
            parsed.serializeSrc += trimmed + QLatin1Char('\n');
        }
    }

    return parsed;
}

NovaJsfxEffect::Metadata NovaJsfxEffect::parseMetadata(const QString& scriptContent)
{
    ParsedScript parsed = parseSource(scriptContent, QStringLiteral("JSFX Plugin"));
    Metadata metadata;
    metadata.name = parsed.name;
    metadata.sliders = std::move(parsed.sliders);
    return metadata;
}

bool NovaJsfxEffect::parseScript(const QString& scriptContent, QString* outError)
{
    // El editor reabre el ultimo texto intentado, aunque el cuerpo no compile.
    m_scriptContent = scriptContent;

    ParsedScript parsed = parseSource(scriptContent, m_name);

    // El motor se construye aparte y solo se publica si todas las secciones
    // compilan. Asi un script roto no deja el efecto anterior a medias.
    auto engine = std::make_unique<NovaJsfxYsfxEngine>();

    std::vector<void*> sliderVars;
    sliderVars.reserve(parsed.sliders.size());
    for (const SliderMeta& meta : parsed.sliders) {
        void* var = engine->registrarVariable("slider" + std::to_string(meta.sliderNum));
        engine->asignarVariable(var, meta.defaultValue);
        sliderVars.push_back(var);
    }

    engine->setSampleRate(m_lastSampleRate);
    engine->setSamplesBlock(m_lastSamplesBlock);
    // El @init inicial ya ve el transporte real si la sesion se lo ha dado.
    const bool hayTransporte = m_transporteActivo.load(std::memory_order_relaxed);
    engine->setTempo(hayTransporte ? m_transporteBpm.load(std::memory_order_relaxed) : 120.0);
    engine->setPlayPosition(hayTransporte ? m_transporteSegundos.load(std::memory_order_relaxed) : 0.0);
    engine->setPlayState(hayTransporte && m_transporteReproduciendo.load(std::memory_order_relaxed) ? 1 : 0);
    engine->setNumChannels(2);

    std::string err;
    if (!engine->cargar(scriptContent.toStdString(), &err)) {
        if (outError) {
            *outError = QStringLiteral("Error en %1").arg(QString::fromStdString(err));
        }
        return false;
    }

    // Inicializar el estado con la tasa de muestreo y calcular las variables
    // derivadas de @slider antes de publicar el motor.
    engine->ejecutarInit();
    engine->ejecutarSlider();

    m_engine = std::move(engine);
    m_sliders = std::move(parsed.sliders);
    m_sliderVars = std::move(sliderVars);
    m_name = parsed.name;
    m_playPosition = 0.0;
    m_serializePresente.store(!parsed.serializeSrc.isEmpty(), std::memory_order_release);
    m_gfxPresente.store(m_engine->tieneGfx(), std::memory_order_release);

    // Se conserva el comportamiento historico: tras cargar se reinicia el
    // estado. Con @serialize no vacio, REAPER no borra la RAM al ejecutar
    // @init, asi que resetSinBloqueo respeta esa regla.
    resetSinBloqueo();
    return true;
}

void NovaJsfxEffect::setParameter(int paramIndex, float value)
{
    std::lock_guard<std::mutex> lock(m_mutex);
    if (!m_engine || paramIndex < 0 || paramIndex >= static_cast<int>(m_sliderVars.size())) {
        return;
    }

    m_engine->setSampleRate(m_lastSampleRate);
    m_engine->setSamplesBlock(m_lastSamplesBlock);
    aplicarTransporteAlMotor();
    m_engine->asignarVariable(m_sliderVars[static_cast<size_t>(paramIndex)], value);
    // Ejecutar codigo de @slider
    m_engine->ejecutarSlider();
}

float NovaJsfxEffect::getParameter(int paramIndex) const
{
    std::lock_guard<std::mutex> lock(m_mutex);
    if (!m_engine || paramIndex < 0 || paramIndex >= static_cast<int>(m_sliderVars.size())) {
        return 0.0f;
    }
    return static_cast<float>(m_engine->leerVariable(m_sliderVars[static_cast<size_t>(paramIndex)]));
}

void NovaJsfxEffect::reset()
{
    std::lock_guard<std::mutex> lock(m_mutex);
    resetSinBloqueo();
}

QByteArray NovaJsfxEffect::saveState()
{
    std::lock_guard<std::mutex> lock(m_mutex);
    if (!m_engine || !m_engine->tieneSerialize()) return {};

    m_engine->setSampleRate(m_lastSampleRate);
    m_engine->setSamplesBlock(m_lastSamplesBlock);
    aplicarTransporteAlMotor();
    m_engine->setNumChannels(2);

    const std::string estado = m_engine->guardarEstado();
    return QByteArray(estado.data(), static_cast<int>(estado.size()));
}

bool NovaJsfxEffect::loadState(const QByteArray& state)
{
    std::lock_guard<std::mutex> lock(m_mutex);
    if (!m_engine || !m_engine->tieneSerialize() || state.isEmpty()) return false;

    m_engine->setSampleRate(m_lastSampleRate);
    m_engine->setSamplesBlock(m_lastSamplesBlock);
    aplicarTransporteAlMotor();
    m_engine->setNumChannels(2);

    // Orden de REAPER: @init con la RAM intacta, luego @serialize en lectura
    // y por ultimo @slider. Los sliders se restauran antes de esta llamada.
    m_engine->ejecutarInit();

    const bool ok = m_engine->cargarEstado(
        std::string(state.constData(), static_cast<size_t>(state.size())));
    // YSFX pone must_compute_slider a true en ysfx_load_state: @slider se
    // ejecuta en el siguiente process(). Llamarlo aqui re-dispara @init.
    return ok;
}

void NovaJsfxEffect::setTransport(double playPositionSeconds, double bpm, bool playing)
{
    m_transporteSegundos.store(playPositionSeconds, std::memory_order_relaxed);
    m_transporteBpm.store(bpm > 0.0 ? bpm : 120.0, std::memory_order_relaxed);
    m_transporteReproduciendo.store(playing, std::memory_order_relaxed);
    m_transporteActivo.store(true, std::memory_order_release);
}

void NovaJsfxEffect::aplicarTransporteAlMotor()
{
    if (!m_engine) return;

    if (m_transporteActivo.load(std::memory_order_acquire)) {
        const double bpm = m_transporteBpm.load(std::memory_order_relaxed);
        m_engine->setTempo(bpm);
        m_engine->setPlayPosition(m_transporteSegundos.load(std::memory_order_relaxed));
        m_engine->setPlayState(
            m_transporteReproduciendo.load(std::memory_order_relaxed) ? 1 : 0);
        return;
    }

    // Sin sesion (o sin transporte publicado) se conserva el comportamiento
    // historico: 120 BPM y el reloj interno del propio efecto.
    m_engine->setTempo(120.0);
    m_engine->setPlayPosition(m_playPosition);
    m_engine->setPlayState(0);
}

void NovaJsfxEffect::resetSinBloqueo()
{
    m_playPosition = 0.0;
    m_lastSamplesBlock = 512;

    if (!m_engine) return;

    // Vaciar la RAM de los scripts es lo que hace el boton de reset: los
    // buffers de delay vuelven a cero y @init los reconstruye. Un @serialize
    // no vacio desactiva ese borrado, igual que en REAPER: el estado interno
    // debe sobrevivir a los reinit (cambios de tasa, reactivacion, etc.).
    if (!hasSerialize()) {
        m_engine->limpiarMemoria();
    }
    m_engine->setSampleRate(m_lastSampleRate);
    m_engine->setSamplesBlock(m_lastSamplesBlock);
    aplicarTransporteAlMotor();
    m_engine->setNumChannels(2);

    const size_t limite = std::min(m_sliders.size(), m_sliderVars.size());
    for (size_t i = 0; i < limite; ++i) {
        m_engine->asignarVariable(m_sliderVars[i], m_sliders[i].defaultValue);
    }

    // Ejecutar @init y @slider
    m_engine->ejecutarInit();
    m_engine->ejecutarSlider();
}

void NovaJsfxEffect::process(float* interleavedBuffer, int numFrames, int channels, int sampleRate)
{
    if (!interleavedBuffer || numFrames <= 0 || channels <= 0) return;

    std::unique_lock<std::mutex> lock(m_mutex, std::try_to_lock);
    if (!lock.owns_lock()) return;
    if (!m_engine) return;

    if (sampleRate != m_lastSampleRate && sampleRate > 0) {
        m_lastSampleRate = sampleRate;
        m_engine->setSampleRate(sampleRate);
        m_engine->ejecutarInit();
        m_engine->ejecutarSlider();
    }

    m_lastSamplesBlock = numFrames;
    m_engine->setSamplesBlock(numFrames);
    aplicarTransporteAlMotor();

    if (sampleRate > 0) {
        m_playPosition += static_cast<double>(numFrames) / static_cast<double>(sampleRate);
    }

    if (!m_engine->tieneSample()) return;

    int effChannels = std::min(channels, 2);
    m_engine->procesarBloque(interleavedBuffer, numFrames, effChannels > 0 ? effChannels : 1);
}

bool NovaJsfxEffect::getGfxDimensions(int &width, int &height) const
{
    std::lock_guard<std::mutex> lock(m_mutex);
    if (!m_engine || !m_engine->tieneGfx()) return false;
    uint32_t w = 0, h = 0;
    if (m_engine->obtenerDimensionesGfx(w, h)) {
        width = static_cast<int>(w);
        height = static_cast<int>(h);
        return true;
    }
    return false;
}

void NovaJsfxEffect::setupGfx(uint8_t *pixels, int width, int height, int stride, double scaleFactor)
{
    std::lock_guard<std::mutex> lock(m_mutex);
    if (!m_engine) return;
    m_engine->configurarGfx(pixels, static_cast<uint32_t>(width), static_cast<uint32_t>(height),
                            static_cast<uint32_t>(stride), scaleFactor);
}

bool NovaJsfxEffect::renderGfx()
{
    std::lock_guard<std::mutex> lock(m_mutex);
    if (!m_engine) return false;
    return m_engine->renderizarGfx();
}

void NovaJsfxEffect::updateGfxMouse(uint32_t mods, int x, int y, uint32_t buttons, double wheelV, double wheelH)
{
    std::lock_guard<std::mutex> lock(m_mutex);
    if (!m_engine) return;
    m_engine->actualizarRatonGfx(mods, x, y, buttons, wheelV, wheelH);
}

void NovaJsfxEffect::setGfxWindowState(bool hasFocus, bool visible, bool mouseOver)
{
    std::lock_guard<std::mutex> lock(m_mutex);
    if (!m_engine) return;
    m_engine->setEstadoVentanaGfx(hasFocus, visible, mouseOver);
}

int NovaJsfxEffect::requestedGfxFps() const
{
    std::lock_guard<std::mutex> lock(m_mutex);
    if (!m_engine) return 30;
    return static_cast<int>(m_engine->fpsSolicitadosGfx());
}


