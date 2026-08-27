#pragma once

#include <QQuickPaintedItem>

// Included rather than forward-declared: moc needs the full type to register
// the pointer property.
#include "backend.h"

// Draws exactly what the signature will look like on the page, using the same
// renderer that produces the artwork embedded in the PDF. Nothing about the
// preview is an approximation: what you see while dragging the rectangle is the
// bitmap that ends up in the file.
class AppearanceItem : public QQuickPaintedItem {
    Q_OBJECT
    // Named "signer", not "backend": a QML property called `backend` would
    // shadow the context object of the same name, so `backend: backend` would
    // silently bind the item to itself.
    Q_PROPERTY(Backend *signer READ signer WRITE setSigner NOTIFY signerChanged)
    Q_PROPERTY(bool preferInk READ preferInk WRITE setPreferInk NOTIFY preferInkChanged)
    Q_PROPERTY(bool overrideMode READ overrideMode WRITE setOverrideMode NOTIFY overrideModeChanged)
    Q_PROPERTY(QString nameOverride READ nameOverride WRITE setNameOverride NOTIFY nameOverrideChanged)

public:
    explicit AppearanceItem(QQuickItem *parent = nullptr);

    Backend *signer() const { return m_signer; }
    void setSigner(Backend *signer);

    // The setup screen previews "typed" and "drawn" side by side, before either
    // has been committed to the identity, so it needs to override the mode.
    bool preferInk() const { return m_preferInk; }
    void setPreferInk(bool preferInk);
    bool overrideMode() const { return m_overrideMode; }
    void setOverrideMode(bool overrideMode);

    // Lets the setup screen preview the name being typed, before it has been
    // committed to the stored identity.
    QString nameOverride() const { return m_nameOverride; }
    void setNameOverride(const QString &name);

    void paint(QPainter *painter) override;

signals:
    void signerChanged();
    void preferInkChanged();
    void overrideModeChanged();
    void nameOverrideChanged();

private:
    Backend *m_signer = nullptr;
    bool m_preferInk = false;
    bool m_overrideMode = false;
    QString m_nameOverride;
};
