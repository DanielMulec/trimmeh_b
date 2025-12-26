#pragma once

#include "clipboard_watcher.h"

#include <QObject>

class KStatusNotifierItem;
class QAction;
class QMenu;

class TrayApp : public QObject {
    Q_OBJECT
public:
    explicit TrayApp(ClipboardWatcher *watcher, QObject *parent = nullptr);

private slots:
    void updateSummary(const QString &summary);
    void updateState();

private:
    ClipboardWatcher *m_watcher = nullptr;
    KStatusNotifierItem *m_item = nullptr;
    QMenu *m_menu = nullptr;
    QAction *m_pasteTrimmed = nullptr;
    QAction *m_pasteOriginal = nullptr;
    QAction *m_restoreLast = nullptr;
    QAction *m_lastSummary = nullptr;
    QAction *m_autoTrimToggle = nullptr;
    QAction *m_quit = nullptr;
};
