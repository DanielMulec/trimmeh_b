#pragma once

#include "clipboard_watcher.h"

#include <QObject>

class KStatusNotifierItem;
class QAction;
class QMenu;
class QWidgetAction;
class QLabel;
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
    QWidgetAction *m_trimmedPreviewAction = nullptr;
    QLabel *m_trimmedPreviewLabel = nullptr;
    QAction *m_pasteOriginal = nullptr;
    QWidgetAction *m_originalPreviewAction = nullptr;
    QLabel *m_originalPreviewLabel = nullptr;
    QAction *m_restoreLast = nullptr;
    QAction *m_lastSummary = nullptr;
    QAction *m_autoTrimToggle = nullptr;
    QAction *m_about = nullptr;
    QAction *m_updateReady = nullptr;
    QAction *m_quit = nullptr;
    PreferencesDialog *m_prefs = nullptr;
};
