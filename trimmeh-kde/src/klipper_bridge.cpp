#include "klipper_bridge.h"

#include <QDBusConnectionInterface>
#include <QDBusError>
#include <QDBusReply>

namespace {
constexpr const char kService[] = "org.kde.klipper";
constexpr const char kPath[] = "/klipper";
constexpr const char kInterface[] = "org.kde.klipper.klipper";
constexpr const char kSignal[] = "clipboardHistoryUpdated";
constexpr const char kMethodGet[] = "getClipboardContents";
constexpr const char kMethodSet[] = "setClipboardContents";
}

KlipperBridge::KlipperBridge()
    : m_bus(QDBusConnection::sessionBus()) {
}

bool KlipperBridge::init(QString *errorMessage) {
    if (!m_bus.isConnected()) {
        if (errorMessage) {
            *errorMessage = QStringLiteral("Failed to connect to session bus: %1")
                                .arg(m_bus.lastError().message());
        }
        return false;
    }

    QDBusConnectionInterface *busIface = m_bus.interface();
    if (!busIface) {
        if (errorMessage) {
            *errorMessage = QStringLiteral("No session bus interface available");
        }
        return false;
    }

    const QString serviceName = QString::fromLatin1(kService);
    if (!busIface->isServiceRegistered(serviceName)) {
        if (errorMessage) {
            *errorMessage = QStringLiteral("Klipper service not registered: %1").arg(serviceName);
        }
        return false;
    }

    m_iface = std::make_unique<QDBusInterface>(serviceName,
                                               QString::fromLatin1(kPath),
                                               QString::fromLatin1(kInterface),
                                               m_bus);

    if (!m_iface || !m_iface->isValid()) {
        if (errorMessage) {
            *errorMessage = QStringLiteral("Klipper interface invalid: %1")
                                .arg(m_iface ? m_iface->lastError().message() : QStringLiteral("unknown"));
        }
        return false;
    }

    m_ready = true;
    return true;
}

bool KlipperBridge::connectClipboardSignal(QObject *receiver, const char *slot, QString *errorMessage) {
    if (!m_ready) {
        if (errorMessage) {
            *errorMessage = QStringLiteral("Klipper bridge not initialized");
        }
        return false;
    }

    const bool connected = m_bus.connect(
        QString::fromLatin1(kService),
        QString::fromLatin1(kPath),
        QString::fromLatin1(kInterface),
        QString::fromLatin1(kSignal),
        receiver,
        slot);

    if (!connected && errorMessage) {
        *errorMessage = QStringLiteral("Failed to connect to Klipper signal: %1")
                            .arg(m_bus.lastError().message());
    }
    return connected;
}

QString KlipperBridge::getClipboardText(QString *errorMessage) {
    if (!m_ready || !m_iface) {
        if (errorMessage) {
            *errorMessage = QStringLiteral("Klipper bridge not initialized");
        }
        return QString();
    }

    QDBusReply<QString> reply = m_iface->call(QString::fromLatin1(kMethodGet));
    if (!reply.isValid()) {
        if (errorMessage) {
            *errorMessage = QStringLiteral("getClipboardContents failed: %1")
                                .arg(reply.error().message());
        }
        return QString();
    }
    return reply.value();
}

bool KlipperBridge::setClipboardText(const QString &text, QString *errorMessage) {
    if (!m_ready || !m_iface) {
        if (errorMessage) {
            *errorMessage = QStringLiteral("Klipper bridge not initialized");
        }
        return false;
    }

    QDBusReply<void> reply = m_iface->call(QString::fromLatin1(kMethodSet), text);
    if (!reply.isValid()) {
        if (errorMessage) {
            *errorMessage = QStringLiteral("setClipboardContents failed: %1")
                                .arg(reply.error().message());
        }
        return false;
    }
    return true;
}
