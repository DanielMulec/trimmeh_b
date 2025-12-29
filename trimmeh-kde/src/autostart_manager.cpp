#include "autostart_manager.h"

#include "app_identity.h"

#include <QCoreApplication>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QSaveFile>
#include <QStandardPaths>

namespace {
constexpr const char kLegacyDesktopFileName[] = "trimmeh-kde.desktop";
}

bool AutostartManager::isEnabled() const {
    return QFile::exists(desktopFilePath()) || QFile::exists(legacyDesktopFilePath());
}

bool AutostartManager::setEnabled(bool enabled, QString *errorMessage) const {
    const QString path = desktopFilePath();
    const QString legacyPath = legacyDesktopFilePath();
    if (path.isEmpty()) {
        if (errorMessage) {
            *errorMessage = QStringLiteral("Unable to resolve autostart path.");
        }
        return false;
    }

    if (!enabled) {
        if (QFile::exists(path) && !QFile::remove(path)) {
            if (errorMessage) {
                *errorMessage = QStringLiteral("Failed to remove autostart entry: %1").arg(path);
            }
            return false;
        }
        if (QFile::exists(legacyPath)) {
            QFile::remove(legacyPath);
        }
        return true;
    }

    const QFileInfo info(path);
    QDir dir(info.absolutePath());
    if (!dir.exists() && !dir.mkpath(QStringLiteral("."))) {
        if (errorMessage) {
            *errorMessage = QStringLiteral("Failed to create autostart directory: %1").arg(dir.path());
        }
        return false;
    }

    QSaveFile file(path);
    if (!file.open(QIODevice::WriteOnly | QIODevice::Truncate)) {
        if (errorMessage) {
            *errorMessage = QStringLiteral("Failed to open autostart entry for writing: %1").arg(path);
        }
        return false;
    }

    const QByteArray contents = desktopFileContents().toUtf8();
    if (file.write(contents) != contents.size()) {
        if (errorMessage) {
            *errorMessage = QStringLiteral("Failed to write autostart entry: %1").arg(path);
        }
        return false;
    }

    if (!file.commit()) {
        if (errorMessage) {
            *errorMessage = QStringLiteral("Failed to commit autostart entry: %1").arg(path);
        }
        return false;
    }

    if (QFile::exists(legacyPath)) {
        QFile::remove(legacyPath);
    }

    return true;
}

QString AutostartManager::desktopFilePath() const {
    const QString configDir = QStandardPaths::writableLocation(QStandardPaths::ConfigLocation);
    if (configDir.isEmpty()) {
        return QString();
    }
    return QDir(configDir).filePath(QStringLiteral("autostart/%1").arg(AppIdentity::desktopFileName()));
}

QString AutostartManager::desktopFileContents() const {
    return QStringLiteral(
        "[Desktop Entry]\n"
        "Type=Application\n"
        "Name=Trimmeh\n"
        "Comment=Clipboard auto-trim for terminal snippets\n"
        "Exec=%1\n"
        "Icon=edit-cut\n"
        "StartupNotify=false\n").arg(quotedExecPath());
}

QString AutostartManager::legacyDesktopFilePath() const {
    const QString configDir = QStandardPaths::writableLocation(QStandardPaths::ConfigLocation);
    if (configDir.isEmpty()) {
        return QString();
    }
    return QDir(configDir).filePath(QStringLiteral("autostart/%1").arg(QString::fromLatin1(kLegacyDesktopFileName)));
}

QString AutostartManager::quotedExecPath() const {
    const QString path = QCoreApplication::applicationFilePath();
    if (path.contains(' ')) {
        QString escaped = path;
        escaped.replace('"', "\\\"");
        return QStringLiteral("\"%1\"").arg(escaped);
    }
    return path;
}
