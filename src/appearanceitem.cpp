#include "appearanceitem.h"

#include <QPainter>

AppearanceItem::AppearanceItem(QQuickItem *parent) : QQuickPaintedItem(parent)
{
    setAntialiasing(true);
    // The artwork is rendered at device resolution and downscaled by the scene
    // The default Image render target already paints at the device pixel
    // ratio, and unlike an FBO it works under software rendering too.
    setRenderTarget(QQuickPaintedItem::Image);
}

void AppearanceItem::setSigner(Backend *signer)
{
    if (m_signer == signer)
        return;
    if (m_signer)
        disconnect(m_signer, nullptr, this, nullptr);
    m_signer = signer;
    if (m_signer)
        connect(m_signer, &Backend::appearanceChanged, this, [this] { update(); });
    emit signerChanged();
    update();
}

void AppearanceItem::setPreferInk(bool preferInk)
{
    if (m_preferInk == preferInk)
        return;
    m_preferInk = preferInk;
    emit preferInkChanged();
    update();
}

void AppearanceItem::setOverrideMode(bool overrideMode)
{
    if (m_overrideMode == overrideMode)
        return;
    m_overrideMode = overrideMode;
    emit overrideModeChanged();
    update();
}

void AppearanceItem::setNameOverride(const QString &name)
{
    if (m_nameOverride == name)
        return;
    m_nameOverride = name;
    emit nameOverrideChanged();
    update();
}

void AppearanceItem::paint(QPainter *painter)
{
    if (!m_signer || width() <= 0 || height() <= 0)
        return;

    AppearanceSpec spec = m_signer->currentAppearanceSpec();
    if (m_overrideMode)
        spec.useInk = m_preferInk;
    if (!m_nameOverride.isEmpty()) {
        // Rebuild from the raw reason and location, not from the already
        // formatted lines, or they would come back doubly prefixed.
        spec.name = m_nameOverride;
        spec.metaLines = buildMetaLines(m_nameOverride, m_signer->reason(), m_signer->location());
    }

    // Render into the item's own pixel box: the item is already sized to the
    // rectangle's aspect ratio by its caller.
    const QImage image = renderAppearance(spec, QSizeF(width(), height()), 1.0);
    painter->drawImage(QRectF(0, 0, width(), height()), image);
}
