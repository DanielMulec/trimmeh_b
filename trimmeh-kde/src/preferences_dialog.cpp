#include "preferences_dialog.h"

#include "portal_paste_injector.h"

#include <QApplication>
#include <QButtonGroup>
#include <QCheckBox>
#include <QDesktopServices>
#include <QDialogButtonBox>
#include <QDir>
#include <QFile>
#include <QFileDevice>
#include <QFileInfo>
#include <QFormLayout>
#include <QGroupBox>
#include <QHBoxLayout>
#include <QIcon>
#include <QKeySequenceEdit>
#include <QLabel>
#include <QPlainTextEdit>
#include <QPushButton>
#include <QProcess>
#include <QProcessEnvironment>
#include <QRadioButton>
#include <QSignalBlocker>
#include <QSpinBox>
#include <QStandardPaths>
#include <QTabWidget>
#include <QUrl>
#include <QVBoxLayout>
#include <QStringList>

namespace {

struct OsReleaseInfo {
    QString id;
    QStringList idLike;
};

QString stripQuotes(const QString &value) {
    if (value.size() >= 2
        && ((value.startsWith(QLatin1Char('"')) && value.endsWith(QLatin1Char('"')))
            || (value.startsWith(QLatin1Char('\'')) && value.endsWith(QLatin1Char('\''))))) {
        return value.mid(1, value.size() - 2);
    }
    return value;
}

OsReleaseInfo readOsRelease() {
    OsReleaseInfo info;
    QFile file(QStringLiteral("/etc/os-release"));
    if (!file.open(QIODevice::ReadOnly | QIODevice::Text)) {
        return info;
    }

    while (!file.atEnd()) {
        QString line = QString::fromUtf8(file.readLine()).trimmed();
        if (line.isEmpty() || line.startsWith(QLatin1Char('#'))) {
            continue;
        }
        const int separator = line.indexOf(QLatin1Char('='));
        if (separator <= 0) {
            continue;
        }
        const QString key = line.left(separator).trimmed();
        QString value = stripQuotes(line.mid(separator + 1).trimmed()).toLower();
        if (key == QStringLiteral("ID")) {
            info.id = value;
        } else if (key == QStringLiteral("ID_LIKE")) {
            info.idLike = value.split(QLatin1Char(' '), Qt::SkipEmptyParts);
        }
    }
    return info;
}

bool distroMatches(const OsReleaseInfo &info, const QString &id) {
    const QString needle = id.toLower();
    if (info.id == needle) {
        return true;
    }
    return info.idLike.contains(needle);
}

bool ensureCliAlias(const QString &source, QString *errorMessage) {
    const QString homeDir = QStandardPaths::writableLocation(QStandardPaths::HomeLocation);
    if (homeDir.isEmpty()) {
        if (errorMessage) {
            *errorMessage = QStringLiteral("Failed to resolve home directory.");
        }
        return false;
    }

    QDir binDir(QDir(homeDir).filePath(QStringLiteral(".local/bin")));
    if (!binDir.exists() && !binDir.mkpath(QStringLiteral("."))) {
        if (errorMessage) {
            *errorMessage = QStringLiteral("Failed to create ~/.local/bin.");
        }
        return false;
    }

    const QString target = binDir.filePath(QStringLiteral("trimmeh"));
    QFileInfo targetInfo(target);
    QFileInfo sourceInfo(source);
    if (targetInfo.exists()) {
        if (targetInfo.canonicalFilePath() == sourceInfo.canonicalFilePath()) {
            return true;
        }
        QFile::remove(target);
    }

    if (!QFile::link(source, target)) {
        if (!QFile::copy(source, target)) {
            if (errorMessage) {
                *errorMessage = QStringLiteral("Failed to install CLI alias.");
            }
            return false;
        }
        QFile::setPermissions(target, QFileDevice::ReadOwner | QFileDevice::WriteOwner | QFileDevice::ExeOwner
                                       | QFileDevice::ReadGroup | QFileDevice::ExeGroup
                                       | QFileDevice::ReadOther | QFileDevice::ExeOther);
    }

    return true;
}

}

PreferencesDialog::PreferencesDialog(ClipboardWatcher *watcher,
                                     TrimCore *core,
                                     PortalPasteInjector *injector,
                                     QWidget *parent)
    : QDialog(parent)
    , m_watcher(watcher)
    , m_core(core)
    , m_injector(injector)
{
    setWindowTitle(QStringLiteral("Trimmeh Settings"));
    setMinimumSize(410, 484);

    auto *layout = new QVBoxLayout(this);
    m_tabs = new QTabWidget(this);

    buildGeneralTab(m_tabs);
    buildAggressivenessTab(m_tabs);
    buildShortcutsTab(m_tabs);
    buildAboutTab(m_tabs);

    layout->addWidget(m_tabs);

    auto *buttons = new QDialogButtonBox(QDialogButtonBox::Close, this);
    connect(buttons, &QDialogButtonBox::rejected, this, &QDialog::reject);
    layout->addWidget(buttons);

    if (m_watcher) {
        connect(m_watcher, &ClipboardWatcher::stateChanged, this, &PreferencesDialog::refreshFromWatcher);
    }
    if (m_injector) {
        connect(m_injector, &PortalPasteInjector::stateChanged, this, &PreferencesDialog::refreshPermission);
        connect(m_injector, &PortalPasteInjector::preauthStateChanged, this, &PreferencesDialog::refreshPermission);
        connect(m_injector, &PortalPasteInjector::preauthStatusChanged, this, &PreferencesDialog::refreshPermission);
    }
    refreshFromWatcher();
    refreshPermission();
    refreshCliStatus();
}

void PreferencesDialog::showAboutTab() {
    if (m_tabs && m_aboutTabIndex >= 0) {
        m_tabs->setCurrentIndex(m_aboutTabIndex);
    }
    show();
    raise();
    activateWindow();
}

void PreferencesDialog::buildGeneralTab(QTabWidget *tabs) {
    auto *panel = new QWidget(this);
    auto *layout = new QVBoxLayout(panel);

    m_permissionGroup = new QGroupBox(QStringLiteral("Hotkey permission"), panel);
    auto *permLayout = new QVBoxLayout(m_permissionGroup);
    m_permissionLabel = new QLabel(QStringLiteral("Enable hotkeys to allow paste shortcuts."), m_permissionGroup);
    m_permissionLabel->setWordWrap(true);
    m_permissionPermanentButton = new QPushButton(QStringLiteral("Enable Hotkeys Permanently"), m_permissionGroup);
    connect(m_permissionPermanentButton, &QPushButton::clicked, this, [this]() {
        if (m_injector) {
            m_injector->requestPreauthorization();
        }
    });
    m_permissionSettingsButton = new QPushButton(QStringLiteral("Open System Settings"), m_permissionGroup);
    connect(m_permissionSettingsButton, &QPushButton::clicked, this, [this]() {
        const QStringList candidates = {
            QStringLiteral("kcmshell6"),
            QStringLiteral("kcmshell5"),
            QStringLiteral("systemsettings"),
            QStringLiteral("systemsettings6"),
        };
        for (const QString &candidate : candidates) {
            const QString exec = QStandardPaths::findExecutable(candidate);
            if (exec.isEmpty()) {
                continue;
            }
            QStringList args;
            if (candidate.startsWith(QStringLiteral("kcmshell"))) {
                args << QStringLiteral("kcm_flatpak");
            } else {
                args << QStringLiteral("kcm_flatpak");
            }
            if (!QProcess::startDetached(exec, args)) {
                args.clear();
                if (QProcess::startDetached(exec, args)) {
                    return;
                }
                continue;
            }
            return;
        }
    });
    m_permissionStatus = new QLabel(QString(), m_permissionGroup);
    m_permissionStatus->setWordWrap(true);
    permLayout->addWidget(m_permissionLabel);
    auto *permButtons = new QHBoxLayout();
    permButtons->addWidget(m_permissionPermanentButton);
    permButtons->addWidget(m_permissionSettingsButton);
    permButtons->addStretch(1);
    permLayout->addLayout(permButtons);
    permLayout->addWidget(m_permissionStatus);

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

    auto *timingGroup = new QGroupBox(QStringLiteral("Paste timing"), panel);
    auto *timingLayout = new QFormLayout(timingGroup);
    m_restoreDelay = new QSpinBox(timingGroup);
    m_restoreDelay->setRange(50, 2000);
    m_restoreDelay->setSingleStep(50);
    m_restoreDelay->setSuffix(QStringLiteral(" ms"));
    m_restoreDelay->setToolTip(QStringLiteral("How long to keep the temporary clipboard before restoring it."));
    connect(m_restoreDelay, QOverload<int>::of(&QSpinBox::valueChanged), this, [this](int value) {
        if (m_watcher) {
            m_watcher->setPasteRestoreDelayMs(value);
        }
    });
    timingLayout->addRow(QStringLiteral("Restore delay"), m_restoreDelay);

    m_clipboardFallbacks = new QCheckBox(QStringLiteral("Use extra clipboard fallbacks"), panel);
    m_clipboardFallbacks->setToolTip(QStringLiteral("Try alternate clipboard formats when plain text is missing."));
    connect(m_clipboardFallbacks, &QCheckBox::toggled, this, [this](bool enabled) {
        if (m_watcher) {
            m_watcher->setUseClipboardFallbacks(enabled);
        }
    });

    auto *cliGroup = new QGroupBox(QStringLiteral("Command-line tool"), panel);
    auto *cliLayout = new QVBoxLayout(cliGroup);
    auto *cliRow = new QHBoxLayout();
    m_installCliButton = new QPushButton(QStringLiteral("Install CLI"), cliGroup);
    connect(m_installCliButton, &QPushButton::clicked, this, &PreferencesDialog::installCli);
    m_cliStatus = new QLabel(QString(), cliGroup);
    m_cliStatus->setWordWrap(true);
    m_cliStatus->setStyleSheet(QStringLiteral("color: palette(mid);"));
    cliRow->addWidget(m_installCliButton);
    cliRow->addWidget(m_cliStatus, 1);
    cliLayout->addLayout(cliRow);
    auto *cliNote = new QLabel(QStringLiteral("Installs the CLI package (if available) and links `trimmeh` into ~/.local/bin."), cliGroup);
    cliNote->setWordWrap(true);
    cliNote->setStyleSheet(QStringLiteral("color: palette(mid);"));
    cliLayout->addWidget(cliNote);

    m_startAtLogin = new QCheckBox(QStringLiteral("Start at Login"), panel);
    m_startAtLogin->setToolTip(QStringLiteral("Launch Trimmeh automatically when you log in."));
    connect(m_startAtLogin, &QCheckBox::toggled, this, [this](bool enabled) {
        if (m_watcher) {
            m_watcher->setStartAtLogin(enabled);
        }
    });

    auto *quitButton = new QPushButton(QStringLiteral("Quit Trimmeh"), panel);
    connect(quitButton, &QPushButton::clicked, qApp, &QCoreApplication::quit);

    layout->addWidget(m_permissionGroup);
    layout->addWidget(m_autoTrim);
    layout->addWidget(m_keepBlank);
    layout->addWidget(m_stripBox);
    layout->addWidget(m_trimPrompts);
    layout->addWidget(timingGroup);
    layout->addWidget(m_clipboardFallbacks);
    layout->addWidget(cliGroup);
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

    auto *iconLabel = new QLabel(panel);
    QPixmap iconPixmap = QIcon::fromTheme(QStringLiteral("edit-cut")).pixmap(64, 64);
    if (!iconPixmap.isNull()) {
        iconLabel->setPixmap(iconPixmap);
    }
    iconLabel->setAlignment(Qt::AlignHCenter);

    auto *title = new QLabel(QStringLiteral("Trimmeh"), panel);
    title->setAlignment(Qt::AlignHCenter);

    const QString version = QCoreApplication::applicationVersion();
    auto *versionLabel = new QLabel(version.isEmpty()
                                        ? QStringLiteral("Version –")
                                        : QStringLiteral("Version %1").arg(version),
                                    panel);
    versionLabel->setAlignment(Qt::AlignHCenter);
    versionLabel->setStyleSheet(QStringLiteral("color: palette(mid);"));

    auto *tagline = new QLabel(QStringLiteral("Paste-once, run-once clipboard cleaner for terminal snippets."), panel);
    tagline->setWordWrap(true);
    tagline->setAlignment(Qt::AlignHCenter);

    auto *links = new QGroupBox(QStringLiteral("Links"), panel);
    auto *linksLayout = new QVBoxLayout(links);

    auto linkButton = [links](const QString &label, const QString &url) {
        auto *btn = new QPushButton(label, links);
        QObject::connect(btn, &QPushButton::clicked, btn, [url]() {
            QDesktopServices::openUrl(QUrl(url));
        });
        return btn;
    };

    linksLayout->addWidget(linkButton(QStringLiteral("GitHub"), QStringLiteral("https://github.com/DanielMulec/trimmeh_b")));
    linksLayout->addWidget(linkButton(QStringLiteral("Website"), QStringLiteral("https://www.danielmulec.com")));
    linksLayout->addWidget(linkButton(QStringLiteral("X.com"), QStringLiteral("https://x.com/danielmulec")));
    linksLayout->addWidget(linkButton(QStringLiteral("Email"), QStringLiteral("mailto:dmulec@gmail.com")));

    auto *updates = new QGroupBox(QStringLiteral("Updates"), panel);
    auto *updatesLayout = new QVBoxLayout(updates);
    auto *autoCheck = new QCheckBox(QStringLiteral("Check for updates automatically"), updates);
    autoCheck->setEnabled(false);
    auto *checkNow = new QPushButton(QStringLiteral("Check for Updates…"), updates);
    checkNow->setEnabled(false);
    auto *updateNote = new QLabel(QStringLiteral("Updates are delivered via your package manager."), updates);
    updateNote->setWordWrap(true);
    updateNote->setStyleSheet(QStringLiteral("color: palette(mid);"));
    updatesLayout->addWidget(autoCheck);
    updatesLayout->addWidget(checkNow);
    updatesLayout->addWidget(updateNote);

    layout->addWidget(iconLabel);
    layout->addWidget(title);
    layout->addWidget(versionLabel);
    layout->addWidget(tagline);
    layout->addWidget(links);
    layout->addWidget(updates);
    layout->addStretch(1);

    m_aboutTabIndex = tabs->addTab(panel, QStringLiteral("About"));
}

void PreferencesDialog::refreshFromWatcher() {
    if (!m_watcher) {
        return;
    }
    if (m_autoTrim) m_autoTrim->setChecked(m_watcher->autoTrimEnabled());
    if (m_keepBlank) m_keepBlank->setChecked(m_watcher->keepBlankLines());
    if (m_stripBox) m_stripBox->setChecked(m_watcher->stripBoxChars());
    if (m_trimPrompts) m_trimPrompts->setChecked(m_watcher->trimPrompts());
    if (m_clipboardFallbacks) m_clipboardFallbacks->setChecked(m_watcher->useClipboardFallbacks());
    if (m_startAtLogin) m_startAtLogin->setChecked(m_watcher->startAtLogin());
    if (m_restoreDelay) {
        const QSignalBlocker block(m_restoreDelay);
        m_restoreDelay->setValue(m_watcher->pasteRestoreDelayMs());
    }

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

void PreferencesDialog::refreshPermission() {
    if (!m_permissionGroup || !m_permissionLabel
        || !m_permissionPermanentButton || !m_permissionSettingsButton || !m_permissionStatus) {
        return;
    }
    if (!m_injector) {
        m_permissionGroup->setVisible(false);
        return;
    }

    if (!m_injector->isAvailable()) {
        m_permissionLabel->setText(QStringLiteral("Hotkey permission portal is unavailable on this system."));
        m_permissionPermanentButton->setEnabled(false);
        m_permissionSettingsButton->setEnabled(false);
        m_permissionStatus->setText(QString());
        m_permissionGroup->setVisible(true);
        return;
    }

    const auto preauthStatus = m_injector->preauthStatus();
    if (preauthStatus == PortalPasteInjector::PreauthStatus::Present) {
        m_permissionLabel->setText(QStringLiteral("Hotkeys enabled permanently. Paste will work when used."));
        m_permissionPermanentButton->setVisible(false);
        m_permissionStatus->setText(QString());
        m_permissionSettingsButton->setEnabled(true);
        m_permissionGroup->setVisible(true);
        return;
    }

    if (m_injector->isRequesting()) {
        m_permissionLabel->setText(QStringLiteral("Waiting for hotkey permission dialog..."));
    } else if (m_injector->isReady()) {
        m_permissionLabel->setText(QStringLiteral("Hotkey permission granted for this session."));
    } else if (m_injector->state() == PortalPasteInjector::State::Error && !m_injector->lastError().isEmpty()) {
        m_permissionLabel->setText(QStringLiteral("Hotkey permission error: %1").arg(m_injector->lastError()));
    } else if (m_injector->state() == PortalPasteInjector::State::Denied) {
        m_permissionLabel->setText(QStringLiteral("Permission was denied. Use a paste action, then paste manually (Ctrl+V)."));
    } else {
        m_permissionLabel->setText(QStringLiteral("Enable hotkeys to allow paste shortcuts."));
    }

    const bool canPreauth = m_injector->canPreauthorize();
    const auto preauthState = m_injector->preauthState();
    m_permissionPermanentButton->setVisible(true);
    m_permissionPermanentButton->setEnabled(canPreauth
                                            && preauthState != PortalPasteInjector::PreauthState::Working);

    bool hasSettings = false;
    const QStringList settingsCandidates = {
        QStringLiteral("kcmshell6"),
        QStringLiteral("kcmshell5"),
        QStringLiteral("systemsettings"),
        QStringLiteral("systemsettings6"),
    };
    for (const QString &candidate : settingsCandidates) {
        if (!QStandardPaths::findExecutable(candidate).isEmpty()) {
            hasSettings = true;
            break;
        }
    }
    m_permissionSettingsButton->setEnabled(hasSettings);

    QString status = m_injector->preauthMessage();
    if (status.isEmpty()) {
        if (preauthStatus == PortalPasteInjector::PreauthStatus::Unknown) {
            status = QStringLiteral("Checking permanent permission...");
        } else if (!canPreauth) {
            status = QStringLiteral("This system can’t store permanent hotkey permission.\n"
                                     "Use a paste action once to grant permission when prompted.");
        } else {
        switch (preauthState) {
        case PortalPasteInjector::PreauthState::Working:
            status = QStringLiteral("Enabling hotkeys permanently...");
            break;
        case PortalPasteInjector::PreauthState::Succeeded:
            status = QStringLiteral("Hotkeys enabled permanently. You should not be asked again.");
            break;
        case PortalPasteInjector::PreauthState::Failed:
            status = QStringLiteral("Failed to enable hotkeys permanently.");
            break;
        case PortalPasteInjector::PreauthState::Unavailable:
            status = QStringLiteral("This system can’t store permanent hotkey permission.\n"
                                     "Use a paste action once to grant permission when prompted.");
            break;
        case PortalPasteInjector::PreauthState::Idle:
            status = QStringLiteral("Enable hotkeys permanently to avoid future prompts.");
            break;
        }
        }
    }
    m_permissionStatus->setText(status);
    m_permissionGroup->setVisible(true);
}

void PreferencesDialog::installCli() {
    if (!m_installCliButton || !m_cliStatus) {
        return;
    }

    if (m_cliProcess) {
        if (m_cliProcess->state() != QProcess::NotRunning) {
            m_cliStatus->setText(QStringLiteral("CLI installer is already running."));
            return;
        }
        m_cliProcess->deleteLater();
        m_cliProcess = nullptr;
    }

    const QString source = QStandardPaths::findExecutable(QStringLiteral("trimmeh-cli"));
    if (!source.isEmpty()) {
        QString errorMessage;
        if (ensureCliAlias(source, &errorMessage)) {
            m_cliStatus->setText(QStringLiteral("CLI installed. Ensure ~/.local/bin is on PATH."));
            m_cliStatus->setStyleSheet(QStringLiteral("color: palette(windowText);"));
            refreshCliStatus();
        } else {
            m_cliStatus->setText(errorMessage);
            m_cliStatus->setStyleSheet(QStringLiteral("color: palette(windowText);"));
        }
        return;
    }

    const QString pkexec = QStandardPaths::findExecutable(QStringLiteral("pkexec"));
    if (pkexec.isEmpty()) {
        m_cliStatus->setText(QStringLiteral("trimmeh-cli not found in PATH. Install it via your package manager, then click Install CLI again."));
        return;
    }

    const OsReleaseInfo osInfo = readOsRelease();
    QString manager = QStringLiteral("dnf");
    QStringList managerArgs;
    QString installHint;

    if (distroMatches(osInfo, QStringLiteral("fedora")) || distroMatches(osInfo, QStringLiteral("rhel"))
        || distroMatches(osInfo, QStringLiteral("centos")) || distroMatches(osInfo, QStringLiteral("rocky"))
        || distroMatches(osInfo, QStringLiteral("almalinux"))) {
        managerArgs << QStringLiteral("install") << QStringLiteral("-y") << QStringLiteral("trimmeh-cli");
        installHint = QStringLiteral("dnf install -y trimmeh-cli");
    } else if (distroMatches(osInfo, QStringLiteral("debian")) || distroMatches(osInfo, QStringLiteral("ubuntu"))
               || distroMatches(osInfo, QStringLiteral("linuxmint"))) {
        manager = QStringLiteral("apt-get");
        managerArgs << QStringLiteral("install") << QStringLiteral("-y") << QStringLiteral("trimmeh-cli");
        installHint = QStringLiteral("apt-get install -y trimmeh-cli");
    } else if (distroMatches(osInfo, QStringLiteral("arch"))) {
        m_cliStatus->setText(QStringLiteral("On Arch, install trimmeh-cli from the AUR, then click Install CLI again."));
        return;
    } else {
        m_cliStatus->setText(QStringLiteral("Install trimmeh-cli manually, then click Install CLI again."));
        return;
    }

    const QString managerPath = QStandardPaths::findExecutable(manager);
    if (managerPath.isEmpty()) {
        m_cliStatus->setText(QStringLiteral("Package manager not found. Install trimmeh-cli manually (%1).").arg(installHint));
        return;
    }

    m_cliProcess = new QProcess(this);
    m_cliProcess->setProgram(pkexec);
    m_cliProcess->setArguments(QStringList{managerPath} + managerArgs);
    m_cliProcess->setProcessChannelMode(QProcess::MergedChannels);
    QProcessEnvironment env = QProcessEnvironment::systemEnvironment();
    if (manager == QStringLiteral("apt-get")) {
        env.insert(QStringLiteral("DEBIAN_FRONTEND"), QStringLiteral("noninteractive"));
        env.insert(QStringLiteral("APT_LISTCHANGES_FRONTEND"), QStringLiteral("none"));
    }
    m_cliProcess->setProcessEnvironment(env);

    m_installCliButton->setEnabled(false);
    m_cliStatus->setText(QStringLiteral("Installing trimmeh-cli..."));
    m_cliStatus->setStyleSheet(QStringLiteral("color: palette(windowText);"));

    const QString managerName = QFileInfo(managerPath).baseName();
    connect(m_cliProcess, &QProcess::finished, this, [this, managerName](int exitCode, QProcess::ExitStatus exitStatus) {
        m_installCliButton->setEnabled(true);
        if (exitStatus != QProcess::NormalExit || exitCode != 0) {
            QString output = QString::fromUtf8(m_cliProcess ? m_cliProcess->readAll() : QByteArray()).trimmed();
            QString lastLine;
            if (!output.isEmpty()) {
                const QStringList lines = output.split(QLatin1Char('\n'), Qt::SkipEmptyParts);
                if (!lines.isEmpty()) {
                    lastLine = lines.constLast().trimmed();
                }
            }
            if (!lastLine.isEmpty()) {
                m_cliStatus->setText(QStringLiteral("CLI install failed via %1: %2").arg(managerName, lastLine));
            } else {
                m_cliStatus->setText(QStringLiteral("CLI install failed via %1.").arg(managerName));
            }
            m_cliStatus->setStyleSheet(QStringLiteral("color: palette(windowText);"));
        } else {
            const QString resolved = QStandardPaths::findExecutable(QStringLiteral("trimmeh-cli"));
            if (resolved.isEmpty()) {
                m_cliStatus->setText(QStringLiteral("Installed via %1, but trimmeh-cli is still not on PATH. Log out and try again.")
                                        .arg(managerName));
                m_cliStatus->setStyleSheet(QStringLiteral("color: palette(windowText);"));
            } else {
                QString errorMessage;
                if (ensureCliAlias(resolved, &errorMessage)) {
                    m_cliStatus->setText(QStringLiteral("CLI installed. Ensure ~/.local/bin is on PATH."));
                    m_cliStatus->setStyleSheet(QStringLiteral("color: palette(windowText);"));
                    refreshCliStatus();
                } else {
                    m_cliStatus->setText(errorMessage);
                    m_cliStatus->setStyleSheet(QStringLiteral("color: palette(windowText);"));
                }
            }
        }
        if (m_cliProcess) {
            m_cliProcess->deleteLater();
            m_cliProcess = nullptr;
        }
    });

    connect(m_cliProcess, &QProcess::errorOccurred, this, [this](QProcess::ProcessError) {
        m_installCliButton->setEnabled(true);
        m_cliStatus->setText(QStringLiteral("CLI install failed to start."));
        m_cliStatus->setStyleSheet(QStringLiteral("color: palette(windowText);"));
        if (m_cliProcess) {
            m_cliProcess->deleteLater();
            m_cliProcess = nullptr;
        }
    });

    m_cliProcess->start();
}

void PreferencesDialog::refreshCliStatus() {
    if (!m_installCliButton || !m_cliStatus) {
        return;
    }

    const QString source = QStandardPaths::findExecutable(QStringLiteral("trimmeh-cli"));
    if (source.isEmpty()) {
        m_installCliButton->setEnabled(true);
        m_cliStatus->setText(QStringLiteral("trimmeh-cli not installed yet."));
        m_cliStatus->setStyleSheet(QStringLiteral("color: palette(mid);"));
        return;
    }

    m_installCliButton->setEnabled(false);
    m_cliStatus->setText(QStringLiteral("CLI installed."));
    m_cliStatus->setStyleSheet(QStringLiteral("color: palette(windowText);"));
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
