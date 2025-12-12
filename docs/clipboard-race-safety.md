# Trimmeh Clipboard Race‑Safety Design (GNOME 49 / GJS)

Purpose: document a race‑safe clipboard watcher architecture and a minimal test harness approach, so future agents can add parity features without destabilizing GNOME Shell.

Context:
- Trimmeh runs **inside `gnome-shell`**. A bad async/re‑entrancy bug can crash the shell or the whole extensions subsystem.
- Rust core (`trimmeh-core`) is pure/single‑threaded; races are almost entirely in the **Shell/GJS** layer.

This doc is **pre‑implementation guidance**. It tells you what to build and why, not the final code.

---

## 1. Failure modes we must prevent

These are the common real‑world race patterns for GNOME clipboard watching:

1. **Stale read → overwrite newer clipboard**
   - Two owner‑change events arrive close together.
   - Event A reads text, trims, then writes after Event B already updated the clipboard.
   - Result: clipboard regresses to “trimmed A”.

2. **Self‑write re‑entrancy loop**
   - We call `set_text`.
   - GNOME emits `owner-change` for our write.
   - We re‑trim our own trimmed output (or restore) repeatedly.

3. **Disable/enable lifecycle races**
   - Extension is disabled/reloaded while async reads or timeouts are still pending.
   - Callback fires after disable and touches stale objects → exceptions → shell instability.

4. **Burst events during “manual” flows**
   - Future “Paste Trimmed/Original” temporarily mutates the clipboard.
   - Those mutations must not trigger auto‑trim.

---

## 2. Design goals / invariants

We want explicit invariants that tests can lock in:

1. **Only the most recent clipboard generation may write.**
2. **Own writes are ignored by hash, not by text equality.**
3. **No callbacks perform work after `disable()`.**
4. **Manual restore/paste flows are shielded from auto‑trim.**
5. **Work is debounced/coalesced to one trim per burst.**

---

## 3. State machine (per selection)

Selections = `CLIPBOARD` and `PRIMARY`.  
Maintain independent state per selection, but same logic.

### States
1. **Idle**
   - No pending work.

2. **Debouncing(gen)**
   - We saw an owner‑change, but wait a short grace delay.
   - If another change arrives, we replace the pending gen and restart the timer.

3. **Processing(gen)**
   - We read clipboard text and (maybe) call wasm trim.
   - If gen becomes stale at any point, abort without writing.

4. **Writing(kind, hash)**
   - We just wrote text ourselves (`kind = trim | restore | manualPaste`).
   - Next owner‑change with matching hash is ignored.

### Transitions (high level)
```
Idle
  └─owner-change→ Debouncing(gen++)

Debouncing(gen)
  ├─owner-change→ Debouncing(gen++)   (cancel/restart timer)
  ├─disable→ Idle                     (cancel timer)
  └─timer fires→ Processing(gen)

Processing(gen)
  ├─disable→ Idle                     (abort)
  ├─read finishes but gen stale→ Idle (abort)
  ├─trim unchanged→ Idle
  └─trim changed→ Writing(trim, newHash)

Writing(kind, hash)
  ├─owner-change w/ same hash→ Idle   (ignore self-write)
  ├─owner-change new hash→ Debouncing(gen++)
  └─disable→ Idle
```

Implementation note: you don’t need an explicit enum in code. A small set of counters + timers yields the same behavior.

---

## 4. Concrete algorithm (reference pseudocode)

Per selection, store:
- `enabled: boolean`
- `gen: number` monotonically increasing
- `pendingGen?: number`
- `debounceId?: number` (GLib source id)
- `lastWrittenHash?: string`
- `lastWriteKind?: 'trim' | 'restore' | 'manual'`
- `restoreGuard?: {hash: string, expiresUsec: number}`
- `lastOriginal?: string`

### Event entrypoint
```ts
onOwnerChange(selection):
  if !enabled: return

  gen += 1
  pendingGen = gen

  if debounceId: GLib.source_remove(debounceId)

  debounceId = GLib.timeout_add(PRIORITY_DEFAULT, graceMs, () => {
     debounceId = null
     process(selection, pendingGen)
     return GLib.SOURCE_REMOVE
  })
```

### Processing
```ts
async process(selection, genAtSchedule):
  if !enabled or pendingGen != genAtSchedule: return  // stale

  text = await readText(selection)  // async callback wrapped in Promise
  if !enabled or pendingGen != genAtSchedule: return  // stale after read
  if !text: return

  now = GLib.get_monotonic_time()
  if restoreGuard exists and now < restoreGuard.expiresUsec:
      if hash(text) == restoreGuard.hash: return
      else restoreGuard = null

  incomingHash = hash(text)
  if lastWrittenHash == incomingHash:
      lastWrittenHash = null  // consume the guard
      return

  if !settings.enableAutoTrim: return

  opts = readOptions()
  aggr = settings.aggressiveness
  res = trimmer.trim(text, aggr, opts)
  if !res.changed: return

  // IMPORTANT: re-check staleness right before writing
  if !enabled or pendingGen != genAtSchedule: return

  lastOriginal = text
  lastWrittenHash = hash(res.output)
  lastWriteKind = 'trim'
  clipboard.set_text(selection, res.output)
```

### Restore (existing feature)
```ts
restore(selection):
  original = lastOriginal
  if !original: return

  lastWrittenHash = hash(original)
  lastWriteKind = 'restore'
  restoreGuard = { hash: lastWrittenHash, expiresUsec: now + 1_500_000 }
  clipboard.set_text(selection, original)
```

### Disable
```ts
disable():
  enabled = false
  disconnect signals
  if debounceId: GLib.source_remove(debounceId)
  debounceId = null
  pendingGen = undefined
```

### Why this is race‑safe
- **Burst coalescing:** only the newest `pendingGen` survives.
- **Stale prevention:** every async boundary checks `pendingGen`.
- **Self‑write skip:** by hash, so normalization or whitespace doesn’t defeat it.
- **Lifecycle safety:** all entrypoints check `enabled`.

---

## 5. Implementation notes (current)

### Manual “Paste Trimmed / Paste Original”
These are implemented in the shell extension:
- Temporary clipboard swap + restore logic: `shell-extension/src/clipboardWatcher.ts`
- Paste injection: `shell-extension/src/virtualPaste.ts` (best‑effort virtual keyboard)

Current behavior:
1. Read current clipboard text.
2. Compute the High-aggressiveness trimmed version (for “Paste Trimmed”) or use cached original (for “Paste Original”).
3. Temporarily write the text to the clipboard while setting self-write guards (`lastWrittenHash` and, for restore, `restoreGuard`).
4. Inject paste (virtual keyboard; waits briefly for hotkey modifiers to be released).
5. Restore the previous clipboard contents after a short delay.

Auto‑trim is prevented from re-triggering on these internal writes via the hash guards.

### Polling fallback
If `St.Clipboard` does not expose the `owner-change` signal (seen on some GNOME builds), the extension falls back to polling.

Current implementation:
- A timer periodically calls `onOwnerChange()` for CLIPBOARD/PRIMARY.
- The usual debounce + read + hash guards still apply, so it stays race-safe.

Potential optimization (not implemented):
- Store `lastSeenHash` per selection to avoid reading/processing unchanged clipboard contents on every poll tick.

---

## 6. Test harness sketch (GJS)

Goal: deterministically reproduce the races above **without** running inside gnome‑shell.

### 6.1 Make watcher injectable
Refactor `ClipboardWatcher` constructor to accept a small clipboard interface:
```ts
interface ClipboardLike {
  get_text(sel: number, cb: (text: string | null) => void): void
  set_text(sel: number, text: string): void
  connect_owner_change(cb: (sel: number) => void): number
  disconnect(id: number): void
}
```
Production adapter wraps `St.Clipboard.get_default()`.

### 6.2 Fake clipboard for tests
Implement `FakeClipboard` in `tests/fakeClipboard.js`:
- `texts: Map<sel, string>`
- `handlers: Set<(sel)=>void>`
- `get_text` returns current text async (use `GLib.idle_add` to mimic real delay).
- `set_text` updates map then schedules owner‑change callbacks (optionally multiple times to simulate GNOME quirks).

### 6.3 Running tests
The watcher core is TypeScript, so bundle it first, then run gjs:
```sh
just bundle-tests
gjs -m tests/clipboard.test.js
```
Use `GLib.MainLoop` + Promises to await timeouts:
```js
function sleep(ms) {
  return new Promise(r => GLib.timeout_add(GLib.PRIORITY_DEFAULT, ms, () => (r(), GLib.SOURCE_REMOVE)));
}
```

### 6.4 Core test cases

1. **Basic trim**
   - Set fake clipboard to multi‑line command.
   - Emit owner‑change.
   - Wait grace delay.
   - Assert clipboard text == trimmed output.

2. **Burst coalescing / stale prevention**
   - Emit owner‑change with text A.
   - Before grace fires, set text B and emit again.
   - Assert final clipboard == trimmed B (A never writes).

3. **Self‑write loop guard**
   - After a trim write, fake clipboard emits owner‑change for that write.
   - Assert watcher does not call `set_text` again (count calls).

4. **Disable mid‑flight**
   - Emit owner‑change.
   - Call `disable()` before grace delay expires.
   - Wait > grace delay.
   - Assert no write occurred.

5. **Restore guard**
   - Trim once, then call `restore()`.
   - Fake clipboard emits owner‑change for restored text.
   - Assert no immediate re‑trim.

These five tests cover all four failure modes in §1.

---

## 7. Suggested rollout process
When adding any new async clipboard feature:
1. Extend fake‑clipboard tests first.
2. Implement feature in watcher.
3. Re‑run harness until stable.
4. Only then wire UI/portal pieces in gnome‑shell.
