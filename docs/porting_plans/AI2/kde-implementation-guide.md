# KDE Plasma Trimmeh Port — Detaillierter Implementation Guide

## Quick Start für Developer

### Git Repository Setup

```bash
# Nach dem Klonen:
cd trimmeh_b
git submodule add https://github.com/KDE/kdeclarative.git 3rdparty/kdeclarative  # Optional
git checkout main

# Branch für KDE Port
git checkout -b feature/kde-plasma-port
```

### Initial Scaffolding (kopieren & anpassen)

```bash
# Struktur erstellen
mkdir -p plasma-applet/{src,package/contents/{ui,config,code},packaging}

# CMakeLists.txt Template (siehe unten)
touch plasma-applet/CMakeLists.txt
touch plasma-applet/src/CMakeLists.txt

# Metadata
cat > plasma-applet/metadata.json << 'EOF'
{
  "KPlugin": {
    "Description": "Clipboard command flattener",
    "Icon": "edit-cut",
    "Id": "org.kde.plasma.trimmeh",
    "License": "MIT",
    "Name": "Trimmeh",
    "ServiceTypes": ["Plasma/Applet"]
  },
  "X-Plasma-API": "declarativeappletscript",
  "X-Plasma-MainScript": "ui/main.qml",
  "X-KDE-PluginInfo-Version": "0.1.0"
}
EOF
```

---

## C FFI Implementation Details

### Step 1: Add FFI Module to Rust Core

**File: `trimmeh-core/src/lib.rs` (modify)**

```rust
#[cfg(feature = "ffi")]
pub mod ffi;

// Export public API
pub use ffi::*;
```

**File: `trimmeh-core/src/ffi.rs` (new)**

```rust
#![allow(non_camel_case_types)]

use std::ptr;
use std::ffi::{CStr, CString};
use std::os::raw::c_char;
use crate::{Aggressiveness, Options, TrimReason, trim};

/// C-compatible trim result
#[repr(C)]
pub struct TrimResult_C {
    /// Pointer to output string (null-terminated)
    pub output: *mut c_char,
    /// Length of output string (excluding null terminator)
    pub output_len: usize,
    /// Whether the input was modified
    pub changed: bool,
    /// Reason for modification (0=None, 1=Flattened, 2=PromptStripped, ...)
    pub reason: u8,
    /// Blake3 hash as hex string
    pub hash: *mut c_char,
}

impl TrimResult_C {
    fn from_rust_result(result: crate::TrimResult) -> Self {
        let output_cstring = CString::new(result.output.clone())
            .unwrap_or_else(|_| CString::new("").unwrap());
        let hash_hex = format!("{:032x}", result.hash);
        let hash_cstring = CString::new(hash_hex)
            .unwrap_or_else(|_| CString::new("").unwrap());

        let reason_code = match result.reason {
            None => 0,
            Some(TrimReason::Flattened) => 1,
            Some(TrimReason::PromptStripped) => 2,
            Some(TrimReason::BoxCharsRemoved) => 3,
            Some(TrimReason::BackslashMerged) => 4,
            Some(TrimReason::SkippedTooLarge) => 5,
        };

        TrimResult_C {
            output: output_cstring.into_raw(),
            output_len: result.output.len(),
            changed: result.changed,
            reason: reason_code,
            hash: hash_cstring.into_raw(),
        }
    }
}

/// Main trimming function exposed to C/C++
/// 
/// # Safety
/// - Caller must ensure input pointer is valid UTF-8
/// - Caller must free returned TrimResult_C with trim_free()
#[no_mangle]
pub unsafe extern "C" fn trim_c(
    input: *const c_char,
    aggressiveness: u8,
    keep_blank_lines: bool,
    strip_box_chars: bool,
    trim_prompts: bool,
    max_lines: usize,
) -> TrimResult_C {
    if input.is_null() {
        return TrimResult_C {
            output: CString::new("").unwrap().into_raw(),
            output_len: 0,
            changed: false,
            reason: 0,
            hash: CString::new("").unwrap().into_raw(),
        };
    }

    let input_str = match CStr::from_ptr(input).to_str() {
        Ok(s) => s,
        Err(_) => return TrimResult_C {
            output: CString::new("").unwrap().into_raw(),
            output_len: 0,
            changed: false,
            reason: 0,
            hash: CString::new("").unwrap().into_raw(),
        },
    };

    let aggr = match aggressiveness {
        0 => Aggressiveness::Low,
        2 => Aggressiveness::High,
        _ => Aggressiveness::Normal,
    };

    let opts = Options {
        keep_blank_lines,
        strip_box_chars,
        trim_prompts,
        max_lines,
    };

    let result = trim(input_str, aggr, opts);
    TrimResult_C::from_rust_result(result)
}

/// Free TrimResult_C allocated by trim_c()
#[no_mangle]
pub unsafe extern "C" fn trim_free(result: *mut TrimResult_C) {
    if result.is_null() {
        return;
    }
    let result_box = Box::from_raw(result);
    
    // Free allocated strings
    if !result_box.output.is_null() {
        let _ = CString::from_raw(result_box.output);
    }
    if !result_box.hash.is_null() {
        let _ = CString::from_raw(result_box.hash);
    }
    // result_box is dropped here
}

/// Hash a string for loop detection
#[no_mangle]
pub unsafe extern "C" fn trimmeh_hash(input: *const c_char) -> *mut c_char {
    if input.is_null() {
        return CString::new("").unwrap().into_raw();
    }

    let input_str = match CStr::from_ptr(input).to_str() {
        Ok(s) => s,
        Err(_) => return CString::new("").unwrap().into_raw(),
    };

    let hash = blake3::hash(input_str.as_bytes());
    let hash_hex = format!("{:x}", hash);
    CString::new(hash_hex).unwrap().into_raw()
}

/// Free hash string
#[no_mangle]
pub unsafe extern "C" fn trimmeh_hash_free(ptr: *mut c_char) {
    if !ptr.is_null() {
        let _ = CString::from_raw(ptr);
    }
}
```

### Step 2: Update Cargo.toml

**File: `trimmeh-core/Cargo.toml`**

```toml
[package]
name = "trimmeh-core"
version = "0.1.0"
edition = "2021"
license = "MIT"
description = "Clipboard command flattener core"

[lib]
crate-type = ["rlib", "cdylib", "staticlib"]

[features]
default = ["ffi"]
ffi = []
wasm = ["wasm-bindgen", "serde", "serde-wasm-bindgen"]

[dependencies]
blake3 = "1.5"
regex = "1.11"
once_cell = "1.19"
wasm-bindgen = { version = "0.2.93", optional = true }
serde = { version = "1.0", features = ["derive"], optional = true }
serde-wasm-bindgen = { version = "0.6", optional = true }

[dev-dependencies]
serde = { version = "1.0", features = ["derive"] }
serde_json = "1.0"
```

### Step 3: Generate C Header with cbindgen

**File: `cbindgen.toml` (root)**

```toml
language = "C"
namespace = ""
autoimport_added_structures = true
sys_includes = ["stdint.h", "stdbool.h"]

[export]
include = ["TrimResult_C"]
```

**Build script:**

```bash
cd trimmeh-core
cargo build --release --features ffi
cbindgen --output ../plasma-applet/src/trimmeh_core.h
```

**Generated header: `plasma-applet/src/trimmeh_core.h`**

```c
#ifndef TRIMMEH_CORE_H
#define TRIMMEH_CORE_H

#include <stdint.h>
#include <stdbool.h>

typedef struct {
    char *output;
    size_t output_len;
    bool changed;
    uint8_t reason;
    char *hash;
} TrimResult_C;

TrimResult_C trim_c(
    const char *input,
    uint8_t aggressiveness,
    bool keep_blank_lines,
    bool strip_box_chars,
    bool trim_prompts,
    size_t max_lines
);

void trim_free(TrimResult_C *result);

char *trimmeh_hash(const char *input);
void trimmeh_hash_free(char *ptr);

#endif
```

---

## C++ Applet Implementation

### ClipboardManager Implementation

**File: `src/clipboardmanager.cpp`**

```cpp
#include "clipboardmanager.h"
#include "trimmeh_core.h"

#include <QClipboard>
#include <QGuiApplication>
#include <QMimeData>
#include <QTimer>
#include <QDebug>
#include <cstring>

ClipboardManager::ClipboardManager(QObject *parent)
    : QObject(parent)
    , m_clipboard(QGuiApplication::clipboard())
{
    // Monitor clipboard changes
    connect(m_clipboard, &QClipboard::dataChanged, this, &ClipboardManager::onClipboardChanged);
    
    // Also check periodically for X11 compatibility
    QTimer *timer = new QTimer(this);
    connect(timer, &QTimer::timeout, this, &ClipboardManager::onClipboardChanged);
    timer->start(1000);  // Check every second
}

ClipboardManager::~ClipboardManager() = default;

QString ClipboardManager::getClipboard() const
{
    const QMimeData *mimeData = m_clipboard->mimeData();
    if (!mimeData || !mimeData->hasText()) {
        return QString();
    }
    return mimeData->text();
}

void ClipboardManager::setClipboard(const QString &text, bool replaceHistory)
{
    QMimeData *mimeData = new QMimeData();
    mimeData->setText(text);
    
    if (replaceHistory) {
        m_clipboard->setMimeData(mimeData, QClipboard::Clipboard);
    } else {
        m_clipboard->setMimeData(mimeData, QClipboard::Selection);
    }
    
    // Mark as own write for loop detection
    m_lastOwnWrite = text;
    m_ownWriteHash = hashClipboard(text);
}

QString ClipboardManager::getLastBackup() const
{
    return m_lastCopy;
}

uint128_t ClipboardManager::hashClipboard(const QString &text)
{
    const char *c_text = text.toUtf8().constData();
    char *hash_hex = trimmeh_hash(c_text);
    
    if (!hash_hex) {
        return 0;
    }
    
    // Parse hex string to uint128_t
    // For simplicity, just use first 32 chars (128 bits in hex)
    uint128_t hash = 0;
    std::sscanf(hash_hex, "%llx", &hash);
    
    trimmeh_hash_free(hash_hex);
    return hash;
}

bool ClipboardManager::isOwnWrite(const QString &text)
{
    // Check if this text was recently written by us
    return hashClipboard(text) == m_ownWriteHash;
}

void ClipboardManager::onClipboardChanged()
{
    QString currentText = getClipboard();
    
    if (currentText.isEmpty()) {
        return;
    }
    
    // Skip if this is our own write (loop prevention)
    if (isOwnWrite(currentText)) {
        return;
    }
    
    emit clipboardChanged(currentText);
    
    // Update hash for next check
    uint128_t newHash = hashClipboard(currentText);
    m_lastHash = newHash;
    emit clipboardHashChanged(newHash);
}

QString ClipboardManager::trim(
    const QString &input,
    int aggressiveness,
    bool keepBlankLines,
    bool stripBoxChars,
    bool trimPrompts,
    int maxLines
)
{
    const char *c_input = input.toUtf8().constData();
    
    TrimResult_C result = trim_c(
        c_input,
        static_cast<uint8_t>(aggressiveness),
        keepBlankLines,
        stripBoxChars,
        trimPrompts,
        static_cast<size_t>(maxLines)
    );
    
    QString output = QString::fromUtf8(result.output);
    trim_free(&result);
    
    return output;
}
```

### TrimmerApplet Implementation

**File: `src/trimmeapplet.cpp` (excerpt)**

```cpp
#include "trimmeapplet.h"
#include "clipboardmanager.h"
#include "hotkeymanager.h"

#include <Plasma/Applet>
#include <KConfigGroup>
#include <KSharedConfig>
#include <QClipboard>
#include <QGuiApplication>
#include <QDebug>

TrimmehApplet::TrimmehApplet(QObject *parent, const KPluginMetaData &metaData, const QVariantList &args)
    : Plasma::Applet(parent, metaData, args)
    , m_clipboard(std::make_unique<ClipboardManager>())
    , m_hotkeys(std::make_unique<HotKeyManager>())
    , m_aggressiveness(1)
    , m_autoTrim(false)
{
    setHasConfigurationInterface(true);
    
    // Load configuration
    KConfigGroup cfg = config();
    m_aggressiveness = cfg.readEntry("Aggressiveness", 1);
    m_autoTrim = cfg.readEntry("AutoTrim", false);
}

TrimmehApplet::~TrimmehApplet() = default;

void TrimmehApplet::init()
{
    Plasma::Applet::init();
    
    // Connect clipboard signals
    connect(m_clipboard.get(), &ClipboardManager::clipboardChanged,
            this, &TrimmehApplet::onClipboardChanged);
    
    // Connect hotkey signals
    connect(m_hotkeys.get(), &HotKeyManager::pasteTrimmedPressed,
            this, &TrimmehApplet::pasteTrimmed);
    connect(m_hotkeys.get(), &HotKeyManager::pasteOriginalPressed,
            this, &TrimmehApplet::pasteOriginal);
    connect(m_hotkeys.get(), &HotKeyManager::restoreLastCopyPressed,
            this, &TrimmehApplet::restoreLastCopy);
}

QString TrimmehApplet::trimClipboard(const QString &text, int aggressiveness)
{
    KConfigGroup cfg = config();
    
    bool keepBlankLines = cfg.readEntry("KeepBlankLines", false);
    bool stripBoxChars = cfg.readEntry("StripBoxChars", true);
    bool trimPrompts = cfg.readEntry("TrimPrompts", true);
    int maxLines = cfg.readEntry("MaxLines", 10);
    
    QString trimmed = m_clipboard->trim(
        text,
        aggressiveness,
        keepBlankLines,
        stripBoxChars,
        trimPrompts,
        maxLines
    );
    
    m_lastPreview = trimmed;
    emit lastPreviewChanged();
    
    return trimmed;
}

void TrimmehApplet::pasteTrimmed()
{
    QString current = m_clipboard->getClipboard();
    m_lastCopy = current;
    
    QString trimmed = trimClipboard(current, m_aggressiveness);
    m_lastTrimmed = trimmed;
    
    // Set clipboard and inject paste
    m_clipboard->setClipboard(trimmed);
    injectPaste();
}

void TrimmehApplet::pasteOriginal()
{
    m_clipboard->setClipboard(m_lastCopy);
    injectPaste();
}

void TrimmehApplet::restoreLastCopy()
{
    m_clipboard->setClipboard(m_lastCopy);
}

void TrimmehApplet::injectPaste()
{
    // Use xdotool for cross-X11/Wayland compatibility
    QProcess process;
    process.start("xdotool", {"key", "ctrl+v"});
    process.waitForFinished(1000);
}

void TrimmehApplet::onClipboardChanged()
{
    QString current = m_clipboard->getClipboard();
    
    if (m_autoTrim && !current.isEmpty()) {
        QString trimmed = trimClipboard(current, m_aggressiveness);
        if (trimmed != current) {
            // Auto-trim enabled: update clipboard silently
            m_clipboard->setClipboard(trimmed);
        }
    }
}

void TrimmehApplet::setAutoTrimEnabled(bool enabled)
{
    m_autoTrim = enabled;
    KConfigGroup cfg = config();
    cfg.writeEntry("AutoTrim", m_autoTrim);
    cfg.sync();
    emit autoTrimEnabledChanged();
}

void TrimmehApplet::setAggressiveness(int level)
{
    m_aggressiveness = qBound(0, level, 2);
    KConfigGroup cfg = config();
    cfg.writeEntry("Aggressiveness", m_aggressiveness);
    cfg.sync();
}

bool TrimmehApplet::autoTrimEnabled() const
{
    return m_autoTrim;
}

int TrimmehApplet::aggressiveness() const
{
    return m_aggressiveness;
}

QString TrimmehApplet::lastPreview() const
{
    return m_lastPreview;
}

K_PLUGIN_CLASS(TrimmehApplet)
#include "trimmeapplet.moc"
```

---

## CMake Configuration

### Main CMakeLists.txt

**File: `plasma-applet/CMakeLists.txt`**

```cmake
cmake_minimum_required(VERSION 3.16)
project(plasma-applet-trimmeh)

# Version
set(PROJECT_VERSION "0.1.0")
set(PROJECT_VERSION_MAJOR 0)

set(KF6_MIN_VERSION "6.0.0")
set(QT_MIN_VERSION "6.5.0")

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_INCLUDE_CURRENT_DIR ON)
set(CMAKE_AUTOMOC ON)
set(CMAKE_AUTORCC ON)

find_package(ECM ${KF6_MIN_VERSION} REQUIRED NO_MODULE)
set(CMAKE_MODULE_PATH ${ECM_MODULE_PATH})

include(KDEInstallDirs)
include(KDECompilerSettings)
include(KDECMakeSettings)
include(ECMFindModuleHelpers)

find_package(Qt6 ${QT_MIN_VERSION} REQUIRED COMPONENTS 
    Core 
    Gui 
    Qml 
    Quick 
    DBus
)

find_package(KF6 ${KF6_MIN_VERSION} REQUIRED COMPONENTS 
    CoreAddons 
    ConfigWidgets 
    GlobalShortcuts 
    WindowSystem 
    Plasma
)

find_package(Plasma ${KF6_MIN_VERSION} REQUIRED)

# Build Rust Core
message(STATUS "Building Rust trimmeh-core...")
add_custom_target(build_rust_core ALL
    COMMAND cargo build -p trimmeh-core --release --features ffi --target-dir ${CMAKE_BINARY_DIR}/rust-target
    WORKING_DIRECTORY ${CMAKE_SOURCE_DIR}/../
    BYPRODUCTS ${CMAKE_BINARY_DIR}/rust-target/release/libtrimmeh_core.so
)

# Generate C Header with cbindgen
message(STATUS "Generating C header from Rust FFI...")
execute_process(
    COMMAND which cbindgen
    OUTPUT_VARIABLE CBINDGEN_PATH
    OUTPUT_STRIP_TRAILING_WHITESPACE
)

if(NOT CBINDGEN_PATH)
    message(FATAL_ERROR "cbindgen not found. Install with: cargo install cbindgen")
endif()

execute_process(
    COMMAND ${CBINDGEN_PATH} -c cbindgen.toml -o ${CMAKE_CURRENT_BINARY_DIR}/trimmeh_core.h
    WORKING_DIRECTORY ${CMAKE_SOURCE_DIR}/../trimmeh-core
)

add_subdirectory(src)

# Installation
install(DIRECTORY package/contents/ DESTINATION ${KDE_INSTALL_DATADIR}/plasma/plasmoids/trimmeh)
install(FILES package/metadata.json DESTINATION ${KDE_INSTALL_DATADIR}/plasma/plasmoids/trimmeh)
install(DIRECTORY package/contents/config/ DESTINATION ${KDE_INSTALL_KSERVICES6DIR}/../plasma/plasmoids/trimmeh/config)

message(STATUS "Plasma Trimmeh applet configured successfully")
```

### Source CMakeLists.txt

**File: `plasma-applet/src/CMakeLists.txt`**

```cmake
add_library(plasma_applet_trimmeh MODULE)

target_sources(plasma_applet_trimmeh PRIVATE
    trimmeapplet.h
    trimmeapplet.cpp
    clipboardmanager.h
    clipboardmanager.cpp
    hotkeymanager.h
    hotkeymanager.cpp
)

include_directories(${CMAKE_CURRENT_BINARY_DIR}/..)

target_link_libraries(plasma_applet_trimmeh
    Qt6::Core
    Qt6::Gui
    Qt6::Qml
    Qt6::Quick
    Qt6::DBus
    KF6::CoreAddons
    KF6::ConfigWidgets
    KF6::GlobalShortcuts
    KF6::WindowSystem
    KF6::Plasma
    Plasma::Core
    ${CMAKE_BINARY_DIR}/rust-target/release/libtrimmeh_core.so
)

# Link against blake3 if needed
target_link_options(plasma_applet_trimmeh PRIVATE -Wl,-rpath,${CMAKE_BINARY_DIR}/rust-target/release)

install(TARGETS plasma_applet_trimmeh
    LIBRARY DESTINATION ${KDE_INSTALL_PLUGINDIR}/plasma/applets
)
```

---

## Testing Strategy

### Unit Tests (Rust)

```bash
# Run all Rust tests with verbose output
cd trimmeh-core
cargo test --features ffi -- --nocapture

# Run specific test
cargo test --test trim_vectors -- --nocapture
```

### Integration Tests (C++)

**File: `tests/test_clipboard_manager.cpp`**

```cpp
#include <gtest/gtest.h>
#include "../src/clipboardmanager.h"

class ClipboardManagerTest : public ::testing::Test {
protected:
    ClipboardManager clipboard;
};

TEST_F(ClipboardManagerTest, TrimBasicCommand) {
    QString input = "echo hello\necho world";
    QString result = clipboard.trim(input, 1, false, true, true, 10);
    EXPECT_EQ(result, "echo hello echo world");
}

TEST_F(ClipboardManagerTest, PreservesBlankLines) {
    QString input = "line1\n\nline2";
    QString result = clipboard.trim(input, 2, true, true, true, 10);
    EXPECT_EQ(result, "line1\n\nline2");
}

TEST_F(ClipboardManagerTest, StripBoxChars) {
    QString input = "│ echo hello\n│ echo world";
    QString result = clipboard.trim(input, 1, false, true, true, 10);
    EXPECT_TRUE(result.indexOf("│") == -1);
}

TEST_F(ClipboardManagerTest, IsOwnWrite) {
    QString text = "test content";
    clipboard.setClipboard(text);
    EXPECT_TRUE(clipboard.isOwnWrite(text));
}
```

---

## Fedora 43 RPM Spec

**File: `packaging/rpm/trimmeh.spec`**

```spec
%global qt_version 6.5.0
%global kf_version 6.0.0

Name:           plasma-applet-trimmeh
Version:        0.1.0
Release:        1%{?dist}
Summary:        KDE Plasma clipboard command flattener applet

License:        MIT
URL:            https://github.com/DanielMulec/trimmeh_b
Source0:        %{url}/archive/v%{version}.tar.gz

Requires:       kf6-plasma >= %{kf_version}
Requires:       qt6-qtbase >= %{qt_version}
Requires:       qt6-qtdeclarative >= %{qt_version}
Requires:       kf6-kconfig >= %{kf_version}
Requires:       kf6-kglobalshortcuts >= %{kf_version}
Requires:       xdotool

BuildRequires:  cmake >= 3.16
BuildRequires:  gcc-c++
BuildRequires:  qt6-qtbase-devel >= %{qt_version}
BuildRequires:  qt6-qtdeclarative-devel >= %{qt_version}
BuildRequires:  kf6-plasma-devel >= %{kf_version}
BuildRequires:  kf6-kconfig-devel >= %{kf_version}
BuildRequires:  kf6-kglobalshortcuts-devel >= %{kf_version}
BuildRequires:  kf6-kwindowsystem-devel >= %{kf_version}
BuildRequires:  extra-cmake-modules
BuildRequires:  cargo
BuildRequires:  rust >= 1.91

%description
Trimmeh is a KDE Plasma applet that flattens multi-line shell snippets on your 
clipboard. It's the Wayland-native cousin of Peter Steinberger's Trimmy, with 
full parity in heuristics and features.

%prep
%setup -q -n trimmeh_b-%{version}

%build
%cmake -B build \
    -DCMAKE_BUILD_TYPE=Release \
    -DKF6_MIN_VERSION=%{kf_version} \
    -DQT_MIN_VERSION=%{qt_version}

%cmake_build

%install
%cmake_install

%files
%doc README.md
%license LICENSE
%{_datadir}/plasma/plasmoids/trimmeh/
%{_libdir}/qt6/plugins/plasma/applets/libplasma_applet_trimmeh.so

%post
systemctl --user try-restart plasmashell >/dev/null 2>&1 || true

%postun
systemctl --user try-restart plasmashell >/dev/null 2>&1 || true

%changelog
* Mon Dec 15 2025 Daniel Mulec <daniel@datenpol.at> - 0.1.0-1
- Initial release for KDE Plasma 6.4+
- Full parity with Trimmy and GNOME Trimmeh
```

---

## QML UI Complete Example

**File: `plasma-applet/package/contents/ui/main.qml`**

```qml
import QtQuick
import Qt.labs.platform as Platform
import org.kde.kirigami 2.20 as Kirigami
import org.kde.plasma.plasmoid
import org.kde.plasma.core as PlasmaCore
import org.kde.plasma.components 3.0 as PlasmaComponents

PlasmoidItem {
    id: root
    
    readonly property var appletInterface: plasmoid
    
    // Check if we're in edit mode
    readonly property bool inEditMode: plasmoid.editMode
    
    Plasmoid.preferredRepresentation: Plasmoid.compactRepresentation
    
    compactRepresentation: PlasmaComponents.ToolButton {
        id: compactButton
        icon.name: "edit-cut-symbolic"
        text: i18n("Trimmeh")
        display: PlasmaComponents.AbstractButton.IconOnly
        
        onClicked: root.expanded = !root.expanded
    }
    
    fullRepresentation: Item {
        Layout.minimumWidth: Kirigami.Units.gridUnit * 24
        Layout.minimumHeight: column.implicitHeight + 2 * Kirigami.Units.gridUnit
        Layout.preferredHeight: Layout.minimumHeight
        
        Kirigami.FormLayout {
            id: column
            anchors.fill: parent
            anchors.margins: Kirigami.Units.gridUnit
            
            // Preview Section
            PlasmaComponents.Label {
                text: i18n("Last Preview:")
                font.bold: true
            }
            
            PlasmaComponents.ScrollView {
                Layout.fillWidth: true
                Layout.preferredHeight: Kirigami.Units.gridUnit * 6
                
                PlasmaComponents.TextArea {
                    id: previewArea
                    readOnly: true
                    text: appletInterface.lastPreview || i18n("(empty)")
                    wrapMode: TextEdit.Wrap
                    background: Rectangle {
                        color: Kirigami.Theme.backgroundColor
                        border.color: Kirigami.Theme.separatorColor
                        border.width: 1
                    }
                }
            }
            
            // Actions
            Row {
                Layout.fillWidth: true
                spacing: Kirigami.Units.smallSpacing
                
                PlasmaComponents.Button {
                    icon.name: "edit-paste-symbolic"
                    text: i18n("Paste Trimmed")
                    Layout.fillWidth: true
                    onClicked: appletInterface.pasteTrimmed()
                }
                
                PlasmaComponents.Button {
                    icon.name: "document-revert-symbolic"
                    text: i18n("Restore")
                    Layout.fillWidth: true
                    onClicked: appletInterface.restoreLastCopy()
                }
            }
            
            // Settings
            PlasmaComponents.CheckBox {
                text: i18n("Enable Auto-Trim")
                checked: appletInterface.autoTrimEnabled
                onCheckedChanged: appletInterface.autoTrimEnabled = checked
            }
            
            PlasmaComponents.ComboBox {
                Kirigami.FormData.label: i18n("Aggressiveness:")
                model: [i18n("Low"), i18n("Normal"), i18n("High")]
                currentIndex: appletInterface.aggressiveness
                onCurrentIndexChanged: appletInterface.aggressiveness = currentIndex
            }
        }
    }
}
```

---

## Build & Test Commands

```bash
# Clean build
cd plasma-applet
rm -rf build
mkdir build && cd build

# Configure
cmake -DCMAKE_BUILD_TYPE=Release -DCMAKE_INSTALL_PREFIX=/usr ..

# Build
cmake --build . -j$(nproc)

# Run tests
ctest --output-on-failure

# Install locally
cmake --install . --prefix ~/.local

# Restart Plasma
kquitapp6 plasmashell & sleep 2 && plasmashell &
```

---

**Version:** 1.1  
**Last Updated:** 2025-12-15  
**Status:** Ready for Development
