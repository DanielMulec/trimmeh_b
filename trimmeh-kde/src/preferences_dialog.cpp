#include "preferences_dialog.h"

#include <QApplication>
#include <QButtonGroup>
#include <QCheckBox>
#include <QDesktopServices>
#include <QDialogButtonBox>
#include <QFormLayout>
#include <QGroupBox>
#include <QHBoxLayout>
#include <QKeySequenceEdit>
#include <QLabel>
#include <QPlainTextEdit>
#include <QPushButton>
#include <QRadioButton>
#include <QSignalBlocker>
#include <QTabWidget>
#include <QUrl>
#include <QVBoxLayout>

PreferencesDialog::PreferencesDialog(ClipboardWatcher *watcher, TrimCore *core, QWidget *parent)
    : QDialog(parent)
    , m_watcher(watcher)
    , m_core(core)
{
    setWindowTitle(QStringLiteral("Trimmeh Settings"));
    setMinimumSize(410, 484);

    auto *layout = new QVBoxLayout(this);
    auto *tabs = new QTabWidget(this);

    buildGeneralTab(tabs);
    buildAggressivenessTab(tabs);
    buildShortcutsTab(tabs);
    buildAboutTab(tabs);

    layout->addWidget(tabs);

    auto *buttons = new QDialogButtonBox(QDialogButtonBox::Close, this);
    connect(buttons, &QDialogButtonBox::rejected, this, &QDialog::reject);
    layout->addWidget(buttons);

    if (m_watcher) {
        connect(m_watcher, &ClipboardWatcher::stateChanged, this, &PreferencesDialog::refreshFromWatcher);
    }
    refreshFromWatcher();
}

void PreferencesDialog::buildGeneralTab(QTabWidget *tabs) {
    auto *panel = new QWidget(this);
    auto *layout = new QVBoxLayout(panel);

    m_autoTrim = new QCheckBox(QStringLiteral("Auto-trim enabled"), panel);
    m_autoTrim->setToolTip(QStringLiteral("Automatically trim clipboard content when it looks like a command."));
    connect(m_autoTrim, &QCheckBox::toggled, this, [this](bool enabled) {
        if (m_watcher) {
            m_watcher->setAutoTrimEnabled(enabled);
        }
    });

    m_keepBlank = new QCheckBox(QStringLiteral("Keep blank lines"), panel);
    m_keepBlank->setToolTip(QStringLiteral("Preserve intentional blank lines instead of collapsing them."));
    connect(m_keepBlank, &QCheckBox::toggled, this, [this](bool enabled) {
        if (m_watcher) {
            m_watcher->setKeepBlankLines(enabled);
        }
        updateAggressivenessPreview();
    });

    m_stripBox = new QCheckBox(QStringLiteral("Remove box drawing chars (|)"), panel);
    m_stripBox->setToolTip(QStringLiteral("Strip prompt-style box gutters before trimming."));
    connect(m_stripBox, &QCheckBox::toggled, this, [this](bool enabled) {
        if (m_watcher) {
            m_watcher->setStripBoxChars(enabled);
        }
        updateAggressivenessPreview();
    });

    m_trimPrompts = new QCheckBox(QStringLiteral("Strip prompts"), panel);
    m_trimPrompts->setToolTip(QStringLiteral("Remove leading shell prompts when they look like commands."));
    connect(m_trimPrompts, &QCheckBox::toggled, this, [this](bool enabled) {
        if (m_watcher) {
            m_watcher->setTrimPrompts(enabled);
        }
        updateAggressivenessPreview();
    });

    auto *fallbacks = new QCheckBox(QStringLiteral("Use extra clipboard fallbacks"), panel);
    fallbacks->setToolTip(QStringLiteral("Try alternate clipboard formats when plain text is missing."));
    fallbacks->setEnabled(false);

    m_startAtLogin = new QCheckBox(QStringLiteral("Start at Login"), panel);
    m_startAtLogin->setToolTip(QStringLiteral("Launch Trimmeh automatically when you log in."));
    connect(m_startAtLogin, &QCheckBox::toggled, this, [this](bool enabled) {
        if (m_watcher) {
            m_watcher->setStartAtLogin(enabled);
        }
    });

    auto *quitButton = new QPushButton(QStringLiteral("Quit Trimmeh"), panel);
    connect(quitButton, &QPushButton::clicked, qApp, &QCoreApplication::quit);

    layout->addWidget(m_autoTrim);
    layout->addWidget(m_keepBlank);
    layout->addWidget(m_stripBox);
    layout->addWidget(m_trimPrompts);
    layout->addWidget(fallbacks);
    layout->addWidget(m_startAtLogin);
    layout->addStretch(1);
    layout->addWidget(quitButton);

    tabs->addTab(panel, QStringLiteral("General"));
}

void PreferencesDialog::buildAggressivenessTab(QTabWidget *tabs) {
    auto *panel = new QWidget(this);
    auto *layout = new QVBoxLayout(panel);

    auto *group = new QGroupBox(QStringLiteral("Aggressiveness"), panel);
    auto *groupLayout = new QVBoxLayout(group);

    m_low = new QRadioButton(QStringLiteral("Low (safer)"), group);
    m_normal = new QRadioButton(QStringLiteral("Normal"), group);
    m_high = new QRadioButton(QStringLiteral("High (more eager)"), group);

    auto *buttons = new QButtonGroup(group);
    buttons->addButton(m_low);
    buttons->addButton(m_normal);
    buttons->addButton(m_high);

    connect(m_low, &QRadioButton::toggled, this, [this](bool checked) {
        if (checked && m_watcher) {
            m_watcher->setAggressiveness(QStringLiteral("low"));
            updateAggressivenessPreview();
        }
    });
    connect(m_normal, &QRadioButton::toggled, this, [this](bool checked) {
        if (checked && m_watcher) {
            m_watcher->setAggressiveness(QStringLiteral("normal"));
            updateAggressivenessPreview();
        }
    });
    connect(m_high, &QRadioButton::toggled, this, [this](bool checked) {
        if (checked && m_watcher) {
            m_watcher->setAggressiveness(QStringLiteral("high"));
            updateAggressivenessPreview();
        }
    });

    groupLayout->addWidget(m_low);
    groupLayout->addWidget(m_normal);
    groupLayout->addWidget(m_high);

    m_blurb = new QLabel(panel);
    m_blurb->setWordWrap(true);
    m_blurb->setText(QStringLiteral("Automatic trimming uses this aggressiveness level. Manual \"Paste Trimmed\" always runs at High."));

    auto *previewGroup = new QGroupBox(QStringLiteral("Preview"), panel);
    auto *previewLayout = new QVBoxLayout(previewGroup);

    m_previewBefore = new QPlainTextEdit(previewGroup);
    m_previewBefore->setReadOnly(true);
    m_previewBefore->setMaximumBlockCount(20);

    m_previewAfter = new QPlainTextEdit(previewGroup);
    m_previewAfter->setReadOnly(true);
    m_previewAfter->setMaximumBlockCount(20);

    previewLayout->addWidget(new QLabel(QStringLiteral("Before"), previewGroup));
    previewLayout->addWidget(m_previewBefore);
    previewLayout->addWidget(new QLabel(QStringLiteral("After"), previewGroup));
    previewLayout->addWidget(m_previewAfter);

    layout->addWidget(group);
    layout->addWidget(m_blurb);
    layout->addWidget(previewGroup, 1);

    tabs->addTab(panel, QStringLiteral("Aggressiveness"));
}

void PreferencesDialog::buildShortcutsTab(QTabWidget *tabs) {
    auto *panel = new QWidget(this);
    auto *layout = new QVBoxLayout(panel);

    auto *trimmedRow = new QHBoxLayout();
    m_pasteTrimmedHotkeyEnabled = new QCheckBox(QStringLiteral("Enable global “Paste Trimmed” hotkey"), panel);
    m_pasteTrimmedHotkey = new QKeySequenceEdit(panel);
    connect(m_pasteTrimmedHotkeyEnabled, &QCheckBox::toggled, this, [this](bool enabled) {
        if (m_pasteTrimmedHotkey) {
            m_pasteTrimmedHotkey->setEnabled(enabled);
        }
        if (m_watcher) {
            m_watcher->setPasteTrimmedHotkeyEnabled(enabled);
        }
    });
    connect(m_pasteTrimmedHotkey, &QKeySequenceEdit::keySequenceChanged, this, [this](const QKeySequence &sequence) {
        if (m_watcher) {
            m_watcher->setPasteTrimmedHotkey(sequence.toString(QKeySequence::PortableText));
        }
    });
    trimmedRow->addWidget(m_pasteTrimmedHotkeyEnabled, 1);
    trimmedRow->addWidget(m_pasteTrimmedHotkey);

    auto *originalRow = new QHBoxLayout();
    m_pasteOriginalHotkeyEnabled = new QCheckBox(QStringLiteral("Enable global “Paste Original” hotkey"), panel);
    m_pasteOriginalHotkey = new QKeySequenceEdit(panel);
    connect(m_pasteOriginalHotkeyEnabled, &QCheckBox::toggled, this, [this](bool enabled) {
        if (m_pasteOriginalHotkey) {
            m_pasteOriginalHotkey->setEnabled(enabled);
        }
        if (m_watcher) {
            m_watcher->setPasteOriginalHotkeyEnabled(enabled);
        }
    });
    connect(m_pasteOriginalHotkey, &QKeySequenceEdit::keySequenceChanged, this, [this](const QKeySequence &sequence) {
        if (m_watcher) {
            m_watcher->setPasteOriginalHotkey(sequence.toString(QKeySequence::PortableText));
        }
    });
    originalRow->addWidget(m_pasteOriginalHotkeyEnabled, 1);
    originalRow->addWidget(m_pasteOriginalHotkey);

    auto *toggleRow = new QHBoxLayout();
    m_toggleAutoTrimHotkeyEnabled = new QCheckBox(QStringLiteral("Enable global Auto-Trim toggle hotkey"), panel);
    m_toggleAutoTrimHotkey = new QKeySequenceEdit(panel);
    connect(m_toggleAutoTrimHotkeyEnabled, &QCheckBox::toggled, this, [this](bool enabled) {
        if (m_toggleAutoTrimHotkey) {
            m_toggleAutoTrimHotkey->setEnabled(enabled);
        }
        if (m_watcher) {
            m_watcher->setToggleAutoTrimHotkeyEnabled(enabled);
        }
    });
    connect(m_toggleAutoTrimHotkey, &QKeySequenceEdit::keySequenceChanged, this, [this](const QKeySequence &sequence) {
        if (m_watcher) {
            m_watcher->setToggleAutoTrimHotkey(sequence.toString(QKeySequence::PortableText));
        }
    });
    toggleRow->addWidget(m_toggleAutoTrimHotkeyEnabled, 1);
    toggleRow->addWidget(m_toggleAutoTrimHotkey);

    auto *note = new QLabel(QStringLiteral(
        "Paste Trimmed always runs at High aggressiveness and restores your clipboard afterward."), panel);
    note->setWordWrap(true);

    layout->addLayout(trimmedRow);
    layout->addLayout(originalRow);
    layout->addLayout(toggleRow);
    layout->addWidget(note);
    layout->addStretch(1);

    tabs->addTab(panel, QStringLiteral("Shortcuts"));
}

void PreferencesDialog::buildAboutTab(QTabWidget *tabs) {
    auto *panel = new QWidget(this);
    auto *layout = new QVBoxLayout(panel);

    auto *title = new QLabel(QStringLiteral("Trimmeh"), panel);
    auto *tagline = new QLabel(QStringLiteral("Paste-once, run-once clipboard cleaner for terminal snippets."), panel);
    tagline->setWordWrap(true);

    auto *links = new QGroupBox(QStringLiteral("Links"), panel);
    auto *linksLayout = new QVBoxLayout(links);

    auto linkButton = [links](const QString &label, const QString &url) {
        auto *btn = new QPushButton(label, links);
        QObject::connect(btn, &QPushButton::clicked, btn, [url]() {
            QDesktopServices::openUrl(QUrl(url));
        });
        return btn;
    };

    linksLayout->addWidget(linkButton(QStringLiteral("GitHub"), QStringLiteral("https://github.com/steipete/Trimmy")));
    linksLayout->addWidget(linkButton(QStringLiteral("Website"), QStringLiteral("https://steipete.me")));
    linksLayout->addWidget(linkButton(QStringLiteral("Twitter"), QStringLiteral("https://twitter.com/steipete")));

    layout->addWidget(title);
    layout->addWidget(tagline);
    layout->addWidget(links);
    layout->addStretch(1);

    tabs->addTab(panel, QStringLiteral("About"));
}

void PreferencesDialog::refreshFromWatcher() {
    if (!m_watcher) {
        return;
    }
    if (m_autoTrim) m_autoTrim->setChecked(m_watcher->autoTrimEnabled());
    if (m_keepBlank) m_keepBlank->setChecked(m_watcher->keepBlankLines());
    if (m_stripBox) m_stripBox->setChecked(m_watcher->stripBoxChars());
    if (m_trimPrompts) m_trimPrompts->setChecked(m_watcher->trimPrompts());
    if (m_startAtLogin) m_startAtLogin->setChecked(m_watcher->startAtLogin());

    const QString aggr = m_watcher->aggressiveness();
    if (m_low) m_low->setChecked(aggr == QStringLiteral("low"));
    if (m_normal) m_normal->setChecked(aggr == QStringLiteral("normal"));
    if (m_high) m_high->setChecked(aggr == QStringLiteral("high"));

    if (m_pasteTrimmedHotkeyEnabled) {
        const QSignalBlocker block(m_pasteTrimmedHotkeyEnabled);
        m_pasteTrimmedHotkeyEnabled->setChecked(m_watcher->pasteTrimmedHotkeyEnabled());
    }
    if (m_pasteOriginalHotkeyEnabled) {
        const QSignalBlocker block(m_pasteOriginalHotkeyEnabled);
        m_pasteOriginalHotkeyEnabled->setChecked(m_watcher->pasteOriginalHotkeyEnabled());
    }
    if (m_toggleAutoTrimHotkeyEnabled) {
        const QSignalBlocker block(m_toggleAutoTrimHotkeyEnabled);
        m_toggleAutoTrimHotkeyEnabled->setChecked(m_watcher->toggleAutoTrimHotkeyEnabled());
    }

    if (m_pasteTrimmedHotkey) {
        const QSignalBlocker block(m_pasteTrimmedHotkey);
        m_pasteTrimmedHotkey->setKeySequence(
            QKeySequence::fromString(m_watcher->pasteTrimmedHotkey(), QKeySequence::PortableText));
        m_pasteTrimmedHotkey->setEnabled(m_watcher->pasteTrimmedHotkeyEnabled());
    }
    if (m_pasteOriginalHotkey) {
        const QSignalBlocker block(m_pasteOriginalHotkey);
        m_pasteOriginalHotkey->setKeySequence(
            QKeySequence::fromString(m_watcher->pasteOriginalHotkey(), QKeySequence::PortableText));
        m_pasteOriginalHotkey->setEnabled(m_watcher->pasteOriginalHotkeyEnabled());
    }
    if (m_toggleAutoTrimHotkey) {
        const QSignalBlocker block(m_toggleAutoTrimHotkey);
        m_toggleAutoTrimHotkey->setKeySequence(
            QKeySequence::fromString(m_watcher->toggleAutoTrimHotkey(), QKeySequence::PortableText));
        m_toggleAutoTrimHotkey->setEnabled(m_watcher->toggleAutoTrimHotkeyEnabled());
    }

    updateAggressivenessPreview();
}

QString PreferencesDialog::sampleForAggressiveness(const QString &level) const {
    if (level == QStringLiteral("low")) {
        return QStringLiteral("ls -la \\\n  | grep '^d' \\\n  > dirs.txt");
    }
    if (level == QStringLiteral("high")) {
        return QStringLiteral("echo \"hello\"\nprint status");
    }
    return QStringLiteral("kubectl get pods \\\n  -n kube-system \\\n  | jq '.items[].metadata.name'");
}

QString PreferencesDialog::trimmedPreviewFor(const QString &level) const {
    if (!m_core) {
        return QString();
    }
    TrimOptions options;
    if (m_watcher) {
        options.keepBlankLines = m_watcher->keepBlankLines();
        options.stripBoxChars = m_watcher->stripBoxChars();
        options.trimPrompts = m_watcher->trimPrompts();
        options.maxLines = m_watcher->maxLines();
    }

    QString error;
    TrimResult result = m_core->trim(sampleForAggressiveness(level), level, options, &error);
    if (!error.isEmpty()) {
        return QStringLiteral("Error: %1").arg(error);
    }
    return result.output;
}

void PreferencesDialog::updateAggressivenessPreview() {
    if (!m_previewBefore || !m_previewAfter || !m_blurb) {
        return;
    }
    QString level = QStringLiteral("normal");
    if (m_low && m_low->isChecked()) level = QStringLiteral("low");
    if (m_high && m_high->isChecked()) level = QStringLiteral("high");

    const QString sample = sampleForAggressiveness(level);
    m_previewBefore->setPlainText(sample);
    m_previewAfter->setPlainText(trimmedPreviewFor(level));

    if (level == QStringLiteral("low")) {
        m_blurb->setText(QStringLiteral("Low keeps light multi-line snippets intact unless they clearly look like shell commands."));
    } else if (level == QStringLiteral("high")) {
        m_blurb->setText(QStringLiteral("High is most eager: it will flatten almost any short multi-line text that resembles a command."));
    } else {
        m_blurb->setText(QStringLiteral("Normal is the default: it flattens typical blog/README commands with pipes or continuations."));
    }
}
