#pragma once

#include <QColor>
#include <QString>
#include <QStringList>

#include "ink.h"

// Who you are when you sign, and what that should look like.
//
// Everything here lives in ~/.config/omasign/. The signing key itself does not:
// it lives in the shared NSS database at ~/.pki/nssdb, the same one Okular and
// Chrome use, so a certificate imported by any of them shows up in all of them.
class Identity {
public:
    struct Certificate {
        QString nickname;
        QString subject;
        QString issuer;
        bool selfSigned = false;
        bool expired = false;
    };

    Identity();

    void load();
    void save() const;

    bool isConfigured() const;

    // --- who
    QString name() const { return m_name; }
    void setName(const QString &name) { m_name = name; }
    QString email() const { return m_email; }
    void setEmail(const QString &email) { m_email = email; }

    // --- what it looks like
    bool useInk() const { return m_useInk; }
    void setUseInk(bool useInk) { m_useInk = useInk; }
    const Ink &ink() const { return m_ink; }
    bool setInk(const Ink &ink); // persists to signature.json
    bool showMeta() const { return m_showMeta; }
    void setShowMeta(bool showMeta) { m_showMeta = showMeta; }
    qreal splitRatio() const { return m_splitRatio; }
    QColor markColour() const { return m_markColour; }
    QColor metaColour() const { return m_metaColour; }
    QString watermarkPath() const { return m_watermarkPath; }
    qreal watermarkOpacity() const { return m_watermarkOpacity; }
    QString fontFamily() const { return m_fontFamily; }

    // --- what goes in the signature dictionary
    QString reason() const { return m_reason; }
    void setReason(const QString &reason) { m_reason = reason; }
    QString location() const { return m_location; }
    void setLocation(const QString &location) { m_location = location; }

    // --- the key
    QString nssDir() const { return m_nssDir; }        // e.g. "sql:/home/you/.pki/nssdb"
    void setNssDir(const QString &nssDir) { m_nssDir = nssDir; }
    QString certNickname() const { return m_certNickname; }
    void setCertNickname(const QString &nickname) { m_certNickname = nickname; }

    // Point poppler at our NSS database. Safe to call more than once, but
    // poppler only honours the first call per process, so do it early -- and it
    // creates the database first if need be, because handing poppler a
    // directory with no database in it wedges NSS for the life of the process.
    void applyNssDir() const;

    // Create the NSS database if it does not exist yet. Returns false only if
    // it could not be created.
    bool ensureNssDatabase(QString *error = nullptr) const;

    QList<Certificate> availableCertificates() const;

    // Create a self-signed certificate for `name`/`email` in the NSS database.
    // Returns the new nickname, or an empty string with `error` set.
    QString createSelfSignedCertificate(const QString &name, const QString &email, QString *error) const;

    static QString configDir();
    static QString configFilePath();
    static QString inkFilePath();

private:
    QString m_name;
    QString m_email;

    bool m_useInk = false;
    Ink m_ink;
    bool m_showMeta = true;
    qreal m_splitRatio = 0.52;
    QColor m_markColour = QColor(0x13, 0x13, 0x13);
    QColor m_metaColour = QColor(0x3a, 0x3a, 0x3a);
    QString m_watermarkPath;
    qreal m_watermarkOpacity = 0.25;
    QString m_fontFamily = QStringLiteral("Inter");

    QString m_reason;
    QString m_location;

    QString m_nssDir;
    QString m_certNickname;
};
