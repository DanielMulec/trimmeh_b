# KDE Trimmeh Port — Architektur, Best Practices & Fallstricke

## 1. Architekturelle Best Practices

### 1.1 Separation of Concerns

```
┌─────────────────────────────────────────────────────────┐
│ QML Layer (Presentation & Events)                       │
│ - Nur UI-Logik                                          │
│ - Input/Output                                          │
│ - Keine Business Logic                                  │
└────────────────┬────────────────────────────────────────┘
                 │ Q_INVOKABLE / signals/slots
┌────────────────▼────────────────────────────────────────┐
│ C++ Layer (TrimmehApplet, Managers)                     │
│ - Clipboard I/O                                         │
│ - Clipboard Monitoring                                  │
│ - Hotkey Binding                                        │
│ - Config Management                                     │
└────────────────┬────────────────────────────────────────┘
                 │ FFI (C extern functions)
┌────────────────▼────────────────────────────────────────┐
│ Rust Core (lib.rs)                                      │
│ - Pure trimming algorithm                               │
│ - No I/O, no platform-specific code                     │
│ - 100% testable, 100% reusable                          │
└─────────────────────────────────────────────────────────┘
```

**Golden Rule:** Rust core kennt NICHTS über Clipboard/Qt/KDE.

### 1.2 Error Handling Strategy

```cpp
// ✅ GOOD: Graceful fallback
if (cbindgen not found) {
    // Use pre-generated header from source tree
    #include "trimmeh_core.h"  // Fallback
}

// ❌ BAD: Hard failure
#include <generated_at_compile_time.h>

// ✅ GOOD: Null checks at FFI boundary
if (result == nullptr) {
    qWarning() << "trim_c returned null";
    return original_text;
}

// ❌ BAD: Dereference without checks
auto output = result->output;  // Unverified!
```

### 1.3 Memory Safety (Rust ↔ C++)

**Memory Ownership Model:**

```
Rust allocates → C++ owns → Must call trim_free()

// Bad: Memory leak
for (int i = 0; i < 1000; i++) {
    trim_c(...);  // ← No trim_free() called!
}

// Good: Proper cleanup
TrimResult_C result = trim_c(...);
QString output = QString::fromUtf8(result.output);
trim_free(&result);  // ← Cleanup happens here

// Better: RAII wrapper
class TrimResultWrapper {
    TrimResult_C result;
public:
    ~TrimResultWrapper() { trim_free(&result); }
    // Automatic cleanup on scope exit
};
```

---

## 2. Common Pitfalls & Solutions

### 2.1 Clipboard Monitoring Loops

**Problem:**
```
User copies text
→ clipboardChanged signal
→ Auto-trim enabled
→ We modify clipboard
→ Our modification triggers another clipboardChanged signal
→ LOOP!
```

**Solution: Hash-based Loop Detection**
```cpp
// Store hash of our own writes
m_ownWriteHash = blake3_hash(trimmed_text);

// On clipboard change:
if (blake3_hash(current_text) == m_ownWriteHash) {
    return;  // Skip, we wrote this
}
```

### 2.2 Wayland Paste Injection Unreliability

**Problem:**
- Some apps ignore synthetic keyboard events on Wayland
- xdotool may not work in all Wayland sessions

**Solutions:**
```cpp
// Approach 1: Provide manual action
PlasmaComponents.Button {
    text: "Paste Trimmed"
    onClicked: {
        clipboard.setClipboard(trimmed);
        injectPaste();  // User expects this to work
    }
}

// Approach 2: Auto-trim without paste injection
// (Less surprising than silent paste that doesn't paste)

// Approach 3: Use multiple injection methods
void injectPaste() {
    // Try xdotool first
    QProcess p1;
    p1.start("xdotool", {"key", "ctrl+v"});
    
    // Fallback: notify user to paste manually
    // (Better than invisible failure)
}
```

### 2.3 Qt/KF6 Version Incompatibilities

**Problem:**
- KDE Plasma 6.4 requires Qt 6.5+
- Fedora 43 might not have KF6 6.4 yet

**Solution:**
```cmake
# In CMakeLists.txt
find_package(KF6 6.0.0 REQUIRED)  # Minimal version

# Check feature availability
if(KF6_VERSION VERSION_GREATER_EQUAL 6.4)
    message(STATUS "Using Plasma 6.4+ features")
else()
    message(WARNING "Using compatibility mode for older KF6")
endif()
```

### 2.4 FFI String Encoding Issues

**Problem:**
- Rust uses UTF-8 exclusively
- Qt uses QString (UTF-16 internally)
- C strings are null-terminated char*

**Correct Pattern:**
```cpp
// ✅ Good conversion
QString input = "...";
const char *c_input = input.toUtf8().constData();  // UTF-8 encoding
TrimResult_C result = trim_c(c_input, ...);

QString output = QString::fromUtf8(result.output);  // Convert back
trim_free(&result);  // Must free C-allocated memory

// ❌ BAD: Encoding mismatch
const char *c_input = input.toLatin1().constData();  // Wrong encoding!
// Non-ASCII characters will be corrupted
```

### 2.5 Config Not Persisting

**Problem:**
```cpp
// ❌ Bad: Config written but not synced
KConfigGroup cfg = config();
cfg.writeEntry("AutoTrim", true);
// Config is still in memory, not written to disk!

// ✅ Good: Explicit sync
cfg.writeEntry("AutoTrim", true);
cfg.sync();  // Force write to disk
```

---

## 3. Performance Considerations

### 3.1 Clipboard Monitoring Performance

**Problem:** Checking clipboard every 1000ms may be too frequent.

```cpp
// Bad: Too aggressive
QTimer *timer = new QTimer(this);
connect(timer, &QTimer::timeout, this, &ClipboardManager::check);
timer->start(100);  // Every 100ms = 10x/second = high CPU

// Good: Reasonable balance
timer->start(1000);  // Every 1 second
// Or use native signals if available

// Better: Hybrid approach
if (QClipboard::supportsSelection()) {
    // Use native signals
    connect(m_clipboard, &QClipboard::dataChanged, ...);
} else {
    // Fallback to polling
    timer->start(1000);
}
```

### 3.2 Regex Compilation

**In trimmeh-core:**
```rust
// ✅ Good: Lazy-initialized static regex (happens once)
static RE_BLANK: Lazy<Regex> = Lazy::new(|| {
    Regex::new(r"\n\s*\n").expect("blank placeholder regex")
});

// ❌ Bad: Recompile regex on every call
fn some_function() {
    let re = Regex::new(r"\n\s*\n")?;  // Recompiled every time!
}
```

### 3.3 Clipboard Trimming Thresholds

```cpp
// Max 10 lines default is good for:
// - Most shell commands (< 10 lines)
// - Quick decision making
// - Memory efficiency

// But consider:
if (input.line_count > max_lines) {
    return original;  // Skip large blobs
}

// This prevents accidental trimming of:
// - Multi-paragraph scripts
// - Code snippets
// - Documentation pastes
```

---

## 4. Testing Strategy

### 4.1 Golden Vector Testing

**Alle Test-Vektoren MÜSSEN zwischen GNOME und KDE identisch sein.**

```bash
# Run shared vectors
cargo test -p trimmeh-core -- --include-ignored golden_vectors

# Location: tests/trim-vectors.json
# Format:
{
  "name": "Test Name",
  "input": "multi\nline\ntext",
  "aggressiveness": "normal",
  "options": {
    "keep_blank_lines": false,
    "strip_box_chars": true,
    "trim_prompts": true,
    "max_lines": 10
  },
  "expected": {
    "output": "multi line text",
    "changed": true,
    "reason": "flattened"
  }
}
```

### 4.2 Integration Tests (KDE-spezifisch)

```cpp
// Test clipboard monitoring
TEST(ClipboardTest, MonitoringWorks) {
    ClipboardManager clipboard;
    bool changed = false;
    
    connect(&clipboard, &ClipboardManager::clipboardChanged,
            [&changed]() { changed = true; });
    
    QClipboard *qclip = QGuiApplication::clipboard();
    qclip->setText("new text");
    
    // Process events to allow signals
    QTest::qWait(1100);  // Wait for timer
    
    EXPECT_TRUE(changed);
}

// Test loop prevention
TEST(ClipboardTest, LoopPrevention) {
    ClipboardManager clipboard;
    int changeCount = 0;
    
    connect(&clipboard, &ClipboardManager::clipboardChanged,
            [&changeCount]() { changeCount++; });
    
    QString text = "test content";
    clipboard.setClipboard(text);  // Our write
    
    QTest::qWait(1100);
    
    // Should NOT emit another clipboardChanged for our own write
    EXPECT_EQ(changeCount, 0);
}

// Test paste injection
TEST(AppletTest, PasteTrimmedWorks) {
    TrimmehApplet applet;
    QString input = "  echo\n  hello  ";
    
    QClipboard *qclip = QGuiApplication::clipboard();
    qclip->setText(input);
    
    applet.pasteTrimmed();
    
    // Check if trimmed version is in clipboard
    QString result = qclip->text();
    EXPECT_EQ(result, "echo hello");
}
```

### 4.3 Regression Testing

**Vor jedem Release:**

```bash
#!/bin/bash

# 1. Run all Rust tests
echo "Testing Rust core..."
cargo test -p trimmeh-core
if [ $? -ne 0 ]; then
    echo "Rust tests failed!"
    exit 1
fi

# 2. Run KDE applet tests
echo "Testing KDE applet..."
cd plasma-applet/build
ctest --output-on-failure
if [ $? -ne 0 ]; then
    echo "KDE tests failed!"
    exit 1
fi

# 3. Manual testing on target system
echo "Manual testing on Fedora 43..."
# Clipboard monitoring
# Auto-trim toggle
# Paste Trimmed action
# Restore Last Copy
# Settings persistence
# Global hotkeys (if configured)

echo "All tests passed!"
```

---

## 5. Deployment Checklist

### Pre-Release
- [ ] All Rust tests pass (`cargo test -p trimmeh-core`)
- [ ] All C++ tests pass (`ctest`)
- [ ] Golden vectors match GNOME Trimmeh exactly
- [ ] RPM builds successfully (`rpmbuild -ba trimmeh.spec`)
- [ ] Manual testing on Fedora 43 + Plasma 6.4
- [ ] Memory leak check (valgrind)
- [ ] No regressions in existing features

### Release
- [ ] Tag version in git (`git tag -a v0.1.0`)
- [ ] Build RPM for release
- [ ] Upload to COPR (Fedora community repo)
- [ ] Update README with installation instructions
- [ ] Announce on KDE forums/reddit

### Post-Release
- [ ] Monitor issue tracker
- [ ] Collect user feedback
- [ ] Plan maintenance patches if needed

---

## 6. Debugging Guide

### 6.1 Enable Qt Debug Output

```cpp
// In main.qml or C++
QLoggingCategory::setFilterRules("org.kde.plasma.trimmeh.debug=true");
```

### 6.2 Memory Debugging

```bash
# Check for memory leaks
valgrind --leak-check=full --show-leak-kinds=all \
    plasmashell

# Profile clipboard monitoring
perf record -F 99 plasmashell
perf report
```

### 6.3 FFI Debugging

```cpp
// Add detailed logging at FFI boundary
const char *c_input = input.toUtf8().constData();
qDebug() << "FFI Input:" << input;
qDebug() << "Input length:" << input.length();
qDebug() << "UTF-8 length:" << input.toUtf8().length();

TrimResult_C result = trim_c(c_input, aggressiveness, ...);

qDebug() << "FFI Output:" << result.output;
qDebug() << "Changed:" << result.changed;
qDebug() << "Reason:" << result.reason;
```

### 6.4 Clipboard Debugging

```cpp
// Monitor what's happening in clipboard
void ClipboardManager::onClipboardChanged() {
    QString current = getClipboard();
    qDebug() << "Clipboard changed";
    qDebug() << "Content:" << current.left(50) << "...";
    qDebug() << "Hash:" << hashClipboard(current);
    qDebug() << "Is our write:" << isOwnWrite(current);
}
```

---

## 7. Dependency Management

### Rust Dependencies
- **blake3**: Crypto hash (for loop detection) — stable, no updates needed
- **regex**: Pattern matching — update quarterly
- **once_cell**: Lazy static initialization — stable

**Version Pinning Strategy:**
```toml
# In Cargo.toml
blake3 = "1.5"     # Patch version free (1.5.x)
regex = "1.11"     # Patch version free (1.11.x)
once_cell = "1.19" # Patch version free (1.19.x)
```

### C++ Dependencies (KDE/Qt)
- **KF6 6.0+**: Framework baseline
- **Qt6 6.5+**: Framework baseline
- **cmake 3.16+**: Build system

**Fallback Strategy:**
```cmake
# If specific feature not available in KF6 6.0:
if(KF6_VERSION VERSION_GREATER_EQUAL 6.1)
    target_compile_definitions(applet PRIVATE HAVE_KF6_1_FEATURES)
else()
    # Use older API or skip feature
endif()
```

---

## 8. Platform-Specific Considerations

### Wayland
- ✅ Native clipboard access works
- ✅ Qt handles Wayland transparently
- ⚠️ Synthetic paste injection unreliable (xdotool → XWayland)
- ✅ Global hotkeys work (KGlobalShortcuts is Wayland-aware)

### X11
- ✅ All features work reliably
- ✅ xdotool works natively
- ⚠️ Performance: Polling clipboard every 1s (Qt doesn't expose native X11 signals)

### Hybrid (X11 + Wayland)
```cpp
// Smart detection
#ifdef Q_OS_LINUX
    if (qEnvironmentVariable("WAYLAND_DISPLAY").isEmpty()) {
        // X11 session
        useNativeSignals();
    } else {
        // Wayland session
        useFallbackPolling();
    }
#endif
```

---

## 9. Migration Path from GNOME

If users want to switch from GNOME Trimmeh to KDE:

```bash
# 1. Backup GNOME settings
gsettings dump org.gnome.shell.extensions.trimmeh > ~/trimmeh-gnome-backup.dconf

# 2. Install KDE Trimmeh
sudo dnf install plasma-applet-trimmeh

# 3. Configuration migration (manual for now)
# KDE uses KConfig XML instead of GSettings
# Provide migration script if needed

# 4. Add to panel
# Right-click KDE panel → Add widgets → Trimmeh
```

**Future Enhancement:** Write migration tool to import GNOME settings to KDE config.

---

## 10. Security Considerations

### Clipboard Content
- ⚠️ Clipboard content may contain sensitive data (passwords, API keys, etc.)
- ✅ We only process in-memory, never log to disk
- ✅ Hash (blake3) is deterministic but non-reversible

**Best Practice:**
```cpp
// Never log clipboard content to file
qDebug() << "Clipboard changed";  // ✅ Good
qDebug() << "Content:" << clipboard;  // ❌ Might leak secrets
qDebug() << "Hash:" << hash;  // ✅ Good (non-reversible)
```

### Hash Collisions
- blake3 has 256-bit output (negligible collision probability)
- Use only for loop detection, not security-critical purposes

### Paste Injection
- ✅ We don't execute anything (just paste text)
- ✅ User must explicitly approve via action button
- ⚠️ Auto-trim could paste unexpected content (add safety toggle)

---

## 11. Maintenance Timeline

| Phase | Duration | Focus |
|-------|----------|-------|
| **Alpha (0.1)** | 6-8 weeks | Feature-complete, test coverage |
| **Beta (0.2)** | 2-3 months | Bug fixes, performance tuning, COPR |
| **RC (0.3)** | 1 month | Final testing, documentation |
| **Stable (1.0)** | TBD | Production-ready release |
| **LTS (1.x)** | 12+ months | Security/bug fixes only |

---

**Version:** 1.0  
**Last Updated:** 2025-12-15  
**Status:** Ready for Reference
