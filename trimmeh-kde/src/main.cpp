#include <QCoreApplication>
#include <QCommandLineParser>
#include <QDateTime>
#include <QDBusConnection>
#include <QDBusConnectionInterface>
#include <QDBusError>
#include <QDBusInterface>
#include <QDBusReply>
#include <QDebug>
#include <QIODevice>
#include <QTextStream>

namespace {
constexpr const char kService[] = "org.kde.klipper";
constexpr const char kPath[] = "/klipper";
constexpr const char kInterface[] = "org.kde.klipper.klipper";
constexpr const char kSignal[] = "clipboardHistoryUpdated";
constexpr const char kMethodGet[] = "getClipboardContents";
constexpr const char kMethodSet[] = "setClipboardContents";
}

class KlipperProbe final : public QObject {
    Q_OBJECT
public:
    explicit KlipperProbe(QObject *parent = nullptr)
        : QObject(parent)
        , m_iface(QString::fromLatin1(kService),
                  QString::fromLatin1(kPath),
                  QString::fromLatin1(kInterface),
                  QDBusConnection::sessionBus())
    {
    }

    bool isInterfaceValid() const {
        return m_iface.isValid();
    }

    QDBusError lastError() const {
        return m_iface.lastError();
    }

    bool setClipboard(const QString &text) {
        QDBusReply<void> reply = m_iface.call(QString::fromLatin1(kMethodSet), text);
        if (!reply.isValid()) {
            qWarning() << "[klipper] setClipboardContents failed:" << reply.error().name()
                       << reply.error().message();
            return false;
        }
        return true;
    }

    void printClipboard(const char *reason) {
        QDBusReply<QString> reply = m_iface.call(QString::fromLatin1(kMethodGet));
        if (!reply.isValid()) {
            qWarning() << "[klipper] getClipboardContents failed:" << reply.error().name()
                       << reply.error().message();
            return;
        }
        const QString text = reply.value();
        const QString ts = QDateTime::currentDateTime().toString(Qt::ISODate);
        const QString tag = reason ? QString::fromLatin1(reason) : QStringLiteral("clipboard");
        qInfo().noquote() << ts << "-" << tag << "-BEGIN";
        qInfo().noquote() << text;
        qInfo().noquote() << ts << "-" << tag << "-END";
    }

public slots:
    void onClipboardHistoryUpdated() {
        printClipboard("signal");
    }

private:
    QDBusInterface m_iface;
};

int main(int argc, char **argv) {
    QCoreApplication app(argc, argv);
    QCoreApplication::setApplicationName("trimmeh-kde-probe");
    QCoreApplication::setApplicationVersion("0.0.1");

    QCommandLineParser parser;
    parser.setApplicationDescription("Klipper D-Bus probe for Trimmeh KDE");
    parser.addHelpOption();
    parser.addVersionOption();

    QCommandLineOption onceOpt("once", "Print clipboard once and exit.");
    parser.addOption(onceOpt);

    QCommandLineOption noInitialOpt("no-initial", "Do not print initial clipboard.");
    parser.addOption(noInitialOpt);

    QCommandLineOption setOpt("set", "Set clipboard via Klipper and exit.", "text");
    parser.addOption(setOpt);

    QCommandLineOption setStdinOpt("set-stdin", "Read stdin and set clipboard via Klipper, then exit.");
    parser.addOption(setStdinOpt);

    parser.process(app);

    if (parser.isSet(setOpt) && parser.isSet(setStdinOpt)) {
        qCritical() << "[klipper] Use only one of --set or --set-stdin.";
        return 1;
    }

    QDBusConnection bus = QDBusConnection::sessionBus();
    if (!bus.isConnected()) {
        qCritical() << "[klipper] Failed to connect to session bus:" << bus.lastError().message();
        return 2;
    }

    QDBusConnectionInterface *busIface = bus.interface();
    if (!busIface) {
        qCritical() << "[klipper] No session bus interface available.";
        return 2;
    }

    const QString serviceName = QString::fromLatin1(kService);
    if (!busIface->isServiceRegistered(serviceName)) {
        qCritical() << "[klipper] Service not registered:" << serviceName;
        return 3;
    }

    KlipperProbe probe;
    if (!probe.isInterfaceValid()) {
        const QDBusError err = probe.lastError();
        qCritical() << "[klipper] Interface invalid:" << err.name() << err.message();
        return 4;
    }

    if (parser.isSet(setOpt) || parser.isSet(setStdinOpt)) {
        QString text;
        if (parser.isSet(setOpt)) {
            text = parser.value(setOpt);
        } else {
            QTextStream in(stdin, QIODevice::ReadOnly);
            text = in.readAll();
        }

        if (!probe.setClipboard(text)) {
            return 6;
        }
        probe.printClipboard("after-set");
        return 0;
    }

    if (!parser.isSet(noInitialOpt)) {
        probe.printClipboard("initial");
    }

    if (parser.isSet(onceOpt)) {
        return 0;
    }

    const bool connected = bus.connect(
        serviceName,
        QString::fromLatin1(kPath),
        QString::fromLatin1(kInterface),
        QString::fromLatin1(kSignal),
        &probe,
        SLOT(onClipboardHistoryUpdated()));

    if (!connected) {
        qCritical() << "[klipper] Failed to connect to signal" << kSignal << ":"
                    << bus.lastError().message();
        return 5;
    }

    qInfo() << "[klipper] Listening for clipboardHistoryUpdated...";
    return app.exec();
}

#include "main.moc"
