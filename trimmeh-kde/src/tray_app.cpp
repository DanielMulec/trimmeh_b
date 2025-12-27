#include "tray_app.h"

#include "portal_paste_injector.h"
#include "preferences_dialog.h"

#include <KStatusNotifierItem>
#include <QAction>
#include <QApplication>
#include <QMenu>

TrayApp::TrayApp(ClipboardWatcher *watcher,
                 TrimCore *core,
                 PortalPasteInjector *injector,
                 QObject *parent)
    : QObject(parent)
    , m_watcher(watcher)
    , m_core(core)
    , m_injector(injector)
{
    m_item = new KStatusNotifierItem(QStringLiteral("trimmeh-kde"), this);
    m_item->setCategory(KStatusNotifierItem::ApplicationStatus);
    m_item->setStatus(KStatusNotifierItem::Active);
    m_item->setTitle(QStringLiteral("Trimmeh"));
    m_item->setIconByName(QStringLiteral("edit-cut"));

    m_menu = new QMenu();

    if (m_injector) {
        m_permissionInfo = m_menu->addAction(QStringLiteral("Input permission needed to paste"));
        m_permissionInfo->setEnabled(false);
        m_permissionGrant = m_menu->addAction(QStringLiteral("Grant Permission"));
        connect(m_permissionGrant, &QAction::triggered, this, [this]() {
            if (m_injector) {
                m_injector->requestPermission();
            }
        });
        m_permissionSeparator = m_menu->addSeparator();
    }

    m_pasteTrimmed = m_menu->addAction(QStringLiteral("Paste Trimmed (High)"));
    connect(m_pasteTrimmed, &QAction::triggered, this, [this]() {
        if (m_watcher) {
            m_watcher->pasteTrimmed();
        }
        updateState();
    });

    m_pasteOriginal = m_menu->addAction(QStringLiteral("Paste Original"));
    connect(m_pasteOriginal, &QAction::triggered, this, [this]() {
        if (m_watcher) {
            m_watcher->pasteOriginal();
        }
        updateState();
    });

    m_restoreLast = m_menu->addAction(QStringLiteral("Restore last copy"));
    connect(m_restoreLast, &QAction::triggered, this, [this]() {
        if (m_watcher) {
            m_watcher->restoreLastCopy();
        }
        updateState();
    });

    m_lastSummary = m_menu->addAction(QStringLiteral("Last: No actions yet"));
    m_lastSummary->setEnabled(false);

    m_menu->addSeparator();

    m_autoTrimToggle = m_menu->addAction(QStringLiteral("Auto-Trim"));
    m_autoTrimToggle->setCheckable(true);
    if (m_watcher) {
        m_autoTrimToggle->setChecked(m_watcher->autoTrimEnabled());
    }
    connect(m_autoTrimToggle, &QAction::toggled, this, [this](bool enabled) {
        if (m_watcher) {
            m_watcher->setAutoTrimEnabled(enabled);
        }
        updateState();
    });

    m_menu->addSeparator();

    auto *settings = m_menu->addAction(QStringLiteral("Settings..."));
    connect(settings, &QAction::triggered, this, [this]() {
        if (!m_prefs) {
            m_prefs = new PreferencesDialog(m_watcher, m_core, m_injector);
        }
        m_prefs->show();
        m_prefs->raise();
        m_prefs->activateWindow();
    });

    m_quit = m_menu->addAction(QStringLiteral("Quit"));
    connect(m_quit, &QAction::triggered, qApp, &QCoreApplication::quit);

    m_item->setContextMenu(m_menu);

    if (m_watcher) {
        updateSummary(m_watcher->lastSummary());
        connect(m_watcher, &ClipboardWatcher::summaryChanged, this, &TrayApp::updateSummary);
        connect(m_watcher, &ClipboardWatcher::stateChanged, this, &TrayApp::updateState);
    }
    if (m_injector) {
        connect(m_injector, &PortalPasteInjector::stateChanged, this, &TrayApp::updatePermissionState);
        updatePermissionState();
    }

    updateState();
}

void TrayApp::updateSummary(const QString &summary) {
    if (!m_lastSummary) {
        return;
    }
    const QString text = summary.isEmpty()
        ? QStringLiteral("Last: No actions yet")
        : QStringLiteral("Last: %1").arg(summary);
    m_lastSummary->setText(text);
}

void TrayApp::updateState() {
    if (!m_watcher) {
        return;
    }
    if (m_autoTrimToggle) {
        const bool enabled = m_watcher->autoTrimEnabled();
        if (m_autoTrimToggle->isChecked() != enabled) {
            m_autoTrimToggle->setChecked(enabled);
        }
    }

    if (m_restoreLast) {
        m_restoreLast->setEnabled(m_watcher->hasLastOriginal());
    }
}

void TrayApp::updatePermissionState() {
    if (!m_injector || !m_permissionInfo || !m_permissionGrant || !m_permissionSeparator) {
        return;
    }

    if (!m_injector->isAvailable()) {
        m_permissionInfo->setText(QStringLiteral("Paste permission portal unavailable"));
        m_permissionInfo->setVisible(true);
        m_permissionGrant->setVisible(false);
        m_permissionSeparator->setVisible(true);
        return;
    }

    if (m_injector->isReady()) {
        m_permissionInfo->setVisible(false);
        m_permissionGrant->setVisible(false);
        m_permissionSeparator->setVisible(false);
        return;
    }

    if (m_injector->isRequesting()) {
        m_permissionInfo->setText(QStringLiteral("Waiting for permission..."));
        m_permissionInfo->setVisible(true);
        m_permissionGrant->setVisible(true);
        m_permissionGrant->setEnabled(false);
        m_permissionSeparator->setVisible(true);
        return;
    }

    if (m_injector->state() == PortalPasteInjector::State::Error && !m_injector->lastError().isEmpty()) {
        m_permissionInfo->setText(QStringLiteral("Portal error: %1").arg(m_injector->lastError()));
        m_permissionInfo->setVisible(true);
        m_permissionGrant->setVisible(true);
        m_permissionGrant->setEnabled(true);
        m_permissionSeparator->setVisible(true);
        return;
    }

    m_permissionInfo->setText(QStringLiteral("Input permission needed to paste"));
    m_permissionInfo->setVisible(true);
    m_permissionGrant->setVisible(true);
    m_permissionGrant->setEnabled(true);
    m_permissionSeparator->setVisible(true);
}
