#pragma once

#include <QList>
#include <QPainterPath>
#include <QPointF>
#include <QString>

// A drawn signature, stored resolution-independently.
//
// Points are normalised into the ink's own bounding box: x and y run 0..1 over
// the box, and `aspect` records how wide that box is relative to its height.
// That lets the same stroke data be re-rendered crisply at any size, into any
// target rectangle, without ever baking in the size of the pad it was drawn on.
struct InkPoint {
    qreal x = 0.0;
    qreal y = 0.0;
    qreal width = 0.0; // stroke width at this point, as a fraction of box height
};

struct InkStroke {
    QList<InkPoint> points;
};

class Ink {
public:
    bool isEmpty() const { return m_strokes.isEmpty(); }
    qreal aspect() const { return m_aspect; }
    const QList<InkStroke> &strokes() const { return m_strokes; }

    // Build from raw pad input. `strokes` are in pad pixel coordinates; the pad
    // size is only used to derive a sensible nib width. Normalisation, velocity
    // tapering and smoothing all happen here, once, at capture time.
    static Ink fromRawStrokes(const QList<QList<QPointF>> &strokes, const QSizeF &padSize);

    bool loadFromFile(const QString &path);
    bool saveToFile(const QString &path) const;

    // Paint the ink into `box`, scaled to fit while preserving aspect and
    // centred. Returns the rectangle actually covered.
    QRectF paint(QPainter *painter, const QRectF &box, const QColor &colour) const;

private:
    qreal m_aspect = 1.0;
    QList<InkStroke> m_strokes;
};
