#pragma once

#include "clipboard_watcher.h"

#include <QObject>

class KStatusNotifierItem;
class QAction;
class QMenu;
class PreferencesDialog;
class PortalPasteInjector;

class TrayApp : public QObject {
    Q_OBJECT
public:
    explicit TrayApp(ClipboardWatcher *watcher,
                     TrimCore *core,
                     PortalPasteInjector *injector = nullptr,
                     QObject *parent = nullptr);

private slots:
    void updateSummary(const QString &summary);
    void updateState();
    void updatePermissionState();

private:
    void updatePasteStats();
    void updatePreviews();
    void updateShortcuts();

    ClipboardWatcher *m_watcher = nullptr;
    TrimCore *m_core = nullptr;
    PortalPasteInjector *m_injector = nullptr;
    KStatusNotifierItem *m_item = nullptr;
    QMenu *m_menu = nullptr;
    QAction *m_permissionInfo = nullptr;
    QAction *m_permissionPermanent = nullptr;
    QAction *m_permissionSeparator = nullptr;
    QAction *m_pasteTrimmed = nullptr;
    QAction *m_trimmedPreview = nullptr;
    QAction *m_pasteOriginal = nullptr;
    QAction *m_originalPreview = nullptr;
    QAction *m_removedPreview = nullptr;
    QAction *m_restoreLast = nullptr;
    QAction *m_lastSummary = nullptr;
    QAction *m_autoTrimToggle = nullptr;
    QAction *m_about = nullptr;
    QAction *m_updateReady = nullptr;
    PreferencesDialog *m_prefs = nullptr;
};
