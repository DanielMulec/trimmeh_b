#include "trimmeh_app.h"

#include <QAction>
#include <QSignalBlocker>

namespace {
constexpr const char* kIconName = "edit-cut";

QAction* makeSeparatorAction(QMenu* menu) {
    QAction* action = menu->addSeparator();
    action->setEnabled(false);
    return action;
}
} // namespace

TrimmehApp::TrimmehApp(QObject* parent)
    : QObject(parent),
      m_settings(),
      m_core(),
      m_watcher(&m_settings, &m_core, this) {
    m_tray = new KStatusNotifierItem(this);
    m_tray->setIconByName(QString::fromLatin1(kIconName));
    m_tray->setTitle(QStringLiteral("Trimmeh"));
    m_tray->setToolTipTitle(QStringLiteral("Trimmeh"));
    m_tray->setToolTipSubTitle(QStringLiteral("Clipboard trimmer for Plasma 6"));
    // Plasma's System Tray "Shown when relevant" policy typically hides Passive items.
    // Mark the app as Active so it surfaces in the compact tray by default.
    m_tray->setStatus(KStatusNotifierItem::Active);

    m_menu = new QMenu();
    m_tray->setContextMenu(m_menu);

    connect(&m_watcher, &TrimmehClipboardWatcher::lastSummaryChanged, this, &TrimmehApp::updateLastSummary);

    rebuildMenu();
    syncMenuStateFromSettings();

    m_watcher.setEnabled(true);
}

void TrimmehApp::rebuildMenu() {
    m_menu->clear();

    m_lastSummaryAction = m_menu->addAction(QStringLiteral("Last: —"));
    m_lastSummaryAction->setEnabled(false);

    makeSeparatorAction(m_menu);

    m_autoTrimAction = m_menu->addAction(QStringLiteral("Auto-trim"));
    m_autoTrimAction->setCheckable(true);
    connect(m_autoTrimAction, &QAction::toggled, this, [this](bool enabled) {
        m_settings.setEnableAutoTrim(enabled);
        m_settings.sync();
    });

    QMenu* aggressivenessMenu = m_menu->addMenu(QStringLiteral("Aggressiveness"));
    m_aggressivenessGroup = new QActionGroup(aggressivenessMenu);
    m_aggressivenessGroup->setExclusive(true);

    auto addAgg = [&](const QString& label, const QString& value) -> QAction* {
        QAction* action = aggressivenessMenu->addAction(label);
        action->setCheckable(true);
        action->setData(value);
        m_aggressivenessGroup->addAction(action);
        return action;
    };

    addAgg(QStringLiteral("Low"), QStringLiteral("low"));
    addAgg(QStringLiteral("Normal"), QStringLiteral("normal"));
    addAgg(QStringLiteral("High"), QStringLiteral("high"));

    connect(m_aggressivenessGroup, &QActionGroup::triggered, this, [this](QAction* action) {
        applyAggressivenessFromAction(action);
        m_settings.sync();
    });

    makeSeparatorAction(m_menu);

    m_keepBlankLinesAction = m_menu->addAction(QStringLiteral("Keep blank lines"));
    m_keepBlankLinesAction->setCheckable(true);
    connect(m_keepBlankLinesAction, &QAction::toggled, this, [this](bool enabled) {
        m_settings.setKeepBlankLines(enabled);
        m_settings.sync();
    });

    m_stripPromptsAction = m_menu->addAction(QStringLiteral("Strip prompts"));
    m_stripPromptsAction->setCheckable(true);
    connect(m_stripPromptsAction, &QAction::toggled, this, [this](bool enabled) {
        m_settings.setTrimPrompts(enabled);
        m_settings.sync();
    });

    m_stripBoxCharsAction = m_menu->addAction(QStringLiteral("Strip box gutters"));
    m_stripBoxCharsAction->setCheckable(true);
    connect(m_stripBoxCharsAction, &QAction::toggled, this, [this](bool enabled) {
        m_settings.setStripBoxChars(enabled);
        m_settings.sync();
    });

    makeSeparatorAction(m_menu);

    m_pasteTrimmedAction = m_menu->addAction(QStringLiteral("Paste Trimmed (High)"));
    connect(m_pasteTrimmedAction, &QAction::triggered, &m_watcher, &TrimmehClipboardWatcher::pasteTrimmedOnce);

    m_pasteOriginalAction = m_menu->addAction(QStringLiteral("Paste Original"));
    connect(m_pasteOriginalAction, &QAction::triggered, &m_watcher, &TrimmehClipboardWatcher::pasteOriginalOnce);

    m_restoreLastCopyAction = m_menu->addAction(QStringLiteral("Restore last copy"));
    connect(m_restoreLastCopyAction, &QAction::triggered, &m_watcher, &TrimmehClipboardWatcher::restoreLastCopy);
}

void TrimmehApp::syncMenuStateFromSettings() {
    {
        const QSignalBlocker autoTrimBlocker(m_autoTrimAction);
        m_autoTrimAction->setChecked(m_settings.enableAutoTrim());
    }
    {
        const QSignalBlocker keepBlankLinesBlocker(m_keepBlankLinesAction);
        m_keepBlankLinesAction->setChecked(m_settings.keepBlankLines());
    }
    {
        const QSignalBlocker stripPromptsBlocker(m_stripPromptsAction);
        m_stripPromptsAction->setChecked(m_settings.trimPrompts());
    }
    {
        const QSignalBlocker stripBoxCharsBlocker(m_stripBoxCharsAction);
        m_stripBoxCharsAction->setChecked(m_settings.stripBoxChars());
    }

    const QString aggr = m_settings.aggressiveness();
    for (QAction* action : m_aggressivenessGroup->actions()) {
        if (action->data().toString() == aggr) {
            action->setChecked(true);
            break;
        }
    }

    const bool hasOriginal = m_watcher.hasLastOriginal();
    m_restoreLastCopyAction->setEnabled(hasOriginal);
    m_pasteOriginalAction->setEnabled(hasOriginal);
}

void TrimmehApp::applyAggressivenessFromAction(const QAction* action) {
    if (!action) {
        return;
    }
    const QString value = action->data().toString();
    if (value == QStringLiteral("low") || value == QStringLiteral("normal") || value == QStringLiteral("high")) {
        m_settings.setAggressiveness(value);
    }
}

void TrimmehApp::updateLastSummary(const QString& summary) {
    if (m_lastSummaryAction) {
        m_lastSummaryAction->setText(QStringLiteral("Last: %1").arg(summary));
    }
    syncMenuStateFromSettings();
}
