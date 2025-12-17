#pragma once

#include "trimmeh_js_core.h"
#include "trimmeh_settings.h"

#include <QObject>
#include <QClipboard>
#include <QTimer>

class TrimmehClipboardWatcher final : public QObject {
    Q_OBJECT

public:
    explicit TrimmehClipboardWatcher(TrimmehSettings* settings, TrimmehJsCore* core, QObject* parent = nullptr);

    void setEnabled(bool enabled);
    bool hasLastOriginal() const;

public slots:
    void restoreLastCopy();
    void pasteTrimmedOnce();
    void pasteOriginalOnce();

signals:
    void lastSummaryChanged(const QString& summary);

private:
    void onClipboardChanged(QClipboard::Mode mode);
    void scheduleProcessing();
    void processScheduled();

    void setClipboardTextGuarded(const QString& text);
    void setLastSummaryFromText(const QString& text);
    QString hashText(const QString& text) const;

    TrimmehSettings* m_settings = nullptr;
    TrimmehJsCore* m_core = nullptr;
    QClipboard* m_clipboard = nullptr;

    bool m_enabled = false;
    quint64 m_generation = 0;
    quint64 m_scheduledGeneration = 0;
    QTimer m_debounceTimer;

    QString m_lastWrittenHash;
    qint64 m_restoreGuardUntilMs = 0;
    QString m_restoreGuardHash;

    QString m_lastOriginal;
    QString m_lastTrimmed;
};
