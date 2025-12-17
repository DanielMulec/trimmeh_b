#include "trimmeh_clipboard_watcher.h"

#include <QClipboard>
#include <QCryptographicHash>
#include <QDateTime>
#include <QGuiApplication>

namespace {
constexpr int kGraceDelayMs = 80;
constexpr int kManualRestoreDelayMs = 400;
constexpr int kRestoreGuardMs = 1500;
constexpr int kSummaryMaxChars = 70;

QString ellipsize(const QString& s, int maxChars) {
    if (s.size() <= maxChars) {
        return s;
    }
    return s.left(maxChars - 1) + QChar(0x2026);
}
} // namespace

TrimmehClipboardWatcher::TrimmehClipboardWatcher(TrimmehSettings* settings, TrimmehJsCore* core, QObject* parent)
    : QObject(parent),
      m_settings(settings),
      m_core(core),
      m_clipboard(QGuiApplication::clipboard()) {
    m_debounceTimer.setSingleShot(true);
    connect(&m_debounceTimer, &QTimer::timeout, this, &TrimmehClipboardWatcher::processScheduled);
}

void TrimmehClipboardWatcher::setEnabled(bool enabled) {
    if (m_enabled == enabled) {
        return;
    }
    m_enabled = enabled;

    if (m_enabled) {
        connect(m_clipboard, &QClipboard::changed, this, &TrimmehClipboardWatcher::onClipboardChanged, Qt::UniqueConnection);
    } else {
        disconnect(m_clipboard, &QClipboard::changed, this, &TrimmehClipboardWatcher::onClipboardChanged);
        m_debounceTimer.stop();
    }
}

bool TrimmehClipboardWatcher::hasLastOriginal() const {
    return !m_lastOriginal.isEmpty();
}

void TrimmehClipboardWatcher::onClipboardChanged(QClipboard::Mode mode) {
    if (mode != QClipboard::Clipboard) {
        return;
    }

    if (!m_enabled) {
        return;
    }

    m_generation += 1;
    m_scheduledGeneration = m_generation;

    scheduleProcessing();
}

void TrimmehClipboardWatcher::scheduleProcessing() {
    m_debounceTimer.start(kGraceDelayMs);
}

void TrimmehClipboardWatcher::processScheduled() {
    if (!m_enabled) {
        return;
    }

    const quint64 expectedGen = m_scheduledGeneration;
    if (expectedGen != m_generation) {
        return;
    }

    const QString text = m_clipboard->text(QClipboard::Clipboard);
    if (text.isEmpty()) {
        return;
    }

    const qint64 nowMs = QDateTime::currentMSecsSinceEpoch();
    const QString incomingHash = hashText(text);

    if (nowMs < m_restoreGuardUntilMs && !m_restoreGuardHash.isEmpty() && incomingHash == m_restoreGuardHash) {
        return;
    }

    if (!m_lastWrittenHash.isEmpty() && incomingHash == m_lastWrittenHash) {
        m_lastWrittenHash.clear();
        return;
    }

    if (!m_settings->enableAutoTrim()) {
        return;
    }

    const auto res = m_core->trim(text, m_settings->aggressiveness(), m_settings->trimOptions());
    if (!res.changed) {
        return;
    }

    if (!m_enabled || expectedGen != m_generation) {
        return;
    }

    m_lastOriginal = text;
    m_lastTrimmed = res.output;
    setLastSummaryFromText(res.output);

    setClipboardTextGuarded(res.output);
}

void TrimmehClipboardWatcher::restoreLastCopy() {
    if (!hasLastOriginal()) {
        return;
    }

    const qint64 nowMs = QDateTime::currentMSecsSinceEpoch();
    m_restoreGuardHash = hashText(m_lastOriginal);
    m_restoreGuardUntilMs = nowMs + kRestoreGuardMs;
    setClipboardTextGuarded(m_lastOriginal);
    setLastSummaryFromText(m_lastOriginal);
}

void TrimmehClipboardWatcher::pasteTrimmedOnce() {
    const QString current = m_clipboard->text(QClipboard::Clipboard);
    if (current.isEmpty()) {
        return;
    }

    const auto res = m_core->trim(current, QStringLiteral("high"), m_settings->trimOptions());
    const QString target = res.changed ? res.output : current;

    const QString previous = current;
    setClipboardTextGuarded(target);
    setLastSummaryFromText(target);

    QTimer::singleShot(kManualRestoreDelayMs, this, [this, previous]() {
        const qint64 nowMs = QDateTime::currentMSecsSinceEpoch();
        m_restoreGuardHash = hashText(previous);
        m_restoreGuardUntilMs = nowMs + kRestoreGuardMs;
        setClipboardTextGuarded(previous);
    });
}

void TrimmehClipboardWatcher::pasteOriginalOnce() {
    if (!hasLastOriginal()) {
        return;
    }

    const QString previous = m_clipboard->text(QClipboard::Clipboard);
    setClipboardTextGuarded(m_lastOriginal);
    setLastSummaryFromText(m_lastOriginal);

    QTimer::singleShot(kManualRestoreDelayMs, this, [this, previous]() {
        const qint64 nowMs = QDateTime::currentMSecsSinceEpoch();
        m_restoreGuardHash = hashText(previous);
        m_restoreGuardUntilMs = nowMs + kRestoreGuardMs;
        setClipboardTextGuarded(previous);
    });
}

void TrimmehClipboardWatcher::setClipboardTextGuarded(const QString& text) {
    m_lastWrittenHash = hashText(text);
    m_clipboard->setText(text, QClipboard::Clipboard);
}

void TrimmehClipboardWatcher::setLastSummaryFromText(const QString& text) {
    const QString compact = ellipsize(text.simplified(), kSummaryMaxChars);
    emit lastSummaryChanged(compact.isEmpty() ? QStringLiteral("—") : compact);
}

QString TrimmehClipboardWatcher::hashText(const QString& text) const {
    const QByteArray digest = QCryptographicHash::hash(text.toUtf8(), QCryptographicHash::Sha256);
    return QString::fromLatin1(digest.toHex());
}
