use std::io::{self, Read};
use std::path::PathBuf;

use anyhow::Result;
use clap::{ArgAction, Args, Parser, Subcommand, ValueEnum};
use similar::TextDiff;
use serde::Serialize;
use trimmeh_core::{trim, Aggressiveness, Options};

#[derive(Parser)]
#[command(name = "trimmeh-cli")]
#[command(about = "CLI companion for Trimmeh clipboard trimmer")]
struct Cli {
    #[command(subcommand)]
    command: Command,
}

#[derive(Subcommand)]
enum Command {
    /// Trim stdin and print the result.
    Trim(TrimArgs),
    /// Show unified diff between input and trimmed output.
    Diff(TrimArgs),
}

#[derive(Args, Clone)]
struct TrimArgs {
    /// Input file (use - for stdin). Alias: --input
    #[arg(long = "trim", alias = "input", value_name = "FILE")]
    input: Option<PathBuf>,
    /// Aggressiveness: low, normal, high
    #[arg(short, long, value_enum, default_value_t = AggLevel::Normal)]
    aggressiveness: AggLevel,
    /// Force high aggressiveness (parity with Trimmy CLI)
    #[arg(short = 'f', long, action = ArgAction::SetTrue)]
    force: bool,
    /// Output JSON {original, trimmed, changed}
    #[arg(long, action = ArgAction::SetTrue)]
    json: bool,
    /// Keep blank lines (otherwise they are dropped)
    #[arg(long = "preserve-blank-lines", action = ArgAction::SetTrue)]
    keep_blank_lines: bool,
    /// Preserve leading box-drawing gutters
    #[arg(long = "keep-box-drawing", action = ArgAction::SetFalse, default_value_t = true)]
    strip_box_chars: bool,
    /// Strip box drawing characters (alias for default behavior)
    #[arg(long = "remove-box-drawing", action = ArgAction::SetTrue)]
    remove_box_drawing: bool,
    /// Preserve shell prompts
    #[arg(long = "keep-prompts", action = ArgAction::SetFalse, default_value_t = true)]
    trim_prompts: bool,
    /// Maximum number of lines; above this we skip trimming
    #[arg(long, default_value_t = 10)]
    max_lines: usize,
}

#[derive(ValueEnum, Clone, Copy)]
enum AggLevel {
    Low,
    Normal,
    High,
}

impl From<AggLevel> for Aggressiveness {
    fn from(value: AggLevel) -> Self {
        match value {
            AggLevel::Low => Aggressiveness::Low,
            AggLevel::Normal => Aggressiveness::Normal,
            AggLevel::High => Aggressiveness::High,
        }
    }
}

fn main() -> Result<()> {
    let cli = Cli::parse();
    match cli.command {
        Command::Trim(args) => run_trim(args),
        Command::Diff(args) => run_diff(args),
    }
}

fn run_trim(args: TrimArgs) -> Result<()> {
    let input = read_input(&args)?;
    let opts = options_from(&args);
    let aggr = if args.force {
        Aggressiveness::High
    } else {
        args.aggressiveness.into()
    };
    let result = trim(&input, aggr, opts);

    if args.json {
        #[derive(Serialize)]
        struct Payload<'a> {
            original: &'a str,
            trimmed: &'a str,
            changed: bool,
            transformed: bool,
        }
        let payload = Payload {
            original: &input,
            trimmed: &result.output,
            changed: result.changed,
            transformed: result.changed,
        };
        match serde_json::to_string_pretty(&payload) {
            Ok(json) => println!("{json}"),
            Err(e) => {
                eprintln!("Failed to encode JSON: {e}");
                std::process::exit(3);
            }
        }
    } else {
        print!("{}", result.output);
        if !result.output.ends_with('\n') {
            println!();
        }
    }

    if !result.changed && !args.force {
        std::process::exit(2);
    }
    Ok(())
}

fn run_diff(args: TrimArgs) -> Result<()> {
    let input = read_input(&args)?;
    let opts = options_from(&args);
    let aggr = if args.force {
        Aggressiveness::High
    } else {
        args.aggressiveness.into()
    };
    let result = trim(&input, aggr, opts);
    if !result.changed {
        eprintln!("(no change)");
        return Ok(());
    }
    let diff = TextDiff::from_lines(&input, &result.output);
    let formatted = diff
        .unified_diff()
        .context_radius(3)
        .header("before", "after")
        .to_string();
    print!("{formatted}");
    Ok(())
}

fn read_stdin() -> Result<String> {
    let mut buf = String::new();
    io::stdin().read_to_string(&mut buf)?;
    Ok(buf)
}

fn read_input(args: &TrimArgs) -> Result<String> {
    if let Some(path) = &args.input {
        if path.as_os_str() == "-" {
            return read_stdin();
        }
        let contents = std::fs::read_to_string(path)?;
        return Ok(contents);
    }
    read_stdin()
}

fn options_from(args: &TrimArgs) -> Options {
    Options {
        keep_blank_lines: args.keep_blank_lines,
        strip_box_chars: if args.remove_box_drawing { true } else { args.strip_box_chars },
        trim_prompts: args.trim_prompts,
        max_lines: args.max_lines,
    }
}
