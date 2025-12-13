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

    let mut current = normalized_input.clone();
    let mut did_prompt_strip = false;
    let mut did_box_strip = false;
    let mut did_backslash_merge = false;

    if opts.strip_box_chars {
        if let Some(cleaned) = strip_box_drawing_characters(&current) {
            did_box_strip = true;
            current = cleaned;
        }
    }

    if opts.trim_prompts {
        if let Some(stripped) = strip_prompt_prefixes(&current) {
            did_prompt_strip = true;
            current = stripped;
        }
    }

    if let Some(repaired) = repair_wrapped_url(&current) {
        current = repaired;
    }

    if let Some((flattened, merged_backslash)) = transform_if_command(&current, aggressiveness, &opts) {
        did_backslash_merge = merged_backslash;
        current = flattened;
    }

    let changed = current != normalized_input;
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
        output: current,
        changed,
        reason,
        hash: hash_u128(blake3::hash(input.as_bytes())),
    }
}

fn normalize_newlines(input: &str) -> String {
    input.replace("\r\n", "\n").replace('\r', "\n")
}

// ---------- Trimmy-parity helpers ----------

static BOX_CLASS: &str = "│┃╎╏┆┇┊┋╽╿￨｜";
static KNOWN_PREFIXES: &[&str] = &[
    "sudo", "./", "~/", "apt", "brew", "git", "python", "pip", "pnpm", "npm", "yarn",
    "cargo", "bundle", "rails", "go", "make", "xcodebuild", "swift", "kubectl", "docker",
    "podman", "aws", "gcloud", "az", "ls", "cd", "cat", "echo", "env", "export", "open",
    "node", "java", "ruby", "perl", "bash", "zsh", "fish", "pwsh", "sh",
];

static RE_BLANK_PLACEHOLDER: Lazy<Regex> =
    Lazy::new(|| Regex::new(r"\n\s*\n").expect("blank placeholder regex"));
static RE_HYPHEN_JOIN: Lazy<Regex> = Lazy::new(|| {
    Regex::new(r"(?m)([A-Za-z0-9._~-])-\s*\n\s*([A-Za-z0-9._~-])").expect("hyphen join")
});
static RE_ADJ_WORD_JOIN: Lazy<Regex> = Lazy::new(|| {
    Regex::new(r"(?m)([A-Z0-9_.-])\s*\n\s*([A-Z0-9_.-])").expect("adj word join")
});
static RE_PATH_JOIN: Lazy<Regex> = Lazy::new(|| {
    Regex::new(r"(?m)([/:~])\s*\n\s*([A-Za-z0-9._-])").expect("path join")
});
static RE_BACKSLASH_NEWLINE: Lazy<Regex> =
    Lazy::new(|| Regex::new(r"\\\s*\n").expect("backslash newline"));
static RE_NEWLINES_TO_SPACE: Lazy<Regex> =
    Lazy::new(|| Regex::new(r"\n+").expect("newline collapse"));
static RE_SPACES: Lazy<Regex> = Lazy::new(|| Regex::new(r"\s+").expect("space collapse"));

static RE_PIPE_OR_OP: Lazy<Regex> = Lazy::new(|| Regex::new(r"[|&]{1,2}").expect("pipe/op"));
static RE_PROMPT_MARK: Lazy<Regex> = Lazy::new(|| Regex::new(r"(?m)(^|\n)\s*\$").expect("prompt mark"));
static RE_SUDO_CMD: Lazy<Regex> = Lazy::new(|| {
    Regex::new(r"(?m)^\s*(sudo\s+)?[A-Za-z0-9./~_-]+\b").expect("sudo cmd")
});
static RE_PATH_TOKEN: Lazy<Regex> = Lazy::new(|| {
    Regex::new(r"[A-Za-z0-9._~-]+/[A-Za-z0-9._~-]+").expect("path token")
});
static RE_LIST_BULLET: Lazy<Regex> =
    Lazy::new(|| Regex::new(r"^[-*•]\s+\S").expect("list bullet"));
static RE_LIST_NUMBERED: Lazy<Regex> =
    Lazy::new(|| Regex::new(r"^[0-9]+[.)]\s+\S").expect("list numbered"));
static RE_LIST_BARETOKEN: Lazy<Regex> =
    Lazy::new(|| Regex::new(r"^[A-Za-z0-9]{4,}$").expect("list bare"));
static RE_SOURCE_KEYWORD: Lazy<Regex> = Lazy::new(|| {
    Regex::new(r"(?m)^\s*(import|package|namespace|using|template|class|struct|enum|extension|protocol|interface|func|def|fn|let|var|public|private|internal|open|protected|if|for|while)\b")
        .expect("src keyword")
});

fn strip_prompt_prefixes(text: &str) -> Option<String> {
    let lines: Vec<&str> = text.split('\n').collect();
    let non_empty: Vec<&str> = lines
        .iter()
        .copied()
        .filter(|l| !l.trim().is_empty())
        .collect();
    if non_empty.is_empty() {
        return None;
    }

    let mut stripped_count = 0usize;
    let mut rebuilt: Vec<String> = Vec::with_capacity(lines.len());

    for line in lines.iter() {
        if let Some(stripped) = strip_prompt_line(line) {
            stripped_count += 1;
            rebuilt.push(stripped);
        } else {
            rebuilt.push((*line).to_string());
        }
    }

    let majority = non_empty.len() / 2 + 1;
    let should_strip = if non_empty.len() == 1 {
        stripped_count == 1
    } else {
        stripped_count >= majority
    };

    if should_strip {
        let result = rebuilt.join("\n");
        if result != text { Some(result) } else { None }
    } else {
        None
    }
}

fn strip_prompt_line(line: &str) -> Option<String> {
    let leading_ws_len = line.chars().take_while(|c| c.is_whitespace()).count();
    let (leading, remainder) = line.split_at(leading_ws_len);
    let mut chars = remainder.chars();
    let first = chars.next()?;
    if first != '#' && first != '$' {
        return None;
    }
    let after_prompt = chars.as_str().trim_start();
    if !is_likely_prompt_command(after_prompt) {
        return None;
    }
    Some(format!("{}{}", leading, after_prompt))
}

fn is_likely_prompt_command(content: &str) -> bool {
    let trimmed = content.trim();
    if trimmed.is_empty() {
        return false;
    }
    if matches!(trimmed.chars().last(), Some('.') | Some('?') | Some('!')) {
        return false;
    }
    let has_punct = trimmed.chars().any(|c| "-./~$".contains(c)) || trimmed.chars().any(|c| c.is_ascii_digit());
    let first_token = trimmed.split_whitespace().next().unwrap_or("").to_lowercase();
    let starts_with_known = KNOWN_PREFIXES.iter().any(|p| first_token.starts_with(p));
    (has_punct || starts_with_known) && is_likely_command_line(trimmed)
}

fn strip_box_drawing_characters(text: &str) -> Option<String> {
    let box_class_re = Regex::new(&format!("[{}]", BOX_CLASS)).expect("box class");
    if box_class_re.find(text).is_none() {
        return None;
    }

    let mut result = text.to_string();
    if result.contains("│ │") {
        result = result.replace("│ │", " ");
    }

    let lines: Vec<&str> = result.split_inclusive('\n').collect();
    let non_empty: Vec<&str> = lines
        .iter()
        .map(|l| l.trim())
        .filter(|l| !l.is_empty())
        .collect();

    let leading_pattern = Regex::new(&format!(r"^\s*[{bc}]+ ?", bc = BOX_CLASS)).expect("leading box");
    let trailing_pattern = Regex::new(&format!(r" ?[{bc}]+\s*$", bc = BOX_CLASS)).expect("trailing box");

    let mut strip_leading = false;
    let mut strip_trailing = false;
    if !non_empty.is_empty() {
        let majority = non_empty.len() / 2 + 1;
        let leading_matches = non_empty
            .iter()
            .filter(|line| leading_pattern.is_match(line))
            .count();
        let trailing_matches = non_empty
            .iter()
            .filter(|line| trailing_pattern.is_match(line))
            .count();
        strip_leading = leading_matches >= majority;
        strip_trailing = trailing_matches >= majority;
    }

    if strip_leading || strip_trailing {
        let mut rebuilt: Vec<String> = Vec::with_capacity(lines.len());
        for line in lines.iter() {
            let mut l = line.to_string();
            if strip_leading {
                l = leading_pattern.replace(&l, "").into_owned();
            }
            if strip_trailing {
                l = trailing_pattern.replace(&l, "").into_owned();
            }
            rebuilt.push(l);
        }
        result = rebuilt.concat();
    }

    let box_after_pipe = Regex::new(&format!(r"\|\s*[{bc}]+\s*", bc = BOX_CLASS)).expect("pipe box");
    result = box_after_pipe.replace_all(&result, "| ").into_owned();

    let box_path_join = Regex::new(&format!(r"([:/])\s*[{bc}]+\s*([A-Za-z0-9])", bc = BOX_CLASS)).expect("box path");
    result = box_path_join.replace_all(&result, "$1$2").into_owned();

    let box_mid_token = Regex::new(&format!(r"(\S)\s*[{bc}]+\s*(\S)", bc = BOX_CLASS)).expect("box mid");
    result = box_mid_token.replace_all(&result, "$1 $2").into_owned();

    let box_anywhere = Regex::new(&format!(r"\s*[{bc}]+\s*", bc = BOX_CLASS)).expect("box any");
    result = box_anywhere.replace_all(&result, " ").into_owned();

    let collapsed = Regex::new(r" {2,}").unwrap().replace_all(&result, " ").into_owned();
    let trimmed = collapsed.trim().to_string();
    if trimmed == text {
        None
    } else {
        Some(trimmed)
    }
}

fn repair_wrapped_url(text: &str) -> Option<String> {
    let trimmed = text.trim();
    let lower = trimmed.to_lowercase();
    let scheme_count = lower.matches("https://").count() + lower.matches("http://").count();
    if scheme_count != 1 {
        return None;
    }
    if !(lower.starts_with("http://") || lower.starts_with("https://")) {
        return None;
    }
    let collapsed = Regex::new(r"\s+").unwrap().replace_all(trimmed, "").into_owned();
    if collapsed == trimmed {
        return None;
    }
    let valid_url_re = Regex::new(r"^https?://[A-Za-z0-9._~:/?#\[\]@!$&'()*+,;=%-]+$").unwrap();
    if valid_url_re.is_match(&collapsed) {
        Some(collapsed)
    } else {
        None
    }
}

fn transform_if_command(
    text: &str,
    aggressiveness: Aggressiveness,
    opts: &Options,
) -> Option<(String, bool)> {
    if !text.contains('\n') {
        return None;
    }

    let lines: Vec<&str> = text.split('\n').collect();
    if lines.len() < 2 {
        return None;
    }
    if lines.len() > 10 {
        return None;
    }
    let newline_count = text.matches('\n').count();
    let aggr_override_high = matches!(aggressiveness, Aggressiveness::High);
    if !aggr_override_high && newline_count > 4 {
        return None;
    }

    let non_empty: Vec<&str> = lines
        .iter()
        .copied()
        .filter(|l| !l.trim().is_empty())
        .collect();

    if !aggr_override_high && is_likely_list(&non_empty) {
        return None;
    }

    let has_line_continuation = text.contains("\\\n");
    let has_line_joiner_at_eol = Regex::new(r"(?m)(\\|[|&]{1,2}|;)\s*$").unwrap().is_match(text);
    let has_indented_pipeline = Regex::new(r"(?m)^\s*[|&]{1,2}\s+\S").unwrap().is_match(text);
    let has_explicit_join = has_line_continuation || has_line_joiner_at_eol || has_indented_pipeline;

    let cmd_line_count = command_line_count(&non_empty);
    if !aggr_override_high
        && !has_explicit_join
        && cmd_line_count == non_empty.len()
        && non_empty.len() >= 3
    {
        return None;
    }

    let strong_signals = has_line_continuation
        || RE_PIPE_OR_OP.is_match(text)
        || RE_PROMPT_MARK.is_match(text)
        || RE_PATH_TOKEN.is_match(text);

    let has_known_prefix = contains_known_command_prefix(&non_empty);
    if !aggr_override_high
        && !strong_signals
        && !has_known_prefix
        && !has_command_punctuation(text)
    {
        return None;
    }

    if !aggr_override_high && is_likely_source_code(text) && !strong_signals {
        return None;
    }

    let mut score = 0;
    if has_line_continuation {
        score += 1;
    }
    if RE_PIPE_OR_OP.is_match(text) {
        score += 1;
    }
    if RE_PROMPT_MARK.is_match(text) {
        score += 1;
    }
    if non_empty.iter().all(|l| is_likely_command_line(l)) {
        score += 1;
    }
    if RE_SUDO_CMD.is_match(text) {
        score += 1;
    }
    if RE_PATH_TOKEN.is_match(text) {
        score += 1;
    }

    let threshold = match aggressiveness {
        Aggressiveness::Low => 3,
        Aggressiveness::Normal => 2,
        Aggressiveness::High => 1,
    };
    if score < threshold {
        return None;
    }

    let (flattened, merged_backslash) = flatten(text, opts.keep_blank_lines);
    if flattened == text {
        None
    } else {
        Some((flattened, merged_backslash))
    }
}

fn is_likely_command_line(line: &str) -> bool {
    let trimmed = line.trim();
    if trimmed.is_empty() {
        return false;
    }
    if trimmed.starts_with("[[") {
        return true;
    }
    if trimmed.ends_with('.') {
        return false;
    }
    let re = Regex::new(r"^(sudo\s+)?[A-Za-z0-9./~_-]+(?:\s+|\z)").unwrap();
    re.is_match(trimmed)
}

fn contains_known_command_prefix(lines: &[&str]) -> bool {
    lines.iter().any(|line| {
        let trimmed = line.trim();
        if trimmed.is_empty() {
            return false;
        }
        if let Some(first) = trimmed.split_whitespace().next() {
            let lower = first.to_lowercase();
            return KNOWN_PREFIXES.iter().any(|p| lower.starts_with(p));
        }
        false
    })
}

fn has_command_punctuation(text: &str) -> bool {
    Regex::new(r"[./~_=:-]").unwrap().is_match(text)
}

fn is_likely_list(lines: &[&str]) -> bool {
    if lines.is_empty() {
        return false;
    }
    let listish = lines.iter().filter(|line| {
        let trimmed = line.trim();
        if trimmed.is_empty() {
            return false;
        }
        let has_spaces = trimmed.chars().any(|c| c.is_whitespace());
        if RE_LIST_BULLET.is_match(trimmed) {
            return true;
        }
        if RE_LIST_NUMBERED.is_match(trimmed) {
            return true;
        }
        if !has_spaces
            && RE_LIST_BARETOKEN.is_match(trimmed)
            && !trimmed.contains('.')
            && !trimmed.contains('/')
            && !trimmed.contains('$')
        {
            return true;
        }
        false
    }).count();

    listish >= (lines.len() / 2 + 1)
}

fn is_likely_source_code(text: &str) -> bool {
    let has_braces = text.contains('{') || text.contains('}') || text.to_lowercase().contains("begin");
    let has_keywords = RE_SOURCE_KEYWORD.is_match(text);
    has_braces && has_keywords
}

fn command_line_count(lines: &[&str]) -> usize {
    lines.iter().filter(|l| is_likely_command_line(l)).count()
}

fn flatten(text: &str, preserve_blank_lines: bool) -> (String, bool) {
    let mut result = text.to_string();
    if preserve_blank_lines {
        result = RE_BLANK_PLACEHOLDER
            .replace_all(&result, "__TRIMMEH_BLANK__PLACEHOLDER__")
            .into_owned();
    }

    result = RE_HYPHEN_JOIN.replace_all(&result, "$1-$2").into_owned();
    result = RE_ADJ_WORD_JOIN.replace_all(&result, "$1$2").into_owned();
    result = RE_PATH_JOIN.replace_all(&result, "$1$2").into_owned();

    let mut merged_backslash = false;
    if RE_BACKSLASH_NEWLINE.is_match(&result) {
        merged_backslash = true;
        result = RE_BACKSLASH_NEWLINE.replace_all(&result, " ").into_owned();
    }

    result = RE_NEWLINES_TO_SPACE.replace_all(&result, " ").into_owned();
    result = RE_SPACES.replace_all(&result, " ").into_owned();

    if preserve_blank_lines {
        result = result.replace("__TRIMMEH_BLANK__PLACEHOLDER__", "\n\n");
    }

    (result.trim().to_string(), merged_backslash)
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
    use serde::Deserialize;

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

    #[test]
    fn blank_lines_preserved() {
        let input = "echo first\necho second\n\necho third";
        let res = trim(
            input,
            Aggressiveness::High,
            Options { keep_blank_lines: true, ..Default::default() },
        );
        assert_eq!(res.output, "echo first echo second\n\necho third");
        assert!(res.changed);
    }

    #[test]
    fn prompt_majority_stripped() {
        let input = "$ echo one\n$ echo two\n$ echo three";
        let res = trim(input, Aggressiveness::High, Options::default());
        assert_eq!(res.output, "echo one echo two echo three");
        assert!(res.changed);
        assert_eq!(res.reason, Some(TrimReason::PromptStripped));
    }

    #[test]
    fn prompt_not_stripped_when_toggle_off() {
        let input = "$ echo one\n$ echo two\n$ echo three";
        let res = trim(
            input,
            Aggressiveness::High,
            Options { trim_prompts: false, ..Default::default() },
        );
        assert_eq!(res.output, "$ echo one $ echo two $ echo three");
    }

    #[test]
    fn url_repair() {
        let input = "https://example.com/very\n/long/path";
        let res = trim(input, Aggressiveness::Normal, Options::default());
        assert_eq!(res.output, "https://example.com/very/long/path");
        assert!(res.changed);
    }

    #[test]
    fn list_is_skipped() {
        let input = "- item one\n- item two\n- item three";
        let res = trim(input, Aggressiveness::Normal, Options::default());
        assert_eq!(res.output, input);
        assert!(!res.changed);
    }

    #[test]
    fn vectors_match_expected() {
        #[derive(Debug, Deserialize)]
        struct Vector {
            name: String,
            input: String,
            aggressiveness: String,
            #[serde(default)]
            options: Option<VectorOptions>,
            expected: Expected,
        }

        #[derive(Debug, Deserialize)]
        struct VectorOptions {
            keep_blank_lines: Option<bool>,
            strip_box_chars: Option<bool>,
            trim_prompts: Option<bool>,
            max_lines: Option<usize>,
        }

        #[derive(Debug, Deserialize)]
        struct Expected {
            output: String,
            changed: bool,
            #[serde(default)]
            reason: Option<String>,
        }

        fn parse_aggr(s: &str) -> Aggressiveness {
            match s {
                "low" => Aggressiveness::Low,
                "high" => Aggressiveness::High,
                _ => Aggressiveness::Normal,
            }
        }

        fn reason_key(reason: Option<TrimReason>) -> Option<&'static str> {
            match reason {
                None => None,
                Some(TrimReason::Flattened) => Some("flattened"),
                Some(TrimReason::PromptStripped) => Some("prompt_stripped"),
                Some(TrimReason::BoxCharsRemoved) => Some("box_chars_removed"),
                Some(TrimReason::BackslashMerged) => Some("backslash_merged"),
                Some(TrimReason::SkippedTooLarge) => Some("skipped_too_large"),
            }
        }

        let json = include_str!("../../tests/trim-vectors.json");
        let vectors: Vec<Vector> = serde_json::from_str(json).expect("parse trim vectors");

        for v in vectors {
            let mut opts = Options::default();
            if let Some(o) = v.options {
                if let Some(val) = o.keep_blank_lines {
                    opts.keep_blank_lines = val;
                }
                if let Some(val) = o.strip_box_chars {
                    opts.strip_box_chars = val;
                }
                if let Some(val) = o.trim_prompts {
                    opts.trim_prompts = val;
                }
                if let Some(val) = o.max_lines {
                    opts.max_lines = val;
                }
            }

            let res = trim(&v.input, parse_aggr(&v.aggressiveness), opts);
            assert_eq!(res.output, v.expected.output, "vector {} output", v.name);
            assert_eq!(res.changed, v.expected.changed, "vector {} changed", v.name);

            if let Some(expected_reason) = v.expected.reason.as_deref() {
                assert_eq!(
                    reason_key(res.reason),
                    Some(expected_reason),
                    "vector {} reason",
                    v.name
                );
            }
        }
    }
}
