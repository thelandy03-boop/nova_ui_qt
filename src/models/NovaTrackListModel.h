#pragma once

#include <QAbstractListModel>
#include <QHash>
#include <QByteArray>
#include <QString>
#include <QVariant>
#include <QtGlobal>
#include <vector>
#include <memory>

// Forward declarations - NO ARDOUR/PBD includes in public header
namespace ARDOUR {
    class Session;
}
class NovaTrackListModelPrivate;

class NovaTrackListModel : public QAbstractListModel
{
    Q_OBJECT

public:
    enum TrackRoles : int {
        TrackIdRole      = Qt::UserRole + 1,
        TrackNameRole,
        TrackTypeRole,
        GainRole,
        MuteRole,
        SoloRole,
        RecEnableRole,
        PanRole,
        ColorRole,
        PeakRole
    };
    Q_ENUM(TrackRoles)

    explicit NovaTrackListModel(QObject *parent = nullptr);
    ~NovaTrackListModel() override;

    void setSession(void *session);

    // QAbstractListModel overrides
    int rowCount(const QModelIndex &parent = QModelIndex()) const override;
    QVariant data(const QModelIndex &index, int role = Qt::DisplayRole) const override;
    QHash<int, QByteArray> roleNames() const override;

    // Métodos dinámicos invocables desde QML
    Q_INVOKABLE void addAudioTrack(const QString &name = QStringLiteral("Audio Track"));
    Q_INVOKABLE void addMidiTrack(const QString &name = QStringLiteral("MIDI Track"));
    Q_INVOKABLE void removeTrack(int row);
    Q_INVOKABLE void setGain(int row, float dB);
    Q_INVOKABLE void setMute(int row, bool muted);
    Q_INVOKABLE void setSolo(int row, bool soloed);
    Q_INVOKABLE void setRecEnable(int row, bool enabled);
    Q_INVOKABLE void setPan(int row, float pan);
    Q_INVOKABLE float peakLevel(int row) const;

signals:
    void trackCountChanged(int count);
    void trackAddFailed(const QString &reason);

private:
    void syncWithArdour();

    std::unique_ptr<NovaTrackListModelPrivate> d_ptr;
};