#include "clipboard_watcher.h"

#include <QCryptographicHash>
#include <QDebug>

ClipboardWatcher::ClipboardWatcher(KlipperBridge *bridge, TrimCore *core, const Settings &settings, QObject *parent)
    : QObject(parent)
    , m_bridge(bridge)
    , m_core(core)
    , m_settings(settings)
{
    m_debounce.setSingleShot(true);
    m_debounce.setInterval(m_settings.graceDelayMs);
    connect(&m_debounce, &QTimer::timeout, this, &ClipboardWatcher::onDebounceTimeout);
}

void ClipboardWatcher::onClipboardHistoryUpdated() {
    if (!m_enabled || !m_bridge || !m_core) {
        return;
    }

    m_gen += 1;
    m_pendingGen = m_gen;
    m_debounce.start();
}

void ClipboardWatcher::onDebounceTimeout() {
    process(m_pendingGen);
}

void ClipboardWatcher::process(quint64 genAtSchedule) {
    if (!m_enabled || genAtSchedule != m_pendingGen) {
        return;
    }

    QString error;
    const QString text = m_bridge->getClipboardText(&error);
    if (!error.isEmpty()) {
        qWarning().noquote() << "[trimmeh-kde]" << error;
        return;
    }

    if (!m_enabled || genAtSchedule != m_pendingGen) {
        return;
    }

    if (text.isEmpty()) {
        return;
    }

    const QString incomingHash = hashText(text);
    if (!m_lastWrittenHash.isEmpty() && incomingHash == m_lastWrittenHash) {
        m_lastWrittenHash.clear();
        return;
    }

    if (!m_settings.autoTrimEnabled) {
        return;
    }

    TrimOptions options;
    options.keepBlankLines = m_settings.keepBlankLines;
    options.stripBoxChars = m_settings.stripBoxChars;
    options.trimPrompts = m_settings.trimPrompts;
    options.maxLines = m_settings.maxLines;

    TrimResult result = m_core->trim(text, m_settings.aggressiveness, options, &error);
    if (!error.isEmpty()) {
        qWarning().noquote() << "[trimmeh-kde] trim error:" << error;
        return;
    }

    if (!result.changed) {
        return;
    }

    if (!m_enabled || genAtSchedule != m_pendingGen) {
        return;
    }

    m_lastOriginal = text;
    m_lastTrimmed = result.output;
    m_lastWrittenHash = hashText(result.output);

    if (!m_bridge->setClipboardText(result.output, &error)) {
        qWarning().noquote() << "[trimmeh-kde]" << error;
    } else {
        qInfo().noquote() << "[trimmeh-kde] trimmed" << result.reason;
    }
}

QString ClipboardWatcher::hashText(const QString &text) const {
    const QByteArray bytes = text.toUtf8();
    const QByteArray digest = QCryptographicHash::hash(bytes, QCryptographicHash::Sha256);
    return QString::fromLatin1(digest.toHex());
}
