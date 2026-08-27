#pragma once

#include <QList>
#include <QPointF>
#include <QQuickPaintedItem>

#include "ink.h"

// A place to draw your signature. Collects raw strokes from the mouse, trackpad
// or a stylus and hands them to Ink, which does the smoothing and normalising.
class SignaturePad : public QQuickPaintedItem {
    Q_OBJECT
    Q_PROPERTY(bool empty READ isEmpty NOTIFY strokesChanged)
    Q_PROPERTY(QColor inkColour READ inkColour WRITE setInkColour NOTIFY inkColourChanged)

public:
    explicit SignaturePad(QQuickItem *parent = nullptr);

    bool isEmpty() const { return m_strokes.isEmpty(); }
    QColor inkColour() const { return m_inkColour; }
    void setInkColour(const QColor &colour);

    Q_INVOKABLE void clear();
    Q_INVOKABLE void undoStroke();

    // The finished, normalised signature. Empty if nothing has been drawn.
    Ink ink() const;

    void paint(QPainter *painter) override;

signals:
    void strokesChanged();
    void inkColourChanged();

protected:
    void mousePressEvent(QMouseEvent *event) override;
    void mouseMoveEvent(QMouseEvent *event) override;
    void mouseReleaseEvent(QMouseEvent *event) override;
    void mouseUngrabEvent() override;

private:
    QList<QList<QPointF>> m_strokes;
    QColor m_inkColour = QColor(0x13, 0x13, 0x13);
    bool m_drawing = false;
};
