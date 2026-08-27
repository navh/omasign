#pragma once

#include <QColor>
#include <QImage>
#include <QSizeF>
#include <QString>
#include <QStringList>

#include "ink.h"

// How a signature should look on the page.
//
// The layout deliberately mirrors what Adobe Acrobat and Foxit produce, because
// that is what the people receiving these documents expect to see: the mark on
// the left at whatever size fits, the audit details on the right in small type,
// and a faint watermark behind both. What it does not do is draw a border or a
// filled background, which is what makes Okular's version look like a defect.
struct AppearanceSpec {
    QString name;
    QStringList metaLines;

    bool useInk = false;
    Ink ink;

    bool showMeta = true;
    qreal splitRatio = 0.52; // share of the width given to the mark

    QColor markColour = QColor(0x13, 0x13, 0x13);
    QColor metaColour = QColor(0x3a, 0x3a, 0x3a);

    // Empty means the bundled Omasign mark; "none" disables the watermark.
    QString watermarkPath;
    qreal watermarkOpacity = 0.25;

    QString fontFamily = QStringLiteral("Inter");
};

// Render `spec` into a transparent RGBA image whose pixel dimensions are
// `rectPt` (in PDF points) multiplied by `scale`. Rendering at the rectangle's
// true aspect ratio matters: poppler stretches the image to fill the annotation,
// so anything else would distort the result.
QImage renderAppearance(const AppearanceSpec &spec, const QSizeF &rectPt, qreal scale);

// The audit block that sits to the right of the mark, in Adobe's wording and
// Adobe's date format (`2026.08.26 16:24:57 +02'00'`).
QStringList buildMetaLines(const QString &name, const QString &reason, const QString &location);
