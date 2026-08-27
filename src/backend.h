#pragma once

#include <QImage>
#include <QMutex>
#include <QObject>
#include <QSizeF>
#include <QUrl>
#include <QVariantList>

#include <memory>

#include "appearance.h"
#include "identity.h"

namespace Poppler {
class Document;
}
class SignaturePad;

// The single object QML talks to: it holds the open document, the signer's
// identity, and the one operation that matters -- turning a dragged rectangle
// into a signed PDF.
class Backend : public QObject {
    Q_OBJECT

    Q_PROPERTY(bool hasDocument READ hasDocument NOTIFY documentChanged)
    Q_PROPERTY(QString documentName READ documentName NOTIFY documentChanged)
    Q_PROPERTY(int pageCount READ pageCount NOTIFY documentChanged)
    Q_PROPERTY(int currentPage READ currentPage WRITE setCurrentPage NOTIFY currentPageChanged)
    Q_PROPERTY(qreal currentPageAspect READ currentPageAspect NOTIFY currentPageChanged)
    Q_PROPERTY(int renderRevision READ renderRevision NOTIFY documentChanged)
    Q_PROPERTY(int existingSignatureCount READ existingSignatureCount NOTIFY documentChanged)

    Q_PROPERTY(bool configured READ configured NOTIFY identityChanged)
    Q_PROPERTY(QString signerName READ signerName WRITE setSignerName NOTIFY identityChanged)
    Q_PROPERTY(QString signerEmail READ signerEmail WRITE setSignerEmail NOTIFY identityChanged)
    Q_PROPERTY(bool useInk READ useInk WRITE setUseInk NOTIFY appearanceChanged)
    Q_PROPERTY(bool hasInk READ hasInk NOTIFY appearanceChanged)
    Q_PROPERTY(bool showMeta READ showMeta WRITE setShowMeta NOTIFY appearanceChanged)
    Q_PROPERTY(QString reason READ reason WRITE setReason NOTIFY appearanceChanged)
    Q_PROPERTY(QString location READ location WRITE setLocation NOTIFY appearanceChanged)
    Q_PROPERTY(QString certNickname READ certNickname WRITE setCertNickname NOTIFY identityChanged)
    Q_PROPERTY(QVariantList certificates READ certificates NOTIFY identityChanged)

    Q_PROPERTY(bool busy READ busy NOTIFY busyChanged)
    Q_PROPERTY(bool darkMode READ darkMode WRITE setDarkMode NOTIFY darkModeChanged)

public:
    explicit Backend(QObject *parent = nullptr);
    ~Backend() override;

    // --- document
    bool hasDocument() const { return m_document != nullptr; }
    QString documentName() const { return m_documentName; }
    int pageCount() const { return m_pageCount; }
    int currentPage() const { return m_currentPage; }
    void setCurrentPage(int page);
    qreal currentPageAspect() const;
    int renderRevision() const { return m_renderRevision; }
    int existingSignatureCount() const { return m_existingSignatureCount; }

    Q_INVOKABLE bool open(const QUrl &url);

    // Page size in PDF points, needed to keep the signature rectangle's aspect
    // ratio honest.
    QSizeF pageSizePoints(int pageIndex) const;

    // Called from the image provider's thread.
    QImage renderPage(int pageIndex, const QSize &requestedSize);

    // --- identity
    bool configured() const { return m_identity.isConfigured(); }
    QString signerName() const { return m_identity.name(); }
    void setSignerName(const QString &name);
    QString signerEmail() const { return m_identity.email(); }
    void setSignerEmail(const QString &email);
    bool useInk() const { return m_identity.useInk(); }
    void setUseInk(bool useInk);
    bool hasInk() const { return !m_identity.ink().isEmpty(); }
    bool showMeta() const { return m_identity.showMeta(); }
    void setShowMeta(bool showMeta);
    QString reason() const { return m_identity.reason(); }
    void setReason(const QString &reason);
    QString location() const { return m_identity.location(); }
    void setLocation(const QString &location);
    QString certNickname() const { return m_identity.certNickname(); }
    void setCertNickname(const QString &nickname);
    QVariantList certificates() const;

    // Name to offer on the setup screen, from the system account.
    Q_INVOKABLE QString suggestedName() const;

    // Store the pad's strokes as this identity's drawn signature.
    Q_INVOKABLE bool captureInk(SignaturePad *pad);

    // Finish first-run setup: persist the identity and, unless one was chosen
    // already, mint a self-signed certificate. Returns an empty string on
    // success, or a message to show the user.
    Q_INVOKABLE QString completeSetup(const QString &name, const QString &email, bool useInk);

    // --- signing
    // `rect` is in normalised page coordinates, origin top-left.
    Q_INVOKABLE void signRectangle(int pageIndex, qreal x, qreal y, qreal width, qreal height);

    Q_INVOKABLE void openLastOutput() const;
    Q_INVOKABLE void revealLastOutput() const;

    bool busy() const { return m_busy; }

    bool darkMode() const { return m_darkMode; }
    void setDarkMode(bool darkMode);

    // Used by the preview item; not exposed to QML as a property because
    // AppearanceSpec is not a QML-friendly type.
    AppearanceSpec currentAppearanceSpec() const;

signals:
    void documentChanged();
    void currentPageChanged();
    void identityChanged();
    void appearanceChanged();
    void busyChanged();
    void darkModeChanged();
    void signedSuccessfully(const QString &outputPath);
    void failed(const QString &message);

private:
    void setBusy(bool busy);

    Identity m_identity;

    // Guards m_document: the image provider renders on a non-GUI thread and
    // poppler's Document is not reentrant.
    mutable QMutex m_documentMutex;
    std::unique_ptr<Poppler::Document> m_document;

    QString m_documentPath;
    QString m_documentName;
    int m_pageCount = 0;
    int m_currentPage = 0;
    int m_renderRevision = 0;
    int m_existingSignatureCount = 0;
    QList<QSizeF> m_pageSizes;

    bool m_busy = false;
    bool m_darkMode = true;
    QString m_lastOutputPath;
};
