#include "signer.h"

#include <QDateTime>
#include <QDir>
#include <QFileInfo>
#include <QImage>
#include <QRegularExpression>
#include <QSet>
#include <QTemporaryDir>

#include <poppler-converter.h>
#include <poppler-form.h>
#include <poppler-qt6.h>

#include <memory>

namespace {

QString describe(Poppler::PDFConverter::Error error)
{
    switch (error) {
    case Poppler::PDFConverter::NoError:
        return QString();
    case Poppler::PDFConverter::FileLockedError:
        return QStringLiteral("The output file is locked by another program.");
    case Poppler::PDFConverter::OpenOutputError:
        return QStringLiteral("The output file could not be written.");
    case Poppler::PDFConverter::NotSupportedInputFileError:
        return QStringLiteral("This PDF cannot be signed by poppler.");
    }
    return QStringLiteral("Unknown error.");
}

} // namespace

QString uniqueFieldName(Poppler::Document *document, const QString &preferredBase)
{
    QSet<QString> taken;
    if (document) {
        for (int i = 0; i < document->numPages(); ++i) {
            const std::unique_ptr<Poppler::Page> page = document->page(i);
            if (!page)
                continue;
            const auto fields = page->formFields();
            for (const auto &field : fields)
                taken.insert(field->fullyQualifiedName());
        }
    }
    int index = 1;
    QString candidate = QStringLiteral("%1_%2").arg(preferredBase).arg(index);
    while (taken.contains(candidate))
        candidate = QStringLiteral("%1_%2").arg(preferredBase).arg(++index);
    return candidate;
}

QString timestampedOutputPath(const QString &inputPath)
{
    const QFileInfo info(inputPath);
    const QString stamp = QDateTime::currentDateTime().toString(QStringLiteral("yyyyMMdd-HHmmss"));
    QString base = info.completeBaseName();

    // Signing an already-signed file should not pile suffixes up forever.
    static const QRegularExpression previous(QStringLiteral("-signed-\\d{8}-\\d{6}$"));
    base.remove(previous);

    QDir dir = info.absoluteDir();
    QString candidate = dir.filePath(QStringLiteral("%1-signed-%2.pdf").arg(base, stamp));
    int extra = 2;
    while (QFile::exists(candidate)) // same second, second signature
        candidate = dir.filePath(QStringLiteral("%1-signed-%2-%3.pdf").arg(base, stamp).arg(extra++));
    return candidate;
}

SignResult signDocument(const SignRequest &request)
{
    SignResult result;
    result.outputPath = request.outputPath;

    if (request.certNickname.isEmpty()) {
        result.error = QStringLiteral("No signing certificate selected.");
        return result;
    }
    if (request.rect.isEmpty() || request.pageSizePt.isEmpty()) {
        result.error = QStringLiteral("The signature rectangle is empty.");
        return result;
    }

    auto document = Poppler::Document::load(request.inputPath,
                                            request.documentOwnerPassword,
                                            request.documentUserPassword);
    if (!document) {
        result.error = QStringLiteral("Could not open %1.").arg(QFileInfo(request.inputPath).fileName());
        return result;
    }
    if (document->isLocked()) {
        result.error = QStringLiteral("This PDF is password protected.");
        return result;
    }
    if (request.pageIndex < 0 || request.pageIndex >= document->numPages()) {
        result.error = QStringLiteral("Page %1 does not exist.").arg(request.pageIndex + 1);
        return result;
    }

    // Render the appearance at the rectangle's true size in points, so nothing
    // is distorted when poppler stretches the bitmap over the annotation.
    const QSizeF rectPt(request.rect.width() * request.pageSizePt.width(),
                        request.rect.height() * request.pageSizePt.height());

    // 8x gives roughly 576 dpi: crisp when the reader zooms in, and still only
    // a few hundred kilobytes for a signature-sized rectangle.
    const QImage appearance = renderAppearance(request.appearance, rectPt, 8.0);

    QTemporaryDir scratch;
    if (!scratch.isValid()) {
        result.error = QStringLiteral("Could not create a temporary directory.");
        return result;
    }
    const QString appearancePath = scratch.filePath(QStringLiteral("appearance.png"));
    if (!appearance.save(appearancePath, "PNG")) {
        result.error = QStringLiteral("Could not render the signature appearance.");
        return result;
    }

    const QString fieldName = request.fieldName.isEmpty()
        ? uniqueFieldName(document.get(), QStringLiteral("Omasign"))
        : request.fieldName;

    Poppler::PDFConverter::NewSignatureData data;
    data.setCertNickname(request.certNickname);
    data.setPassword(request.certPassword);
    data.setPage(request.pageIndex);
    data.setBoundingRectangle(request.rect);
    data.setFieldPartialName(fieldName);
    data.setReason(request.reason);
    data.setLocation(request.location);
    data.setDocumentOwnerPassword(request.documentOwnerPassword);
    data.setDocumentUserPassword(request.documentUserPassword);

    // The whole design lives in the PNG. Poppler still insists on drawing its
    // own text, and skips the image entirely when the text is empty, so hand it
    // a single space: it renders as nothing, and the image gets drawn.
    data.setSignatureText(QStringLiteral(" "));
    data.setSignatureLeftText(QString());
    data.setFontSize(1.0);
    data.setLeftFontSize(1.0);
    // Not transparent: poppler dereferences the font colour unconditionally in
    // AnnotAppearanceBuilder::drawSignatureFieldText, so a null colour crashes.
    // The text is a space, so the colour never shows.
    data.setFontColor(Qt::black);

    // These two are what separate this from Okular's grey box: no border, no
    // fill, so the page shows through everywhere the artwork is transparent.
    data.setBorderWidth(0.0);
    data.setBorderColor(Qt::transparent);
    data.setBackgroundColor(Qt::transparent);

    data.setImagePath(appearancePath);

    std::unique_ptr<Poppler::PDFConverter> converter(document->pdfConverter());
    if (!converter) {
        result.error = QStringLiteral("This PDF cannot be converted for signing.");
        return result;
    }
    converter->setOutputFileName(request.outputPath);

    if (!converter->sign(data)) {
        const QString detail = describe(converter->lastError());
        result.error = detail.isEmpty()
            ? QStringLiteral("Signing failed. Check that the certificate is still valid.")
            : detail;
        QFile::remove(request.outputPath); // do not leave a truncated file behind
        return result;
    }

    result.ok = true;
    return result;
}
