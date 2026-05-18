# Terry — Agent Context

A Discworld-flavoured terminal wrapper written in C++17.
Pick this file up and you know where you are.

---

## What It Is

**Terry** wraps any shell (cmd, PowerShell, bash) in an animated Discworld TUI. The child
shell runs completely untouched underneath. The wrapper composites an animated border,
particle effects, and sprite characters on top at ~60 fps using FTXUI.

The name comes from Terry Pratchett. The UI vocabulary is Discworld throughout.

**Executable:** `terry.exe` (Windows) / `terry` (Linux)  
**Remote:** `https://github.com/vimplusplus/Terry.git`, branch `main`  
**Licence:** MIT

---

## Tech Stack

| Thing | Detail |
|-------|--------|
| Language | C++17 |
| TUI framework | FTXUI v5.0.0 — fetched at CMake configure time |
| PTY (Windows) | ConPTY (`CreatePseudoConsole`) — requires Windows 10 build 1809+ |
| PTY (Linux) | `forkpty()` |
| Build system | CMake 3.16+ |
| Compiler (Windows) | Visual Studio 2022 Community (VS 17 / MSBuild 18) |
| HTTP (Windows) | WinINet (for rincewind VS Code download) |

---

## Build Commands

```powershell
# Configure (first time, or after CMakeLists.txt changes)
$cmake = "C:\Program Files\Microsoft Visual Studio\18\Community\Common7\IDE\CommonExtensions\Microsoft\CMake\CMake\bin\cmake.exe"
& $cmake -B build -S . -G "Visual Studio 17 2022" -A x64

# Build
& $cmake --build build --config Release

# Kill running instance first (LNK1104 if terry.exe is open)
Stop-Process -Name terry -Force -ErrorAction SilentlyContinue
```

Output: `build\Release\terry.exe`

Configure also runs `scripts/embed_skills.cmake` which reads SKILL.md files from
`references/` and generates `src/generated/skill_content.h` (hex char arrays —
raw string literals would hit MSVC's 65535-byte limit on large files).

---

## Source Files

| File | Role |
|------|------|
| `src/main.cpp` | Entry point. Loads config, creates TuiApp, runs event loop |
| `src/tui_app.cpp` | ~1100 lines. The whole TUI: render loop, CatchEvent, all states |
| `include/tui_app.h` | TuiApp class declaration. AppState enum, all member declarations |
| `src/cell_buffer.cpp` | 2D grid of terminal cells (char + colour) |
| `src/vt_parser.cpp` | ANSI/VT100 escape sequence parser; writes to CellBuffer |
| `src/shell_host_windows.cpp` | ConPTY process + I/O threads |
| `src/shell_host_posix.cpp` | forkpty process + I/O threads |
| `src/boot_sequence.cpp` | Animated boot: ASCII A'Tuin art → Death greeting lines |
| `src/particle_system.cpp` | Drifting background glyph particles |
| `src/animations.cpp` | Luggage sprite (status bar walker) + Rincewind sprite |
| `src/background_art.cpp` | Full-screen ASCII art backgrounds |
| `src/vscode_launcher.cpp` | Detect/install/open VS Code (rincewind command) |
| `src/skill_registry.cpp` | Static array of 19 curated skills; includes generated content |
| `src/generated/skill_content.h` | AUTO-GENERATED — do not edit. 19 SKILL.md files as hex char arrays |
| `scripts/embed_skills.cmake` | Reads `references/*/SKILL.md`, writes skill_content.h |

---

## AppState Machine

```
Boot ──► Live ──► LuggageBrowser ──► LuggageInstall ──► Live
              └──► Rincewind ──────────────────────────► Live
              └──► Exit
```

- **Boot** — animated boot sequence (A'Tuin art + Death greeting). Auto-advances to Live.
- **Live** — normal shell pass-through. Intercepts typed commands before they hit the shell.
- **LuggageBrowser** — full-screen skill selector. Triggered by typing `luggage` + Enter.
- **LuggageInstall** — path input → threaded install → log view.
- **Rincewind** — VS Code detection/install/launch overlay. Triggered by `rincewind` + Enter.

### Intercepted commands (Live state)
| Typed | Effect |
|-------|--------|
| `luggage` + Enter | Opens LuggageBrowser (does NOT reach shell) |
| `rincewind` + Enter | Opens Rincewind overlay (does NOT reach shell) |
| Anything else + Enter | Passes through to child shell normally |

---

## The Luggage Command

Type `luggage` at the prompt → full-screen browser opens with 19 curated skills in two tiers:

**Recommended (pre-checked):** caveman, caveman-commit, caveman-compress, caveman-help,
caveman-review, caveman-stats, cavecrew, openspec-propose, openspec-apply-change,
openspec-explore, openspec-archive-change, skill-creator

**Optional:** pdf, pptx, docx, xlsx, officecli, canvas-design, frontend-design

Navigate: `↑↓` / `jk`. Toggle: `Space`. Confirm: `Enter`. Cancel: `Escape`.

On confirm → path input screen → Enter → background thread (`DoLuggageInstall`) creates:

```
<path>/
├── input/               ← drop source files here
├── output/              ← AI-generated files land here
├── theme/
│   └── raw/             ← drop brand documents here
├── .github/
│   └── skills/
│       ├── <id>/SKILL.md    ← one per selected skill
│       └── theme-extract/SKILL.md  ← always installed
├── copilot-instructions.md  ← wires all skills + workspace conventions
├── SKILLS.md                ← plain-English guide
└── skills-lock.json         ← provenance (source repos + timestamps)
```

Open the folder in VS Code with GitHub Copilot → every skill is live immediately.

**Theme workflow:** drop brand PDFs/decks into `theme/raw/`, say *"generate my theme"* →
Copilot writes `theme/brand-guide.md` (persisted brand guide). All future document/
presentation output reads it automatically.

---

## The Rincewind Command

Type `rincewind` at the prompt → overlay opens and:
1. Detects VS Code (`code.cmd` on PATH, or registry)
2. If not found → downloads VS Code installer via WinINet → silent install
3. Opens current working directory in VS Code (`ShellExecuteW`)

Implemented in `src/vscode_launcher.cpp`.

---

## Key Conventions

- **Commits:** conventional commits (`feat:`, `fix:`, `refactor:` etc.)
- **String literals:** Use hex char arrays (not raw string literals) for large embedded
  content — MSVC C2026 limits string literals to 65535 bytes.
- **Threading:** `DoLuggageInstall` and `StartRincewindWorker` run on `std::thread`.
  Post log lines via `std::lock_guard<std::mutex>` on `luggage_log_mutex_` /
  `rincewind_log_mutex_`. Set `*_done_ = true` when finished.
- **FTXUI:** `CatchEvent` returns `true` to consume an event (stop it reaching shell),
  `false` to pass it through.
- **No fabrication:** Never invent facts about clients, brand values, or features.
  If unconfirmed, say "unknown — confirm with client."

---

## OpenSpec Workflow

Changes are proposed, designed, and implemented via the OpenSpec workflow:

```
openspec/
├── config.yaml           ← schema: spec-driven
├── changes/
│   ├── <name>/           ← active changes
│   │   ├── proposal.md
│   │   ├── design.md
│   │   └── tasks.md
│   └── archive/
│       └── YYYY-MM-DD-<name>/   ← archived after implementation
└── specs/
```

**Skills available** (in `.github/skills/`):
- `/opsx:propose` — write a new change proposal + design + tasks in one step
- `/opsx:apply` — implement tasks from a change
- `/opsx:archive` — archive a completed change

**Active changes:** none (all archived as of 2026-05-18)

---

## References Folder

`references/` contains source SKILL.md files embedded at build time. Not shipped in the
final binary's data section directly — embedded via `scripts/embed_skills.cmake` into
`src/generated/skill_content.h`.

```
references/
├── Agentic Skills/caveman/skills/<name>/SKILL.md   ← 7 caveman skills
├── BillingPlatform/.github/skills/<name>/SKILL.md  ← 12 skills (openspec + docs)
└── Incite templates/                                ← brand reference docs (not embedded)
```

---

## Current State (as of 2026-05-18, commit a484b39)

All three major features are complete and shipped:

| Feature | Commit | Status |
|---------|--------|--------|
| Boot sequence + animated border | earlier | ✓ done |
| Rincewind (VS Code launcher) | `a0c2f5d` | ✓ done |
| Luggage skill browser (19 skills) | `f79a4fb` | ✓ done |
| Luggage workspace scaffold (input/output/theme) | `a484b39` | ✓ done |

No active OpenSpec changes. Ready for new proposals.

---

## What A New Agent Should Know

1. **Build first.** Always kill `terry.exe` before building or you get LNK1104.
2. **Don't edit `skill_content.h`** — it's auto-generated. Edit the source SKILL.md in
   `references/` and re-run cmake configure.
3. **`tui_app.cpp` is the main file.** Most features live here.
4. **CatchEvent returns bool.** `true` = consumed (don't pass to shell). `false` = pass through.
5. **Use OpenSpec** for any non-trivial change. `/opsx:propose` → implement → `/opsx:archive`.
6. **Caveman mode is on** by default (see `.github/copilot-instructions.md`). Responses are terse.
   Code and commits are written normally regardless.
