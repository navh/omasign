#include "signaturepad.h"

#include <QMouseEvent>
#include <QPainter>

SignaturePad::SignaturePad(QQuickItem *parent) : QQuickPaintedItem(parent)
{
    setAcceptedMouseButtons(Qt::LeftButton);
    setAntialiasing(true);
    // Image render target, not an FBO: software rendering has no FBO and
    // would silently drop everything painted here.
    setRenderTarget(QQuickPaintedItem::Image);
}

void SignaturePad::setInkColour(const QColor &colour)
{
    if (m_inkColour == colour)
        return;
    m_inkColour = colour;
    emit inkColourChanged();
    update();
}

void SignaturePad::clear()
{
    if (m_strokes.isEmpty())
        return;
    m_strokes.clear();
    emit strokesChanged();
    update();
}

void SignaturePad::undoStroke()
{
    if (m_strokes.isEmpty())
        return;
    m_strokes.removeLast();
    emit strokesChanged();
    update();
}

Ink SignaturePad::ink() const
{
    if (m_strokes.isEmpty())
        return Ink();
    return Ink::fromRawStrokes(m_strokes, QSizeF(width(), height()));
}

void SignaturePad::mousePressEvent(QMouseEvent *event)
{
    // The pad is often inside a Flickable (the setup view scrolls once the
    // window is short). A Flickable steals the mouse from its children as soon
    // as the drag passes the threshold, which turns a signature into a scroll.
    // Holding the grab for the length of the stroke is how Qt says to opt out.
    setKeepMouseGrab(true);

    m_drawing = true;
    m_strokes.append(QList<QPointF>{event->position()});
    emit strokesChanged();
    update();
}

void SignaturePad::mouseMoveEvent(QMouseEvent *event)
{
    if (!m_drawing || m_strokes.isEmpty())
        return;
    m_strokes.last().append(event->position());
    update();
}

void SignaturePad::mouseReleaseEvent(QMouseEvent *event)
{
    Q_UNUSED(event)
    setKeepMouseGrab(false);
    m_drawing = false;
    emit strokesChanged();
    update();
}

// Something took the grab away mid-stroke -- a modal opening over the pad, say.
// End the stroke rather than leaving it forever unfinished, which would keep
// paint() showing the raw line instead of the smoothed one.
void SignaturePad::mouseUngrabEvent()
{
    QQuickPaintedItem::mouseUngrabEvent();
    if (!m_drawing)
        return;
    m_drawing = false;
    emit strokesChanged();
    update();
}

void SignaturePad::paint(QPainter *painter)
{
    if (m_strokes.isEmpty())
        return;

    painter->setRenderHint(QPainter::Antialiasing);

    // While the pen is still down, draw the raw strokes so the line keeps up
    // with the cursor. Once it lifts, show the smoothed, tapered result -- the
    // same treatment that will be applied on the page.
    if (m_drawing) {
        painter->setPen(QPen(m_inkColour, qMax(1.6, height() * 0.035),
                             Qt::SolidLine, Qt::RoundCap, Qt::RoundJoin));
        for (const QList<QPointF> &stroke : m_strokes) {
            if (stroke.size() < 2)
                continue;
            painter->drawPolyline(stroke.constData(), int(stroke.size()));
        }
        return;
    }

    const Ink smoothed = Ink::fromRawStrokes(m_strokes, QSizeF(width(), height()));
    if (smoothed.isEmpty())
        return;

    // Paint it where it was actually drawn rather than fitted to the pad, so the
    // line does not jump under the cursor the moment the pen lifts.
    qreal minX = 0.0, minY = 0.0, maxX = 0.0, maxY = 0.0;
    bool seen = false;
    for (const QList<QPointF> &stroke : m_strokes) {
        for (const QPointF &p : stroke) {
            if (!seen) {
                minX = maxX = p.x();
                minY = maxY = p.y();
                seen = true;
            } else {
                minX = qMin(minX, p.x());
                maxX = qMax(maxX, p.x());
                minY = qMin(minY, p.y());
                maxY = qMax(maxY, p.y());
            }
        }
    }
    if (!seen)
        return;
    smoothed.paint(painter, QRectF(minX, minY, maxX - minX, maxY - minY), m_inkColour);
}
