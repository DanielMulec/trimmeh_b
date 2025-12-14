# trimmeh-core API (Rust) + shared JS core

Target stack is current as of Dec 7, 2025: Rust 1.91.0 (Edition 2024), GNOME 49 runtime (GJS).

## Public surface
- Crate: `trimmeh_core`
- Function: `fn trim(input: &str, aggressiveness: Aggressiveness, opts: Options) -> TrimResult`
- Types:
  - `enum Aggressiveness { Low, Normal, High }`
  - `struct Options { keep_blank_lines: bool, strip_box_chars: bool, trim_prompts: bool, max_lines: usize }`
  - `struct TrimResult { output: String, changed: bool, reason: Option<TrimReason>, hash: u128 }`
  - `enum TrimReason { Flattened, PromptStripped, BoxCharsRemoved, BackslashMerged, SkippedTooLarge }`

## Rewrite pipeline
1. Guard: if `line_count > max_lines` => return unchanged with `SkippedTooLarge`.
2. Normalize line endings to `\n`.
3. Strip leading prompt tokens when `trim_prompts` is true (`$ `, `# `, `% `, `>`; common PS1 forms like `[user@host project]$` and `hostname%`).
4. Remove box-drawing gutters when `strip_box_chars` is true (`│`, `┃`, `┃`, `▕`, leading `|` that align vertically, Markdown fences ` ``` ` are preserved).
5. Merge lines:
   - If a line ends with `\`, drop the backslash and newline.
   - Else, join with a single space.
   - Preserve multiple spaces inside a line.
6. Collapse redundant whitespace around `&&`, `||`, `;`, `\` (avoids accidental concatenation).
7. If result differs, set `changed = true`, compute `hash = blake3_128(output)`.

## Defaults
- `Aggressiveness::Normal`
- `Options { keep_blank_lines: false, strip_box_chars: true, trim_prompts: true, max_lines: 10 }`
- Low = fewer prompt patterns; High = more aggressive prompt/box stripping and whitespace collapsing (e.g., removes Markdown bullet prefixes like `- $ command`).

## Test vectors (goldens)
Canonical vectors live in `tests/trim-vectors.json` and are used by both:
- Rust test in `trimmeh-core` (prevents regression/drift)
- GJS test runner for the JS core

- **Backslash merge**  
  In: `python - <<'PY'\nprint(\"ok\")\\\n\nPY`  
  Out: `python - <<'PY' print("ok") PY`
- **Prompt strip**  
  In: `$ sudo dnf install foo` → `sudo dnf install foo`
- **Box gutter**  
  In: `│ sudo dnf upgrade && \n│ reboot` → `sudo dnf upgrade && reboot`
- **Already flat**  
  In: `echo hi` → unchanged (`changed=false`)
- **Oversize skip**  
  20-line blob → unchanged with `reason=SkippedTooLarge`

## Shared JS core
The GNOME extension uses a runtime-agnostic JS/TS core (no `.wasm`) so it can be reviewed easily for EGO and reused for a future KDE/Plasma (QML/JS) port:
- Source: `trimmeh-core-js/src/index.ts`
- Extension adapter: `shell-extension/src/trimmer.ts`

## CLI expectations
- `trimmeh-cli trim` reads stdin → stdout.
- `trimmeh-cli diff` prints before/after with unified diff for debugging.
- Shares vectors with the JS core to avoid drift.
