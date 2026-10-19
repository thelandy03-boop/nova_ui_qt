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
    return new NovaWaveformRenderer();
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

void NovaWaveformItem::setSession(void *session) {
    // Not used directly; the session is used by the model to fetch peaks.
    // The renderer gets peaks via setPeaks() called from outside.
    Q_UNUSED(session);
}