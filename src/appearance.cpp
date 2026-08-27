#include "appearance.h"

#include <QDateTime>
#include <QFile>
#include <QFont>
#include <QFontMetricsF>
#include <QPainter>
#include <QRectF>
#include <QSvgRenderer>

#include <cmath>

namespace {

QFont makeFont(const QString &family, qreal pixelSize, QFont::Weight weight)
{
    QFont font(family);
    font.setStyleHint(QFont::SansSerif);
    font.setWeight(weight);
    font.setPixelSize(qMax(1, int(std::floor(pixelSize))));
    // Hinting shifts glyph positions to the pixel grid, which is right for a
    // screen and wrong for artwork that will be scaled into a PDF.
    font.setHintingPreference(QFont::PreferNoHinting);
    return font;
}

// Largest pixel size at which `text`, word-wrapped, still fits inside `box`.
// Shrinking to fit is the whole point: it is what stops a long name from being
// clipped mid-glyph the way Okular clips it.
qreal fitPixelSize(const QString &text, const QRectF &box, const QString &family,
                   QFont::Weight weight, qreal ceilingPx)
{
    if (text.isEmpty() || box.isEmpty())
        return 1.0;

    // Leave a sliver of slack so antialiased edges never touch the boundary.
    const qreal maxW = box.width() * 0.995;
    const qreal maxH = box.height() * 0.995;

    qreal lo = 1.0;
    qreal hi = qMax(2.0, qMin(ceilingPx, box.height()));
    const auto fits = [&](qreal px) {
        const QFontMetricsF fm(makeFont(family, px, weight));
        const QRectF need = fm.boundingRect(QRectF(0, 0, maxW, 1e6),
                                            Qt::AlignLeft | Qt::TextWordWrap, text);
        return need.width() <= maxW && need.height() <= maxH;
    };
    if (fits(hi))
        return hi;
    for (int i = 0; i < 24; ++i) { // binary search converges well inside 24 steps
        const qreal mid = (lo + hi) / 2.0;
        if (fits(mid))
            lo = mid;
        else
            hi = mid;
    }
    return lo;
}

void paintWatermark(QPainter *painter, const QRectF &box, const AppearanceSpec &spec)
{
    if (spec.watermarkOpacity <= 0.0 || spec.watermarkPath == QLatin1String("none"))
        return;

    const QString path = spec.watermarkPath.isEmpty()
        ? QStringLiteral(":/marks/omasign-mark.svg")
        : spec.watermarkPath;
    if (!QFile::exists(path))
        return;

    QSvgRenderer renderer(path);
    if (!renderer.isValid())
        return;

    // Square-ish marks look best sized off the height and centred; a wide logo
    // (a university crest, say) is allowed to use the width instead.
    QSizeF natural = renderer.defaultSize();
    if (natural.isEmpty())
        natural = QSizeF(1, 1);
    qreal h = box.height() * 0.92;
    qreal w = h * (natural.width() / natural.height());
    if (w > box.width() * 0.9) {
        w = box.width() * 0.9;
        h = w * (natural.height() / natural.width());
    }
    const QRectF target(box.center().x() - w / 2.0, box.center().y() - h / 2.0, w, h);

    painter->save();
    painter->setOpacity(spec.watermarkOpacity);
    renderer.render(painter, target);
    painter->restore();
}

} // namespace

QStringList buildMetaLines(const QString &name, const QString &reason, const QString &location)
{
    QStringList lines;
    lines << QStringLiteral("Digitally signed by %1").arg(name);

    // Adobe writes the offset as +HH'MM'. Qt has no format specifier for that,
    // so build it from the offset in seconds.
    const QDateTime now = QDateTime::currentDateTime();
    const int offsetSeconds = now.offsetFromUtc();
    const QChar sign = offsetSeconds < 0 ? QLatin1Char('-') : QLatin1Char('+');
    const int absSeconds = std::abs(offsetSeconds);
    const QString offset = QStringLiteral("%1%2'%3'")
        .arg(sign)
        .arg(absSeconds / 3600, 2, 10, QLatin1Char('0'))
        .arg((absSeconds % 3600) / 60, 2, 10, QLatin1Char('0'));

    lines << QStringLiteral("Date: %1 %2")
                 .arg(now.toString(QStringLiteral("yyyy.MM.dd HH:mm:ss")), offset);

    if (!reason.isEmpty())
        lines << QStringLiteral("Reason: %1").arg(reason);
    if (!location.isEmpty())
        lines << QStringLiteral("Location: %1").arg(location);
    return lines;
}

QImage renderAppearance(const AppearanceSpec &spec, const QSizeF &rectPt, qreal scale)
{
    const int w = qMax(1, int(std::lround(rectPt.width() * scale)));
    const int h = qMax(1, int(std::lround(rectPt.height() * scale)));

    QImage image(w, h, QImage::Format_ARGB32);
    image.fill(Qt::transparent);

    QPainter painter(&image);
    painter.setRenderHint(QPainter::Antialiasing);
    painter.setRenderHint(QPainter::TextAntialiasing);
    painter.setRenderHint(QPainter::SmoothPixmapTransform);

    // A small inset keeps the mark from butting against whatever rule or box on
    // the form the user dragged the rectangle over.
    const qreal inset = qMax(1.0, qMin(w, h) * 0.04);
    const QRectF content(inset, inset, w - 2 * inset, h - 2 * inset);
    if (content.isEmpty())
        return image;

    const QStringList metaLines = spec.showMeta ? spec.metaLines : QStringList();
    const bool split = !metaLines.isEmpty();

    QRectF markBox = content;
    QRectF metaBox;
    if (split) {
        // Enough air that the mark and the fine print read as two columns
        // rather than one run-on block.
        const qreal gutter = content.width() * 0.05;
        const qreal markWidth = content.width() * qBound(0.25, spec.splitRatio, 0.85) - gutter / 2.0;
        markBox = QRectF(content.left(), content.top(), markWidth, content.height());
        metaBox = QRectF(markBox.right() + gutter, content.top(),
                         content.right() - (markBox.right() + gutter), content.height());
    }

    paintWatermark(&painter, content, spec);

    // --- the mark itself: drawn ink, or the name set in clean type
    if (spec.useInk && !spec.ink.isEmpty()) {
        spec.ink.paint(&painter, markBox, spec.markColour);
    } else if (!spec.name.isEmpty()) {
        const qreal px = fitPixelSize(spec.name, markBox, spec.fontFamily,
                                      QFont::Normal, markBox.height());
        painter.setFont(makeFont(spec.fontFamily, px, QFont::Normal));
        painter.setPen(spec.markColour);
        painter.drawText(markBox, Qt::AlignCenter | Qt::TextWordWrap, spec.name);
    }

    // --- the audit block
    if (split) {
        const QString meta = metaLines.join(QLatin1Char('\n'));
        // Cap the size so a tall rectangle does not blow the fine print up into
        // something that competes with the signature for attention.
        const qreal ceiling = qMin(metaBox.height() * 0.30, metaBox.height() / metaLines.size());
        const qreal px = fitPixelSize(meta, metaBox, spec.fontFamily, QFont::Normal, ceiling);
        painter.setFont(makeFont(spec.fontFamily, px, QFont::Normal));
        painter.setPen(spec.metaColour);
        painter.drawText(metaBox, Qt::AlignVCenter | Qt::AlignLeft | Qt::TextWordWrap, meta);
    }

    painter.end();
    return image;
}
