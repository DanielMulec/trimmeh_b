#pragma once

#include "klipper_bridge.h"
#include "settings.h"
#include "trim_core.h"

#include <QTimer>

class ClipboardWatcher : public QObject {
    Q_OBJECT
public:
    ClipboardWatcher(KlipperBridge *bridge, TrimCore *core, const Settings &settings, QObject *parent = nullptr);

public slots:
    void onClipboardHistoryUpdated();

private slots:
    void onDebounceTimeout();

private:
    void process(quint64 genAtSchedule);
    QString hashText(const QString &text) const;

    KlipperBridge *m_bridge = nullptr;
    TrimCore *m_core = nullptr;
    Settings m_settings;
    QTimer m_debounce;
    quint64 m_gen = 0;
    quint64 m_pendingGen = 0;
    QString m_lastWrittenHash;
    QString m_lastOriginal;
    QString m_lastTrimmed;
    bool m_enabled = true;
};
