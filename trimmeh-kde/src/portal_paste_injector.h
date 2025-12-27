#pragma once

#include <QObject>
#include <QDBusConnection>
#include <QDBusInterface>
#include <QVariantMap>

class PortalPasteInjector : public QObject {
    Q_OBJECT
public:
    enum class State {
        Idle,
        Requesting,
        Ready,
        Denied,
        Unavailable,
        Error,
    };

    enum class PasteResult {
        Injected,
        PermissionRequired,
        Unavailable,
        Failed,
    };

    explicit PortalPasteInjector(QObject *parent = nullptr);

    State state() const { return m_state; }
    bool isReady() const { return m_state == State::Ready; }
    bool isAvailable() const { return m_state != State::Unavailable; }
    bool isRequesting() const { return m_state == State::Requesting; }
    QString lastError() const { return m_lastError; }

    void requestPermission();
    PasteResult injectPaste();

signals:
    void stateChanged();

private slots:
    void onSessionClosed(const QVariantMap &details);

private:
    void updateState(State state, const QString &error = QString());
    void clearSession();
    void createSession();
    void selectDevices();
    void startSession();

    void handleCreateSessionResponse(uint response, const QVariantMap &results);
    void handleSelectDevicesResponse(uint response, const QVariantMap &results);
    void handleStartResponse(uint response, const QVariantMap &results);

    bool sendKeycode(int keycode, uint state);
    bool sendShiftInsert();
    bool sendCtrlV();

    QString makeToken(const QString &prefix) const;
    QString makeRequestPath(const QString &token) const;
    QString restoreToken() const;
    void saveRestoreToken(const QString &token) const;

    QDBusConnection m_bus;
    QDBusInterface m_iface;
    QString m_sessionHandle;
    State m_state = State::Idle;
    QString m_lastError;
};
