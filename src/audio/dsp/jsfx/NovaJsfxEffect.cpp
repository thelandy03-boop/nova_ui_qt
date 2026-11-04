#include "audio/dsp/jsfx/NovaJsfxEffect.h"
#include <QFile>
#include <QFileInfo>
#include <QTextStream>
#include <QRegularExpression>
#include <algorithm>

NovaJsfxEffect::NovaJsfxEffect()
{
    m_varSpl0 = m_vm.getOrRegisterVariable("spl0");
    m_varSpl1 = m_vm.getOrRegisterVariable("spl1");
    m_varSrate = m_vm.getOrRegisterVariable("srate");
    m_varSamplesBlock = m_vm.getOrRegisterVariable("samplesblock");
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

bool NovaJsfxEffect::parseScript(const QString& scriptContent, QString* outError)
{
    m_scriptContent = scriptContent;
    m_sliders.clear();
    m_sliderVarIndices.clear();

    QString currentSection = QStringLiteral("header");
    QString initSrc, sliderSrc, blockSrc, sampleSrc;

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
        } else if (trimmed.startsWith(QStringLiteral("@"), Qt::CaseInsensitive)) {
            // Ignorar otras secciones como @gfx o @serialize por ahora
            currentSection = QStringLiteral("other");
            continue;
        }

        if (currentSection == QStringLiteral("header")) {
            if (trimmed.startsWith(QStringLiteral("desc:"), Qt::CaseInsensitive)) {
                m_name = trimmed.mid(5).trimmed();
            } else {
                auto match = sliderRegex.match(trimmed);
                if (match.hasMatch()) {
                    SliderMeta meta;
                    meta.index = static_cast<int>(m_sliders.size());
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

                    int varIdx = m_vm.getOrRegisterVariable(QStringLiteral("slider%1").arg(meta.sliderNum).toStdString());
                    m_vm.setVariable(varIdx, meta.defaultValue);

                    m_sliders.push_back(meta);
                    m_sliderVarIndices.push_back(varIdx);
                }
            }
        } else if (currentSection == QStringLiteral("init")) {
            initSrc += trimmed + QLatin1Char('\n');
        } else if (currentSection == QStringLiteral("slider")) {
            sliderSrc += trimmed + QLatin1Char('\n');
        } else if (currentSection == QStringLiteral("block")) {
            blockSrc += trimmed + QLatin1Char('\n');
        } else if (currentSection == QStringLiteral("sample")) {
            sampleSrc += trimmed + QLatin1Char('\n');
        }
    }

    // Compilar programas EEL2 en la VM
    std::string err;
    if (!m_vm.compile(initSrc.toStdString(), m_progInit, &err)) {
        if (outError) *outError = QStringLiteral("Error en @init: %1").arg(QString::fromStdString(err));
        return false;
    }
    if (!m_vm.compile(sliderSrc.toStdString(), m_progSlider, &err)) {
        if (outError) *outError = QStringLiteral("Error en @slider: %1").arg(QString::fromStdString(err));
        return false;
    }
    if (!m_vm.compile(blockSrc.toStdString(), m_progBlock, &err)) {
        if (outError) *outError = QStringLiteral("Error en @block: %1").arg(QString::fromStdString(err));
        return false;
    }
    if (!m_vm.compile(sampleSrc.toStdString(), m_progSample, &err)) {
        if (outError) *outError = QStringLiteral("Error en @sample: %1").arg(QString::fromStdString(err));
        return false;
    }

    // Inicializar estado con la tasa de muestreo y calcular variables derivadas en @slider
    m_vm.setVariable(m_varSrate, m_lastSampleRate);
    m_vm.execute(m_progInit);
    m_vm.execute(m_progSlider);

    reset();
    return true;
}

void NovaJsfxEffect::setParameter(int paramIndex, float value)
{
    if (paramIndex >= 0 && paramIndex < static_cast<int>(m_sliderVarIndices.size())) {
        std::lock_guard<std::mutex> lock(m_mutex);
        m_vm.setVariable(m_sliderVarIndices[static_cast<size_t>(paramIndex)], value);
        // Ejecutar código de @slider
        m_vm.execute(m_progSlider);
    }
}

float NovaJsfxEffect::getParameter(int paramIndex) const
{
    if (paramIndex >= 0 && paramIndex < static_cast<int>(m_sliderVarIndices.size())) {
        std::lock_guard<std::mutex> lock(m_mutex);
        return static_cast<float>(m_vm.getVariable(m_sliderVarIndices[static_cast<size_t>(paramIndex)]));
    }
    return 0.0f;
}

void NovaJsfxEffect::reset()
{
    m_vm.clearMemory();
    m_vm.setVariable(m_varSrate, m_lastSampleRate);

    // Asignar valores por defecto a los sliders en la VM
    for (size_t i = 0; i < m_sliders.size(); ++i) {
        m_vm.setVariable(m_sliderVarIndices[i], m_sliders[i].defaultValue);
    }

    // Ejecutar @init y @slider
    m_vm.execute(m_progInit);
    m_vm.execute(m_progSlider);
}

void NovaJsfxEffect::process(float* interleavedBuffer, int numFrames, int channels, int sampleRate)
{
    if (!interleavedBuffer || numFrames <= 0 || channels <= 0) return;

    std::unique_lock<std::mutex> lock(m_mutex, std::try_to_lock);
    if (!lock.owns_lock()) return;

    if (sampleRate != m_lastSampleRate && sampleRate > 0) {
        m_lastSampleRate = sampleRate;
        m_vm.setVariable(m_varSrate, sampleRate);
        m_vm.execute(m_progInit);
        m_vm.execute(m_progSlider);
    }

    m_vm.setVariable(m_varSamplesBlock, numFrames);

    // Ejecutar @block una vez por bloque de audio
    m_vm.execute(m_progBlock);

    // Si no hay código en @sample, no hay nada que procesar
    if (m_progSample.empty()) return;

    int effChannels = std::min(channels, 2);

    // Ejecutar @sample por cada fotograma de audio
    for (int frame = 0; frame < numFrames; ++frame) {
        int idxL = frame * channels;
        int idxR = (effChannels > 1) ? (idxL + 1) : idxL;

        m_vm.setVariable(m_varSpl0, interleavedBuffer[idxL]);
        m_vm.setVariable(m_varSpl1, interleavedBuffer[idxR]);

        m_vm.execute(m_progSample);

        interleavedBuffer[idxL] = static_cast<float>(m_vm.getVariable(m_varSpl0));
        if (effChannels > 1) {
            interleavedBuffer[idxR] = static_cast<float>(m_vm.getVariable(m_varSpl1));
        }
    }
}
