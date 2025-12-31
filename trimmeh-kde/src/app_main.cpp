#include "app_identity.h"
#include "autostart_manager.h"
#include "clipboard_watcher.h"
#include "hotkey_manager.h"
#include "klipper_bridge.h"
#include "portal_paste_injector.h"
#include "settings.h"
#include "settings_store.h"
#include "tray_app.h"
#include "trim_core.h"

#include <QApplication>
#include <QCommandLineParser>
#include <QDebug>
#include <QDir>
#include <QFileInfo>
#include <QGuiApplication>
#include <QStandardPaths>
#include <QSettings>

namespace {
QString coreBundlePath() {
    const QString appDir = QApplication::applicationDirPath();
    const QString local = QDir(appDir).filePath(QStringLiteral("trimmeh-core.js"));
    if (QFileInfo::exists(local)) {
        return local;
    }

    const QStringList libDirs = QStandardPaths::standardLocations(QStandardPaths::LibraryLocation);
    for (const QString &dir : libDirs) {
        const QString candidate = QDir(dir).filePath(QStringLiteral("trimmeh/trimmeh-core.js"));
        if (QFileInfo::exists(candidate)) {
            return candidate;
        }
    }

    return local;
}
}

int main(int argc, char **argv) {
    QGuiApplication::setDesktopFileName(AppIdentity::appId());
    QApplication app(argc, argv);
    QCoreApplication::setOrganizationName("Trimmeh");
    QCoreApplication::setOrganizationDomain("trimmeh.dev");
    QApplication::setApplicationName("trimmeh-kde");
    QApplication::setApplicationVersion("0.0.1");
    QApplication::setQuitOnLastWindowClosed(false);

    QCommandLineParser parser;
    parser.setApplicationDescription("Trimmeh KDE (Klipper D-Bus auto-trim)");
    parser.addHelpOption();
    parser.addVersionOption();
    parser.process(app);

    QString identityError;
    if (!AppIdentity::ensureDesktopFile(&identityError)) {
        qWarning().noquote() << "[trimmeh-kde]" << identityError;
    }
    identityError.clear();
    if (!AppIdentity::registerWithPortal(&identityError)) {
        qInfo().noquote() << "[trimmeh-kde]" << identityError;
    }

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

    SettingsStore store;
    AutostartManager autostart;
    Settings settings = store.load();
    QSettings raw;
    if (raw.contains(QStringLiteral("startAtLogin"))) {
        QString error;
        if (!autostart.setEnabled(settings.startAtLogin, &error)) {
            qWarning().noquote() << "[trimmeh-kde]" << error;
        }
        settings.startAtLogin = autostart.isEnabled();
    } else {
        settings.startAtLogin = autostart.isEnabled();
    }
    store.save(settings);

    PortalPasteInjector injector;
    ClipboardWatcher watcher(&bridge, &core, settings, &store, &autostart, &injector);
    if (!bridge.connectClipboardSignal(&watcher, SLOT(onClipboardHistoryUpdated()), &error)) {
        qCritical().noquote() << "[trimmeh-kde]" << error;
        return 5;
    }

    HotkeyManager hotkeys(&watcher);
    TrayApp tray(&watcher, &core, &injector);

    qInfo() << "[trimmeh-kde] Listening for clipboardHistoryUpdated...";
    return app.exec();
}
