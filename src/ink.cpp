#include "ink.h"

#include <QFile>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QPainter>
#include <QSizeF>

#include <algorithm>
#include <cmath>

namespace {

// Chaikin corner cutting. Two passes turn the polyline a mouse produces into
// something that reads as a pen stroke rather than a series of line segments.
QList<QPointF> smooth(const QList<QPointF> &in, int passes)
{
    QList<QPointF> pts = in;
    for (int pass = 0; pass < passes && pts.size() > 2; ++pass) {
        QList<QPointF> out;
        out.reserve(pts.size() * 2);
        out.append(pts.first());
        for (int i = 0; i + 1 < pts.size(); ++i) {
            const QPointF &a = pts.at(i);
            const QPointF &b = pts.at(i + 1);
            out.append(a * 0.75 + b * 0.25);
            out.append(a * 0.25 + b * 0.75);
        }
        out.append(pts.last());
        pts = out;
    }
    return pts;
}

// Drop points that land almost on top of each other, so a slow hand does not
// produce hundreds of redundant samples.
QList<QPointF> decimate(const QList<QPointF> &in, qreal minStep)
{
    QList<QPointF> out;
    for (const QPointF &p : in) {
        if (out.isEmpty() || QLineF(out.last(), p).length() >= minStep)
            out.append(p);
    }
    if (out.size() == 1 && in.size() > 1)
        out.append(in.last());
    return out;
}

} // namespace

Ink Ink::fromRawStrokes(const QList<QList<QPointF>> &rawStrokes, const QSizeF &padSize)
{
    Ink ink;
    const qreal reference = qMax(1.0, qMin(padSize.width(), padSize.height()));

    // Pass one: clean up each stroke and find the overall bounding box.
    // Tracked as four scalars rather than a QRectF, because a rectangle grown
    // by uniting zero-sized rectangles stays null and never grows at all.
    QList<QList<QPointF>> cleaned;
    qreal minX = 0.0, minY = 0.0, maxX = 0.0, maxY = 0.0;
    bool seen = false;
    for (const QList<QPointF> &raw : rawStrokes) {
        QList<QPointF> pts = smooth(decimate(raw, reference * 0.004), 2);
        if (pts.size() < 2) {
            // A tap: keep it as a dot so dotted i's and periods survive.
            if (pts.size() == 1)
                pts.append(pts.first() + QPointF(0.001 * reference, 0.0));
            else
                continue;
        }
        cleaned.append(pts);
        for (const QPointF &p : pts) {
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
    if (cleaned.isEmpty() || !seen)
        return ink;
    const QRectF bounds(minX, minY, maxX - minX, maxY - minY);

    // Guard against a signature that is a single horizontal line: without a
    // floor on height the normalisation below would explode.
    const qreal boxH = qMax(bounds.height(), reference * 0.02);
    const qreal boxW = qMax(bounds.width(), reference * 0.02);
    ink.m_aspect = boxW / boxH;

    // Pass two: normalise, and derive a nib width that thins where the pen
    // moved fast. This is what separates ink from a uniform-width scribble.
    const qreal nominal = 0.055;  // fraction of box height
    const qreal fastStep = boxH * 0.06;
    for (const QList<QPointF> &pts : cleaned) {
        InkStroke stroke;
        stroke.points.reserve(pts.size());
        for (int i = 0; i < pts.size(); ++i) {
            const qreal step = (i == 0) ? 0.0 : QLineF(pts.at(i - 1), pts.at(i)).length();
            const qreal speed = qBound(0.0, step / fastStep, 1.0);
            qreal w = nominal * (1.0 - 0.45 * speed);

            // Taper the first and last few samples so strokes start and end
            // like a pen lifting rather than a butt-ended rectangle.
            const int fromEnd = qMin(i, pts.size() - 1 - i);
            const int taper = qMin(6, pts.size() / 4);
            if (taper > 0 && fromEnd < taper)
                w *= 0.35 + 0.65 * (qreal(fromEnd) / taper);

            InkPoint ip;
            ip.x = (pts.at(i).x() - bounds.left()) / boxW;
            ip.y = (pts.at(i).y() - bounds.top()) / boxH;
            ip.width = w;
            stroke.points.append(ip);
        }
        ink.m_strokes.append(stroke);
    }
    return ink;
}

bool Ink::loadFromFile(const QString &path)
{
    QFile file(path);
    if (!file.open(QIODevice::ReadOnly))
        return false;
    const QJsonObject root = QJsonDocument::fromJson(file.readAll()).object();
    if (root.isEmpty())
        return false;

    const qreal aspect = root.value(QStringLiteral("aspect")).toDouble();
    if (!(aspect > 0.0) || !std::isfinite(aspect))
        return false;

    QList<InkStroke> strokes;
    const QJsonArray strokeArray = root.value(QStringLiteral("strokes")).toArray();
    for (const QJsonValue &strokeValue : strokeArray) {
        InkStroke stroke;
        const QJsonArray points = strokeValue.toArray();
        for (const QJsonValue &pointValue : points) {
            const QJsonArray triple = pointValue.toArray();
            if (triple.size() < 3)
                continue;
            InkPoint p;
            p.x = triple.at(0).toDouble();
            p.y = triple.at(1).toDouble();
            p.width = triple.at(2).toDouble();
            stroke.points.append(p);
        }
        if (stroke.points.size() >= 2)
            strokes.append(stroke);
    }
    if (strokes.isEmpty())
        return false;

    m_aspect = aspect;
    m_strokes = strokes;
    return true;
}

bool Ink::saveToFile(const QString &path) const
{
    QJsonArray strokeArray;
    for (const InkStroke &stroke : m_strokes) {
        QJsonArray points;
        for (const InkPoint &p : stroke.points) {
            QJsonArray triple;
            // Three decimals is roughly a tenth of a millimetre at signature
            // sizes, and keeps the stored file small and diffable.
            triple.append(std::round(p.x * 1000.0) / 1000.0);
            triple.append(std::round(p.y * 1000.0) / 1000.0);
            triple.append(std::round(p.width * 1000.0) / 1000.0);
            points.append(triple);
        }
        strokeArray.append(points);
    }
    QJsonObject root;
    root.insert(QStringLiteral("aspect"), std::round(m_aspect * 1000.0) / 1000.0);
    root.insert(QStringLiteral("strokes"), strokeArray);

    QFile file(path);
    if (!file.open(QIODevice::WriteOnly | QIODevice::Truncate))
        return false;
    return file.write(QJsonDocument(root).toJson(QJsonDocument::Compact)) >= 0;
}

QRectF Ink::paint(QPainter *painter, const QRectF &box, const QColor &colour) const
{
    if (m_strokes.isEmpty() || box.isEmpty())
        return QRectF();

    // Fit the ink's aspect inside the box without distorting it.
    qreal h = box.height();
    qreal w = h * m_aspect;
    if (w > box.width()) {
        w = box.width();
        h = w / m_aspect;
    }
    const QRectF target(box.left() + (box.width() - w) / 2.0,
                        box.top() + (box.height() - h) / 2.0, w, h);

    painter->save();
    painter->setBrush(colour);
    painter->setPen(Qt::NoPen);
    for (const InkStroke &stroke : m_strokes) {
        // Each segment is drawn as its own round-capped line so the width can
        // vary along the stroke; round caps make the joins invisible.
        for (int i = 0; i + 1 < stroke.points.size(); ++i) {
            const InkPoint &a = stroke.points.at(i);
            const InkPoint &b = stroke.points.at(i + 1);
            const QPointF pa(target.left() + a.x * target.width(), target.top() + a.y * target.height());
            const QPointF pb(target.left() + b.x * target.width(), target.top() + b.y * target.height());
            const qreal wa = qMax(a.width * target.height(), 0.4);
            const qreal wb = qMax(b.width * target.height(), 0.4);
            painter->setPen(QPen(colour, (wa + wb) / 2.0, Qt::SolidLine, Qt::RoundCap, Qt::RoundJoin));
            painter->drawLine(pa, pb);
        }
    }
    painter->restore();
    return target;
}
