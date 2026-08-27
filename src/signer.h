#pragma once

#include <QRectF>
#include <QSizeF>
#include <QString>

#include "appearance.h"

namespace Poppler {
class Document;
}

struct SignRequest {
    QString inputPath;
    QString outputPath;

    int pageIndex = 0;
    // The signature rectangle in normalised page coordinates: 0..1 across and
    // down, origin at the page's top-left. This is what poppler's Qt annotation
    // API expects, and it is also exactly what a drag over a rendered page
    // gives us, so no conversion happens in between.
    QRectF rect;
    QSizeF pageSizePt;

    QString certNickname;
    QString certPassword;
    QString fieldName;
    QString reason;
    QString location;

    QByteArray documentOwnerPassword;
    QByteArray documentUserPassword;

    AppearanceSpec appearance;
};

struct SignResult {
    bool ok = false;
    QString error;
    QString outputPath;
};

// Sign `request.inputPath` into `request.outputPath`, leaving any signatures
// that are already on the document intact (poppler writes an incremental
// update, which is what lets several people sign the same form in turn).
SignResult signDocument(const SignRequest &request);

// A field name not already used by the document. Signature fields must be
// uniquely named, and forms often already contain Signature_1, Signature_2, ...
QString uniqueFieldName(Poppler::Document *document, const QString &preferredBase);

// "Form A.pdf" -> "Form A-signed-20260826-162457.pdf", next to the original.
// The timestamp is what keeps this out of Okular's refuse-to-overwrite dance.
QString timestampedOutputPath(const QString &inputPath);
