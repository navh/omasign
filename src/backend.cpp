#include "backend.h"

#include "signaturepad.h"
#include "signer.h"

#include <QDesktopServices>
#include <QDir>
#include <QFileInfo>
#include <QGuiApplication>
#include <QMutexLocker>
#include <QTimer>
#include <QVariantMap>

#include <pwd.h>
#include <unistd.h>

#include <poppler-form.h>
#include <poppler-qt6.h>

Backend::Backend(QObject *parent) : QObject(parent)
{
    m_identity.load();
    // Poppler latches the NSS directory on first use, so point it at our
    // database before anything asks for the certificate list.
    m_identity.applyNssDir();

    // A certificate can be removed from the database behind our back; do not
    // keep claiming to be configured with one that is gone.
    if (!m_identity.certNickname().isEmpty()) {
        bool stillThere = false;
        for (const Identity::Certificate &cert : m_identity.availableCertificates())
            stillThere = stillThere || cert.nickname == m_identity.certNickname();
        if (!stillThere)
            m_identity.setCertNickname(QString());
    }
}

Backend::~Backend() = default;

// ---------------------------------------------------------------- document

bool Backend::open(const QUrl &url)
{
    const QString path = url.isLocalFile() ? url.toLocalFile() : url.toString();
    if (path.isEmpty())
        return false;

    if (!QFileInfo::exists(path)) {
        emit failed(tr("%1 does not exist.").arg(QFileInfo(path).fileName()));
        return false;
    }

    auto document = Poppler::Document::load(path);
    if (!document) {
        emit failed(tr("%1 is not a PDF Omasign can read.").arg(QFileInfo(path).fileName()));
        return false;
    }
    if (document->isLocked()) {
        emit failed(tr("%1 is password protected.").arg(QFileInfo(path).fileName()));
        return false;
    }

    document->setRenderHint(Poppler::Document::Antialiasing, true);
    document->setRenderHint(Poppler::Document::TextAntialiasing, true);
    document->setRenderHint(Poppler::Document::TextSlightHinting, true);

    {
        QMutexLocker locker(&m_documentMutex);
        m_document = std::move(document);
        m_pageCount = m_document->numPages();
        m_pageSizes.clear();
        m_pageSizes.reserve(m_pageCount);
        for (int i = 0; i < m_pageCount; ++i) {
            const std::unique_ptr<Poppler::Page> page = m_document->page(i);
            m_pageSizes.append(page ? page->pageSizeF() : QSizeF(595, 842));
        }
        m_existingSignatureCount = int(m_document->signatures().size());
    }

    m_documentPath = path;
    m_documentName = QFileInfo(path).fileName();
    m_currentPage = 0;
    ++m_renderRevision;

    emit documentChanged();
    emit currentPageChanged();
    return true;
}

void Backend::setCurrentPage(int page)
{
    const int clamped = qBound(0, page, qMax(0, m_pageCount - 1));
    if (clamped == m_currentPage)
        return;
    m_currentPage = clamped;
    emit currentPageChanged();
}

qreal Backend::currentPageAspect() const
{
    const QSizeF size = pageSizePoints(m_currentPage);
    if (size.height() <= 0.0)
        return 1.0;
    return size.width() / size.height();
}

QSizeF Backend::pageSizePoints(int pageIndex) const
{
    if (pageIndex < 0 || pageIndex >= m_pageSizes.size())
        return QSizeF(595, 842);
    return m_pageSizes.at(pageIndex);
}

QImage Backend::renderPage(int pageIndex, const QSize &requestedSize)
{
    QMutexLocker locker(&m_documentMutex);
    if (!m_document || pageIndex < 0 || pageIndex >= m_document->numPages())
        return QImage();

    const std::unique_ptr<Poppler::Page> page = m_document->page(pageIndex);
    if (!page)
        return QImage();

    const QSizeF sizePt = page->pageSizeF();
    if (sizePt.isEmpty())
        return QImage();

    // Choose a DPI from the width QML asked for, so the page is rendered once
    // at the size it will actually be shown at.
    qreal dpi = 150.0;
    if (requestedSize.width() > 0)
        dpi = 72.0 * requestedSize.width() / sizePt.width();
    else if (requestedSize.height() > 0)
        dpi = 72.0 * requestedSize.height() / sizePt.height();
    dpi = qBound(36.0, dpi, 600.0);

    return page->renderToImage(dpi, dpi);
}

// ---------------------------------------------------------------- identity

void Backend::setSignerName(const QString &name)
{
    if (m_identity.name() == name)
        return;
    m_identity.setName(name);
    m_identity.save();
    emit identityChanged();
    emit appearanceChanged();
}

void Backend::setSignerEmail(const QString &email)
{
    if (m_identity.email() == email)
        return;
    m_identity.setEmail(email);
    m_identity.save();
    emit identityChanged();
}

void Backend::setUseInk(bool useInk)
{
    if (m_identity.useInk() == useInk)
        return;
    m_identity.setUseInk(useInk);
    m_identity.save();
    emit appearanceChanged();
}

void Backend::setShowMeta(bool showMeta)
{
    if (m_identity.showMeta() == showMeta)
        return;
    m_identity.setShowMeta(showMeta);
    m_identity.save();
    emit appearanceChanged();
}

void Backend::setReason(const QString &reason)
{
    if (m_identity.reason() == reason)
        return;
    m_identity.setReason(reason);
    m_identity.save();
    emit appearanceChanged();
}

void Backend::setLocation(const QString &location)
{
    if (m_identity.location() == location)
        return;
    m_identity.setLocation(location);
    m_identity.save();
    emit appearanceChanged();
}

void Backend::setCertNickname(const QString &nickname)
{
    if (m_identity.certNickname() == nickname)
        return;
    m_identity.setCertNickname(nickname);
    m_identity.save();
    emit identityChanged();
}

QVariantList Backend::certificates() const
{
    QVariantList list;
    for (const Identity::Certificate &cert : m_identity.availableCertificates()) {
        QVariantMap entry;
        entry.insert(QStringLiteral("nickname"), cert.nickname);
        entry.insert(QStringLiteral("subject"), cert.subject);
        entry.insert(QStringLiteral("issuer"), cert.issuer);
        entry.insert(QStringLiteral("selfSigned"), cert.selfSigned);
        entry.insert(QStringLiteral("expired"), cert.expired);
        list.append(entry);
    }
    return list;
}

QString Backend::suggestedName() const
{
    if (!m_identity.name().isEmpty())
        return m_identity.name();

    // The account's GECOS field usually holds a real full name -- much better
    // to sign with than a login name.
    if (const struct passwd *entry = getpwuid(getuid())) {
        const QString gecos = QString::fromLocal8Bit(entry->pw_gecos).section(QLatin1Char(','), 0, 0).trimmed();
        if (!gecos.isEmpty())
            return gecos;
    }
    return qEnvironmentVariable("USER", qEnvironmentVariable("USERNAME"));
}

bool Backend::captureInk(SignaturePad *pad)
{
    if (!pad)
        return false;
    const Ink ink = pad->ink();
    if (ink.isEmpty())
        return false;
    if (!m_identity.setInk(ink)) {
        emit failed(tr("Could not save the drawn signature."));
        return false;
    }
    emit appearanceChanged();
    return true;
}

QString Backend::completeSetup(const QString &name, const QString &email, bool useInk)
{
    const QString trimmedName = name.trimmed();
    if (trimmedName.isEmpty())
        return tr("Please enter the name you sign with.");
    if (useInk && m_identity.ink().isEmpty())
        return tr("Draw your signature first, or switch to a typed one.");

    m_identity.setName(trimmedName);
    m_identity.setEmail(email.trimmed());
    m_identity.setUseInk(useInk);

    if (m_identity.certNickname().isEmpty()) {
        QString error;
        const QString nickname =
            m_identity.createSelfSignedCertificate(trimmedName, email.trimmed(), &error);
        if (nickname.isEmpty()) {
            return error.isEmpty() ? tr("Could not create a signing certificate.") : error;
        }
        m_identity.setCertNickname(nickname);
    }

    m_identity.save();
    emit identityChanged();
    emit appearanceChanged();
    return QString();
}

// ---------------------------------------------------------------- appearance

AppearanceSpec Backend::currentAppearanceSpec() const
{
    AppearanceSpec spec;
    spec.name = m_identity.name();
    spec.metaLines = buildMetaLines(m_identity.name(), m_identity.reason(), m_identity.location());
    spec.useInk = m_identity.useInk();
    spec.ink = m_identity.ink();
    spec.showMeta = m_identity.showMeta();
    spec.splitRatio = m_identity.splitRatio();
    spec.markColour = m_identity.markColour();
    spec.metaColour = m_identity.metaColour();
    spec.watermarkPath = m_identity.watermarkPath();
    spec.watermarkOpacity = m_identity.watermarkOpacity();
    spec.fontFamily = m_identity.fontFamily();
    return spec;
}

// ---------------------------------------------------------------- signing

void Backend::signRectangle(int pageIndex, qreal x, qreal y, qreal width, qreal height)
{
    if (!hasDocument()) {
        emit failed(tr("Open a PDF first."));
        return;
    }
    if (!configured()) {
        emit failed(tr("Set up your signature first."));
        return;
    }
    if (width <= 0.0 || height <= 0.0) {
        emit failed(tr("Drag a rectangle where the signature should go."));
        return;
    }

    SignRequest request;
    request.inputPath = m_documentPath;
    request.outputPath = timestampedOutputPath(m_documentPath);
    request.pageIndex = pageIndex;
    request.rect = QRectF(x, y, width, height);
    request.pageSizePt = pageSizePoints(pageIndex);
    request.certNickname = m_identity.certNickname();
    request.reason = m_identity.reason();
    request.location = m_identity.location();
    request.appearance = currentAppearanceSpec();

    setBusy(true);

    // Signing blocks, and poppler is not safe to drive from two threads at
    // once, so it stays on this one -- but yield first, or the busy state would
    // never make it to the screen before the freeze.
    QTimer::singleShot(0, this, [this, request] {
        const SignResult result = signDocument(request);
        setBusy(false);

        if (!result.ok) {
            emit failed(result.error);
            return;
        }

        m_lastOutputPath = result.outputPath;
        emit signedSuccessfully(result.outputPath);

        // Carry on from the signed copy, so a second signature on the same
        // document builds on the first rather than starting over from the
        // unsigned original -- staying on the page the user was looking at.
        const int page = request.pageIndex;
        if (open(QUrl::fromLocalFile(result.outputPath)))
            setCurrentPage(page);
    });
}

void Backend::openLastOutput() const
{
    if (m_lastOutputPath.isEmpty())
        return;
    QDesktopServices::openUrl(QUrl::fromLocalFile(m_lastOutputPath));
}

void Backend::revealLastOutput() const
{
    if (m_lastOutputPath.isEmpty())
        return;
    // No portable "show in file manager" API; opening the containing directory
    // is what every desktop here does anyway.
    QDesktopServices::openUrl(QUrl::fromLocalFile(QFileInfo(m_lastOutputPath).absolutePath()));
}

void Backend::setBusy(bool busy)
{
    if (m_busy == busy)
        return;
    m_busy = busy;
    emit busyChanged();
}

void Backend::setDarkMode(bool darkMode)
{
    if (m_darkMode == darkMode)
        return;
    m_darkMode = darkMode;
    emit darkModeChanged();
}
