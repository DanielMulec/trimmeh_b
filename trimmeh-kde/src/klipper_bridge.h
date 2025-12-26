#pragma once

#include <QDBusConnection>
#include <QDBusInterface>
#include <QString>

#include <memory>

class KlipperBridge {
public:
    KlipperBridge();
    bool init(QString *errorMessage = nullptr);
    bool connectClipboardSignal(QObject *receiver, const char *slot, QString *errorMessage = nullptr);

    QString getClipboardText(QString *errorMessage = nullptr);
    bool setClipboardText(const QString &text, QString *errorMessage = nullptr);

private:
    QDBusConnection m_bus;
    std::unique_ptr<QDBusInterface> m_iface;
    bool m_ready = false;
};
