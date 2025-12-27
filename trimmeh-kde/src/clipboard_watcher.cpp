#include "clipboard_watcher.h"

#include "autostart_manager.h"
#include "settings_store.h"

#include <QCryptographicHash>
#include <QDebug>
#include <QTimer>

ClipboardWatcher::ClipboardWatcher(KlipperBridge *bridge,
                                   TrimCore *core,
                                   const Settings &settings,
                                   SettingsStore *store,
                                   AutostartManager *autostart,
                                   QObject *parent)
    : QObject(parent)
    , m_bridge(bridge)
    , m_core(core)
    , m_settings(settings)
    , m_store(store)
    , m_autostart(autostart)
{
    m_debounce.setSingleShot(true);
    m_debounce.setInterval(m_settings.graceDelayMs);
    connect(&m_debounce, &QTimer::timeout, this, &ClipboardWatcher::onDebounceTimeout);
}

void ClipboardWatcher::setAutoTrimEnabled(bool enabled) {
    if (m_settings.autoTrimEnabled == enabled) {
        return;
    }
    m_settings.autoTrimEnabled = enabled;
    persistSettings();
    emit stateChanged();
}

void ClipboardWatcher::setKeepBlankLines(bool enabled) {
    if (m_settings.keepBlankLines == enabled) {
        return;
    }
    m_settings.keepBlankLines = enabled;
    persistSettings();
    emit stateChanged();
}

void ClipboardWatcher::setStripBoxChars(bool enabled) {
    if (m_settings.stripBoxChars == enabled) {
        return;
    }
    m_settings.stripBoxChars = enabled;
    persistSettings();
    emit stateChanged();
}

void ClipboardWatcher::setTrimPrompts(bool enabled) {
    if (m_settings.trimPrompts == enabled) {
        return;
    }
    m_settings.trimPrompts = enabled;
    persistSettings();
    emit stateChanged();
}

void ClipboardWatcher::setMaxLines(int maxLines) {
    if (m_settings.maxLines == maxLines) {
        return;
    }
    m_settings.maxLines = maxLines;
    persistSettings();
    emit stateChanged();
}

void ClipboardWatcher::setAggressiveness(const QString &level) {
    if (m_settings.aggressiveness == level) {
        return;
    }
    m_settings.aggressiveness = level;
    persistSettings();
    emit stateChanged();
}

void ClipboardWatcher::setStartAtLogin(bool enabled) {
    if (m_autostart) {
        QString error;
        if (!m_autostart->setEnabled(enabled, &error)) {
            qWarning().noquote() << "[trimmeh-kde]" << error;
        }
        const bool actual = m_autostart->isEnabled();
        if (m_settings.startAtLogin == actual) {
            if (actual != enabled) {
                emit stateChanged();
            }
            return;
        }
        m_settings.startAtLogin = actual;
    } else {
        if (m_settings.startAtLogin == enabled) {
            return;
        }
        m_settings.startAtLogin = enabled;
    }
    persistSettings();
    emit stateChanged();
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
    updateSummary(result.output);
    m_lastWrittenHash = hashText(result.output);

    if (!m_bridge->setClipboardText(result.output, &error)) {
        qWarning().noquote() << "[trimmeh-kde]" << error;
    } else {
        qInfo().noquote() << "[trimmeh-kde] trimmed" << result.reason;
    }
}

bool ClipboardWatcher::pasteTrimmed() {
    if (!m_bridge || !m_core) {
        return false;
    }

    QString error;
    QString source = m_lastOriginal;
    if (source.isEmpty()) {
        source = m_bridge->getClipboardText(&error);
        if (!error.isEmpty()) {
            qWarning().noquote() << "[trimmeh-kde]" << error;
            return false;
        }
    }
    if (source.isEmpty()) {
        updateSummary(QStringLiteral("Nothing to paste."));
        return false;
    }

    TrimOptions options;
    options.keepBlankLines = m_settings.keepBlankLines;
    options.stripBoxChars = m_settings.stripBoxChars;
    options.trimPrompts = m_settings.trimPrompts;
    options.maxLines = m_settings.maxLines;

    TrimResult result = m_core->trim(source, QStringLiteral("high"), options, &error);
    if (!error.isEmpty()) {
        qWarning().noquote() << "[trimmeh-kde] trim error:" << error;
        return false;
    }

    QString previous = m_bridge->getClipboardText(&error);
    if (!error.isEmpty()) {
        qWarning().noquote() << "[trimmeh-kde]" << error;
        return false;
    }

    m_lastOriginal = source;
    m_lastTrimmed = result.output;
    updateSummary(result.output);

    return swapClipboardTemporarily(result.output, previous);
}

bool ClipboardWatcher::pasteOriginal() {
    if (!m_bridge) {
        return false;
    }

    QString error;
    QString original = m_lastOriginal;
    if (original.isEmpty()) {
        original = m_bridge->getClipboardText(&error);
        if (!error.isEmpty()) {
            qWarning().noquote() << "[trimmeh-kde]" << error;
            return false;
        }
    }

    if (original.isEmpty()) {
        updateSummary(QStringLiteral("Nothing to paste."));
        return false;
    }

    QString previous = m_bridge->getClipboardText(&error);
    if (!error.isEmpty()) {
        qWarning().noquote() << "[trimmeh-kde]" << error;
        return false;
    }

    m_lastOriginal = original;
    m_lastTrimmed.clear();
    updateSummary(original);

    return swapClipboardTemporarily(original, previous);
}

bool ClipboardWatcher::restoreLastCopy() {
    if (!m_bridge) {
        return false;
    }

    if (m_lastOriginal.isEmpty()) {
        updateSummary(QStringLiteral("Nothing to restore."));
        return false;
    }

    QString error;
    const QString original = m_lastOriginal;
    m_lastTrimmed.clear();
    updateSummary(original);
    m_lastWrittenHash = hashText(original);

    if (!m_bridge->setClipboardText(original, &error)) {
        qWarning().noquote() << "[trimmeh-kde]" << error;
        return false;
    }

    return true;
}

void ClipboardWatcher::updateSummary(const QString &text) {
    const QString summary = summarize(text);
    if (summary == m_lastSummary) {
        return;
    }
    m_lastSummary = summary;
    emit summaryChanged(m_lastSummary);
    emit stateChanged();
}

QString ClipboardWatcher::summarize(const QString &text) const {
    QString singleLine = text;
    singleLine.replace('\n', ' ');
    return ellipsize(singleLine.trimmed(), 90);
}

QString ClipboardWatcher::ellipsize(const QString &text, int limit) const {
    if (limit < 4 || text.size() <= limit) {
        return text;
    }
    const int keep = limit - 3;
    const int head = keep / 2;
    const int tail = keep - head;
    return text.left(head) + QStringLiteral("...") + text.right(tail);
}

QString ClipboardWatcher::hashText(const QString &text) const {
    const QByteArray bytes = text.toUtf8();
    const QByteArray digest = QCryptographicHash::hash(bytes, QCryptographicHash::Sha256);
    return QString::fromLatin1(digest.toHex());
}

bool ClipboardWatcher::swapClipboardTemporarily(const QString &text, const QString &previous) {
    if (!m_bridge) {
        return false;
    }

    QString error;
    m_lastWrittenHash = hashText(text);
    if (!m_bridge->setClipboardText(text, &error)) {
        qWarning().noquote() << "[trimmeh-kde]" << error;
        return false;
    }
    qInfo().noquote() << "[trimmeh-kde] manual swap window" << m_settings.pasteRestoreDelayMs << "ms";

    if (previous.isEmpty()) {
        return true;
    }

    QTimer::singleShot(m_settings.pasteRestoreDelayMs, this, [this, previous]() {
        if (!m_bridge) {
            return;
        }
        QString err;
        m_lastWrittenHash = hashText(previous);
        if (!m_bridge->setClipboardText(previous, &err)) {
            qWarning().noquote() << "[trimmeh-kde]" << err;
        }
    });

    return true;
}

void ClipboardWatcher::persistSettings() {
    if (!m_store) {
        return;
    }
    m_store->save(m_settings);
}
