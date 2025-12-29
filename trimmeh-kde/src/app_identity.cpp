#include "app_identity.h"

#include <QCoreApplication>
#include <QDBusConnection>
#include <QDBusInterface>
#include <QDBusReply>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QSaveFile>
#include <QStandardPaths>

namespace {
constexpr const char kAppId[] = "dev.trimmeh.TrimmehKDE";
constexpr const char kDesktopFileName[] = "dev.trimmeh.TrimmehKDE.desktop";

QString quotedExecPath() {
    const QString path = QCoreApplication::applicationFilePath();
    if (path.contains(' ')) {
        QString escaped = path;
        escaped.replace('"', "\\\"");
        return QStringLiteral("\"%1\"").arg(escaped);
    }
    return path;
}
}

namespace AppIdentity {
QString appId() {
    return QString::fromLatin1(kAppId);
}

QString desktopFileName() {
    return QString::fromLatin1(kDesktopFileName);
}

QString desktopFilePath() {
    const QString appsDir = QStandardPaths::writableLocation(QStandardPaths::ApplicationsLocation);
    if (appsDir.isEmpty()) {
        return QString();
    }
    return QDir(appsDir).filePath(desktopFileName());
}

QString desktopFileContents() {
    return QStringLiteral(
        "[Desktop Entry]\n"
        "Type=Application\n"
        "Name=Trimmeh\n"
        "Comment=Clipboard auto-trim for terminal snippets\n"
        "Exec=%1\n"
        "Icon=edit-cut\n"
        "Terminal=false\n"
        "NoDisplay=true\n"
        "Categories=Utility;\n").arg(quotedExecPath());
}

QString preauthCommand() {
    return QStringLiteral("flatpak permission-set kde-authorized remote-desktop %1 yes").arg(appId());
}

bool ensureDesktopFile(QString *errorMessage) {
    const QString path = desktopFilePath();
    if (path.isEmpty()) {
        if (errorMessage) {
            *errorMessage = QStringLiteral("Unable to resolve applications directory.");
        }
        return false;
    }

    const QString existingPath = QStandardPaths::locate(QStandardPaths::ApplicationsLocation,
                                                        desktopFileName(),
                                                        QStandardPaths::LocateFile);
    if (!existingPath.isEmpty() && existingPath != path) {
        return true;
    }

    const QFileInfo info(path);
    QDir dir(info.absolutePath());
    if (!dir.exists() && !dir.mkpath(QStringLiteral("."))) {
        if (errorMessage) {
            *errorMessage = QStringLiteral("Failed to create applications directory: %1").arg(dir.path());
        }
        return false;
    }

    const QString contents = desktopFileContents();
    QFile existingFile(path);
    if (existingFile.exists()) {
        if (existingFile.open(QIODevice::ReadOnly)) {
            const QByteArray current = existingFile.readAll();
            if (current == contents.toUtf8()) {
                return true;
            }
        }
    }

    QSaveFile file(path);
    if (!file.open(QIODevice::WriteOnly | QIODevice::Truncate)) {
        if (errorMessage) {
            *errorMessage = QStringLiteral("Failed to open desktop entry for writing: %1").arg(path);
        }
        return false;
    }

    const QByteArray data = contents.toUtf8();
    if (file.write(data) != data.size()) {
        if (errorMessage) {
            *errorMessage = QStringLiteral("Failed to write desktop entry: %1").arg(path);
        }
        return false;
    }

    if (!file.commit()) {
        if (errorMessage) {
            *errorMessage = QStringLiteral("Failed to commit desktop entry: %1").arg(path);
        }
        return false;
    }

    return true;
}

bool registerWithPortal(QString *errorMessage) {
    constexpr const char kPortalService[] = "org.freedesktop.portal.Desktop";
    constexpr const char kPortalPath[] = "/org/freedesktop/portal/desktop";
    constexpr const char kRegistryIface[] = "org.freedesktop.host.portal.Registry";

    QDBusConnection bus = QDBusConnection::sessionBus();
    if (!bus.isConnected()) {
        if (errorMessage) {
            *errorMessage = QStringLiteral("Failed to connect to session bus: %1").arg(bus.lastError().message());
        }
        return false;
    }

    QDBusInterface iface(QString::fromLatin1(kPortalService),
                         QString::fromLatin1(kPortalPath),
                         QString::fromLatin1(kRegistryIface),
                         bus);
    if (!iface.isValid()) {
        if (errorMessage) {
            *errorMessage = QStringLiteral("Portal Registry unavailable: %1").arg(iface.lastError().message());
        }
        return false;
    }

    QVariantMap options;
    QDBusReply<void> reply = iface.call(QStringLiteral("Register"), appId(), options);
    if (!reply.isValid()) {
        const QString message = reply.error().message();
        if (message.contains(QStringLiteral("Connection already associated with an application ID"),
                             Qt::CaseInsensitive)) {
            return true;
        }
        if (errorMessage) {
            *errorMessage = QStringLiteral("Portal Registry register failed: %1").arg(message);
        }
        return false;
    }

    return true;
}
} // namespace AppIdentity
