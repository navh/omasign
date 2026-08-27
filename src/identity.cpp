#include "identity.h"

#include <QDateTime>
#include <QDir>
#include <QFile>
#include <QProcess>
#include <QRandomGenerator>
#include <QSettings>
#include <QStandardPaths>
#include <QTemporaryFile>

#include <poppler-form.h>
#include <poppler-qt6.h>

namespace {

QString defaultNssDir()
{
    // The shared per-user NSS database. Chrome, Evolution and Okular all use
    // it, so a certificate imported with `pk12util -d sql:$HOME/.pki/nssdb`
    // becomes available to Omasign with no further configuration.
    return QStringLiteral("sql:%1/.pki/nssdb").arg(QDir::homePath());
}

QString nssDirPath(const QString &nssDir)
{
    QString path = nssDir;
    if (path.startsWith(QLatin1String("sql:")))
        path.remove(0, 4);
    else if (path.startsWith(QLatin1String("dbm:")))
        path.remove(0, 4);
    return path;
}

// Run `certutil` and return true on success, capturing anything it complained
// about. certutil is part of nss-tools, which we declare as a runtime dependency.
bool runCertutil(const QStringList &arguments, QString *error)
{
    QProcess process;
    process.start(QStringLiteral("certutil"), arguments);
    if (!process.waitForStarted(5000)) {
        if (error)
            *error = QStringLiteral("certutil was not found. Install the nss-tools package.");
        return false;
    }
    // Key generation on a slow machine can take a while; it is not interactive.
    if (!process.waitForFinished(120000)) {
        process.kill();
        if (error)
            *error = QStringLiteral("certutil timed out.");
        return false;
    }
    if (process.exitStatus() != QProcess::NormalExit || process.exitCode() != 0) {
        if (error) {
            const QString stderrText = QString::fromUtf8(process.readAllStandardError()).trimmed();
            *error = stderrText.isEmpty()
                ? QStringLiteral("certutil failed with exit code %1.").arg(process.exitCode())
                : stderrText;
        }
        return false;
    }
    return true;
}

} // namespace

Identity::Identity()
{
    m_nssDir = defaultNssDir();
}

QString Identity::configDir()
{
    // AppConfigLocation already includes the application name.
    return QStandardPaths::writableLocation(QStandardPaths::AppConfigLocation);
}

QString Identity::configFilePath()
{
    return configDir() + QStringLiteral("/omasign.conf");
}

QString Identity::inkFilePath()
{
    return configDir() + QStringLiteral("/signature.json");
}

void Identity::load()
{
    QSettings settings(configFilePath(), QSettings::IniFormat);

    m_name = settings.value(QStringLiteral("identity/name")).toString();
    m_email = settings.value(QStringLiteral("identity/email")).toString();
    m_certNickname = settings.value(QStringLiteral("identity/certNickname")).toString();
    m_nssDir = settings.value(QStringLiteral("identity/nssDir"), defaultNssDir()).toString();

    m_useInk = settings.value(QStringLiteral("appearance/useInk"), false).toBool();
    m_showMeta = settings.value(QStringLiteral("appearance/showMeta"), true).toBool();
    m_splitRatio = settings.value(QStringLiteral("appearance/splitRatio"), 0.52).toDouble();
    m_markColour = QColor(settings.value(QStringLiteral("appearance/markColour"),
                                         QStringLiteral("#131313")).toString());
    m_metaColour = QColor(settings.value(QStringLiteral("appearance/metaColour"),
                                         QStringLiteral("#3a3a3a")).toString());
    m_watermarkPath = settings.value(QStringLiteral("appearance/watermark")).toString();
    m_watermarkOpacity = settings.value(QStringLiteral("appearance/watermarkOpacity"), 0.25).toDouble();
    m_fontFamily = settings.value(QStringLiteral("appearance/fontFamily"),
                                  QStringLiteral("Inter")).toString();

    m_reason = settings.value(QStringLiteral("signing/reason")).toString();
    m_location = settings.value(QStringLiteral("signing/location")).toString();

    if (!m_markColour.isValid())
        m_markColour = QColor(0x13, 0x13, 0x13);
    if (!m_metaColour.isValid())
        m_metaColour = QColor(0x3a, 0x3a, 0x3a);
    m_splitRatio = qBound(0.25, m_splitRatio, 0.85);
    m_watermarkOpacity = qBound(0.0, m_watermarkOpacity, 1.0);

    Ink ink;
    if (ink.loadFromFile(inkFilePath()))
        m_ink = ink;
    else
        m_useInk = false; // nothing drawn yet, so fall back to typed
}

void Identity::save() const
{
    QDir().mkpath(configDir());
    QSettings settings(configFilePath(), QSettings::IniFormat);

    settings.setValue(QStringLiteral("identity/name"), m_name);
    settings.setValue(QStringLiteral("identity/email"), m_email);
    settings.setValue(QStringLiteral("identity/certNickname"), m_certNickname);
    settings.setValue(QStringLiteral("identity/nssDir"), m_nssDir);

    settings.setValue(QStringLiteral("appearance/useInk"), m_useInk);
    settings.setValue(QStringLiteral("appearance/showMeta"), m_showMeta);
    settings.setValue(QStringLiteral("appearance/splitRatio"), m_splitRatio);
    settings.setValue(QStringLiteral("appearance/markColour"), m_markColour.name());
    settings.setValue(QStringLiteral("appearance/metaColour"), m_metaColour.name());
    settings.setValue(QStringLiteral("appearance/watermark"), m_watermarkPath);
    settings.setValue(QStringLiteral("appearance/watermarkOpacity"), m_watermarkOpacity);
    settings.setValue(QStringLiteral("appearance/fontFamily"), m_fontFamily);

    settings.setValue(QStringLiteral("signing/reason"), m_reason);
    settings.setValue(QStringLiteral("signing/location"), m_location);
    settings.sync();
}

bool Identity::isConfigured() const
{
    return !m_name.isEmpty() && !m_certNickname.isEmpty();
}

bool Identity::setInk(const Ink &ink)
{
    QDir().mkpath(configDir());
    if (!ink.saveToFile(inkFilePath()))
        return false;
    m_ink = ink;
    return true;
}

bool Identity::ensureNssDatabase(QString *error) const
{
    const QString dbPath = nssDirPath(m_nssDir);
    if (dbPath.isEmpty()) {
        if (error)
            *error = QStringLiteral("No NSS database configured.");
        return false;
    }
    if (QFile::exists(dbPath + QStringLiteral("/cert9.db"))
        || QFile::exists(dbPath + QStringLiteral("/cert8.db")))
        return true;

    QDir().mkpath(dbPath);

    // certutil prompts for a password unless handed an (empty) password file,
    // and would otherwise block forever behind the GUI.
    QTemporaryFile passwordFile;
    if (!passwordFile.open()) {
        if (error)
            *error = QStringLiteral("Could not create a temporary file.");
        return false;
    }
    passwordFile.close();

    return runCertutil({QStringLiteral("-N"), QStringLiteral("-d"), m_nssDir,
                        QStringLiteral("-f"), passwordFile.fileName()}, error);
}

void Identity::applyNssDir() const
{
    // Order matters. If poppler initialises NSS against a directory that has no
    // database in it, NSS fails and stays failed: certificates created later in
    // the same run are invisible, which is exactly what first-run setup does.
    ensureNssDatabase();
    Poppler::setNSSDir(m_nssDir);
}

QList<Identity::Certificate> Identity::availableCertificates() const
{
    QList<Certificate> result;
    const QDateTime now = QDateTime::currentDateTime();
    const auto certs = Poppler::getAvailableSigningCertificates();
    for (const Poppler::CertificateInfo &info : certs) {
        Certificate cert;
        cert.nickname = info.nickName();
        cert.subject = info.subjectInfo(Poppler::CertificateInfo::DistinguishedName);
        cert.issuer = info.issuerInfo(Poppler::CertificateInfo::DistinguishedName);
        cert.selfSigned = info.isSelfSigned();
        cert.expired = info.validityEnd().isValid() && info.validityEnd() < now;
        result.append(cert);
    }
    return result;
}

QString Identity::createSelfSignedCertificate(const QString &name, const QString &email,
                                              QString *error) const
{
    if (!ensureNssDatabase(error))
        return QString();

    QTemporaryFile passwordFile;
    if (!passwordFile.open()) {
        if (error)
            *error = QStringLiteral("Could not create a temporary file.");
        return QString();
    }
    passwordFile.close();

    // certutil reads its entropy file to EOF, so it must be a finite file --
    // handing it /dev/urandom hangs the process.
    QTemporaryFile noiseFile;
    if (!noiseFile.open()) {
        if (error)
            *error = QStringLiteral("Could not create a temporary file.");
        return QString();
    }
    QByteArray noise(4096, Qt::Uninitialized);
    QRandomGenerator::system()->fillRange(reinterpret_cast<quint32 *>(noise.data()),
                                          noise.size() / int(sizeof(quint32)));
    noiseFile.write(noise);
    noiseFile.close();

    // Keep the nickname stable and recognisable, but never collide with a
    // certificate that is already in the database.
    QString nickname = QStringLiteral("Omasign - %1").arg(name);
    {
        QStringList taken;
        for (const Certificate &cert : availableCertificates())
            taken << cert.nickname;
        int suffix = 2;
        const QString base = nickname;
        while (taken.contains(nickname))
            nickname = QStringLiteral("%1 (%2)").arg(base).arg(suffix++);
    }

    QString subject = QStringLiteral("CN=%1").arg(name);
    if (!email.isEmpty())
        subject += QStringLiteral(",E=%1").arg(email);

    const quint32 serial = QRandomGenerator::system()->bounded(1u, 0x7fffffffu);

    const QStringList arguments {
        QStringLiteral("-S"),
        QStringLiteral("-d"), m_nssDir,
        QStringLiteral("-f"), passwordFile.fileName(),
        QStringLiteral("-n"), nickname,
        QStringLiteral("-s"), subject,
        QStringLiteral("-x"),                                  // self-signed
        QStringLiteral("-t"), QStringLiteral("CT,C,C"),         // trusted locally
        QStringLiteral("-k"), QStringLiteral("rsa"),
        QStringLiteral("-g"), QStringLiteral("3072"),
        QStringLiteral("-Z"), QStringLiteral("SHA256"),
        QStringLiteral("-v"), QStringLiteral("120"),            // months
        QStringLiteral("-m"), QString::number(serial),
        // nonRepudiation is the key usage verifiers look for on a document
        // signing certificate; emailProtection is the EKU Acrobat accepts.
        QStringLiteral("--keyUsage"), QStringLiteral("digitalSignature,nonRepudiation"),
        QStringLiteral("--extKeyUsage"), QStringLiteral("emailProtection"),
        QStringLiteral("-z"), noiseFile.fileName(),
    };
    if (!runCertutil(arguments, error))
        return QString();

    return nickname;
}
