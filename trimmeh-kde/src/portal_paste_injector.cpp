#include "portal_paste_injector.h"

#include <QDBusConnectionInterface>
#include <QDBusObjectPath>
#include <QDBusReply>
#include <QDebug>
#include <QSettings>
#include <QUuid>

namespace {
constexpr const char kPortalService[] = "org.freedesktop.portal.Desktop";
constexpr const char kPortalPath[] = "/org/freedesktop/portal/desktop";
constexpr const char kRemoteDesktopIface[] = "org.freedesktop.portal.RemoteDesktop";
constexpr const char kRequestIface[] = "org.freedesktop.portal.Request";
constexpr const char kSessionIface[] = "org.freedesktop.portal.Session";

constexpr int kDeviceKeyboard = 1;
constexpr uint kPersistModePersistent = 2;

constexpr int kKeyLeftCtrl = 29;
constexpr int kKeyLeftShift = 42;
constexpr int kKeyV = 47;
constexpr int kKeyInsert = 110;
constexpr uint kKeyPressed = 1;
constexpr uint kKeyReleased = 0;

class PortalRequestWatcher : public QObject {
    Q_OBJECT
public:
    using Callback = std::function<void(uint, const QVariantMap &)>;

    PortalRequestWatcher(const QDBusConnection &bus,
                         const QString &path,
                         Callback callback,
                         QObject *parent = nullptr)
        : QObject(parent)
        , m_bus(bus)
        , m_path(path)
        , m_callback(std::move(callback))
    {
        m_connected = m_bus.connect(kPortalService,
                                    m_path,
                                    kRequestIface,
                                    QStringLiteral("Response"),
                                    this,
                                    SLOT(onResponse(uint,QVariantMap)));
    }

    bool isConnected() const { return m_connected; }

    void stop() {
        if (m_connected) {
            m_bus.disconnect(kPortalService,
                             m_path,
                             kRequestIface,
                             QStringLiteral("Response"),
                             this,
                             SLOT(onResponse(uint,QVariantMap)));
        }
        deleteLater();
    }

private slots:
    void onResponse(uint response, const QVariantMap &results) {
        if (m_connected) {
            m_bus.disconnect(kPortalService,
                             m_path,
                             kRequestIface,
                             QStringLiteral("Response"),
                             this,
                             SLOT(onResponse(uint,QVariantMap)));
        }
        if (m_callback) {
            m_callback(response, results);
        }
        deleteLater();
    }

private:
    QDBusConnection m_bus;
    QString m_path;
    Callback m_callback;
    bool m_connected = false;
};
}

PortalPasteInjector::PortalPasteInjector(QObject *parent)
    : QObject(parent)
    , m_bus(QDBusConnection::sessionBus())
    , m_iface(QString::fromLatin1(kPortalService),
              QString::fromLatin1(kPortalPath),
              QString::fromLatin1(kRemoteDesktopIface),
              m_bus)
{
    if (!m_bus.isConnected()) {
        updateState(State::Unavailable,
                    QStringLiteral("Failed to connect to session bus: %1").arg(m_bus.lastError().message()));
        return;
    }

    QDBusConnectionInterface *busIface = m_bus.interface();
    if (!busIface || !busIface->isServiceRegistered(QString::fromLatin1(kPortalService))) {
        updateState(State::Unavailable,
                    QStringLiteral("Portal service not available: %1").arg(QString::fromLatin1(kPortalService)));
        return;
    }

    if (!m_iface.isValid()) {
        updateState(State::Unavailable,
                    QStringLiteral("RemoteDesktop portal unavailable: %1").arg(m_iface.lastError().message()));
        return;
    }
}

void PortalPasteInjector::requestPermission() {
    if (m_state == State::Requesting || m_state == State::Ready) {
        return;
    }
    if (m_state == State::Unavailable) {
        return;
    }

    qInfo() << "[trimmeh-kde] portal permission request started";
    updateState(State::Requesting);
    clearSession();
    createSession();
}

PortalPasteInjector::PasteResult PortalPasteInjector::injectPaste() {
    if (m_state == State::Ready) {
        if (sendShiftInsert() || sendCtrlV()) {
            return PasteResult::Injected;
        }
        updateState(State::Error, QStringLiteral("Failed to inject paste via portal."));
        return PasteResult::Failed;
    }

    if (m_state == State::Unavailable) {
        return PasteResult::Unavailable;
    }

    if (m_state == State::Denied) {
        return PasteResult::PermissionRequired;
    }

    if (m_state == State::Idle || m_state == State::Error) {
        requestPermission();
    }

    return PasteResult::PermissionRequired;
}

void PortalPasteInjector::updateState(State state, const QString &error) {
    const bool changed = (m_state != state) || (m_lastError != error);
    m_state = state;
    m_lastError = error;
    if (changed) {
        if (m_state == State::Error && !m_lastError.isEmpty()) {
            qWarning().noquote() << "[trimmeh-kde] portal error:" << m_lastError;
        } else {
            qInfo() << "[trimmeh-kde] portal state:" << static_cast<int>(m_state);
        }
    }
    if (changed) {
        emit stateChanged();
    }
}

void PortalPasteInjector::clearSession() {
    if (!m_sessionHandle.isEmpty()) {
        QDBusInterface sessionIface(QString::fromLatin1(kPortalService),
                                    m_sessionHandle,
                                    QString::fromLatin1(kSessionIface),
                                    m_bus);
        sessionIface.call(QStringLiteral("Close"));
    }
    m_sessionHandle.clear();
}

void PortalPasteInjector::createSession() {
    const QString handleToken = makeToken(QStringLiteral("trimmeh"));
    const QString sessionToken = makeToken(QStringLiteral("trimmeh_session"));
    const QString expectedHandle = makeRequestPath(handleToken);

    qInfo().noquote() << "[trimmeh-kde] portal CreateSession handle" << expectedHandle;

    auto callback = [this](uint response, const QVariantMap &results) {
        handleCreateSessionResponse(response, results);
    };

    PortalRequestWatcher *watcher = new PortalRequestWatcher(m_bus, expectedHandle, callback, this);
    if (!watcher->isConnected()) {
        watcher->stop();
        updateState(State::Error, QStringLiteral("Failed to watch portal request response."));
        return;
    }

    QVariantMap options;
    options.insert(QStringLiteral("handle_token"), handleToken);
    options.insert(QStringLiteral("session_handle_token"), sessionToken);

    QDBusReply<QDBusObjectPath> reply = m_iface.call(QStringLiteral("CreateSession"), options);
    if (!reply.isValid()) {
        watcher->stop();
        updateState(State::Error,
                    QStringLiteral("CreateSession failed: %1").arg(reply.error().message()));
        return;
    }

    const QString actualHandle = reply.value().path();
    qInfo().noquote() << "[trimmeh-kde] portal CreateSession reply handle" << actualHandle;
    if (!actualHandle.isEmpty() && actualHandle != expectedHandle) {
        watcher->stop();
        watcher = new PortalRequestWatcher(m_bus, actualHandle, callback, this);
        if (!watcher->isConnected()) {
            watcher->stop();
            updateState(State::Error, QStringLiteral("Failed to watch portal request response."));
            return;
        }
    }
}

void PortalPasteInjector::selectDevices() {
    const QString handleToken = makeToken(QStringLiteral("trimmeh_select"));
    const QString expectedHandle = makeRequestPath(handleToken);

    qInfo().noquote() << "[trimmeh-kde] portal SelectDevices handle" << expectedHandle;

    auto callback = [this](uint response, const QVariantMap &results) {
        handleSelectDevicesResponse(response, results);
    };

    PortalRequestWatcher *watcher = new PortalRequestWatcher(m_bus, expectedHandle, callback, this);
    if (!watcher->isConnected()) {
        watcher->stop();
        updateState(State::Error, QStringLiteral("Failed to watch portal request response."));
        return;
    }

    QVariantMap options;
    options.insert(QStringLiteral("handle_token"), handleToken);
    options.insert(QStringLiteral("types"), static_cast<uint>(kDeviceKeyboard));
    options.insert(QStringLiteral("persist_mode"), kPersistModePersistent);
    const QString token = restoreToken();
    if (!token.isEmpty()) {
        options.insert(QStringLiteral("restore_token"), token);
    }

    QDBusReply<QDBusObjectPath> reply = m_iface.call(QStringLiteral("SelectDevices"),
                                                     QDBusObjectPath(m_sessionHandle),
                                                     options);
    if (!reply.isValid()) {
        watcher->stop();
        updateState(State::Error,
                    QStringLiteral("SelectDevices failed: %1").arg(reply.error().message()));
        return;
    }

    const QString actualHandle = reply.value().path();
    qInfo().noquote() << "[trimmeh-kde] portal SelectDevices reply handle" << actualHandle;
    if (!actualHandle.isEmpty() && actualHandle != expectedHandle) {
        watcher->stop();
        watcher = new PortalRequestWatcher(m_bus, actualHandle, callback, this);
        if (!watcher->isConnected()) {
            watcher->stop();
            updateState(State::Error, QStringLiteral("Failed to watch portal request response."));
            return;
        }
    }
}

void PortalPasteInjector::startSession() {
    const QString handleToken = makeToken(QStringLiteral("trimmeh_start"));
    const QString expectedHandle = makeRequestPath(handleToken);

    qInfo().noquote() << "[trimmeh-kde] portal Start handle" << expectedHandle;

    auto callback = [this](uint response, const QVariantMap &results) {
        handleStartResponse(response, results);
    };

    PortalRequestWatcher *watcher = new PortalRequestWatcher(m_bus, expectedHandle, callback, this);
    if (!watcher->isConnected()) {
        watcher->stop();
        updateState(State::Error, QStringLiteral("Failed to watch portal request response."));
        return;
    }

    QVariantMap options;
    options.insert(QStringLiteral("handle_token"), handleToken);

    const QString parentWindow;
    QDBusReply<QDBusObjectPath> reply = m_iface.call(QStringLiteral("Start"),
                                                     QDBusObjectPath(m_sessionHandle),
                                                     parentWindow,
                                                     options);
    if (!reply.isValid()) {
        watcher->stop();
        updateState(State::Error,
                    QStringLiteral("Start failed: %1").arg(reply.error().message()));
        return;
    }

    const QString actualHandle = reply.value().path();
    qInfo().noquote() << "[trimmeh-kde] portal Start reply handle" << actualHandle;
    if (!actualHandle.isEmpty() && actualHandle != expectedHandle) {
        watcher->stop();
        watcher = new PortalRequestWatcher(m_bus, actualHandle, callback, this);
        if (!watcher->isConnected()) {
            watcher->stop();
            updateState(State::Error, QStringLiteral("Failed to watch portal request response."));
            return;
        }
    }
}

void PortalPasteInjector::handleCreateSessionResponse(uint response, const QVariantMap &results) {
    qInfo().noquote() << "[trimmeh-kde] portal CreateSession response" << response;
    if (response != 0) {
        updateState(State::Denied, QStringLiteral("Portal session request denied."));
        return;
    }

    const QString sessionHandle = results.value(QStringLiteral("session_handle")).toString();
    if (sessionHandle.isEmpty()) {
        updateState(State::Error, QStringLiteral("Portal did not return a session handle."));
        return;
    }

    m_sessionHandle = sessionHandle;
    m_bus.connect(kPortalService,
                  m_sessionHandle,
                  kSessionIface,
                  QStringLiteral("Closed"),
                  this,
                  SLOT(onSessionClosed(QVariantMap)));

    selectDevices();
}

void PortalPasteInjector::handleSelectDevicesResponse(uint response, const QVariantMap &results) {
    qInfo().noquote() << "[trimmeh-kde] portal SelectDevices response" << response;
    if (response != 0) {
        updateState(State::Denied, QStringLiteral("Portal device selection denied."));
        return;
    }

    Q_UNUSED(results);
    startSession();
}

void PortalPasteInjector::handleStartResponse(uint response, const QVariantMap &results) {
    qInfo().noquote() << "[trimmeh-kde] portal Start response" << response;
    if (response != 0) {
        updateState(State::Denied, QStringLiteral("Portal start denied."));
        return;
    }

    const uint devices = results.value(QStringLiteral("devices")).toUInt();
    if ((devices & kDeviceKeyboard) == 0) {
        updateState(State::Denied, QStringLiteral("Keyboard permission not granted."));
        return;
    }

    const QString token = results.value(QStringLiteral("restore_token")).toString();
    if (!token.isEmpty()) {
        saveRestoreToken(token);
    }

    updateState(State::Ready);
}

void PortalPasteInjector::onSessionClosed(const QVariantMap &details) {
    Q_UNUSED(details);
    m_sessionHandle.clear();
    updateState(State::Idle);
}

bool PortalPasteInjector::sendKeycode(int keycode, uint state) {
    if (m_sessionHandle.isEmpty()) {
        return false;
    }
    QVariantMap options;
    QDBusReply<void> reply = m_iface.call(QStringLiteral("NotifyKeyboardKeycode"),
                                          QDBusObjectPath(m_sessionHandle),
                                          options,
                                          keycode,
                                          state);
    if (!reply.isValid()) {
        m_lastError = reply.error().message();
        qWarning().noquote() << "[trimmeh-kde] portal NotifyKeyboardKeycode failed:" << m_lastError;
        return false;
    }
    return true;
}

bool PortalPasteInjector::sendShiftInsert() {
    if (!sendKeycode(kKeyLeftShift, kKeyPressed)) {
        return false;
    }
    bool ok = sendKeycode(kKeyInsert, kKeyPressed);
    if (ok) {
        ok = sendKeycode(kKeyInsert, kKeyReleased);
    }
    sendKeycode(kKeyLeftShift, kKeyReleased);
    return ok;
}

bool PortalPasteInjector::sendCtrlV() {
    if (!sendKeycode(kKeyLeftCtrl, kKeyPressed)) {
        return false;
    }
    bool ok = sendKeycode(kKeyV, kKeyPressed);
    if (ok) {
        ok = sendKeycode(kKeyV, kKeyReleased);
    }
    sendKeycode(kKeyLeftCtrl, kKeyReleased);
    return ok;
}

QString PortalPasteInjector::makeToken(const QString &prefix) const {
    QString token = QUuid::createUuid().toString(QUuid::WithoutBraces);
    token.replace('-', '_');
    return QStringLiteral("%1_%2").arg(prefix, token);
}

QString PortalPasteInjector::makeRequestPath(const QString &token) const {
    QString sender = m_bus.baseService();
    sender.remove(':');
    sender.replace('.', '_');
    return QStringLiteral("/org/freedesktop/portal/desktop/request/%1/%2").arg(sender, token);
}

QString PortalPasteInjector::restoreToken() const {
    QSettings settings;
    return settings.value(QStringLiteral("portalRestoreToken")).toString();
}

void PortalPasteInjector::saveRestoreToken(const QString &token) const {
    QSettings settings;
    settings.setValue(QStringLiteral("portalRestoreToken"), token);
    settings.sync();
}

#include "portal_paste_injector.moc"
