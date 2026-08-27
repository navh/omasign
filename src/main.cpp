#include <QFile>
#include <QFontDatabase>
#include <QGuiApplication>
#include <QIcon>
#include <QQmlApplicationEngine>
#include <QQmlContext>
#include <QQmlError>
#include <QQuickStyle>
#include <QUrl>

#include "appearanceitem.h"
#include "backend.h"
#include "pageimageprovider.h"
#include "signaturepad.h"
#include "systemtheme.h"

int main(int argc, char *argv[])
{
    QGuiApplication app(argc, argv);
    app.setApplicationName(QStringLiteral("omasign"));
    app.setApplicationDisplayName(QStringLiteral("Omasign"));
    app.setDesktopFileName(QStringLiteral("omasign"));
    app.setWindowIcon(QIcon::fromTheme(QStringLiteral("omasign")));

    // Inter is bundled so that a signature looks the same on every machine,
    // whatever fonts happen to be installed there.
    QFontDatabase::addApplicationFont(QStringLiteral(":/fonts/Inter-Regular.ttf"));
    QFontDatabase::addApplicationFont(QStringLiteral(":/fonts/Inter-Medium.ttf"));
    QFontDatabase::addApplicationFont(QStringLiteral(":/fonts/Inter-SemiBold.ttf"));

    QQuickStyle::setStyle(QStringLiteral("Basic"));

    Backend backend(&app);
    SystemTheme systemTheme(&app);
    backend.setDarkMode(systemTheme.darkMode());
    QObject::connect(&systemTheme, &SystemTheme::darkModeChanged, &backend, &Backend::setDarkMode);

    qmlRegisterSingletonType(QUrl(QStringLiteral("qrc:/Theme.qml")), "Omasign", 1, 0, "Theme");
    qmlRegisterType<AppearanceItem>("Omasign", 1, 0, "SignatureAppearance");
    qmlRegisterType<SignaturePad>("Omasign", 1, 0, "SignaturePad");

    QQmlApplicationEngine engine;
    QObject::connect(&engine, &QQmlApplicationEngine::warnings, &app,
                     [](const QList<QQmlError> &warnings) {
        for (const QQmlError &warning : warnings)
            qWarning().noquote() << warning.toString();
    });
    engine.addImageProvider(QStringLiteral("page"), new PageImageProvider(&backend));
    engine.rootContext()->setContextProperty(QStringLiteral("backend"), &backend);

    engine.load(QUrl(QStringLiteral("qrc:/Main.qml")));
    if (engine.rootObjects().isEmpty()) {
        qCritical() << "Could not load the Omasign interface; resource available:"
                    << QFile::exists(QStringLiteral(":/Main.qml"));
        return -1;
    }

    const QStringList arguments = app.arguments();
    if (arguments.size() > 1)
        backend.open(QUrl::fromLocalFile(arguments.at(1)));

    return app.exec();
}
