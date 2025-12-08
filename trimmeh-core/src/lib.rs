//! Core trimming logic for Trimmeh.
use blake3::Hash;
use once_cell::sync::Lazy;
use regex::Regex;

#[cfg(feature = "wasm")]
use wasm_bindgen::prelude::*;

#[cfg(feature = "wasm")]
use serde::{Deserialize, Serialize};

#[derive(Debug, Clone, Copy, PartialEq, Eq)]
pub enum Aggressiveness {
    Low,
    Normal,
    High,
}

#[cfg_attr(feature = "wasm", derive(Serialize, Deserialize))]
#[derive(Debug, Clone, PartialEq, Eq)]
pub struct Options {
    pub keep_blank_lines: bool,
    pub strip_box_chars: bool,
    pub trim_prompts: bool,
    pub max_lines: usize,
}

impl Default for Options {
    fn default() -> Self {
        Self {
            keep_blank_lines: false,
            strip_box_chars: true,
            trim_prompts: true,
            max_lines: 10,
        }
    }
}

#[cfg_attr(feature = "wasm", derive(Serialize, Deserialize))]
#[derive(Debug, Clone, Copy, PartialEq, Eq)]
pub enum TrimReason {
    Flattened,
    PromptStripped,
    BoxCharsRemoved,
    BackslashMerged,
    SkippedTooLarge,
}

#[cfg_attr(feature = "wasm", derive(Serialize, Deserialize))]
#[derive(Debug, Clone, PartialEq, Eq)]
pub struct TrimResult {
    pub output: String,
    pub changed: bool,
    pub reason: Option<TrimReason>,
    pub hash: u128,
}

/// Trim multiline shell snippets into a single runnable line.
pub fn trim(input: &str, aggressiveness: Aggressiveness, opts: Options) -> TrimResult {
    // Normalize line endings for processing.
    let normalized_input = normalize_newlines(input);
    let line_count = normalized_input.split('\n').count();
    if line_count > opts.max_lines {
        return TrimResult {
            output: input.to_string(),
            changed: false,
            reason: Some(TrimReason::SkippedTooLarge),
            hash: hash_u128(blake3::hash(input.as_bytes())),
        };
    }

    const BLANK_PLACEHOLDER: &str = "__TRIMMEH_BLANK__PLACEHOLDER__";

    let mut processed: Vec<String> = Vec::new();
    let mut did_prompt_strip = false;
    let mut did_box_strip = false;

    for line in normalized_input.split('\n') {
        let mut current = line.to_string();
        let before_prompt = current.clone();
        if opts.trim_prompts {
            current = strip_prompt(&current, aggressiveness);
            if current != before_prompt {
                did_prompt_strip = true;
            }
        }

        let before_box = current.clone();
        if opts.strip_box_chars {
            current = strip_box_chars(&current, aggressiveness);
            if current != before_box {
                did_box_strip = true;
            }
        }

        if current.trim().is_empty() {
            if opts.keep_blank_lines {
                processed.push(BLANK_PLACEHOLDER.to_string());
            }
            continue;
        }

        processed.push(current);
    }

    let mut output_parts: Vec<String> = Vec::new();
    let mut did_backslash_merge = false;

    for line in processed.iter() {
        if line == BLANK_PLACEHOLDER {
            output_parts.push(BLANK_PLACEHOLDER.to_string());
            continue;
        }

        let mut current = line.trim_end().to_string();
        if current.ends_with('\\') {
            did_backslash_merge = true;
            while current.ends_with('\\') {
                current.pop();
            }
            while current.ends_with([' ', '\t']) {
                current.pop();
            }
        }

        output_parts.push(current);
    }

    let mut output = output_parts.join(" ");
    output = collapse_operator_spacing(&output);

    if opts.keep_blank_lines {
        output = output.replace(BLANK_PLACEHOLDER, "\n\n");
    }

    let changed = output != normalized_input;
    let reason = if !changed {
        None
    } else if did_backslash_merge {
        Some(TrimReason::BackslashMerged)
    } else if did_box_strip {
        Some(TrimReason::BoxCharsRemoved)
    } else if did_prompt_strip {
        Some(TrimReason::PromptStripped)
    } else {
        Some(TrimReason::Flattened)
    };

    TrimResult {
        output,
        changed,
        reason,
        hash: hash_u128(blake3::hash(input.as_bytes())),
    }
}

fn normalize_newlines(input: &str) -> String {
    input.replace("\r\n", "\n").replace('\r', "\n")
}

fn strip_prompt(line: &str, aggressiveness: Aggressiveness) -> String {
    let trimmed = line.trim_start();
    let prompt_prefixes = ["$ ", "# ", "> ", "% "];
    for prefix in prompt_prefixes {
        if trimmed.starts_with(prefix) {
            return trimmed[prefix.len()..].to_string();
        }
    }

    // Common PS1 forms: [user@host dir]$ command
    if let Some(idx) = trimmed.find("]$ ") {
        return trimmed[(idx + 3)..].to_string();
    }
    if let Some(idx) = trimmed.find("]% ") {
        return trimmed[(idx + 3)..].to_string();
    }
    if let Some(idx) = trimmed.find(">$ ") {
        return trimmed[(idx + 3)..].to_string();
    }

    if matches!(aggressiveness, Aggressiveness::High) {
        let high_prefixes = ["- $ ", "-# ", "| $ ", "• $ "];
        for prefix in high_prefixes {
            if trimmed.starts_with(prefix) {
                return trimmed[prefix.len()..].to_string();
            }
        }
    }

    line.to_string()
}

fn strip_box_chars(line: &str, aggressiveness: Aggressiveness) -> String {
    let trimmed = line.trim_start();
    if trimmed.starts_with("```") || trimmed.starts_with("~~~") {
        return line.to_string();
    }

    let mut chars = trimmed.chars();
    if let Some(first) = chars.next() {
        if matches!(first, '│' | '┃' | '▕' | '|') {
            let rest = chars.as_str().trim_start_matches(' ');
            let leading_spaces = line.len() - trimmed.len();
            let mut rebuilt = String::new();
            for _ in 0..leading_spaces {
                rebuilt.push(' ');
            }
            rebuilt.push_str(rest);
            return rebuilt;
        }

        if matches!(aggressiveness, Aggressiveness::High) && first == '>' {
            let rest = chars.as_str().trim_start_matches(' ');
            let leading_spaces = line.len() - trimmed.len();
            let mut rebuilt = String::new();
            for _ in 0..leading_spaces {
                rebuilt.push(' ');
            }
            rebuilt.push_str(rest);
            return rebuilt;
        }
    }

    line.to_string()
}

static OPS_RE: Lazy<Regex> =
    Lazy::new(|| Regex::new(r"\s*(&&|\|\||;|\\)\s*").expect("operator regex"));

fn collapse_operator_spacing(input: &str) -> String {
    let collapsed = OPS_RE.replace_all(input, " $1 ").into_owned();
    collapse_extra_spaces(&collapsed)
}

fn collapse_extra_spaces(input: &str) -> String {
    let mut out = String::with_capacity(input.len());
    let mut last_space = false;
    for ch in input.chars() {
        if ch.is_whitespace() {
            if !last_space {
                out.push(' ');
            }
            last_space = true;
        } else {
            out.push(ch);
            last_space = false;
        }
    }
    out.trim().to_string()
}

fn hash_u128(hash: Hash) -> u128 {
    let mut bytes = [0u8; 16];
    bytes.copy_from_slice(&hash.as_bytes()[..16]);
    u128::from_be_bytes(bytes)
}

#[cfg(feature = "wasm")]
#[wasm_bindgen]
pub fn trim_js(input: &str, aggressiveness: u8, opts: JsValue) -> JsValue {
    let aggr = match aggressiveness {
        0 => Aggressiveness::Low,
        2 => Aggressiveness::High,
        _ => Aggressiveness::Normal,
    };
    let opts: Options =
        serde_wasm_bindgen::from_value(opts).unwrap_or_else(|_| Options::default());
    let result = trim(input, aggr, opts);

    #[derive(Serialize)]
    struct WasmResult<'a> {
        output: &'a str,
        changed: bool,
        reason: Option<TrimReason>,
        hash_hex: String,
    }

    let wasm_result = WasmResult {
        output: &result.output,
        changed: result.changed,
        reason: result.reason,
        hash_hex: format!("{:032x}", result.hash),
    };

    serde_wasm_bindgen::to_value(&wasm_result).unwrap()
}

#[cfg(test)]
mod tests {
    use super::*;

    #[test]
    fn backslash_merge() {
        let input = "python - <<'PY'\nprint(\"ok\")\\\n\nPY";
        let res = trim(input, Aggressiveness::Normal, Options::default());
        assert_eq!(res.output, "python - <<'PY' print(\"ok\") PY");
        assert!(res.changed);
        assert_eq!(res.reason, Some(TrimReason::BackslashMerged));
    }

    #[test]
    fn prompt_strip() {
        let input = "$ sudo dnf install foo";
        let res = trim(input, Aggressiveness::Normal, Options::default());
        assert_eq!(res.output, "sudo dnf install foo");
        assert!(res.changed);
        assert_eq!(res.reason, Some(TrimReason::PromptStripped));
    }

    #[test]
    fn box_gutter_strip() {
        let input = "│ sudo dnf upgrade && \n│ reboot";
        let res = trim(input, Aggressiveness::Normal, Options::default());
        assert_eq!(res.output, "sudo dnf upgrade && reboot");
        assert!(res.changed);
        assert_eq!(res.reason, Some(TrimReason::BoxCharsRemoved));
    }

    #[test]
    fn already_flat() {
        let input = "echo hi";
        let res = trim(input, Aggressiveness::Normal, Options::default());
        assert_eq!(res.output, "echo hi");
        assert!(!res.changed);
        assert_eq!(res.reason, None);
    }

    #[test]
    fn oversize_skip() {
        let blob = "line\n".repeat(20);
        let res = trim(&blob, Aggressiveness::Normal, Options { max_lines: 5, ..Default::default() });
        assert_eq!(res.output, blob);
        assert!(!res.changed);
        assert_eq!(res.reason, Some(TrimReason::SkippedTooLarge));
    }
}
