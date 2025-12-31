#include "tray_app.h"

#include "portal_paste_injector.h"
#include "preferences_dialog.h"

#include <KStatusNotifierItem>
#include <QAction>
#include <QApplication>
#include <QFontDatabase>
#include <QHBoxLayout>
#include <QKeySequence>
#include <QLabel>
#include <QMenu>
#include <QVector>
#include <QWidget>
#include <QWidgetAction>

namespace {
constexpr int kMenuPreviewLimit = 100;
constexpr int kMenuPreviewWidth = 260;

QString displayString(const QString &text) {
    QString out = text;
    out.replace('\n', QStringLiteral("\u23CE "));
    out.replace('\t', QStringLiteral("\u21E5 "));
    return out;
}

QString displayStringWithVisibleWhitespace(const QString &text) {
    QString out;
    out.reserve(text.size());
    for (const QChar ch : text) {
        if (ch == QLatin1Char(' ')) {
            out.append(QChar(0x00B7));
        } else if (ch == QLatin1Char('\t')) {
            out.append(QChar(0x21E5));
        } else if (ch == QLatin1Char('\n')) {
            out.append(QChar(0x23CE));
        } else {
            out.append(ch);
        }
    }
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

struct StruckPreview {
    QString text;
    QVector<char> removed;
};

StruckPreview makeStruckPreview(const QString &original, const QString &trimmed) {
    StruckPreview result;
    const int oCount = original.size();
    const int tCount = trimmed.size();
    QVector<char> removed(oCount, 0);
    int i = 0;
    int j = 0;
    while (i < oCount && j < tCount) {
        if (original.at(i) == trimmed.at(j)) {
            i += 1;
            j += 1;
        } else {
            removed[i] = 1;
            i += 1;
        }
    }
    while (i < oCount) {
        removed[i] = 1;
        i += 1;
    }

    QString mapped;
    mapped.reserve(oCount);
    QVector<char> mappedRemoved;
    mappedRemoved.reserve(oCount);
    for (int idx = 0; idx < oCount; ++idx) {
        const QChar ch = original.at(idx);
        if (ch == QLatin1Char(' ')) {
            mapped.append(QChar(0x00B7));
        } else if (ch == QLatin1Char('\t')) {
            mapped.append(QChar(0x21E5));
        } else if (ch == QLatin1Char('\n')) {
            mapped.append(QChar(0x23CE));
        } else {
            mapped.append(ch);
        }
        mappedRemoved.append(removed[idx]);
    }

    result.text = mapped;
    result.removed = mappedRemoved;
    return result;
}

StruckPreview ellipsizeStruck(const StruckPreview &preview, int limit) {
    if (limit < 4 || preview.text.size() <= limit) {
        return preview;
    }
    const int keep = limit - 3;
    const int head = keep / 2;
    const int tail = keep - head;
    StruckPreview out;
    out.text = preview.text.left(head) + QStringLiteral("...") + preview.text.right(tail);
    out.removed = preview.removed.mid(0, head);
    out.removed.reserve(out.text.size());
    out.removed.append(0);
    out.removed.append(0);
    out.removed.append(0);
    out.removed.append(preview.removed.mid(preview.removed.size() - tail, tail));
    return out;
}

QString toStrikeHtml(const StruckPreview &preview) {
    QString html;
    html.reserve(preview.text.size() * 8);
    html.append(QStringLiteral("<span style=\"white-space: pre-wrap;\">"));
    const int count = preview.text.size();
    for (int idx = 0; idx < count; ++idx) {
        const QString chunk = QString(preview.text.at(idx)).toHtmlEscaped();
        if (idx < preview.removed.size() && preview.removed[idx]) {
            html.append(QStringLiteral("<span style=\"text-decoration:line-through;\">"));
            html.append(chunk);
            html.append(QStringLiteral("</span>"));
        } else {
            html.append(chunk);
        }
    }
    html.append(QStringLiteral("</span>"));
    return html;
}

QString toPreviewHtml(const QString &text) {
    const QString escaped = text.toHtmlEscaped();
    return QStringLiteral("<span style=\"white-space: pre-wrap;\">%1</span>").arg(escaped);
}

QWidgetAction *makePreviewAction(QMenu *menu, QLabel **labelOut) {
    auto *container = new QWidget(menu);
    auto *layout = new QHBoxLayout(container);
    layout->setContentsMargins(24, 0, 12, 0);

    auto *label = new QLabel(container);
    label->setWordWrap(true);
    label->setTextFormat(Qt::RichText);
    label->setTextInteractionFlags(Qt::NoTextInteraction);
    label->setAlignment(Qt::AlignLeft | Qt::AlignTop);
    label->setSizePolicy(QSizePolicy::Preferred, QSizePolicy::Minimum);
    QFont font = QFontDatabase::systemFont(QFontDatabase::FixedFont);
    if (font.pointSizeF() > 0) {
        font.setPointSizeF(font.pointSizeF() - 1);
    }
    label->setFont(font);
    label->setMinimumWidth(kMenuPreviewWidth);
    label->setMaximumWidth(kMenuPreviewWidth);
    label->setEnabled(false);

    layout->addWidget(label);
    container->setEnabled(false);

    auto *action = new QWidgetAction(menu);
    action->setDefaultWidget(container);
    if (labelOut) {
        *labelOut = label;
    }
    return action;
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
    m_trimmedPreviewAction = makePreviewAction(m_menu, &m_trimmedPreviewLabel);
    m_menu->addAction(m_trimmedPreviewAction);

    m_pasteOriginal = m_menu->addAction(QStringLiteral("Paste Original"));
    connect(m_pasteOriginal, &QAction::triggered, this, [this]() {
        if (m_watcher) {
            m_watcher->pasteOriginal();
        }
        updateState();
    });
    m_originalPreviewAction = makePreviewAction(m_menu, &m_originalPreviewLabel);
    m_menu->addAction(m_originalPreviewAction);

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

    if (m_trimmedPreviewLabel) {
        const QString source = !trimmed.isEmpty() ? trimmed : summary;
        const QString text = source.isEmpty()
            ? QStringLiteral("No trimmed text yet")
            : ellipsizeMiddle(displayString(source), kMenuPreviewLimit);
        m_trimmedPreviewLabel->setText(toPreviewHtml(text));
    }

    if (m_originalPreviewLabel) {
        QString html;
        if (!original.isEmpty()) {
            const QString trimmedSource = !trimmed.isEmpty() ? trimmed : original;
            StruckPreview preview = makeStruckPreview(original, trimmedSource);
            preview = ellipsizeStruck(preview, kMenuPreviewLimit);
            html = toStrikeHtml(preview);
        } else if (!summary.isEmpty()) {
            html = toPreviewHtml(ellipsizeMiddle(displayString(summary), kMenuPreviewLimit));
        } else {
            html = toPreviewHtml(QStringLiteral("No actions yet"));
        }
        m_originalPreviewLabel->setText(html);
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
