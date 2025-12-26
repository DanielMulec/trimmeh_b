#pragma once

#include "klipper_bridge.h"
#include "settings.h"
#include "trim_core.h"

#include <QObject>
#include <QTimer>

class ClipboardWatcher : public QObject {
    Q_OBJECT
public:
    ClipboardWatcher(KlipperBridge *bridge, TrimCore *core, const Settings &settings, QObject *parent = nullptr);

    bool autoTrimEnabled() const { return m_settings.autoTrimEnabled; }
    void setAutoTrimEnabled(bool enabled);

    QString lastSummary() const { return m_lastSummary; }
    QString lastOriginal() const { return m_lastOriginal; }
    QString lastTrimmed() const { return m_lastTrimmed; }
    bool hasLastOriginal() const { return !m_lastOriginal.isEmpty(); }

    bool pasteTrimmed();
    bool pasteOriginal();
    bool restoreLastCopy();

public slots:
    void onClipboardHistoryUpdated();

signals:
    void summaryChanged(const QString &summary);
    void stateChanged();

private slots:
    void onDebounceTimeout();

private:
    void process(quint64 genAtSchedule);
    void updateSummary(const QString &text);
    QString summarize(const QString &text) const;
    QString ellipsize(const QString &text, int limit) const;
    QString hashText(const QString &text) const;
    bool swapClipboardTemporarily(const QString &text, const QString &previous);

    KlipperBridge *m_bridge = nullptr;
    TrimCore *m_core = nullptr;
    Settings m_settings;
    QTimer m_debounce;
    quint64 m_gen = 0;
    quint64 m_pendingGen = 0;
    QString m_lastWrittenHash;
    QString m_lastOriginal;
    QString m_lastTrimmed;
    QString m_lastSummary;
    bool m_enabled = true;
};
