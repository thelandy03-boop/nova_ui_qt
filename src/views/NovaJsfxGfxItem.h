#pragma once

#include <QQuickPaintedItem>
#include <QImage>
#include <QTimer>
#include <memory>

class NovaJsfxEffect;

/**
 * Componente QML que renderiza la interfaz grafica interactiva de un script
 * JSFX (@gfx) ejecutado por YSFX / LICE.
 *
 * Mapea automaticamente la salida grafica a un QImage y redirige los eventos
 * tactiles, raton y rueda del usuario hacia el script.
 */
class NovaJsfxGfxItem : public QQuickPaintedItem
{
    Q_OBJECT
    Q_PROPERTY(int trackIndex READ trackIndex WRITE setTrackIndex NOTIFY trackIndexChanged)
    Q_PROPERTY(int pluginIndex READ pluginIndex WRITE setPluginIndex NOTIFY pluginIndexChanged)
    Q_PROPERTY(bool hasGfx READ hasGfx NOTIFY hasGfxChanged)
    Q_PROPERTY(int preferredWidth READ preferredWidth NOTIFY preferredDimensionsChanged)
    Q_PROPERTY(int preferredHeight READ preferredHeight NOTIFY preferredDimensionsChanged)
    Q_PROPERTY(int fps READ fps WRITE setFps NOTIFY fpsChanged)

public:
    explicit NovaJsfxGfxItem(QQuickItem *parent = nullptr);
    ~NovaJsfxGfxItem() override;

    int trackIndex() const { return m_trackIndex; }
    void setTrackIndex(int index);

    int pluginIndex() const { return m_pluginIndex; }
    void setPluginIndex(int index);

    bool hasGfx() const { return m_hasGfx; }
    int preferredWidth() const { return m_preferredWidth; }
    int preferredHeight() const { return m_preferredHeight; }

    int fps() const { return m_fps; }
    void setFps(int fps);

    void paint(QPainter *painter) override;

signals:
    void trackIndexChanged();
    void pluginIndexChanged();
    void hasGfxChanged();
    void preferredDimensionsChanged();
    void fpsChanged();

protected:
    void geometryChange(const QRectF &newGeometry, const QRectF &oldGeometry) override;
    void mousePressEvent(QMouseEvent *event) override;
    void mouseMoveEvent(QMouseEvent *event) override;
    void mouseReleaseEvent(QMouseEvent *event) override;
    void wheelEvent(QWheelEvent *event) override;
    void hoverEnterEvent(QHoverEvent *event) override;
    void hoverMoveEvent(QHoverEvent *event) override;
    void hoverLeaveEvent(QHoverEvent *event) override;
    void itemChange(ItemChange change, const ItemChangeData &value) override;

private slots:
    void onRenderTimer();

private:
    void updateEffectBinding();
    void resizeBuffer(int width, int height);
    uint32_t extractMouseMods(Qt::KeyboardModifiers qmods) const;
    uint32_t extractMouseButtons(Qt::MouseButtons qbtns) const;

    int m_trackIndex = -1;
    int m_pluginIndex = -1;
    bool m_hasGfx = false;
    int m_preferredWidth = 0;
    int m_preferredHeight = 0;
    int m_fps = 30;

    std::shared_ptr<NovaJsfxEffect> m_effect;
    QImage m_image;
    QTimer m_renderTimer;
};
