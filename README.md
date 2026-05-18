# Terry

```
╔══════════════════════════════════════════════════════════════╗
║                        T E R R Y                            ║
╠══════════════════════════════════════════════════════════════╣
║                                                             ║
║              ▄▄▄▄▄▄▄▄▄▄▄▄▄▄▄▄▄▄▄▄▄▄▄▄▄▄▄▄▄▄▄              ║
║            ██████████████████████████████████████           ║
║           ████████████████████████████████████████          ║
║           ████████████████████████████████████████          ║
║           ████████████████████████████████████████          ║
║   ████████████████████████████████████████████████████████  ║
║   ████▀▀▀▀▀▀▀▀▀▀▀▀▀▀▀▀▀▀▀▀▀▀▀▀▀▀▀▀▀▀▀▀▀▀▀▀▀▀▀▀████  ║
║                                                             ║
║                  ╭───────────────────────╮                  ║
║                  │   ╔══════╗  ╔══════╗  │                  ║
║                  │   ║  ◉   ╠══╣   ◉  ║  │                  ║
║                  │   ╚══════╝  ╚══════╝  │                  ║
║                  │         ╰──────╯      │                  ║
║                  │  ≋≋≋≋≋≋≋≋≋≋≋≋≋≋≋≋≋  │                  ║
║                  │ ≋≋≋≋≋≋≋≋≋≋≋≋≋≋≋≋≋≋≋ │                  ║
║                  │≋≋≋≋≋≋≋ ╭──╮ ≋≋≋≋≋≋≋≋│                  ║
║                  │ ≋≋≋≋≋≋≋≋≋≋≋≋≋≋≋≋≋≋≋ │                  ║
║                  │  ≋≋≋≋≋≋≋≋≋≋≋≋≋≋≋≋≋  │                  ║
║                  ╰───────────────────────╯                  ║
║                          ▓▓▓▓▓▓▓▓▓▓▓                       ║
║                         ▓▓▓▓▓▓▓▓▓▓▓▓▓                      ║
║                                                             ║
║       Sir Terry Pratchett OBE   ·   1948 – 2015            ║
║                  GNU Terry Pratchett                        ║
║                                                             ║
╚══════════════════════════════════════════════════════════════╝
```

---

Terry is a C++ TUI terminal wrapper if Terry Pratchett had designed it. It wraps any shell (cmd, PowerShell, bash) in a Discworld-flavoured animated interface. The shell underneath is completely untouched. It just looks magical.

## Features

- Animated octarine border that breathes through deep purple → electric green → mid purple
- Discworld-flavoured boot sequence: Great A'Tuin ASCII art and Death's greeting
- Magic particle drift across the terminal background
- The Luggage walking silently along the status bar, going somewhere important
- Rincewind appearing briefly every few minutes, running across the top of the screen
- Sardonic Discworld vocabulary throughout all UI chrome (never the shell output)
- Fully functional: every command, every shell, exactly as normal underneath

## Build

**Requirements:** C++17 compiler, CMake 3.16+, internet connection (FTXUI is fetched automatically)

**Windows:**
```
cmake -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build --config Release
.\build\Release\terry.exe
```

**Linux:**
```
cmake -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build
./build/terry
```

## Configuration

Terry uses a small plaintext config file to set your preferred shell.

| Platform | Path                          |
|----------|-------------------------------|
| Windows  | `%APPDATA%\terry\config`      |
| Linux    | `~/.terry`                    |

Example contents:
```
shell=C:\Windows\System32\PowerShell\v1.0\powershell.exe
```

Leave the file absent to use the platform default (`cmd.exe` on Windows, `/bin/bash` on Linux).

## Supported Platforms

| Platform        | Shell support               | Notes                          |
|-----------------|-----------------------------|--------------------------------|
| Windows 10 1809+ | cmd, PowerShell, any `.exe` | Requires ConPTY (built in)     |
| Linux           | bash, zsh, fish, any POSIX  | Uses standard POSIX PTY        |

## The Characters

| Character   | Where they appear                                          |
|-------------|------------------------------------------------------------|
| **DEATH**   | Boot greeting, status bar label, farewell on exit          |
| The Luggage | Walks the status bar. Silent. Purposeful. Going somewhere. |
| Rincewind   | Runs across the top of the screen every few minutes.       |
