#include "NovaWaveformItem.h"
#include "NovaWaveformRenderer.h"
#include <QQuickWindow>

NovaWaveformItem::NovaWaveformItem(QQuickItem *parent)
    : QQuickFramebufferObject(parent)
{
    setAcceptedMouseButtons(Qt::AllButtons);
}

NovaWaveformItem::~NovaWaveformItem() {
}

QQuickFramebufferObject::Renderer *NovaWaveformItem::createRenderer() const {
    auto *r = new NovaWaveformRenderer();
    m_renderer = r;
    return r;
}

void NovaWaveformItem::releaseResources()
{
    // 🔒 Qt avisa aqui cuando va a perder el contexto de OpenGL: es el
    // UNICO momento seguro para liberar buffers/VAO/programa. Si se libera
    // en el destructor del renderer, el contexto puede estar ya destruido y
    // la app peta con SIGSEGV al salir (dentro de QSGNode::destroy()).
    if (m_renderer) {
        m_renderer->releaseGLResources();
    }
    QQuickFramebufferObject::releaseResources();
}

void NovaWaveformItem::setRegionIndex(int index) {
    if (m_regionIndex != index) {
        m_regionIndex = index;
        emit regionIndexChanged();
    }
}

void NovaWaveformItem::setRegionId(const QString &id) {
    if (m_regionId != id) {
        m_regionId = id;
        emit regionIdChanged();
    }
}

void NovaWaveformItem::setPeakColor(const QColor &color) {
    if (m_peakColor != color) {
        m_peakColor = color;
        emit peakColorChanged();
        update();
    }
}

void NovaWaveformItem::setRmsColor(const QColor &color) {
    if (m_rmsColor != color) {
        m_rmsColor = color;
        emit rmsColorChanged();
        update();
    }
}

void NovaWaveformItem::setSelected(bool selected) {
    if (m_selected != selected) {
        m_selected = selected;
        emit selectedChanged();
        update();
    }
}

void NovaWaveformItem::setFadeInBeats(double beats) {
    if (qFuzzyCompare(m_fadeInBeats, beats)) return;
    m_fadeInBeats = beats;
    emit fadeInBeatsChanged();
    update();
}

void NovaWaveformItem::setFadeOutBeats(double beats) {
    if (qFuzzyCompare(m_fadeOutBeats, beats)) return;
    m_fadeOutBeats = beats;
    emit fadeOutBeatsChanged();
    update();
}

void NovaWaveformItem::setClipGainDb(float dB) {
    if (qFuzzyCompare(m_clipGainDb, dB)) return;
    m_clipGainDb = dB;
    emit clipGainDbChanged();
    update();
}

void NovaWaveformItem::setStartBeat(double beat) {
    if (!qFuzzyCompare(m_startBeat, beat)) {
        m_startBeat = beat;
        emit startBeatChanged();
        update();
    }
}

void NovaWaveformItem::setLengthBeats(double beats) {
    if (!qFuzzyCompare(m_lengthBeats, beats)) {
        m_lengthBeats = beats;
        emit lengthBeatsChanged();
        update();
    }
}

void NovaWaveformItem::setPixelsPerBeat(double px) {
    if (!qFuzzyCompare(m_pixelsPerBeat, px)) {
        m_pixelsPerBeat = px;
        emit pixelsPerBeatChanged();
        update();
    }
}

void NovaWaveformItem::setSession(void *session) {
    // Not used directly; the session is used by the model to fetch peaks.
    // The renderer gets peaks via setPeaks() called from outside.
    Q_UNUSED(session);
}