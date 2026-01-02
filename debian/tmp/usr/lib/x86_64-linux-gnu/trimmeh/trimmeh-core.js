(() => {
  // trimmeh-core-js/src/index.ts
  var DEFAULT_TRIM_OPTIONS = {
    keep_blank_lines: false,
    strip_box_chars: true,
    trim_prompts: true,
    max_lines: 10
  };
  function trim(input, aggressiveness, options) {
    const opts = Object.assign({}, DEFAULT_TRIM_OPTIONS, options || {});
    const normalizedInput = normalizeNewlines(input);
    const lineCount = normalizedInput.split("\n").length;
    if (lineCount > opts.max_lines) {
      return {
        output: input,
        changed: false,
        reason: "skipped_too_large"
      };
    }
    let current = normalizedInput;
    let didPromptStrip = false;
    let didBoxStrip = false;
    let didBackslashMerge = false;
    if (opts.strip_box_chars) {
      const cleaned = stripBoxDrawingCharacters(current);
      if (cleaned !== null) {
        didBoxStrip = true;
        current = cleaned;
      }
    }
    if (opts.trim_prompts) {
      const stripped = stripPromptPrefixes(current);
      if (stripped !== null) {
        didPromptStrip = true;
        current = stripped;
      }
    }
    const repaired = repairWrappedUrl(current);
    if (repaired !== null) {
      current = repaired;
    }
    const cmd = transformIfCommand(current, aggressiveness, opts);
    if (cmd !== null) {
      current = cmd.output;
      didBackslashMerge = cmd.mergedBackslash;
    }
    const changed = current !== normalizedInput;
    const reason = !changed ? void 0 : didBackslashMerge ? "backslash_merged" : didBoxStrip ? "box_chars_removed" : didPromptStrip ? "prompt_stripped" : "flattened";
    return { output: current, changed, reason };
  }
  function normalizeNewlines(input) {
    return input.replace(/\r\n/g, "\n").replace(/\r/g, "\n");
  }
  var BOX_CLASS = "\u2502\u2503\u254E\u254F\u2506\u2507\u250A\u250B\u257D\u257F\uFFE8\uFF5C";
  var KNOWN_PREFIXES = [
    "sudo",
    "./",
    "~/",
    "apt",
    "brew",
    "git",
    "python",
    "pip",
    "pnpm",
    "npm",
    "yarn",
    "cargo",
    "bundle",
    "rails",
    "go",
    "make",
    "xcodebuild",
    "swift",
    "kubectl",
    "docker",
    "podman",
    "aws",
    "gcloud",
    "az",
    "ls",
    "cd",
    "cat",
    "echo",
    "env",
    "export",
    "open",
    "node",
    "java",
    "ruby",
    "perl",
    "bash",
    "zsh",
    "fish",
    "pwsh",
    "sh"
  ];
  var RE_BLANK_PLACEHOLDER = /\n\s*\n/g;
  var RE_HYPHEN_JOIN = /([A-Za-z0-9._~-])-\s*\n\s*([A-Za-z0-9._~-])/gm;
  var RE_ADJ_WORD_JOIN = /([A-Z0-9_.-])\s*\n\s*([A-Z0-9_.-])/gm;
  var RE_PATH_JOIN = /([/:~])\s*\n\s*([A-Za-z0-9._-])/gm;
  var RE_BACKSLASH_NEWLINE = /\\\s*\n/g;
  var RE_BACKSLASH_NEWLINE_TEST = /\\\s*\n/;
  var RE_NEWLINES_TO_SPACE = /\n+/g;
  var RE_SPACES = /\s+/g;
  var RE_PIPE_OR_OP = /[|&]{1,2}/;
  var RE_PROMPT_MARK = /(^|\n)\s*\$/m;
  var RE_SUDO_CMD = /^\s*(sudo\s+)?[A-Za-z0-9./~_-]+\b/m;
  var RE_PATH_TOKEN = /[A-Za-z0-9._~-]+\/[A-Za-z0-9._~-]+/;
  var RE_LIST_BULLET = /^[-*•]\s+\S/;
  var RE_LIST_NUMBERED = /^[0-9]+[.)]\s+\S/;
  var RE_LIST_BARETOKEN = /^[A-Za-z0-9]{4,}$/;
  var RE_SOURCE_KEYWORD = /^\s*(import|package|namespace|using|template|class|struct|enum|extension|protocol|interface|func|def|fn|let|var|public|private|internal|open|protected|if|for|while)\b/m;
  function stripPromptPrefixes(text) {
    const lines = text.split("\n");
    const nonEmpty = lines.filter((l) => l.trim().length > 0);
    if (nonEmpty.length === 0) {
      return null;
    }
    let strippedCount = 0;
    const rebuilt = [];
    rebuilt.length = lines.length;
    for (let i = 0; i < lines.length; i += 1) {
      const line = lines[i];
      const stripped = stripPromptLine(line);
      if (stripped !== null) {
        strippedCount += 1;
        rebuilt[i] = stripped;
      } else {
        rebuilt[i] = line;
      }
    }
    const majority = Math.floor(nonEmpty.length / 2) + 1;
    const shouldStrip = nonEmpty.length === 1 ? strippedCount === 1 : strippedCount >= majority;
    if (!shouldStrip) {
      return null;
    }
    const result = rebuilt.join("\n");
    return result === text ? null : result;
  }
  function stripPromptLine(line) {
    const leadingMatch = line.match(/^\s*/);
    const leading = leadingMatch ? leadingMatch[0] : "";
    const remainder = line.slice(leading.length);
    if (remainder.length === 0) {
      return null;
    }
    const first = remainder[0];
    if (first !== "#" && first !== "$") {
      return null;
    }
    const afterPrompt = remainder.slice(1).trimStart();
    if (!isLikelyPromptCommand(afterPrompt)) {
      return null;
    }
    return `${leading}${afterPrompt}`;
  }
  function isLikelyPromptCommand(content) {
    const trimmed = content.trim();
    if (trimmed.length === 0) {
      return false;
    }
    const last = trimmed[trimmed.length - 1];
    if (last === "." || last === "?" || last === "!") {
      return false;
    }
    const hasPunct = /[-./~$]/.test(trimmed) || /\d/.test(trimmed);
    const firstToken = (trimmed.split(/\s+/)[0] || "").toLowerCase();
    const startsWithKnown = KNOWN_PREFIXES.some((p) => firstToken.startsWith(p));
    return (hasPunct || startsWithKnown) && isLikelyCommandLine(trimmed);
  }
  function stripBoxDrawingCharacters(text) {
    const boxAny = new RegExp(`[${BOX_CLASS}]`);
    if (!boxAny.test(text)) {
      return null;
    }
    let result = text;
    if (result.includes("\u2502 \u2502")) {
      result = result.split("\u2502 \u2502").join(" ");
    }
    const lines = result.split("\n");
    const nonEmpty = lines.map((l) => l.trim()).filter((l) => l.length > 0);
    const leadingPattern = new RegExp(`^\\s*[${BOX_CLASS}]+ ?`);
    const trailingPattern = new RegExp(` ?[${BOX_CLASS}]+\\s*$`);
    let stripLeading = false;
    let stripTrailing = false;
    if (nonEmpty.length > 0) {
      const majority = Math.floor(nonEmpty.length / 2) + 1;
      const leadingMatches = nonEmpty.filter((line) => leadingPattern.test(line)).length;
      const trailingMatches = nonEmpty.filter((line) => trailingPattern.test(line)).length;
      stripLeading = leadingMatches >= majority;
      stripTrailing = trailingMatches >= majority;
    }
    if (stripLeading || stripTrailing) {
      const rebuilt = [];
      rebuilt.length = lines.length;
      for (let i = 0; i < lines.length; i += 1) {
        let line = lines[i];
        if (stripLeading) {
          line = line.replace(leadingPattern, "");
        }
        if (stripTrailing) {
          line = line.replace(trailingPattern, "");
        }
        rebuilt[i] = line;
      }
      result = rebuilt.join("\n");
    }
    const boxAfterPipe = new RegExp(`\\|\\s*[${BOX_CLASS}]+\\s*`, "g");
    result = result.replace(boxAfterPipe, "| ");
    const boxPathJoin = new RegExp(`([:/])\\s*[${BOX_CLASS}]+\\s*([A-Za-z0-9])`, "g");
    result = result.replace(boxPathJoin, "$1$2");
    const boxMidToken = new RegExp(`(\\S)\\s*[${BOX_CLASS}]+\\s*(\\S)`, "g");
    result = result.replace(boxMidToken, "$1 $2");
    const boxAnywhere = new RegExp(`\\s*[${BOX_CLASS}]+\\s*`, "g");
    result = result.replace(boxAnywhere, " ");
    result = result.replace(/ {2,}/g, " ");
    const trimmed = result.trim();
    return trimmed === text ? null : trimmed;
  }
  function repairWrappedUrl(text) {
    const trimmed = text.trim();
    const lower = trimmed.toLowerCase();
    const schemeCount = countOccurrences(lower, "https://") + countOccurrences(lower, "http://");
    if (schemeCount !== 1) {
      return null;
    }
    if (!(lower.startsWith("http://") || lower.startsWith("https://"))) {
      return null;
    }
    const collapsed = trimmed.replace(/\s+/g, "");
    if (collapsed === trimmed) {
      return null;
    }
    const validUrl = /^https?:\/\/[A-Za-z0-9._~:/?#\[\]@!$&'()*+,;=%-]+$/;
    return validUrl.test(collapsed) ? collapsed : null;
  }
  function countOccurrences(haystack, needle) {
    if (needle.length === 0) {
      return 0;
    }
    let count = 0;
    let idx = 0;
    for (; ; ) {
      const found = haystack.indexOf(needle, idx);
      if (found === -1) {
        break;
      }
      count += 1;
      idx = found + needle.length;
    }
    return count;
  }
  function transformIfCommand(text, aggressiveness, opts) {
    if (!text.includes("\n")) {
      return null;
    }
    const lines = text.split("\n");
    if (lines.length < 2) {
      return null;
    }
    if (lines.length > 10) {
      return null;
    }
    const newlineCount = lines.length - 1;
    const overrideHigh = aggressiveness === "high";
    if (!overrideHigh && newlineCount > 4) {
      return null;
    }
    const nonEmpty = lines.filter((l) => l.trim().length > 0);
    if (!overrideHigh && isLikelyList(nonEmpty)) {
      return null;
    }
    const hasLineContinuation = text.includes("\\\n");
    const hasLineJoinerAtEol = /(\\|[|&]{1,2}|;)\s*$/m.test(text);
    const hasIndentedPipeline = /^\s*[|&]{1,2}\s+\S/m.test(text);
    const hasExplicitJoin = hasLineContinuation || hasLineJoinerAtEol || hasIndentedPipeline;
    const cmdLineCount = commandLineCount(nonEmpty);
    if (!overrideHigh && !hasExplicitJoin && cmdLineCount === nonEmpty.length && nonEmpty.length >= 3) {
      return null;
    }
    const strongSignals = hasLineContinuation || RE_PIPE_OR_OP.test(text) || RE_PROMPT_MARK.test(text) || RE_PATH_TOKEN.test(text);
    const hasKnownPrefix = containsKnownCommandPrefix(nonEmpty);
    if (!overrideHigh && !strongSignals && !hasKnownPrefix && !hasCommandPunctuation(text)) {
      return null;
    }
    if (!overrideHigh && isLikelySourceCode(text) && !strongSignals) {
      return null;
    }
    let score = 0;
    if (hasLineContinuation) {
      score += 1;
    }
    if (RE_PIPE_OR_OP.test(text)) {
      score += 1;
    }
    if (RE_PROMPT_MARK.test(text)) {
      score += 1;
    }
    if (nonEmpty.length > 0 && nonEmpty.every((l) => isLikelyCommandLine(l))) {
      score += 1;
    }
    if (RE_SUDO_CMD.test(text)) {
      score += 1;
    }
    if (RE_PATH_TOKEN.test(text)) {
      score += 1;
    }
    const threshold = aggressiveness === "low" ? 3 : aggressiveness === "normal" ? 2 : 1;
    if (score < threshold) {
      return null;
    }
    const flattened = flatten(text, opts.keep_blank_lines);
    if (flattened.output === text) {
      return null;
    }
    return flattened;
  }
  function isLikelyCommandLine(line) {
    const trimmed = line.trim();
    if (trimmed.length === 0) {
      return false;
    }
    if (trimmed.startsWith("[[")) {
      return true;
    }
    if (trimmed.endsWith(".")) {
      return false;
    }
    return /^(sudo\s+)?[A-Za-z0-9./~_-]+(?:\s+|$)/.test(trimmed);
  }
  function containsKnownCommandPrefix(lines) {
    return lines.some((line) => {
      const trimmed = line.trim();
      if (trimmed.length === 0) {
        return false;
      }
      const first = trimmed.split(/\s+/)[0] || "";
      const lower = first.toLowerCase();
      return KNOWN_PREFIXES.some((p) => lower.startsWith(p));
    });
  }
  function hasCommandPunctuation(text) {
    return /[./~_=:-]/.test(text);
  }
  function isLikelyList(lines) {
    if (lines.length === 0) {
      return false;
    }
    const listish = lines.filter((line) => {
      const trimmed = line.trim();
      if (trimmed.length === 0) {
        return false;
      }
      const hasSpaces = /\s/.test(trimmed);
      if (RE_LIST_BULLET.test(trimmed)) {
        return true;
      }
      if (RE_LIST_NUMBERED.test(trimmed)) {
        return true;
      }
      if (!hasSpaces && RE_LIST_BARETOKEN.test(trimmed) && !trimmed.includes(".") && !trimmed.includes("/") && !trimmed.includes("$")) {
        return true;
      }
      return false;
    }).length;
    return listish >= Math.floor(lines.length / 2) + 1;
  }
  function isLikelySourceCode(text) {
    const hasBraces = text.includes("{") || text.includes("}") || text.toLowerCase().includes("begin");
    const hasKeywords = RE_SOURCE_KEYWORD.test(text);
    return hasBraces && hasKeywords;
  }
  function commandLineCount(lines) {
    return lines.filter(isLikelyCommandLine).length;
  }
  function flatten(text, preserveBlankLines) {
    let result = text;
    if (preserveBlankLines) {
      result = result.replace(RE_BLANK_PLACEHOLDER, "__TRIMMEH_BLANK__PLACEHOLDER__");
    }
    result = result.replace(RE_HYPHEN_JOIN, "$1-$2");
    result = result.replace(RE_ADJ_WORD_JOIN, "$1$2");
    result = result.replace(RE_PATH_JOIN, "$1$2");
    let mergedBackslash = false;
    if (RE_BACKSLASH_NEWLINE_TEST.test(result)) {
      mergedBackslash = true;
      result = result.replace(RE_BACKSLASH_NEWLINE, " ");
    }
    result = result.replace(RE_NEWLINES_TO_SPACE, " ");
    result = result.replace(RE_SPACES, " ");
    if (preserveBlankLines) {
      result = result.split("__TRIMMEH_BLANK__PLACEHOLDER__").join("\n\n");
    }
    return { output: result.trim(), mergedBackslash };
  }

  // trimmeh-core-js/src/kde-entry.ts
  globalThis.TrimmehCore = {
    trim,
    DEFAULT_TRIM_OPTIONS
  };
})();
