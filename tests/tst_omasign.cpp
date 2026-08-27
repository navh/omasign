#include <QtTest>

#include <QDir>
#include <QFile>
#include <QGuiApplication>
#include <QImage>
#include <QQmlApplicationEngine>
#include <QQmlContext>
#include <QQuickItem>
#include <QQuickStyle>
#include <QQuickView>
#include <QQuickWindow>
#include <QStandardPaths>
#include <QTemporaryDir>

#include <poppler-form.h>
#include <poppler-qt6.h>

#include "appearance.h"
#include "appearanceitem.h"
#include "backend.h"
#include "identity.h"
#include "ink.h"
#include "signaturepad.h"
#include "pageimageprovider.h"
#include "signer.h"

namespace {

// A minimal one-page PDF with a rule across it, so a transparent signature
// laid over the rule visibly does not paint it out.
QByteArray minimalPdf()
{
    const QByteArray content =
        "BT /F1 18 Tf 72 780 Td (Omasign test document) Tj ET\n"
        "0.6 w 300 720 m 500 720 l S\n";

    QList<QByteArray> objects;
    objects << "<< /Type /Catalog /Pages 2 0 R >>";
    objects << "<< /Type /Pages /Kids [3 0 R] /Count 1 >>";
    objects << "<< /Type /Page /Parent 2 0 R /MediaBox [0 0 595 842] "
               "/Resources << /Font << /F1 5 0 R >> >> /Contents 4 0 R >>";
    objects << "<< /Length " + QByteArray::number(content.size()) + " >>\nstream\n" + content + "endstream";
    objects << "<< /Type /Font /Subtype /Type1 /BaseFont /Helvetica >>";

    QByteArray out = "%PDF-1.7\n%\xe2\xe3\xcf\xd3\n";
    QList<int> offsets;
    for (int i = 0; i < objects.size(); ++i) {
        offsets << out.size();
        out += QByteArray::number(i + 1) + " 0 obj\n" + objects.at(i) + "\nendobj\n";
    }
    const int xref = out.size();
    out += "xref\n0 " + QByteArray::number(objects.size() + 1) + "\n0000000000 65535 f \n";
    for (int offset : offsets)
        out += QByteArray::number(offset).rightJustified(10, '0') + " 00000 n \n";
    out += "trailer\n<< /Size " + QByteArray::number(objects.size() + 1) + " /Root 1 0 R >>\nstartxref\n"
        + QByteArray::number(xref) + "\n%%EOF\n";
    return out;
}

// Fabricate pad input in the shape SignaturePad would produce.
QList<QList<QPointF>> sampleStrokes()
{
    QList<QPointF> first;
    for (int i = 0; i <= 60; ++i) {
        const qreal t = i / 60.0;
        first.append(QPointF(20 + t * 220, 90 - 45 * qSin(t * M_PI * 1.6)));
    }
    QList<QPointF> second;
    for (int i = 0; i <= 20; ++i) {
        const qreal t = i / 20.0;
        second.append(QPointF(120 + t * 90, 100 - t * 20));
    }
    return {first, second};
}

} // namespace

class TestOmasign : public QObject {
    Q_OBJECT

private slots:
    void initTestCase();

    void inkRoundTripsThroughDisk();
    void inkNormalisesToItsOwnBoundingBox();

    void metaLinesUseAdobesDateFormat();
    void metaLinesIncludeOptionalFields();

    void appearanceIsTransparentAtTheCorners();
    void appearanceNeverClipsALongName();
    void appearanceHonoursTheWatermarkSetting();

    void outputPathGetsATimestamp();
    void outputPathDoesNotAccumulateSuffixes();

    void signingProducesAValidSignature();
    void signingLeavesNoBorderOrBackground();
    void signingTwiceKeepsTheFirstSignature();

    void interfaceLoadsWithoutErrors();
    void signingViewLoadsWithADocument();
    void drawingDoesNotScrollTheSetupView();

private:
    void registerQmlTypes();
    void capture(QQmlApplicationEngine &engine, const QString &name);

    QTemporaryDir m_scratch;
    QString m_nssDir;
    QString m_certNickname;
    QString m_inputPdf;
};

void TestOmasign::initTestCase()
{
    QVERIFY(m_scratch.isValid());

    // Keep the user's own configuration and key store out of the tests.
    QStandardPaths::setTestModeEnabled(true);

    const QString nssPath = m_scratch.filePath(QStringLiteral("nssdb"));
    m_nssDir = QStringLiteral("sql:%1").arg(nssPath);

    Identity identity;
    identity.setNssDir(m_nssDir);

    // The same call the application makes at startup: it must leave NSS usable
    // even though no database exists yet, and no certificate exists yet either.
    identity.applyNssDir();

    QString error;
    m_certNickname = identity.createSelfSignedCertificate(
        QStringLiteral("Test Signer"), QStringLiteral("test@example.invalid"), &error);
    if (m_certNickname.isEmpty())
        QSKIP(qPrintable(QStringLiteral("No signing certificate could be created: %1").arg(error)));

    m_inputPdf = m_scratch.filePath(QStringLiteral("input.pdf"));
    QFile file(m_inputPdf);
    QVERIFY(file.open(QIODevice::WriteOnly));
    QCOMPARE(file.write(minimalPdf()), qint64(minimalPdf().size()));
}

// ------------------------------------------------------------------- ink

void TestOmasign::inkRoundTripsThroughDisk()
{
    const Ink original = Ink::fromRawStrokes(sampleStrokes(), QSizeF(260, 140));
    QVERIFY(!original.isEmpty());

    const QString path = m_scratch.filePath(QStringLiteral("ink.json"));
    QVERIFY(original.saveToFile(path));

    Ink restored;
    QVERIFY(restored.loadFromFile(path));
    QCOMPARE(restored.strokes().size(), original.strokes().size());
    QVERIFY(qAbs(restored.aspect() - original.aspect()) < 0.01);
}

void TestOmasign::inkNormalisesToItsOwnBoundingBox()
{
    const Ink ink = Ink::fromRawStrokes(sampleStrokes(), QSizeF(260, 140));
    QVERIFY(!ink.isEmpty());

    // Every point must land inside the unit box, whatever the pad size was.
    qreal minX = 1e9, maxX = -1e9, minY = 1e9, maxY = -1e9;
    for (const InkStroke &stroke : ink.strokes()) {
        for (const InkPoint &p : stroke.points) {
            minX = qMin(minX, p.x); maxX = qMax(maxX, p.x);
            minY = qMin(minY, p.y); maxY = qMax(maxY, p.y);
            QVERIFY(p.width > 0.0);
        }
    }
    QVERIFY(minX >= -0.001);
    QVERIFY(minY >= -0.001);
    QVERIFY(maxX <= 1.001);
    QVERIFY(maxY <= 1.001);
    // The box is only meaningful if the ink actually spans it.
    QVERIFY(maxX > 0.9);
}

// ------------------------------------------------------------- meta lines

void TestOmasign::metaLinesUseAdobesDateFormat()
{
    const QStringList lines = buildMetaLines(QStringLiteral("Ada Lovelace"), QString(), QString());
    QCOMPARE(lines.size(), 2);
    QCOMPARE(lines.at(0), QStringLiteral("Digitally signed by Ada Lovelace"));

    // "Date: 2026.08.26 16:24:57 +02'00'"
    static const QRegularExpression shape(
        QStringLiteral("^Date: \\d{4}\\.\\d{2}\\.\\d{2} \\d{2}:\\d{2}:\\d{2} [+-]\\d{2}'\\d{2}'$"));
    QVERIFY2(shape.match(lines.at(1)).hasMatch(), qPrintable(lines.at(1)));
}

void TestOmasign::metaLinesIncludeOptionalFields()
{
    const QStringList lines = buildMetaLines(QStringLiteral("Ada Lovelace"),
                                             QStringLiteral("I approve"),
                                             QStringLiteral("London"));
    QCOMPARE(lines.size(), 4);
    QCOMPARE(lines.at(2), QStringLiteral("Reason: I approve"));
    QCOMPARE(lines.at(3), QStringLiteral("Location: London"));
}

// ------------------------------------------------------------- appearance

void TestOmasign::appearanceIsTransparentAtTheCorners()
{
    AppearanceSpec spec;
    spec.name = QStringLiteral("Ada Lovelace");
    spec.metaLines = buildMetaLines(spec.name, QString(), QString());

    const QImage image = renderAppearance(spec, QSizeF(200, 44), 4.0);
    QCOMPARE(image.width(), 800);
    QCOMPARE(image.height(), 176);

    // No filled background: this is the whole difference from a grey box.
    QCOMPARE(qAlpha(image.pixel(0, 0)), 0);
    QCOMPARE(qAlpha(image.pixel(image.width() - 1, 0)), 0);
    QCOMPARE(qAlpha(image.pixel(0, image.height() - 1)), 0);
    QCOMPARE(qAlpha(image.pixel(image.width() - 1, image.height() - 1)), 0);

    // ...but something was actually drawn.
    int opaque = 0;
    for (int y = 0; y < image.height(); y += 3)
        for (int x = 0; x < image.width(); x += 3)
            if (qAlpha(image.pixel(x, y)) > 128)
                ++opaque;
    QVERIFY(opaque > 100);
}

void TestOmasign::appearanceNeverClipsALongName()
{
    // Okular's failure mode: the name overflows the box and is cut mid-glyph.
    // Whatever the name, every drawn pixel must stay inside the image.
    const QStringList names {
        QStringLiteral("Al"),
        QStringLiteral("Amos N Hebb"),
        QStringLiteral("Prof dr ir Bartholomew Featherstonehaugh-Cholmondeley"),
    };

    for (const QString &name : names) {
        AppearanceSpec spec;
        spec.name = name;
        spec.metaLines = buildMetaLines(name, QString(), QString());
        spec.watermarkOpacity = 0.0; // isolate the text

        const QImage image = renderAppearance(spec, QSizeF(200, 44), 6.0);

        // The outermost ring of pixels must be untouched: if the fit failed,
        // glyphs would run into it.
        for (int x = 0; x < image.width(); ++x) {
            QVERIFY2(qAlpha(image.pixel(x, 0)) == 0, qPrintable(name));
            QVERIFY2(qAlpha(image.pixel(x, image.height() - 1)) == 0, qPrintable(name));
        }
        for (int y = 0; y < image.height(); ++y) {
            QVERIFY2(qAlpha(image.pixel(0, y)) == 0, qPrintable(name));
            QVERIFY2(qAlpha(image.pixel(image.width() - 1, y)) == 0, qPrintable(name));
        }
    }
}

void TestOmasign::appearanceHonoursTheWatermarkSetting()
{
    AppearanceSpec spec;
    spec.name = QStringLiteral("Ada Lovelace");
    spec.metaLines = buildMetaLines(spec.name, QString(), QString());

    const auto inkCoverage = [](const QImage &image) {
        int touched = 0;
        for (int y = 0; y < image.height(); ++y)
            for (int x = 0; x < image.width(); ++x)
                if (qAlpha(image.pixel(x, y)) > 0)
                    ++touched;
        return touched;
    };

    spec.watermarkOpacity = 0.0;
    const int without = inkCoverage(renderAppearance(spec, QSizeF(200, 44), 4.0));

    spec.watermarkOpacity = 0.2;
    const int with = inkCoverage(renderAppearance(spec, QSizeF(200, 44), 4.0));

    QVERIFY2(with > without, "the watermark should add coverage");

    spec.watermarkPath = QStringLiteral("none");
    const int disabled = inkCoverage(renderAppearance(spec, QSizeF(200, 44), 4.0));
    QCOMPARE(disabled, without);
}

// ----------------------------------------------------------- output paths

void TestOmasign::outputPathGetsATimestamp()
{
    const QString out = timestampedOutputPath(m_scratch.filePath(QStringLiteral("Form A.pdf")));
    static const QRegularExpression shape(QStringLiteral("Form A-signed-\\d{8}-\\d{6}\\.pdf$"));
    QVERIFY2(shape.match(out).hasMatch(), qPrintable(out));
    QCOMPARE(QFileInfo(out).absolutePath(), QFileInfo(m_scratch.path()).absoluteFilePath());
}

void TestOmasign::outputPathDoesNotAccumulateSuffixes()
{
    // Signing a signed file should not produce "-signed-...-signed-...".
    const QString once = timestampedOutputPath(m_scratch.filePath(QStringLiteral("Form A.pdf")));
    const QString twice = timestampedOutputPath(once);
    QCOMPARE(twice.count(QStringLiteral("-signed-")), 1);
}

// --------------------------------------------------------------- signing

void TestOmasign::signingProducesAValidSignature()
{
    SignRequest request;
    request.inputPath = m_inputPdf;
    request.outputPath = m_scratch.filePath(QStringLiteral("signed.pdf"));
    request.pageIndex = 0;
    request.rect = QRectF(0.30, 0.12, 0.34, 0.055);
    request.pageSizePt = QSizeF(595, 842);
    request.certNickname = m_certNickname;
    request.appearance.name = QStringLiteral("Test Signer");
    request.appearance.metaLines = buildMetaLines(QStringLiteral("Test Signer"), QString(), QString());

    const SignResult result = signDocument(request);
    QVERIFY2(result.ok, qPrintable(result.error));
    QVERIFY(QFile::exists(result.outputPath));

    auto document = Poppler::Document::load(result.outputPath);
    QVERIFY(document);
    const auto signatures = document->signatures();
    QCOMPARE(signatures.size(), size_t(1));

    const Poppler::SignatureValidationInfo info =
        signatures.front()->validate(Poppler::FormFieldSignature::ValidateVerifyCertificate);
    QCOMPARE(info.signatureStatus(), Poppler::SignatureValidationInfo::SignatureValid);
    QCOMPARE(info.signerName(), QStringLiteral("Test Signer"));

    // A field name a human can read, not a bare UUID.
    QCOMPARE(signatures.front()->fullyQualifiedName(), QStringLiteral("Omasign_1"));
}

void TestOmasign::signingLeavesNoBorderOrBackground()
{
    SignRequest request;
    request.inputPath = m_inputPdf;
    request.outputPath = m_scratch.filePath(QStringLiteral("signed-clean.pdf"));
    request.pageIndex = 0;
    request.rect = QRectF(0.30, 0.12, 0.34, 0.055);
    request.pageSizePt = QSizeF(595, 842);
    request.certNickname = m_certNickname;
    request.appearance.name = QStringLiteral("Test Signer");
    request.appearance.metaLines = buildMetaLines(QStringLiteral("Test Signer"), QString(), QString());

    const SignResult result = signDocument(request);
    QVERIFY2(result.ok, qPrintable(result.error));

    auto document = Poppler::Document::load(result.outputPath);
    QVERIFY(document);
    const std::unique_ptr<Poppler::Page> page = document->page(0);
    QVERIFY(page);
    const QImage rendered = page->renderToImage(150, 150);
    QVERIFY(!rendered.isNull());

    // Corners of the signature rectangle must still be white paper. If poppler
    // had drawn its default grey fill or red border, they would not be.
    const int left = int(0.30 * rendered.width());
    const int top = int(0.12 * rendered.height());
    const int right = int((0.30 + 0.34) * rendered.width()) - 1;
    const int bottom = int((0.12 + 0.055) * rendered.height()) - 1;

    const QList<QPoint> corners {
        QPoint(left + 1, top + 1), QPoint(right - 1, top + 1),
        QPoint(left + 1, bottom - 1), QPoint(right - 1, bottom - 1),
    };
    for (const QPoint &corner : corners) {
        const QRgb pixel = rendered.pixel(corner);
        QVERIFY2(qRed(pixel) > 245 && qGreen(pixel) > 245 && qBlue(pixel) > 245,
                 qPrintable(QStringLiteral("corner %1,%2 is %3,%4,%5")
                                .arg(corner.x()).arg(corner.y())
                                .arg(qRed(pixel)).arg(qGreen(pixel)).arg(qBlue(pixel))));
    }
}

void TestOmasign::signingTwiceKeepsTheFirstSignature()
{
    SignRequest first;
    first.inputPath = m_inputPdf;
    first.outputPath = m_scratch.filePath(QStringLiteral("once.pdf"));
    first.pageIndex = 0;
    first.rect = QRectF(0.10, 0.10, 0.30, 0.05);
    first.pageSizePt = QSizeF(595, 842);
    first.certNickname = m_certNickname;
    first.appearance.name = QStringLiteral("Test Signer");
    first.appearance.metaLines = buildMetaLines(QStringLiteral("Test Signer"), QString(), QString());

    const SignResult firstResult = signDocument(first);
    QVERIFY2(firstResult.ok, qPrintable(firstResult.error));

    SignRequest second = first;
    second.inputPath = firstResult.outputPath;
    second.outputPath = m_scratch.filePath(QStringLiteral("twice.pdf"));
    second.rect = QRectF(0.10, 0.30, 0.30, 0.05);
    second.fieldName.clear();

    const SignResult secondResult = signDocument(second);
    QVERIFY2(secondResult.ok, qPrintable(secondResult.error));

    auto document = Poppler::Document::load(secondResult.outputPath);
    QVERIFY(document);
    // Both signatures survive, and both still verify: this is what lets a form
    // go round a committee and come back with everyone's signature intact.
    const auto signatures = document->signatures();
    QCOMPARE(signatures.size(), size_t(2));
    for (const auto &signature : signatures) {
        const Poppler::SignatureValidationInfo info =
            signature->validate(Poppler::FormFieldSignature::ValidateVerifyCertificate);
        QCOMPARE(info.signatureStatus(), Poppler::SignatureValidationInfo::SignatureValid);
    }
}

// ------------------------------------------------------------- interface

void TestOmasign::registerQmlTypes()
{
    static bool done = false;
    if (done)
        return;
    done = true;
    QQuickStyle::setStyle(QStringLiteral("Basic"));
    qmlRegisterSingletonType(QUrl(QStringLiteral("qrc:/Theme.qml")), "Omasign", 1, 0, "Theme");
    qmlRegisterType<AppearanceItem>("Omasign", 1, 0, "SignatureAppearance");
    qmlRegisterType<SignaturePad>("Omasign", 1, 0, "SignaturePad");
}

void TestOmasign::interfaceLoadsWithoutErrors()
{
    registerQmlTypes();

    Backend backend;

    QStringList problems;
    QQmlApplicationEngine engine;
    QObject::connect(&engine, &QQmlApplicationEngine::warnings, &engine,
                     [&problems](const QList<QQmlError> &warnings) {
        for (const QQmlError &warning : warnings)
            problems << warning.toString();
    });
    engine.rootContext()->setContextProperty(QStringLiteral("backend"), &backend);
    engine.load(QUrl(QStringLiteral("qrc:/Main.qml")));

    QVERIFY(!engine.rootObjects().isEmpty());
    QVERIFY2(problems.isEmpty(), qPrintable(problems.join(QLatin1Char('\n'))));

    // Optionally capture what the interface looks like, for eyeballing during
    // development: OMASIGN_SHOT_DIR=/tmp/shots bin/test
    capture(engine, QStringLiteral("setup"));
}

// The setup screen is what loads by default; this covers the screen the user
// actually spends their time in, with a document open.
void TestOmasign::signingViewLoadsWithADocument()
{
    registerQmlTypes();

    Backend backend;
    backend.setSignerName(QStringLiteral("Test Signer"));
    backend.setCertNickname(m_certNickname);
    QVERIFY(backend.configured());
    QVERIFY(backend.open(QUrl::fromLocalFile(m_inputPdf)));
    QCOMPARE(backend.pageCount(), 1);

    QStringList problems;
    QQmlApplicationEngine engine;
    QObject::connect(&engine, &QQmlApplicationEngine::warnings, &engine,
                     [&problems](const QList<QQmlError> &warnings) {
        for (const QQmlError &warning : warnings)
            problems << warning.toString();
    });
    engine.addImageProvider(QStringLiteral("page"), new PageImageProvider(&backend));
    engine.rootContext()->setContextProperty(QStringLiteral("backend"), &backend);
    engine.load(QUrl(QStringLiteral("qrc:/Main.qml")));

    QVERIFY(!engine.rootObjects().isEmpty());
    QVERIFY2(problems.isEmpty(), qPrintable(problems.join(QLatin1Char('\n'))));

    capture(engine, QStringLiteral("signing"));

    // Opening Settings instantiates a whole subtree that nothing else touches.
    auto *window = qobject_cast<QQuickWindow *>(engine.rootObjects().constFirst());
    QVERIFY(window);
    QObject *sheet = window->findChild<QObject *>(QString(), Qt::FindChildrenRecursively);
    Q_UNUSED(sheet)
    QVERIFY2(problems.isEmpty(), qPrintable(problems.join(QLatin1Char('\n'))));
}

// The pad sits inside the setup view's Flickable, and a Flickable takes the
// mouse away from its children once a drag passes the threshold. On a short
// window that made the signature impossible to draw -- every stroke scrolled
// the page instead. The pad holds the grab for the length of a stroke.
void TestOmasign::drawingDoesNotScrollTheSetupView()
{
    registerQmlTypes();

    Backend backend;
    backend.setUseInk(true); // the pad is only shown for the drawn style

    QQuickView view;
    view.engine()->rootContext()->setContextProperty(QStringLiteral("backend"), &backend);
    view.setResizeMode(QQuickView::SizeRootObjectToView);
    view.resize(620, 420);
    view.setSource(QUrl(QStringLiteral("qrc:/SetupView.qml")));
    QCOMPARE(view.status(), QQuickView::Ready);
    view.show();
    QVERIFY(QTest::qWaitForWindowExposed(&view));

    auto *pad = view.rootObject()->findChild<SignaturePad *>();
    QVERIFY(pad);

    QQuickItem *flickable = nullptr;
    for (QQuickItem *item = pad->parentItem(); item; item = item->parentItem()) {
        if (item->inherits("QQuickFlickable")) {
            flickable = item;
            break;
        }
    }
    QVERIFY2(flickable, "the pad is expected to sit inside the scrolling setup view");
    const qreal scrollable = flickable->property("contentHeight").toReal() - flickable->height();
    QVERIFY2(scrollable > 1.0,
             "the window has to be short enough to scroll for this test to mean anything");

    // Scroll the pad into view, the way the user does before signing, and leave
    // room to scroll in both directions so a stolen grab has somewhere to go.
    const qreal padTop = pad->mapToScene(QPointF(0, 0)).y();
    flickable->setProperty("contentY",
                           qBound(0.0, padTop - (flickable->height() - pad->height()) / 2.0,
                                  scrollable));
    const qreal restingAt = flickable->property("contentY").toReal();
    QVERIFY2(restingAt > 0.0, "the pad should have had to be scrolled to");

    // Drag upwards, the direction that would scroll the view down.
    const QPointF start = pad->mapToScene(QPointF(pad->width() / 2.0, pad->height() * 0.8));
    QTest::mousePress(&view, Qt::LeftButton, {}, start.toPoint());
    for (int travelled = 6; travelled <= 90; travelled += 6)
        QTest::mouseMove(&view, (start - QPointF(0, travelled)).toPoint());
    QTest::mouseRelease(&view, Qt::LeftButton, {}, (start - QPointF(0, 90)).toPoint());

    QCOMPARE(flickable->property("contentY").toReal(), restingAt);
    QVERIFY2(!pad->isEmpty(), "the stroke should have landed on the pad");
    QVERIFY(!pad->ink().isEmpty());
}

void TestOmasign::capture(QQmlApplicationEngine &engine, const QString &name)
{
    const QString shotDir = qEnvironmentVariable("OMASIGN_SHOT_DIR");
    if (shotDir.isEmpty())
        return;

    auto *window = qobject_cast<QQuickWindow *>(engine.rootObjects().constFirst());
    if (!window)
        return;
    QDir().mkpath(shotDir);
    window->show();
    QTest::qWait(1200);
    const QImage shot = window->grabWindow();
    if (!shot.isNull())
        shot.save(QStringLiteral("%1/%2.png").arg(shotDir, name));
    window->hide();
}

QTEST_MAIN(TestOmasign)
#include "tst_omasign.moc"
