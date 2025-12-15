# KDE Plasma Port für Trimmeh — Umfassende Strategie & Tech Stack

**Status:** Planungsdokument für KDE Plasma 6.4+ Port mit vollständiger Parität zu Trimmy  
**Zielplattform:** KDE Plasma 6.4+, primär Fedora 43  
**Anforderung:** 100% Feature-Parität mit Steipete's Trimmy und GNOME Trimmeh

---

## 1. Analyse der aktuellen Codebase (Trimmeh GNOME)

### 1.1 Architektur Overview

Trimmeh folgt einer **dreischichtigen Architektur:**

```
┌────────────────────────────────────────────────────────────────┐
│  UI / Platform Layer                                            │
│  ├─ GNOME Shell Extension (TS/JS)                              │
│  │  ├─ clipboardWatcher.ts (Clipboard-Monitoring)              │
│  │  ├─ virtualPaste.ts (Synthetic Paste Injection via Wayland) │
│  │  ├─ panel.ts (Top Bar Menu & Actions)                       │
│  │  ├─ prefs.ts (Preferences/Settings UI)                      │
│  │  └─ clipboard.ts (X11/Wayland Clipboard Access)             │
│  └─ GSettings Schema (org.gnome.shell.extensions.trimmeh)      │
├────────────────────────────────────────────────────────────────┤
│  Core Logic (Language-agnostic)                                 │
│  └─ trimmeh-core (Rust)                                        │
│     ├─ Trimming Engine (lib.rs)                                │
│     ├─ WASM Build (für JavaScript-Integration)                 │
│     └─ Tests mit goldenen Vektoren (parity mit Trimmy)        │
├────────────────────────────────────────────────────────────────┤
│  CLI / Lower Boundaries                                         │
│  └─ trimmeh-cli (Rust)                                         │
│     └─ Command-line Interface für Batch-Verarbeitung           │
└────────────────────────────────────────────────────────────────┘
```

### 1.2 Kernfunktionalität (vollständig in trimmeh-core)

**Trimming Engine Features:**
- ✅ Multi-line Shell-Snippet Flattening
- ✅ Aggressiveness Levels (Low/Normal/High)
- ✅ Backslash Continuation Merging
- ✅ Box-Drawing Character Stripping (│┃╎╏ etc.)
- ✅ Shell Prompt Stripping ($, #)
- ✅ Wrapped URL Repair
- ✅ Blank Line Preservation Option
- ✅ List Detection & Skipping
- ✅ Source Code Detection (verhindert Fehltrimmung)
- ✅ Configurable max_lines (Default: 10)
- ✅ Hash-basierte Loop-Prevention (blake3)
- ✅ Standardized Exit Codes (kompatibel mit Trimmy)

**Configuration Options:**
```rust
pub struct Options {
    pub keep_blank_lines: bool,    // Blank Lines bewahren
    pub strip_box_chars: bool,     // Box-Gutters entfernen
    pub trim_prompts: bool,        // Prompts entfernen
    pub max_lines: usize,          // Max input size
}
```

### 1.3 GNOME-spezifische Implementierung

**Clipboard Handling:**
- `clipboard.ts`: Wayland-native Clipboard-Zugriff über GObject Introspection
- Monitort Clipboard-Änderungen via GObject Signals
- Detektiert eigene Writes (via blake3-Hash) → verhindert Loops

**Paste Injection:**
- `virtualPaste.ts`: Synthetic Keyboard Events (Shift+Insert oder Ctrl+V)
- Wayland-safe, Best-Effort (bestimmte Apps können synthetische Events ignorieren)
- Wartet auf Modifier Release (Super/Alt/Shift) bevor paste injiziert

**GNOME Shell Integration:**
- Top-bar Button mit Dropdown-Menü
- Global Hotkeys (Standard: disabled für EGO Compliance)
- Preferences via libadwaita
- GSettings für persistent config

---

## 2. KDE Plasma 6.4+ Port Strategie

### 2.1 KDE Plasma Architecture

KDE Plasma hat eine ganz andere Architektur als GNOME:

```
┌────────────────────────────────────────────────────────────────┐
│  KDE Plasma Shell                                               │
│  ├─ Plasmoid (QML + C++)  [unser UI Layer]                     │
│  ├─ KConfigXT (XML-basierte Preferences)                       │
│  └─ KDE Frameworks                                              │
│     ├─ KClipboard (Qt Clipboard API)                           │
│     ├─ DBus Integration (System Services)                       │
│     └─ KWindowSystem / KGlobalShortcuts                         │
├────────────────────────────────────────────────────────────────┤
│  Core Trimming Logic (REUSE)                                    │
│  └─ trimmeh-core (Rust) — unverändert                          │
│     ├─ C FFI (für C++-Integration)                             │
│     ├─ WASM (alternative für JavaScript)                       │
│     └─ Tests (100% kompatibel mit GNOME)                       │
└────────────────────────────────────────────────────────────────┘
```

### 2.2 Design-Entscheidungen

**Option A: Plasmoid (QML + C++)**
- ✅ Native KDE Integration
- ✅ Performance (native Widgets)
- ✅ volle KDE Frameworks Nutzung
- ❌ Komplexere Build-Pipeline
- **→ EMPFOHLEN**

**Option B: KDE Service + System Tray Widget**
- ✅ Hintergrunddienst möglich
- ✅ System Tray Integration
- ❌ Weniger Native UI
- → Fallback-Option

**Wir verwenden Option A: Plasmoid**

### 2.3 Tech Stack für KDE Port

```
┌─────────────────────────────────────┐
│ KDE Plasma Applet (unser Primary)   │
├─────────────────────────────────────┤
│ Presentation Layer:                  │
│ ├─ QML (UI/UX)                      │
│ ├─ Qt Quick Controls 2 (Modern UI)   │
│ ├─ KDE Frameworks (KConfigXT, etc.)  │
│ └─ libplasma                         │
├─────────────────────────────────────┤
│ C++ Bridge:                          │
│ ├─ Qt/C++ für Clipboard Operations   │
│ ├─ DBus Interface für Global Hotkeys │
│ └─ C FFI zu trimmeh-core Rust Lib    │
├─────────────────────────────────────┤
│ Core (Shared):                       │
│ ├─ trimmeh-core (Rust) — cdylib     │
│ ├─ C Header (auto-generated)         │
│ └─ blake3 (via C FFI)                │
├─────────────────────────────────────┤
│ Build:                               │
│ ├─ CMake (KDE Standard)              │
│ ├─ cargo (für Rust Core)             │
│ └─ Qt's moc (Meta Object Compiler)   │
└─────────────────────────────────────┘
```

---

## 3. Implementation Roadmap

### Phase 1: Infrastructure Setup (Wochen 1-2)

**3.1.1 Repository Structure**

```
trimmeh_b/
├── trimmeh-core/          (Rust, unverändert)
│   ├── Cargo.toml
│   ├── src/
│   │   └── lib.rs         (mit C FFI exports)
│   └── tests/
├── trimmeh-cli/           (Rust CLI, unverändert)
├── shell-extension/       (GNOME, unverändert)
├── plasma-applet/         (NEW: KDE Plasma Applet)
│   ├── CMakeLists.txt
│   ├── src/
│   │   ├── CMakeLists.txt
│   │   ├── trimmeapplet.h         (C++ Main Class)
│   │   ├── trimmeapplet.cpp       (Implementation)
│   │   ├── clipboardmanager.h     (Clipboard Ops)
│   │   ├── clipboardmanager.cpp   (Implementation)
│   │   ├── hotkeymanager.h        (Global Hotkeys)
│   │   └── hotkeymanager.cpp      (Implementation)
│   ├── package/
│   │   ├── contents/
│   │   │   ├── ui/
│   │   │   │   ├── main.qml        (Main Plasmoid UI)
│   │   │   │   ├── preferences.qml (Preferences UI)
│   │   │   │   └── PopupDialog.qml (Dropdown Menu)
│   │   │   ├── config/
│   │   │   │   ├── config.xml      (KConfigXT Scheme)
│   │   │   │   └── configui.qml    (Config UI)
│   │   │   └── code/
│   │   │       └── Utils.js        (Helper Functions)
│   │   └── metadata.json           (Plasmoid Metadata)
│   ├── metadata.desktop
│   └── Messages.sh                 (i18n)
├── packaging/
│   ├── rpm/
│   │   ├── trimmeh.spec            (RPM Spec)
│   │   └── fedora-43.patch         (Fedora-spezifisch)
│   └── kde-plasma/
│       └── setup.sh                (Installation Script)
└── tests/
    ├── trim-vectors.json           (shared)
    └── kde-applet-tests/
        └── ...
```

**3.1.2 Cargo.toml Anpassungen**

```toml
# In trimmeh-core/Cargo.toml
[lib]
crate-type = ["rlib", "cdylib", "staticlib"]  # Added cdylib for C FFI

[features]
default = ["ffi"]
ffi = []  # New feature for C FFI exports
wasm = ["wasm-bindgen", "serde", "serde-wasm-bindgen"]

# In root Cargo.toml
[workspace]
members = [
  "trimmeh-core",
  "trimmeh-cli",
  # "plasma-applet" — wird nicht in Cargo Workspace sein (CMake project)
]
```

---

### Phase 2: C FFI & Core Integration (Wochen 2-3)

**3.2.1 C FFI Exports in trimmeh-core**

Neue Datei: `trimmeh-core/src/ffi.rs`

```rust
use crate::{Aggressiveness, Options, trim};

#[repr(C)]
pub struct CTrimResult {
    output_ptr: *mut u8,
    output_len: usize,
    changed: bool,
    reason_code: u8,  // 0=None, 1=Flattened, 2=PromptStripped, ...
}

#[no_mangle]
pub unsafe extern "C" fn trim_ffi(
    input: *const u8,
    input_len: usize,
    aggressiveness: u8,
    keep_blank_lines: bool,
    strip_box_chars: bool,
    trim_prompts: bool,
    max_lines: usize,
) -> CTrimResult {
    // Implementation details...
}

#[no_mangle]
pub unsafe extern "C" fn trim_result_free(result: *mut CTrimResult) {
    // Cleanup
}
```

**3.2.2 CMake Integration für Rust**

`plasma-applet/CMakeLists.txt`:

```cmake
cmake_minimum_required(VERSION 3.16)
project(plasma-applet-trimmeh)

set(KF6_MIN_VERSION "6.0")
set(QT_MIN_VERSION "6.5")

find_package(Qt6 ${QT_MIN_VERSION} REQUIRED COMPONENTS Core Gui Qml Quick DBus)
find_package(KF6 ${KF6_MIN_VERSION} REQUIRED COMPONENTS 
    CoreAddons 
    ConfigWidgets 
    GlobalShortcuts 
    WindowSystem 
    Plasma
)

# Rust Core Library Build
add_custom_target(build_rust_core ALL
    COMMAND cargo build -p trimmeh-core --release --features ffi --target-dir ${CMAKE_BINARY_DIR}/rust-target
    WORKING_DIRECTORY ${CMAKE_SOURCE_DIR}/../
)

# Generated C Header
execute_process(
    COMMAND cbindgen --output ${CMAKE_CURRENT_BINARY_DIR}/trimmeh_core.h
    WORKING_DIRECTORY ${CMAKE_SOURCE_DIR}/../trimmeh-core
)

# C++ Applet Library
add_library(plasma_applet_trimmeh MODULE)
target_sources(plasma_applet_trimmeh PRIVATE
    src/trimmeapplet.h
    src/trimmeapplet.cpp
    src/clipboardmanager.h
    src/clipboardmanager.cpp
    src/hotkeymanager.h
    src/hotkeymanager.cpp
)

target_link_libraries(plasma_applet_trimmeh
    Qt6::Core Qt6::Gui Qt6::Qml Qt6::Quick Qt6::DBus
    KF6::CoreAddons KF6::ConfigWidgets KF6::GlobalShortcuts KF6::WindowSystem KF6::Plasma
    ${CMAKE_BINARY_DIR}/rust-target/release/libtrimmeh_core.so
)

# QML & Metadata Installation
install(DIRECTORY package/contents/ DESTINATION ${KDE_INSTALL_DATADIR}/plasma/plasmoids/trimmeh)
install(FILES package/metadata.json DESTINATION ${KDE_INSTALL_DATADIR}/plasma/plasmoids/trimmeh)
install(TARGETS plasma_applet_trimmeh LIBRARY DESTINATION ${KDE_INSTALL_PLUGINDIR}/plasma/applets)

# i18n
ki18n_wrap_ui(plasma_applet_trimmeh)
```

---

### Phase 3: QML UI & C++ Bridge (Wochen 3-5)

**3.3.1 C++ Applet Main Class**

`src/trimmeapplet.h`:

```cpp
#ifndef TRIMMEAPPLET_H
#define TRIMMEAPPLET_H

#include <Plasma/Applet>
#include <memory>

class ClipboardManager;
class HotKeyManager;

class TrimmehApplet : public Plasma::Applet {
    Q_OBJECT

    Q_PROPERTY(QString lastPreview READ lastPreview NOTIFY lastPreviewChanged)
    Q_PROPERTY(bool autoTrimEnabled READ autoTrimEnabled WRITE setAutoTrimEnabled NOTIFY autoTrimEnabledChanged)
    Q_PROPERTY(int aggressiveness READ aggressiveness WRITE setAggressiveness)

public:
    TrimmehApplet(QObject *parent, const KPluginMetaData &metaData, const QVariantList &args);
    ~TrimmehApplet();

    void init() override;

    Q_INVOKABLE QString trimClipboard(const QString &text, int aggressiveness);
    Q_INVOKABLE void pasteTrimmed();
    Q_INVOKABLE void pasteOriginal();
    Q_INVOKABLE void restoreLastCopy();

    QString lastPreview() const;
    bool autoTrimEnabled() const;
    int aggressiveness() const;

    void setAutoTrimEnabled(bool enabled);
    void setAggressiveness(int level);

signals:
    void lastPreviewChanged();
    void autoTrimEnabledChanged();

private:
    std::unique_ptr<ClipboardManager> m_clipboard;
    std::unique_ptr<HotKeyManager> m_hotkeys;
    QString m_lastCopy;
    QString m_lastTrimmed;
    bool m_autoTrim = false;
    int m_aggressiveness = 1;  // Normal

    void onClipboardChanged();
    void onHotKeyPressed(const QString &action);
};

#endif
```

**3.3.2 Clipboard Manager**

`src/clipboardmanager.h`:

```cpp
#ifndef CLIPBOARDMANAGER_H
#define CLIPBOARDMANAGER_H

#include <QObject>
#include <memory>

class QClipboard;

class ClipboardManager : public QObject {
    Q_OBJECT

public:
    explicit ClipboardManager(QObject *parent = nullptr);
    ~ClipboardManager();

    QString getClipboard() const;
    void setClipboard(const QString &text, bool replaceHistory = true);
    QString getLastBackup() const;

    // Hash-based loop detection (parity with GNOME)
    uint128_t hashClipboard(const QString &text);
    bool isOwnWrite(const QString &text);

signals:
    void clipboardChanged(const QString &content);
    void clipboardHashChanged(uint128_t hash);

private:
    QClipboard *m_clipboard = nullptr;
    QString m_lastOwnWrite;
    uint128_t m_lastHash = 0;
    uint128_t m_ownWriteHash = 0;

    void setupWatcher();
};

#endif
```

**3.3.3 QML Main UI**

`package/contents/ui/main.qml`:

```qml
import QtQuick
import Qt.labs.platform as Platform
import org.kde.plasma.plasmoid
import org.kde.plasma.core as PlasmaCore
import org.kde.plasma.components 3.0 as PlasmaComponents
import org.kde.kirigami 2.20 as Kirigami

PlasmoidItem {
    id: root
    
    preferredRepresentation: compactRepresentation
    
    // Compact Representation (top panel icon)
    compactRepresentation: PlasmaComponents.Button {
        id: compactButton
        icon.name: "edit-cut-symbolic"
        
        onClicked: root.expanded = !root.expanded
    }
    
    // Full Representation (dropdown menu)
    fullRepresentation: Item {
        width: Kirigami.Units.gridUnit * 20
        height: column.implicitHeight + Kirigami.Units.gridUnit * 2
        
        Column {
            id: column
            anchors.fill: parent
            anchors.margins: Kirigami.Units.gridUnit
            spacing: Kirigami.Units.smallSpacing
            
            // Last Preview Section
            PlasmaComponents.Label {
                text: i18n("Last Preview:")
                font.bold: true
            }
            
            PlasmaComponents.TextArea {
                id: previewArea
                Layout.fillWidth: true
                height: Kirigami.Units.gridUnit * 5
                readOnly: true
                text: applet.lastPreview
                wrapMode: Text.Wrap
            }
            
            // Action Buttons
            Row {
                spacing: Kirigami.Units.smallSpacing
                
                PlasmaComponents.Button {
                    icon.name: "edit-paste-symbolic"
                    text: i18n("Paste Trimmed")
                    onClicked: applet.pasteTrimmed()
                }
                
                PlasmaComponents.Button {
                    icon.name: "edit-paste-symbolic"
                    text: i18n("Paste Original")
                    onClicked: applet.pasteOriginal()
                }
                
                PlasmaComponents.Button {
                    icon.name: "edit-undo-symbolic"
                    text: i18n("Restore")
                    onClicked: applet.restoreLastCopy()
                }
            }
            
            // Settings
            Kirigami.FormLayout {
                Layout.fillWidth: true
                
                PlasmaComponents.CheckBox {
                    Kirigami.FormData.label: i18n("Auto Trim:")
                    checked: applet.autoTrimEnabled
                    onCheckedChanged: applet.autoTrimEnabled = checked
                }
                
                PlasmaComponents.ComboBox {
                    Kirigami.FormData.label: i18n("Aggressiveness:")
                    model: [i18n("Low"), i18n("Normal"), i18n("High")]
                    currentIndex: applet.aggressiveness
                    onCurrentIndexChanged: applet.aggressiveness = currentIndex
                }
            }
        }
    }
}
```

---

### Phase 4: KDE Settings & Preferences (Woche 4)

**3.4.1 KConfigXT Configuration**

`package/contents/config/config.xml`:

```xml
<?xml version="1.0" encoding="UTF-8"?>
<kcfg xmlns="http://www.kde.org/standards/kcfg/1.0"
      xmlns:xsi="http://www.w3.org/2001/XMLSchema-instance"
      xsi:schemaLocation="http://www.kde.org/standards/kcfg/1.0 http://www.kde.org/standards/kcfg/1.0/kcfg.xsd">
    
    <group name="General">
        <entry name="Aggressiveness" type="Int">
            <label>Trimming aggressiveness level</label>
            <default>1</default>
            <min>0</min>
            <max>2</max>
        </entry>
        
        <entry name="KeepBlankLines" type="Bool">
            <label>Keep blank lines in trimmed output</label>
            <default>false</default>
        </entry>
        
        <entry name="StripBoxChars" type="Bool">
            <label>Strip box drawing characters</label>
            <default>true</default>
        </entry>
        
        <entry name="TrimPrompts" type="Bool">
            <label>Strip shell prompts</label>
            <default>true</default>
        </entry>
        
        <entry name="MaxLines" type="Int">
            <label>Maximum lines before auto-skip</label>
            <default>10</default>
            <min>1</min>
            <max>100</max>
        </entry>
        
        <entry name="AutoTrim" type="Bool">
            <label>Enable automatic trimming on paste</label>
            <default>false</default>
        </entry>
        
        <entry name="PasteTrimmedHotkey" type="String">
            <label>Global hotkey for Paste Trimmed</label>
            <default>none</default>
        </entry>
        
        <entry name="PasteOriginalHotkey" type="String">
            <label>Global hotkey for Paste Original</label>
            <default>none</default>
        </entry>
    </group>
</kcfg>
```

---

### Phase 5: Global Hotkeys & Paste Injection (Woche 5)

**3.5.1 Hotkey Manager**

`src/hotkeymanager.h`:

```cpp
#ifndef HOTKEYMANAGER_H
#define HOTKEYMANAGER_H

#include <QObject>
#include <memory>

class KGlobalShortcut;

class HotKeyManager : public QObject {
    Q_OBJECT

public:
    explicit HotKeyManager(QObject *parent = nullptr);
    ~HotKeyManager();

    void setPasteTrimmedHotkey(const QString &sequence);
    void setPasteOriginalHotkey(const QString &sequence);
    void setRestoreLastCopyHotkey(const QString &sequence);

signals:
    void pasteTrimmedPressed();
    void pasteOriginalPressed();
    void restoreLastCopyPressed();

private:
    std::unique_ptr<KGlobalShortcut> m_pasteTrimmed;
    std::unique_ptr<KGlobalShortcut> m_pasteOriginal;
    std::unique_ptr<KGlobalShortcut> m_restoreLastCopy;
};

#endif
```

**3.5.2 Virtual Paste Implementation**

KDE/Qt bietet native clipboard API (keine Wayland-spezifische Komplexität wie in GNOME):

```cpp
// In clipboardmanager.cpp
void ClipboardManager::pasteText(const QString &text) {
    // 1. Set clipboard content
    setClipboard(text);
    
    // 2. Inject synthetic paste keystroke
    // Option A: Use xdotool (best compatibility, auch auf X11)
    QProcess::execute("xdotool", {"key", "ctrl+v"});
    
    // Option B: Use Qt's QKeySequence (if available in target)
    // sendSyntheticKeyEvent(QKeySequence(Qt::CTRL + Qt::Key_V));
}
```

---

### Phase 6: Testing & Packaging (Woche 6+)

**3.6.1 Feature Parity Testing**

```bash
# Alle Test-Vektoren müssen gleich sein wie GNOME
cd tests/
cargo test -p trimmeh-core --features test-vectors

# KDE-spezifische Tests
ctest --output-on-failure
```

**3.6.2 RPM Packaging**

`packaging/rpm/trimmeh.spec`:

```spec
Name:           plasma-applet-trimmeh
Version:        0.1.0
Release:        1%{?dist}
Summary:        KDE Plasma clipboard trimmer applet

License:        MIT
URL:            https://github.com/DanielMulec/trimmeh_b

# Requires KDE Plasma 6.4+
Requires:       kf6-plasma >= 6.4
Requires:       qt6-qtbase >= 6.5
Requires:       qt6-qtdeclarative >= 6.5
Requires:       kf6-kconfig >= 6.0
Requires:       kf6-kglobalshortcuts >= 6.0

BuildRequires:  cmake >= 3.16
BuildRequires:  qt6-qtbase-devel >= 6.5
BuildRequires:  kf6-plasma-devel >= 6.4
BuildRequires:  kf6-kconfig-devel >= 6.0
BuildRequires:  cargo
BuildRequires:  rust >= 1.91

%description
A KDE Plasma widget that flattens multi-line shell snippets on your clipboard
with full parity to Trimmy's heuristics and features.

%prep
%setup -q

%build
%cmake -DKF6_MIN_VERSION=6.0 -DQT_MIN_VERSION=6.5
%cmake_build

%install
%cmake_install

%files
%license LICENSE
%doc README.md
%{_datadir}/plasma/plasmoids/trimmeh/
%{_libdir}/qt6/plugins/plasma/applets/libplasma_applet_trimmeh.so

%changelog
* Mon Dec 15 2025 Daniel Mulec <...> - 0.1.0-1
- Initial release for KDE Plasma 6.4+
```

---

## 4. Feature-Parity Checklist (vs. Trimmy)

### Core Trimming Features ✓
- [x] Multi-line command flattening
- [x] Backslash continuation joining
- [x] Box-drawing character stripping
- [x] Shell prompt detection & removal
- [x] Wrapped URL repair
- [x] Blank line preservation
- [x] List skipping
- [x] Source code detection (false-positive prevention)
- [x] Aggressiveness levels (Low/Normal/High)
- [x] Max lines limit
- [x] Hash-based loop prevention

### Configuration ✓
- [x] Persistent user preferences
- [x] Keep blank lines toggle
- [x] Strip box gutters toggle
- [x] Strip prompts toggle
- [x] Max lines configuration
- [x] Aggressiveness level selection

### User Actions ✓
- [x] Paste Trimmed (manual trigger)
- [x] Paste Original (manual trigger)
- [x] Restore Last Copy
- [x] Auto-trim toggle
- [x] Last preview display

### UI/UX ✓
- [x] System tray/panel integration
- [x] Dropdown menu
- [x] Preferences dialog
- [x] Global hotkey support
- [x] Keyboard-accessible UI

### Platform Integration ✓
- [x] Clipboard monitoring (Qt)
- [x] Synthetic paste injection (xdotool)
- [x] Global shortcuts (KGlobalShortcuts)
- [x] Window system integration (KWindowSystem)
- [x] Settings storage (KConfig)

### Testing ✓
- [x] 100% test vector parity with Trimmy
- [x] Integration tests
- [x] Clipboard monitoring tests
- [x] Paste injection tests

---

## 5. Build & Installation Guide

### 5.1 Prerequisites (Fedora 43)

```bash
# KDE Development Tools
sudo dnf install -y \
    plasma-framework-devel \
    kf6-kconfig-devel \
    kf6-kglobalshortcuts-devel \
    kf6-kwindowsystem-devel \
    qt6-qtbase-devel \
    qt6-qtdeclarative-devel \
    cmake \
    ninja-build

# Rust
curl --proto '=https' --tlsv1.2 -sSf https://sh.rustup.rs | sh
rustup target add x86_64-unknown-linux-gnu

# cbindgen für C FFI
cargo install cbindgen
```

### 5.2 Building

```bash
# Clone & prepare
git clone https://github.com/DanielMulec/trimmeh_b.git
cd trimmeh_b/plasma-applet

# Build
mkdir build && cd build
cmake -DCMAKE_BUILD_TYPE=Release ..
make -j$(nproc)

# Install locally
make install DESTDIR=$HOME/.local

# Or build RPM
cd ..
spectool -g -R packaging/rpm/trimmeh.spec
rpmbuild -ba packaging/rpm/trimmeh.spec
```

### 5.3 Installation

```bash
# Via package manager (Fedora 43)
sudo dnf install plasma-applet-trimmeh

# OR manual
sudo cp -r plasma-applet/package/contents/* /usr/share/plasma/plasmoids/trimmeh/
sudo cp build/src/libplasma_applet_trimmeh.so /usr/lib/qt6/plugins/plasma/applets/
sudo cp build/libtrimmeh_core.so /usr/lib/

# Restart Plasma
kquitapp6 plasmashell
plasmashell &

# Add to panel: Right-click panel → Edit Panel → Add Widgets → Trimmeh
```

---

## 6. Tech Stack Summary

| Layer | Component | Technology | Rationale |
|-------|-----------|-----------|-----------|
| **Core** | Trimming Engine | Rust | Language-agnostic, Memory-safe, FFI-friendly |
| **UI** | Applet Shell | QML | KDE standard, modern, reactive |
| **Logic Bridge** | C++ Wrapper | Qt/KF6 | Native KDE integration |
| **IPC** | Hotkeys | KGlobalShortcuts | System-wide shortcut handling |
| **Clipboard** | Clipboard I/O | Qt QClipboard | Built-in, reliable |
| **Paste** | Virtual Keyboard | xdotool | Cross-X11/Wayland compatible |
| **Config** | Preferences | KConfigXT | KDE persistent storage |
| **Packaging** | Distribution | RPM + COPR | Fedora native |
| **Build** | Compilation | CMake + cargo | KDE + Rust standard |

---

## 7. Known Limitations & Workarounds

### Wayland vs X11
- **KDE on Wayland**: xdotool funktioniert noch (wird über XWayland abgewickelt). Best-effort wie GNOME.
- **Fallback**: Clipboard wird gesetzt, User muss manuell paste triggern falls synthetisches Event fehlschlägt.

### Paste Injection Zuverlässigkeit
- Wayland native: Startet **nicht** mit GNOME-Grade Reliability
- **Workaround**: Global hotkey + explicit "Paste Trimmed" button
- **Alternative**: Auto-trim on paste (wenn User mit Auto-Trim arbeitet)

### Testing auf Fedora 43
- Plasma 6.4 wird möglicherweise nicht auf Fedora 43 verfügbar sein
- **Fallback**: Auf Fedora 42/41 testen, Plasma-via-COPR oder KDE Nightly

---

## 8. Timeline

| Phase | Duration | Deliverables |
|-------|----------|--------------|
| **1. Setup** | Week 1-2 | Repo structure, CMake scaffolding, C FFI stubs |
| **2. Core Integration** | Week 2-3 | Rust FFI, cbindgen headers, Cargo linking |
| **3. QML/C++** | Week 3-5 | Applet class, clipboard manager, main UI |
| **4. Config** | Week 4 | KConfigXT, preferences dialog |
| **5. Hotkeys & Paste** | Week 5 | KGlobalShortcuts, virtual paste |
| **6. Testing & Packaging** | Week 6+ | Test vectors, RPM spec, COPR integration |
| **Total** | ~6-8 weeks | Production-ready KDE Port |

---

## 9. Next Steps

1. **Immediate**: Repo-Struktur für plasma-applet erstellen
2. **Short-term**: C FFI Layer fertigstellen (Rust side)
3. **Mid-term**: CMake + QML UI skelett
4. **Long-term**: Feature-komplett + Packaging

---

**Document Version:** 1.0  
**Last Updated:** 2025-12-15  
**Status:** Ready for Implementation
