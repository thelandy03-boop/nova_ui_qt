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
    void syncWithArdour();

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

    // Gestión real de Plug-ins e inserciones de efectos (Ardour Core)
    Q_INVOKABLE QVariantList trackPlugins(int trackRow) const;
    Q_INVOKABLE QVariantList availablePlugins() const;
    Q_INVOKABLE bool addPluginToTrack(int trackRow, const QString &pluginId);
    Q_INVOKABLE bool removePluginFromTrack(int trackRow, int pluginIndex);
    Q_INVOKABLE bool setPluginBypassed(int trackRow, int pluginIndex, bool bypassed);
    Q_INVOKABLE bool setTrackPluginsBypassed(int trackRow, bool bypassed);
    Q_INVOKABLE bool isTrackPluginsBypassed(int trackRow) const;
    Q_INVOKABLE bool setPluginParameter(int trackRow, int pluginIndex, int paramIndex, float value);
    Q_INVOKABLE bool importJsfxScript(const QString &filePath);
    Q_INVOKABLE bool isJsfxPlugin(int trackRow, int pluginIndex) const;
    Q_INVOKABLE QString getPluginScript(int trackRow, int pluginIndex) const;
    Q_INVOKABLE QString updatePluginScript(int trackRow, int pluginIndex, const QString &sourceCode);
    Q_INVOKABLE bool createCustomJsfxPlugin(int trackRow, const QString &name, const QString &sourceCode);
    Q_INVOKABLE QString compileJsfxScript(const QString &sourceCode);
    Q_INVOKABLE QVariantList parseJsfxSliders(const QString &sourceCode) const;
    Q_INVOKABLE bool hasJsfxGfx(int trackRow, int pluginIndex) const;
    std::shared_ptr<class NovaJsfxEffect> getJsfxEffect(int trackRow, int pluginIndex) const;
    Q_INVOKABLE void sendMidiNote(int trackRow, int pluginIndex, int note, int velocity, int channel = 0);
    Q_INVOKABLE void sendMidiControlChange(int trackRow, int pluginIndex, int controller, int value, int channel = 0);
    Q_INVOKABLE void sendMidiPitchBend(int trackRow, int pluginIndex, int value, int channel = 0);
    Q_INVOKABLE QString getJsfxDescription(const QString &sourceCode) const;

    /**
     * Instantanea completa de los plugins de una pista para guardarlos en un
     * .novapatch: id, nombre, categoria, script, bypass, sliders y estado de
     * @serialize.
     */
    Q_INVOKABLE QVariantList exportPluginsSnapshot(int trackRow) const;

    /**
     * Reemplaza los plugins de una pista con la instantanea dada. Se usa al
     * aplicar un parche sobre una pista recien creada.
     */
    Q_INVOKABLE bool restorePluginsSnapshot(int trackRow, const QVariantList &plugins);

    /** Devuelve la cadena DSP activa para una pista dada. */
    class NovaDspChain* dspChainForTrack(int trackRow) const;

signals:
    void trackCountChanged(int count);
    void trackAddFailed(const QString &reason);
    void trackPluginsChanged(int trackRow);

private:
    void connectControlSignals();

    std::unique_ptr<NovaTrackListModelPrivate> d_ptr;
};