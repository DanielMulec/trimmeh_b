#include "hotkey_manager.h"

#include "clipboard_watcher.h"

#include <KGlobalAccel>
#include <QAction>
#include <QKeySequence>

namespace {
QKeySequence toSequence(const QString &text) {
    if (text.isEmpty()) {
        return QKeySequence();
    }
    return QKeySequence::fromString(text, QKeySequence::PortableText);
}
}

HotkeyManager::HotkeyManager(ClipboardWatcher *watcher, QObject *parent)
    : QObject(parent)
    , m_watcher(watcher)
{
    m_pasteTrimmed = new QAction(QStringLiteral("Paste Trimmed"), this);
    m_pasteTrimmed->setObjectName(QStringLiteral("paste_trimmed"));
    connect(m_pasteTrimmed, &QAction::triggered, this, [this]() {
        if (m_watcher) {
            m_watcher->pasteTrimmed();
        }
    });

    m_pasteOriginal = new QAction(QStringLiteral("Paste Original"), this);
    m_pasteOriginal->setObjectName(QStringLiteral("paste_original"));
    connect(m_pasteOriginal, &QAction::triggered, this, [this]() {
        if (m_watcher) {
            m_watcher->pasteOriginal();
        }
    });

    m_toggleAutoTrim = new QAction(QStringLiteral("Toggle Auto-Trim"), this);
    m_toggleAutoTrim->setObjectName(QStringLiteral("toggle_auto_trim"));
    connect(m_toggleAutoTrim, &QAction::triggered, this, [this]() {
        if (m_watcher) {
            m_watcher->setAutoTrimEnabled(!m_watcher->autoTrimEnabled());
        }
    });

    if (m_watcher) {
        connect(m_watcher, &ClipboardWatcher::stateChanged, this, &HotkeyManager::syncFromWatcher);
    }
    syncFromWatcher();
}

void HotkeyManager::syncFromWatcher() {
    if (!m_watcher) {
        return;
    }
    applyAction(m_pasteTrimmed,
                m_watcher->pasteTrimmedHotkeyEnabled(),
                m_watcher->pasteTrimmedHotkey());
    applyAction(m_pasteOriginal,
                m_watcher->pasteOriginalHotkeyEnabled(),
                m_watcher->pasteOriginalHotkey());
    applyAction(m_toggleAutoTrim,
                m_watcher->toggleAutoTrimHotkeyEnabled(),
                m_watcher->toggleAutoTrimHotkey());
}

void HotkeyManager::applyAction(QAction *action, bool enabled, const QString &sequence) {
    if (!action) {
        return;
    }
    const QKeySequence key = toSequence(sequence);
    QList<QKeySequence> keys;
    if (enabled && !key.isEmpty()) {
        keys << key;
    }
    action->setEnabled(enabled && !key.isEmpty());
    KGlobalAccel::self()->setShortcut(action, keys, KGlobalAccel::NoAutoloading);
}
