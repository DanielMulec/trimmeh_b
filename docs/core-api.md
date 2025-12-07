# trimmeh-core API (Rust + Wasm)

Target stack is current as of Dec 7, 2025: Rust 1.91.0 (Edition 2024), wasm-bindgen compatible with gjs in GNOME 49.

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

## Wasm build contract
- `wasm-bindgen --target no-modules --weak-refs --reference-types` (keeps output loadable by gjs without ESM).
- Expose `trim` via `#[wasm_bindgen] pub fn trim_js(input: &str, aggressiveness: u8, opts: JsValue) -> JsValue`.
- Output file name: `libtrimmeh_core.wasm`; accompanying JS shim kept minimal and bundle-free.
- Deterministic builds: `RUSTFLAGS="-C panic=abort -Zremap-path-prefix=."` and `wasm-opt -Oz` if available.

## CLI expectations
- `trimmeh-cli trim` reads stdin → stdout.
- `trimmeh-cli diff` prints before/after with unified diff for debugging.
- Same code path as wasm to avoid drift.
