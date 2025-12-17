#pragma once

#include "trimmeh_clipboard_watcher.h"
#include "trimmeh_js_core.h"
#include "trimmeh_settings.h"

#include <KStatusNotifierItem>
#include <QObject>

#include <QActionGroup>
#include <QMenu>

class TrimmehApp final : public QObject {
    Q_OBJECT

public:
    explicit TrimmehApp(QObject* parent = nullptr);

private:
    void rebuildMenu();
    void syncMenuStateFromSettings();
    void applyAggressivenessFromAction(const QAction* action);
    void updateLastSummary(const QString& summary);

    TrimmehSettings m_settings;
    TrimmehJsCore m_core;
    TrimmehClipboardWatcher m_watcher;

    KStatusNotifierItem* m_tray = nullptr;
    QMenu* m_menu = nullptr;
    QAction* m_lastSummaryAction = nullptr;
    QAction* m_autoTrimAction = nullptr;
    QActionGroup* m_aggressivenessGroup = nullptr;
    QAction* m_keepBlankLinesAction = nullptr;
    QAction* m_stripPromptsAction = nullptr;
    QAction* m_stripBoxCharsAction = nullptr;
    QAction* m_restoreLastCopyAction = nullptr;
    QAction* m_pasteTrimmedAction = nullptr;
    QAction* m_pasteOriginalAction = nullptr;
};

