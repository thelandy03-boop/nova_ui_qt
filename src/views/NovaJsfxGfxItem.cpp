#include "views/NovaJsfxGfxItem.h"
#include "audio/dsp/jsfx/NovaJsfxEffect.h"
#include "NovaAudioEngine.h"
#include "models/NovaTrackListModel.h"

#include <QPainter>
#include <QQuickWindow>
#include <algorithm>

NovaJsfxGfxItem::NovaJsfxGfxItem(QQuickItem *parent)
    : QQuickPaintedItem(parent)
{
    setAcceptedMouseButtons(Qt::AllButtons);
    setAcceptHoverEvents(true);
    setFlag(ItemHasContents, true);

    connect(&m_renderTimer, &QTimer::timeout, this, &NovaJsfxGfxItem::onRenderTimer);
    m_renderTimer.setInterval(1000 / m_fps);
    m_renderTimer.start();
}

NovaJsfxGfxItem::~NovaJsfxGfxItem()
{
    m_renderTimer.stop();
}

void NovaJsfxGfxItem::setTrackIndex(int index)
{
    if (m_trackIndex == index) return;
    m_trackIndex = index;
    Q_EMIT trackIndexChanged();
    updateEffectBinding();
}

void NovaJsfxGfxItem::setPluginIndex(int index)
{
    if (m_pluginIndex == index) return;
    m_pluginIndex = index;
    Q_EMIT pluginIndexChanged();
    updateEffectBinding();
}

void NovaJsfxGfxItem::setFps(int fps)
{
    int nuevoFps = std::clamp(fps, 10, 120);
    if (m_fps == nuevoFps) return;
    m_fps = nuevoFps;
    m_renderTimer.setInterval(1000 / m_fps);
    Q_EMIT fpsChanged();
}

void NovaJsfxGfxItem::updateEffectBinding()
{
    auto engine = NovaAudioEngine::instance();
    if (!engine || !engine->tracks() || m_trackIndex < 0 || m_pluginIndex < 0) {
        m_effect = nullptr;
    } else {
        m_effect = engine->tracks()->getJsfxEffect(m_trackIndex, m_pluginIndex);
    }

    bool previousHasGfx = m_hasGfx;
    m_hasGfx = (m_effect && m_effect->hasGfx());
    if (m_hasGfx != previousHasGfx) {
        Q_EMIT hasGfxChanged();
    }

    if (m_hasGfx) {
        int w = 0, h = 0;
        if (m_effect->getGfxDimensions(w, h)) {
            if (w != m_preferredWidth || h != m_preferredHeight) {
                m_preferredWidth = w;
                m_preferredHeight = h;
                Q_EMIT preferredDimensionsChanged();
            }
        }
        int reqFps = m_effect->requestedGfxFps();
        if (reqFps > 0 && reqFps != m_fps) {
            setFps(reqFps);
        }
    }

    int curW = static_cast<int>(width());
    int curH = static_cast<int>(height());
    if (curW > 0 && curH > 0) {
        resizeBuffer(curW, curH);
    }
}

void NovaJsfxGfxItem::resizeBuffer(int width, int height)
{
    if (width <= 0 || height <= 0) return;

    if (m_image.size() != QSize(width, height)) {
        m_image = QImage(width, height, QImage::Format_ARGB32_Premultiplied);
        m_image.fill(Qt::black);
    }

    if (m_effect && m_hasGfx) {
        double dpr = window() ? window()->devicePixelRatio() : 1.0;
        m_effect->setupGfx(m_image.bits(), width, height,
                           static_cast<int>(m_image.bytesPerLine()), dpr);
        if (m_effect->renderGfx()) {
            update();
        }
    }
}

void NovaJsfxGfxItem::geometryChange(const QRectF &newGeometry, const QRectF &oldGeometry)
{
    QQuickPaintedItem::geometryChange(newGeometry, oldGeometry);
    int nw = static_cast<int>(newGeometry.width());
    int nh = static_cast<int>(newGeometry.height());
    if (nw > 0 && nh > 0 && (nw != static_cast<int>(oldGeometry.width()) || nh != static_cast<int>(oldGeometry.height()))) {
        resizeBuffer(nw, nh);
    }
}

void NovaJsfxGfxItem::itemChange(ItemChange change, const ItemChangeData &value)
{
    QQuickPaintedItem::itemChange(change, value);
    if (change == ItemVisibleHasChanged) {
        if (m_effect) {
            m_effect->setGfxWindowState(hasActiveFocus(), isVisible(), false);
        }
    }
}

void NovaJsfxGfxItem::onRenderTimer()
{
    if (!isVisible() || !m_effect || !m_hasGfx || m_image.isNull()) return;

    if (m_effect->renderGfx()) {
        update();
    }
}

void NovaJsfxGfxItem::paint(QPainter *painter)
{
    if (!m_image.isNull()) {
        painter->drawImage(0, 0, m_image);
    }
}

uint32_t NovaJsfxGfxItem::extractMouseMods(Qt::KeyboardModifiers qmods) const
{
    uint32_t mods = 0;
    if (qmods & Qt::ShiftModifier)   mods |= (1 << 0);
    if (qmods & Qt::ControlModifier) mods |= (1 << 1);
    if (qmods & Qt::AltModifier)     mods |= (1 << 2);
    if (qmods & Qt::MetaModifier)    mods |= (1 << 3);
    return mods;
}

uint32_t NovaJsfxGfxItem::extractMouseButtons(Qt::MouseButtons qbtns) const
{
    uint32_t buttons = 0;
    if (qbtns & Qt::LeftButton)   buttons |= 1;
    if (qbtns & Qt::RightButton)  buttons |= 2;
    if (qbtns & Qt::MiddleButton) buttons |= 64;
    return buttons;
}

void NovaJsfxGfxItem::mousePressEvent(QMouseEvent *event)
{
    if (!m_effect || !m_hasGfx) {
        QQuickPaintedItem::mousePressEvent(event);
        return;
    }
    event->accept();
    uint32_t mods = extractMouseMods(event->modifiers());
    uint32_t btns = extractMouseButtons(event->buttons());
    m_effect->updateGfxMouse(mods, static_cast<int>(event->position().x()),
                             static_cast<int>(event->position().y()), btns);
    if (m_effect->renderGfx()) {
        update();
    }
}

void NovaJsfxGfxItem::mouseMoveEvent(QMouseEvent *event)
{
    if (!m_effect || !m_hasGfx) {
        QQuickPaintedItem::mouseMoveEvent(event);
        return;
    }
    event->accept();
    uint32_t mods = extractMouseMods(event->modifiers());
    uint32_t btns = extractMouseButtons(event->buttons());
    m_effect->updateGfxMouse(mods, static_cast<int>(event->position().x()),
                             static_cast<int>(event->position().y()), btns);
    if (m_effect->renderGfx()) {
        update();
    }
}

void NovaJsfxGfxItem::mouseReleaseEvent(QMouseEvent *event)
{
    if (!m_effect || !m_hasGfx) {
        QQuickPaintedItem::mouseReleaseEvent(event);
        return;
    }
    event->accept();
    uint32_t mods = extractMouseMods(event->modifiers());
    uint32_t btns = extractMouseButtons(event->buttons());
    m_effect->updateGfxMouse(mods, static_cast<int>(event->position().x()),
                             static_cast<int>(event->position().y()), btns);
    if (m_effect->renderGfx()) {
        update();
    }
}

void NovaJsfxGfxItem::wheelEvent(QWheelEvent *event)
{
    if (!m_effect || !m_hasGfx) {
        QQuickPaintedItem::wheelEvent(event);
        return;
    }
    event->accept();
    uint32_t mods = extractMouseMods(event->modifiers());
    uint32_t btns = extractMouseButtons(event->buttons());
    double wheelV = event->angleDelta().y() / 120.0;
    double wheelH = event->angleDelta().x() / 120.0;
    m_effect->updateGfxMouse(mods, static_cast<int>(event->position().x()),
                             static_cast<int>(event->position().y()), btns, wheelV, wheelH);
    if (m_effect->renderGfx()) {
        update();
    }
}

void NovaJsfxGfxItem::hoverEnterEvent(QHoverEvent *event)
{
    Q_UNUSED(event);
    if (m_effect) {
        m_effect->setGfxWindowState(hasActiveFocus(), isVisible(), true);
    }
}

void NovaJsfxGfxItem::hoverMoveEvent(QHoverEvent *event)
{
    if (!m_effect || !m_hasGfx) return;
    uint32_t mods = extractMouseMods(event->modifiers());
    m_effect->updateGfxMouse(mods, static_cast<int>(event->position().x()),
                             static_cast<int>(event->position().y()), 0);
}

void NovaJsfxGfxItem::hoverLeaveEvent(QHoverEvent *event)
{
    Q_UNUSED(event);
    if (m_effect) {
        m_effect->setGfxWindowState(hasActiveFocus(), isVisible(), false);
    }
}
