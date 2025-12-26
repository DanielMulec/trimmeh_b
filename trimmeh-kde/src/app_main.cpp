#include "clipboard_watcher.h"
#include "klipper_bridge.h"
#include "settings.h"
#include "trim_core.h"

#include <QCoreApplication>
#include <QCommandLineParser>
#include <QDebug>
#include <QDir>
#include <QFileInfo>

namespace {
QString coreBundlePath() {
    const QString appDir = QCoreApplication::applicationDirPath();
    return QDir(appDir).filePath(QStringLiteral("trimmeh-core.js"));
}
}

int main(int argc, char **argv) {
    QCoreApplication app(argc, argv);
    QCoreApplication::setApplicationName("trimmeh-kde");
    QCoreApplication::setApplicationVersion("0.0.1");

    QCommandLineParser parser;
    parser.setApplicationDescription("Trimmeh KDE (Klipper D-Bus auto-trim)");
    parser.addHelpOption();
    parser.addVersionOption();
    parser.process(app);

    const QString corePath = coreBundlePath();
    if (!QFileInfo::exists(corePath)) {
        qCritical().noquote() << "[trimmeh-kde] Missing JS bundle:" << corePath;
        qCritical() << "[trimmeh-kde] Run the build step that bundles trimmeh-core-js.";
        return 2;
    }

    TrimCore core;
    QString error;
    if (!core.load(corePath, &error)) {
        qCritical().noquote() << "[trimmeh-kde]" << error;
        return 3;
    }

    KlipperBridge bridge;
    if (!bridge.init(&error)) {
        qCritical().noquote() << "[trimmeh-kde]" << error;
        return 4;
    }

    Settings settings;

    ClipboardWatcher watcher(&bridge, &core, settings);
    if (!bridge.connectClipboardSignal(&watcher, SLOT(onClipboardHistoryUpdated()), &error)) {
        qCritical().noquote() << "[trimmeh-kde]" << error;
        return 5;
    }

    qInfo() << "[trimmeh-kde] Listening for clipboardHistoryUpdated...";
    return app.exec();
}
