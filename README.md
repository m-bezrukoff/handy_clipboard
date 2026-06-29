# MaxB Handy Clipboard

MaxB Handy Clipboard is a small native Win32 clipboard history utility for
Windows 11 x64. It watches the clipboard, keeps up to 20 recent text entries,
and opens a compact history window from a global hotkey.

## Features

- Stores recent plain-text clipboard entries in memory.
- Opens clipboard history with a configurable global shortcut.
- Restores a selected entry to the clipboard and pastes it into the previous
  active window.
- Supports deleting individual history entries.
- Runs from the Windows system tray.
- Provides light and dark themes.
- Provides English and Russian UI language selection, with English selected by
  default.
- Remembers the history window size and position.

## Build

Requirements:

- Windows 11 x64
- Visual Studio Build Tools with the C++ x64 toolchain
- PowerShell

Build the application:

```powershell
.\build.ps1
```

The executable is written to:

```text
build\MaxB Handy Clipboard.exe
```

## Usage

1. Run `build\MaxB Handy Clipboard.exe`.
2. Copy text as usual.
3. Press `Ctrl+Shift+V` to open clipboard history.
4. Click a history entry to restore it to the clipboard and paste it into the
   previous active window.
5. Click the small delete button on the right side of an entry to remove only
   that entry.

## Tray Controls

- Left-click the tray icon to open clipboard history.
- Right-click the tray icon to open settings.

## Settings

The settings window lets you:

- Change the global shortcut, including shortcuts that use the `Win` key.
- Use modifier-only shortcuts such as `Ctrl+Shift+Win`.
- Enable or disable launch at Windows startup.
- Switch between light and dark themes.
- Switch the UI language between English and Russian.
- Exit the application.

## Interface

The history and settings windows use a compact Windows 11-style interface with
Segoe UI typography, 6 px corner radius, light separators, hover states, and
resizable clipboard history.

When the history window is open, new copied text entries appear at the top of
the list immediately. On first launch, the history window opens near the center
of the current monitor; after that, the application remembers its size and
position.

## Notes

- Clipboard history is stored only in memory and is cleared when the application
  exits.
- The application stores up to 20 entries and sorts them by recency. New and
  recently used entries appear higher in the list.
- Only plain text (`CF_UNICODETEXT`) is stored. Files, images, and other
  clipboard formats are ignored.
- A single text entry is limited to 4 MB to keep memory usage predictable.
- Pasting into applications running as administrator may fail if MaxB Handy
  Clipboard is running without elevated privileges.
