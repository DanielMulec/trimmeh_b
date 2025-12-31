#include "tray_app.h"

#include "portal_paste_injector.h"
#include "preferences_dialog.h"

#include <KStatusNotifierItem>
#include <QAction>
#include <QApplication>
#include <QKeySequence>
#include <QMenu>

namespace {
constexpr int kMenuPreviewLimit = 100;

QString displayString(const QString &text) {
    QString out = text;
    out.replace('\n', QStringLiteral("\u23CE "));
    out.replace('\t', QStringLiteral("\u21E5 "));
    return out;
}

QString ellipsizeMiddle(const QString &text, int limit) {
    if (limit < 4 || text.size() <= limit) {
        return text;
    }
    const int keep = limit - 3;
    const int head = keep / 2;
    const int tail = keep - head;
    return text.left(head) + QStringLiteral("...") + text.right(tail);
}


int truncationCount(int count, int limit) {
    if (count <= limit || limit <= 0) {
        return 0;
    }
    return (count + limit - 1) / limit - 1;
}

QString kString(int count) {
    const double k = static_cast<double>(count) / 1000.0;
    return k >= 10.0
        ? QStringLiteral("%1k").arg(QString::number(k, 'f', 0))
        : QStringLiteral("%1k").arg(QString::number(k, 'f', 1));
}

QString prettyBadge(int count, int limit, bool showTruncations) {
    const QString chars = count >= 1000
        ? QStringLiteral("%1 chars").arg(kString(count))
        : QStringLiteral("%1 chars").arg(count);

    if (!showTruncations || limit <= 0) {
        return QStringLiteral(" \u00b7 %1").arg(chars);
    }

    const int truncations = truncationCount(count, limit);
    if (truncations <= 0) {
        return QStringLiteral(" \u00b7 %1").arg(chars);
    }

    return QStringLiteral(" \u00b7 %1 \u00b7 %2 trimmed").arg(chars).arg(truncations);
}
}

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
        m_permissionInfo = m_menu->addAction(QStringLiteral("Enable hotkeys to allow paste shortcuts"));
        m_permissionInfo->setEnabled(false);
        m_permissionPermanent = m_menu->addAction(QStringLiteral("Enable Hotkeys Permanently"));
        connect(m_permissionPermanent, &QAction::triggered, this, [this]() {
            if (m_injector) {
                m_injector->requestPreauthorization();
            }
        });
        m_permissionSeparator = m_menu->addSeparator();
    }

    m_pasteTrimmed = m_menu->addAction(QStringLiteral("Paste Trimmed"));
    connect(m_pasteTrimmed, &QAction::triggered, this, [this]() {
        if (m_watcher) {
            m_watcher->pasteTrimmed();
        }
        updateState();
    });
    m_trimmedPreview = m_menu->addAction(QString());
    m_trimmedPreview->setEnabled(false);

    m_pasteOriginal = m_menu->addAction(QStringLiteral("Paste Original"));
    connect(m_pasteOriginal, &QAction::triggered, this, [this]() {
        if (m_watcher) {
            m_watcher->pasteOriginal();
        }
        updateState();
    });
    m_originalPreview = m_menu->addAction(QString());
    m_originalPreview->setEnabled(false);
    m_removedPreview = m_menu->addAction(QString());
    m_removedPreview->setEnabled(false);
    m_removedPreview->setVisible(false);

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

    m_about = m_menu->addAction(QStringLiteral("About Trimmeh"));
    connect(m_about, &QAction::triggered, this, [this]() {
        if (!m_prefs) {
            m_prefs = new PreferencesDialog(m_watcher, m_core, m_injector);
        }
        m_prefs->showAboutTab();
    });

    m_updateReady = m_menu->addAction(QStringLiteral("Update ready, restart now?"));
    m_updateReady->setVisible(false);

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
        connect(m_injector, &PortalPasteInjector::preauthStateChanged, this, &TrayApp::updatePermissionState);
        connect(m_injector, &PortalPasteInjector::preauthStatusChanged, this, &TrayApp::updatePermissionState);
        updatePermissionState();
    }

    updateState();
}

void TrayApp::updateSummary(const QString &summary) {
    if (!m_lastSummary) {
        return;
    }
    const QString formatted = displayString(summary);
    const QString text = formatted.isEmpty()
        ? QStringLiteral("Last: No actions yet")
        : QStringLiteral("Last: %1").arg(formatted);
    m_lastSummary->setText(text);
    updatePreviews();
    updatePasteStats();
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
    updatePreviews();
    updateShortcuts();
    updatePasteStats();
}

void TrayApp::updatePasteStats() {
    if (!m_watcher || !m_pasteTrimmed || !m_pasteOriginal) {
        return;
    }

    const QString original = m_watcher->lastOriginal();
    const QString trimmed = m_watcher->lastTrimmed();
    const int originalLen = original.size();
    const int trimmedLen = trimmed.size();

    QString trimmedSuffix;
    if (!trimmed.isEmpty()) {
        trimmedSuffix = prettyBadge(trimmedLen, kMenuPreviewLimit, true);
        if (!original.isEmpty() && originalLen > trimmedLen) {
            trimmedSuffix += QStringLiteral(" \u00b7 %1 trimmed").arg(originalLen - trimmedLen);
        }
    }

    QString originalSuffix;
    if (!original.isEmpty()) {
        originalSuffix = prettyBadge(originalLen, kMenuPreviewLimit, false);
    }

    m_pasteTrimmed->setText(QStringLiteral("Paste Trimmed%1").arg(trimmedSuffix));
    m_pasteOriginal->setText(QStringLiteral("Paste Original%1").arg(originalSuffix));
}

void TrayApp::updatePreviews() {
    if (!m_watcher) {
        return;
    }

    const QString summary = m_watcher->lastSummary();
    const QString trimmed = m_watcher->lastTrimmed();
    const QString original = m_watcher->lastOriginal();

    if (m_trimmedPreview) {
        const QString source = !trimmed.isEmpty() ? trimmed : summary;
        const QString text = source.isEmpty()
            ? QStringLiteral("Preview: No trimmed text yet")
            : QStringLiteral("Preview: %1").arg(ellipsizeMiddle(displayString(source), kMenuPreviewLimit));
        m_trimmedPreview->setText(text);
    }

    if (m_originalPreview) {
        QString text;
        if (!original.isEmpty()) {
            text = QStringLiteral("Original: %1").arg(ellipsizeMiddle(displayString(original), kMenuPreviewLimit));
        } else if (!summary.isEmpty()) {
            text = QStringLiteral("Original: %1").arg(ellipsizeMiddle(displayString(summary), kMenuPreviewLimit));
        } else {
            text = QStringLiteral("Original: No actions yet");
        }
        m_originalPreview->setText(text);
    }

    if (m_removedPreview) {
        if (!original.isEmpty() && !trimmed.isEmpty() && original.size() > trimmed.size()) {
            m_removedPreview->setText(QStringLiteral("Removed: %1 chars").arg(original.size() - trimmed.size()));
            m_removedPreview->setVisible(true);
        } else {
            m_removedPreview->setVisible(false);
        }
    }
}

void TrayApp::updateShortcuts() {
    if (!m_watcher) {
        return;
    }
    if (m_pasteTrimmed) {
        if (m_watcher->pasteTrimmedHotkeyEnabled() && !m_watcher->pasteTrimmedHotkey().isEmpty()) {
            m_pasteTrimmed->setShortcut(QKeySequence::fromString(
                m_watcher->pasteTrimmedHotkey(),
                QKeySequence::PortableText));
        } else {
            m_pasteTrimmed->setShortcut(QKeySequence());
        }
    }
    if (m_pasteOriginal) {
        if (m_watcher->pasteOriginalHotkeyEnabled() && !m_watcher->pasteOriginalHotkey().isEmpty()) {
            m_pasteOriginal->setShortcut(QKeySequence::fromString(
                m_watcher->pasteOriginalHotkey(),
                QKeySequence::PortableText));
        } else {
            m_pasteOriginal->setShortcut(QKeySequence());
        }
    }
}

void TrayApp::updatePermissionState() {
    if (!m_injector || !m_permissionInfo || !m_permissionPermanent || !m_permissionSeparator) {
        return;
    }

    if (!m_injector->isAvailable()) {
        m_permissionInfo->setText(QStringLiteral("Hotkey permission portal unavailable. Paste manually (Ctrl+V)."));
        m_permissionInfo->setVisible(true);
        m_permissionPermanent->setVisible(false);
        m_permissionSeparator->setVisible(true);
        return;
    }

    if (m_injector->isReady()) {
        m_permissionInfo->setVisible(false);
        m_permissionPermanent->setVisible(false);
        m_permissionSeparator->setVisible(false);
        return;
    }

    const bool canPreauth = m_injector->canPreauthorize();
    const auto preauthStatus = m_injector->preauthStatus();

    if (preauthStatus == PortalPasteInjector::PreauthStatus::Present) {
        if (m_injector->isReady()) {
            m_permissionInfo->setVisible(false);
            m_permissionPermanent->setVisible(false);
            m_permissionSeparator->setVisible(false);
            return;
        }
        m_permissionInfo->setText(QStringLiteral("Hotkeys enabled permanently. Paste will work when used."));
        m_permissionInfo->setVisible(true);
        m_permissionPermanent->setVisible(false);
        m_permissionSeparator->setVisible(true);
        return;
    }

    if (m_injector->isRequesting()) {
        m_permissionInfo->setText(QStringLiteral("Waiting for hotkey permission..."));
        m_permissionInfo->setVisible(true);
        m_permissionPermanent->setVisible(true);
        m_permissionPermanent->setEnabled(canPreauth
                                          && m_injector->preauthState()
                                              != PortalPasteInjector::PreauthState::Working);
        m_permissionSeparator->setVisible(true);
        return;
    }

    if (m_injector->state() == PortalPasteInjector::State::Error && !m_injector->lastError().isEmpty()) {
        m_permissionInfo->setText(QStringLiteral("Hotkey permission error: %1").arg(m_injector->lastError()));
        m_permissionInfo->setVisible(true);
        m_permissionPermanent->setVisible(true);
        m_permissionPermanent->setEnabled(canPreauth
                                          && m_injector->preauthState()
                                              != PortalPasteInjector::PreauthState::Working);
        m_permissionSeparator->setVisible(true);
        return;
    }

    if (m_injector->state() == PortalPasteInjector::State::Denied) {
        m_permissionInfo->setText(QStringLiteral("Permission was denied. Use a paste action, then paste manually (Ctrl+V)."));
        m_permissionInfo->setVisible(true);
        m_permissionPermanent->setVisible(true);
        m_permissionPermanent->setEnabled(canPreauth
                                          && m_injector->preauthState()
                                              != PortalPasteInjector::PreauthState::Working);
        m_permissionSeparator->setVisible(true);
        return;
    }

    if (!canPreauth) {
        m_permissionInfo->setText(QStringLiteral("Hotkeys need a one-time system prompt. Use a paste action once to grant permission."));
    } else {
        m_permissionInfo->setText(QStringLiteral("Enable hotkeys to allow paste shortcuts"));
    }
    m_permissionInfo->setVisible(true);
    m_permissionPermanent->setVisible(true);
    m_permissionPermanent->setEnabled(canPreauth
                                      && m_injector->preauthState()
                                          != PortalPasteInjector::PreauthState::Working);
    m_permissionSeparator->setVisible(true);
}
