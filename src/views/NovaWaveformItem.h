#ifndef NOVA_WAVEFORM_ITEM_H
#define NOVA_WAVEFORM_ITEM_H

#include <QQuickFramebufferObject>
#include <QColor>
#include <QString>
#include <memory>

// Forward declaration - NO ARDOUR includes in public header
namespace ARDOUR {
    class Session;
}

class NovaWaveformItem : public QQuickFramebufferObject {
    Q_OBJECT
    Q_PROPERTY(int regionIndex READ regionIndex WRITE setRegionIndex NOTIFY regionIndexChanged)
    Q_PROPERTY(QString regionId READ regionId WRITE setRegionId NOTIFY regionIdChanged)
    Q_PROPERTY(QColor peakColor READ peakColor WRITE setPeakColor NOTIFY peakColorChanged)
    Q_PROPERTY(QColor rmsColor READ rmsColor WRITE setRmsColor NOTIFY rmsColorChanged)
    Q_PROPERTY(bool selected READ isSelected WRITE setSelected NOTIFY selectedChanged)
    Q_PROPERTY(double fadeInBeats READ fadeInBeats WRITE setFadeInBeats NOTIFY fadeInBeatsChanged)
    Q_PROPERTY(double fadeOutBeats READ fadeOutBeats WRITE setFadeOutBeats NOTIFY fadeOutBeatsChanged)
    Q_PROPERTY(float clipGainDb READ clipGainDb WRITE setClipGainDb NOTIFY clipGainDbChanged)

public:
    explicit NovaWaveformItem(QQuickItem *parent = nullptr);
    ~NovaWaveformItem() override;

    QQuickFramebufferObject::Renderer *createRenderer() const override;

    int regionIndex() const { return m_regionIndex; }
    void setRegionIndex(int index);

    QString regionId() const { return m_regionId; }
    void setRegionId(const QString &id);

    QColor peakColor() const { return m_peakColor; }
    void setPeakColor(const QColor &color);

    QColor rmsColor() const { return m_rmsColor; }
    void setRmsColor(const QColor &color);

    bool isSelected() const { return m_selected; }
    void setSelected(bool selected);

    double fadeInBeats() const { return m_fadeInBeats; }
    void setFadeInBeats(double beats);

    double fadeOutBeats() const { return m_fadeOutBeats; }
    void setFadeOutBeats(double beats);

    float clipGainDb() const { return m_clipGainDb; }
    void setClipGainDb(float dB);

    static void setSession(void *session);

signals:
    void regionIndexChanged();
    void regionIdChanged();
    void peakColorChanged();
    void rmsColorChanged();
    void selectedChanged();
    void fadeInBeatsChanged();
    void fadeOutBeatsChanged();
    void clipGainDbChanged();

private:
    int m_regionIndex = -1;
    QString m_regionId;
    QColor m_peakColor = Qt::white;
    QColor m_rmsColor = QColor(255, 255, 255, 128);
    bool m_selected = false;
    double m_fadeInBeats = 0.0;
    double m_fadeOutBeats = 0.0;
    float m_clipGainDb = 0.0f;
    static inline void *s_session = nullptr;
};

#endif // NOVA_WAVEFORM_ITEM_H