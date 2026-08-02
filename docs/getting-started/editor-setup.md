# Editor Setup

Uranite is an indentation-sensitive language. Unlike brace-delimited languages where whitespace is cosmetic, Uranite interprets leading whitespace as structural block boundaries. A misconfigured editor that mixes tabs and spaces, silently trims indentation from blank lines, or uses the wrong indentation width will produce compilation errors or silently corrupt program structure. Getting editor settings right is not a preference — it is a correctness requirement.

This guide covers configuration for VS Code, Vim/Neovim, JetBrains IDEs, Emacs, and Sublime Text, followed by integration with `uranite-fmt` for automated formatting and linting.

---

## Table of Contents

- [Editor Setup](#editor-setup)
  - [Table of Contents](#table-of-contents)
  - [Why Whitespace Settings Matter](#why-whitespace-settings-matter)
  - [EditorConfig](#editorconfig)
  - [Visual Studio Code](#visual-studio-code)
    - [Workspace Settings](#workspace-settings)
    - [Syntax Highlighting](#syntax-highlighting)
    - [Format on Save](#format-on-save)
  - [Vim and Neovim](#vim-and-neovim)
    - [Filetype Detection](#filetype-detection)
    - [Indentation Configuration](#indentation-configuration)
    - [Syntax Highlighting for Vim](#syntax-highlighting-for-vim)
    - [Format on Save for Vim](#format-on-save-for-vim)
  - [JetBrains IDEs](#jetbrains-ides)
    - [File Type Registration](#file-type-registration)
    - [Code Style Configuration](#code-style-configuration)
    - [External Tool Integration](#external-tool-integration)
  - [Emacs](#emacs)
  - [Sublime Text](#sublime-text)
  - [The uranite-fmt Formatter](#the-uranite-fmt-formatter)
    - [Formatter Invocation](#formatter-invocation)
    - [Formatting Rules](#formatting-rules)
    - [The Linter](#the-linter)
    - [Lint Rules](#lint-rules)
    - [CI Pipeline Integration](#ci-pipeline-integration)

---

## Why Whitespace Settings Matter

Uranite uses indentation to define block structure. At each line, the compiler counts leading whitespace (spaces count as 1 unit, tabs count as 4 units) and compares the result against the current nesting level:

- If indentation **increases** from the previous level, a new block opens. This is how function bodies, `if` branches, loop bodies, and class definitions begin.
- If indentation **decreases**, one or more blocks close. The indentation must match an existing outer level exactly.
- If indentation stays the **same**, the current block continues.
- If indentation does not match **any** enclosing level, the compiler reports a fatal error: "inconsistent indentation — indentation does not match any outer level".

Because tabs are counted as 4 units internally, a tab character followed by 2 spaces produces an indentation level of 6. This means mixing tabs and spaces within a file creates ambiguous indentation that varies depending on the editor's tab display width. The only safe configuration is **spaces-only with a width of 4**.

Three editor settings are critical:

| Setting | Required Value | Why |
|---|---|---|
| Insert spaces (not tabs) | Enabled | Mixing tabs and spaces produces inconsistent indentation levels across editors. Spaces-only eliminates ambiguity. |
| Tab size / indent width | 4 | Uranite counts one tab as 4 spaces. Using a different width causes indentation levels to disagree with visual alignment, producing cryptic errors. |
| Trim trailing whitespace | Disabled or cautious | Aggressive trailing whitespace trimming can remove indentation from intentionally blank lines inside blocks, altering block structure and producing unexpected errors. |

Additionally, ensure "insert final newline" is enabled. A missing final newline can cause edge-case tokenization issues in some editor configurations.

---

## EditorConfig

The simplest way to enforce correct settings across all editors is an `.editorconfig` file in the project root. Most editors and IDEs (VS Code, Vim with plugin, JetBrains, Sublime Text, Emacs) support EditorConfig natively or via a plugin:

```ini
[*.urn]
indent_style = space
indent_size = 4
tab_width = 4
end_of_line = lf
charset = utf-8
trim_trailing_whitespace = false
insert_final_newline = true
```

This file is committed to version control and ensures every contributor's editor uses the same whitespace settings regardless of their personal configuration. The editor-specific sections below provide finer-grained control, but this `.editorconfig` covers the essentials.

---

## Visual Studio Code

### Workspace Settings

Create a `.vscode/settings.json` file in your project root with Uranite-specific overrides:

```json
{
    "[uranite]": {
        "editor.tabSize": 4,
        "editor.insertSpaces": true,
        "editor.detectIndentation": false,
        "editor.trimAutoWhitespace": false,
        "editor.renderWhitespace": "boundary",
        "editor.insertFinalNewline": true,
        "files.trimTrailingWhitespace": false
    },
    "files.associations": {
        "*.urn": "uranite"
    }
}
```

Each setting serves a specific purpose:

- **`editor.tabSize: 4`** — Matches Uranite's indentation unit. Visual indentation aligns with how the compiler counts whitespace.
- **`editor.insertSpaces: true`** — Pressing Tab inserts 4 space characters, not a tab character.
- **`editor.detectIndentation: false`** — Prevents VS Code from overriding the tab size based on existing file content. Without this, opening a file that uses 2-space indentation in a comment block could cause VS Code to silently switch to 2-space mode.
- **`editor.trimAutoWhitespace: false`** — Prevents VS Code from stripping whitespace from lines that become "empty" during editing. In an indentation-sensitive language, an indented blank line inside a function body is structurally meaningful.
- **`editor.renderWhitespace: "boundary"`** — Renders whitespace characters at word boundaries, making mixed tabs/spaces visually obvious without cluttering the display.
- **`editor.insertFinalNewline: true`** — Ensures the file ends with a newline.
- **`files.trimTrailingWhitespace: false`** — Disables the global trailing whitespace trimmer for `.urn` files.

### Syntax Highlighting

Until a dedicated Uranite language extension is available, Python's syntax highlighting provides reasonable coverage because both languages share indentation-based blocks and many keywords (`class`, `if`, `elif`, `else`, `for`, `while`, `return`, `import`, `from`, `try`, `except`, `finally`, `and`, `or`, `not`, `is`, `in`, `async`, `await`, `lambda`, `raise`, `break`, `continue`, `pass`, `True`, `False`, `None`).

To use Python highlighting while keeping Uranite-specific settings, add the file association:

```json
{
    "files.associations": {
        "*.urn": "python"
    }
}
```

Python mode correctly handles indentation-based block folding, string highlighting (including triple-quoted `"""..."""` doccomments), `#` line comments, and numeric literal formatting.

Python mode does not highlight Uranite-specific keywords like `match`, `unit`, `interface`, `implements`, `extends`, `override`, `trait`, `unsafe`, `defer`, `delete`, `extern`, `raises`, `backed`, `addressof`, `move`, `own`, `mut`, `volatile`, `parent`, `property`, `final`, `native`, `virtual`, `reference`, `instanceof`, `subclassof`, `asm`, `where`, `yield`, `switch`, `case`, `type`, `use`, or generic type syntax (`ArrayList<E>`). This is a pragmatic interim solution. When the Uranite VS Code extension is released, it will provide full semantic highlighting, diagnostics integration, and LSP support.

**Note on settings scope:** If you use `"*.urn": "python"`, the language ID for `.urn` files becomes `python`, which means the `[uranite]` settings block will not apply. In a Uranite-only project, this is harmless — the settings affect only Python files in the workspace. In a mixed Uranite/Python project, either keep `"*.urn": "uranite"` (losing syntax highlighting) or accept that `[python]` settings apply to both file types.

### Format on Save

To run `uranite-fmt` on every save, install the "Run on Save" extension (`emeraldwalk.RunOnSave`) and add to your settings:

```json
{
    "emeraldwalk.runonsave": {
        "commands": [
            {
                "match": "\\.urn$",
                "cmd": "uranite-fmt --write ${file}"
            }
        ]
    }
}
```

This invokes `uranite-fmt` with the `--write` flag, which formats the file and overwrites it in place. VS Code detects the external file change and reloads the buffer.

For projects where `uranite-fmt` is built locally, use the full path to the binary:

```json
{
    "emeraldwalk.runonsave": {
        "commands": [
            {
                "match": "\\.urn$",
                "cmd": "${workspaceFolder}/build/uranite-fmt --write ${file}"
            }
        ]
    }
}
```

---

## Vim and Neovim

### Filetype Detection

Add filetype detection for `.urn` files. Create the file `~/.vim/ftdetect/uranite.vim` (or `~/.config/nvim/ftdetect/uranite.vim` for Neovim):

```vim
autocmd BufNewFile,BufRead *.urn setfiletype uranite
```

This causes Vim to recognize `.urn` files and apply Uranite-specific settings defined in the filetype plugin.

### Indentation Configuration

Create `~/.vim/ftplugin/uranite.vim` (or `~/.config/nvim/ftplugin/uranite.vim`):

```vim
setlocal expandtab
setlocal shiftwidth=4
setlocal softtabstop=4
setlocal tabstop=4
setlocal autoindent
setlocal smartindent
setlocal fileformat=unix
setlocal fileencoding=utf-8
setlocal fixendofline
```

Each option:

- **`expandtab`** — Converts tab key presses into space characters.
- **`shiftwidth=4`** — Sets the number of spaces used for each level of auto-indentation and the `>>` / `<<` shift commands.
- **`softtabstop=4`** — Makes Tab insert 4 spaces and Backspace delete 4 spaces at indentation boundaries.
- **`tabstop=4`** — Sets the visual display width of any existing tab characters to 4 columns.
- **`autoindent`** — Copies indentation from the current line when starting a new line.
- **`smartindent`** — Increases indentation after lines ending with `:`, matching Uranite's block-opening syntax.
- **`fixendofline`** — Ensures files end with a newline on save.

To prevent trailing whitespace stripping (which some Vim configurations enable globally), verify your `.vimrc` does not contain an autocmd that strips trailing whitespace for all filetypes. If it does, exclude `.urn` files:

```vim
autocmd BufWritePre * if &filetype != 'uranite' | %s/\s\+$//e | endif
```

### Syntax Highlighting for Vim

As an interim measure, Python syntax highlighting works for `.urn` files. To use Python highlighting while keeping Uranite-specific indentation settings, set the filetype to `uranite` and the syntax to `python`:

```vim
autocmd BufNewFile,BufRead *.urn setfiletype uranite
autocmd BufNewFile,BufRead *.urn setlocal syntax=python
```

This preserves the `uranite` filetype (so the ftplugin settings apply) while borrowing Python's syntax highlighting rules.

### Format on Save for Vim

Add to `~/.vim/ftplugin/uranite.vim`:

```vim
autocmd BufWritePost <buffer> silent! execute '!uranite-fmt --write %' | edit!
```

This runs `uranite-fmt --write` on the current file after every save, then reloads the buffer to reflect changes. The `silent!` prefix suppresses the shell output.

For Neovim with Lua configuration, add to `~/.config/nvim/after/ftplugin/uranite.lua`:

```lua
vim.api.nvim_create_autocmd("BufWritePost", {
    buffer = 0,
    callback = function()
        local filepath = vim.fn.expand("%:p")
        vim.fn.system({"uranite-fmt", "--write", filepath})
        vim.cmd("edit!")
    end,
})
```

---

## JetBrains IDEs

IntelliJ IDEA, CLion, and PyCharm support custom file type registration for syntax highlighting and editor behavior. The following steps apply to all JetBrains IDEs.

### File Type Registration

1. Open **Settings** (Ctrl+Alt+S) and navigate to **Editor > File Types**.
2. Click the **+** button under "Recognized File Types" to create a new file type.
3. Set the name to "Uranite" and the description to "Uranite source file".
4. In the **Syntax Highlighting** tab, configure:
   - **Line comment**: `#`
   - **Block comment start**: `"""` / **Block comment end**: `"""`
   - **Keywords (Set 1 — Declarations and control flow)**: `abstract`, `as`, `async`, `await`, `backed`, `break`, `case`, `class`, `const`, `continue`, `defer`, `delete`, `elif`, `else`, `enum`, `except`, `export`, `extends`, `extern`, `final`, `finally`, `for`, `from`, `function`, `if`, `implements`, `import`, `in`, `instanceof`, `interface`, `is`, `lambda`, `match`, `move`, `mut`, `native`, `new`, `not`, `and`, `or`, `override`, `own`, `package`, `parent`, `pass`, `private`, `property`, `protect`, `public`, `raise`, `raises`, `readonly`, `reference`, `return`, `self`, `static`, `struct`, `subclassof`, `switch`, `trait`, `try`, `type`, `unit`, `unsafe`, `use`, `virtual`, `volatile`, `where`, `while`, `yield`, `addressof`, `asm`
   - **Keywords (Set 2 — Types and values)**: `I8`, `I16`, `I32`, `I64`, `U8`, `U16`, `U32`, `U64`, `F32`, `F64`, `Boolean`, `Char`, `String`, `Void`, `None`, `True`, `False`
5. In the **File Name Patterns** section, add `*.urn`.
6. Click **Apply**.

### Code Style Configuration

Navigate to **Settings > Editor > Code Style** and create a scheme for Uranite files:

1. Set **Tab size** to 4, **Indent** to 4, **Continuation indent** to 8.
2. Enable **Use tab character**: No (unchecked).
3. Set **Keep indents on empty lines**: Yes (checked). This prevents the IDE from stripping indentation from blank lines inside blocks.

Alternatively, use the `.editorconfig` file described in the [EditorConfig](#editorconfig) section. JetBrains IDEs support EditorConfig natively.

### External Tool Integration

To run `uranite-fmt` from within the IDE:

1. Navigate to **Settings > Tools > External Tools**.
2. Click **+** to add a new tool:
   - **Name**: "Uranite Format"
   - **Program**: path to `uranite-fmt` binary (e.g., `$ProjectFileDir$/build/uranite-fmt`)
   - **Arguments**: `--write $FilePath$`
   - **Working directory**: `$ProjectFileDir$`
3. Optionally assign a keyboard shortcut under **Settings > Keymap > External Tools > Uranite Format**.

To run formatting automatically on save, install the "File Watchers" plugin, then add a watcher:

1. Navigate to **Settings > Tools > File Watchers**.
2. Click **+** and select "Custom".
3. Configure:
   - **File type**: the "Uranite" type you registered
   - **Program**: path to `uranite-fmt`
   - **Arguments**: `--write $FilePath$`
   - **Output paths to refresh**: `$FilePath$`
   - **Auto-save edited files to trigger the watcher**: enabled

---

## Emacs

Add Uranite file association and indentation settings to your Emacs configuration (`~/.emacs` or `~/.emacs.d/init.el`):

```elisp
(add-to-list 'auto-mode-alist '("\\.urn\\'" . python-mode))

(add-hook 'python-mode-hook
          (lambda ()
            (when (and buffer-file-name
                       (string-match-p "\\.urn\\'" buffer-file-name))
              (setq-local indent-tabs-mode nil)
              (setq-local tab-width 4)
              (setq-local python-indent-offset 4)
              (setq-local require-final-newline t)
              (setq-local delete-trailing-lines nil))))
```

This associates `.urn` files with `python-mode` for syntax highlighting while configuring Uranite-appropriate indentation settings. `python-indent-offset` of 4 matches Uranite's indentation width, and `indent-tabs-mode nil` ensures spaces are used.

To integrate `uranite-fmt` as a format-on-save hook:

```elisp
(defun uranite-format-buffer ()
  "Run uranite-fmt on the current buffer."
  (when (and buffer-file-name
             (string-match-p "\\.urn\\'" buffer-file-name))
    (let ((current-point (point)))
      (shell-command-on-region
       (point-min) (point-max)
       "uranite-fmt"
       (current-buffer) t)
      (goto-char current-point))))

(add-hook 'before-save-hook #'uranite-format-buffer)
```

To disable trailing whitespace cleanup for `.urn` files specifically, ensure `delete-trailing-whitespace` is not in your `before-save-hook` globally, or guard it:

```elisp
(add-hook 'before-save-hook
          (lambda ()
            (unless (and buffer-file-name
                         (string-match-p "\\.urn\\'" buffer-file-name))
              (delete-trailing-whitespace))))
```

---

## Sublime Text

Create a Uranite-specific settings file at `Packages/User/Uranite.sublime-settings`:

```json
{
    "tab_size": 4,
    "translate_tabs_to_spaces": true,
    "trim_trailing_white_space_on_save": false,
    "ensure_newline_at_eof": true,
    "detect_indentation": false
}
```

To associate `.urn` files with Python syntax highlighting, open any `.urn` file and select **View > Syntax > Python**. To make this permanent, navigate to **View > Syntax > Open all with current extension as... > Python**.

To run `uranite-fmt` on save, create a build system at `Packages/User/UraniteFormat.sublime-build`:

```json
{
    "cmd": ["uranite-fmt", "--write", "$file"],
    "selector": "source.python",
    "working_dir": "$project_path"
}
```

For automatic formatting, install the "SublimeOnSaveBuild" package via Package Control, which triggers the build system on every save.

---

## The uranite-fmt Formatter

`uranite-fmt` is the official Uranite source code formatter and linter. It is built as part of the standard toolchain and produces the `uranite-fmt` binary in the build directory. The formatter parses source files, applies canonical style rules, and outputs the result. Comments are preserved across reformatting — their positions are adjusted to match the new layout.

### Formatter Invocation

| Command | Description |
|---|---|
| `uranite-fmt file.urn` | Format a file and print the result to stdout. |
| `uranite-fmt --write file.urn` | Format a file and overwrite it in place. |
| `uranite-fmt --check file.urn` | Check if a file is already formatted. Exit code 0 if formatted, 1 if not. |
| `uranite-fmt --lint file.urn` | Run the linter on a file and print diagnostics to stderr. |
| `uranite-fmt src/` | Recursively format all `.urn` files in a directory (prints to stdout). |
| `uranite-fmt --write src/` | Recursively format all `.urn` files in a directory in place. |
| `uranite-fmt --check src/` | Check all `.urn` files in a directory. Reports counts of formatted and unformatted files. |
| `uranite-fmt --lint src/` | Lint all `.urn` files in a directory. |

When operating on a directory, `uranite-fmt` recursively discovers all files with the `.urn` extension, processes each independently, and prints a summary:

```
12 files checked: 10 formatted, 2 need formatting
```

`--write` and `--check` are mutually exclusive. `--lint` activates the linter instead of the formatter.

### Formatting Rules

The formatter applies a fixed set of layout rules that encode canonical Uranite style:

| Rule | Default | Effect |
|---|---|---|
| Indentation width | 4 | Number of spaces per indentation level. |
| Spaces inside parentheses | Yes | `foo( x, y )` instead of `foo(x, y)`. Applies when arguments are present. |
| Spaces inside empty parentheses | No | `foo()` instead of `foo( )`. Empty parentheses are compact. |
| Blank line after package | Yes | Inserts a blank line after the `package` declaration. |
| Blank line between import groups | Yes | Inserts a blank line between groups of `import` statements. |
| Blank line between top-level declarations | Yes | Inserts a blank line between top-level classes, functions, and other declarations. |
| Maximum line length | 120 | Hard line-length limit. Lines exceeding this width trigger wrapping. |
| Sort imports by package | No | When enabled, alphabetically sorts imports by package path. |
| Group imports by package prefix | No | When enabled, clusters imports sharing the same root package prefix into adjacent groups. |

The parenthesization rule (`foo( x, y )` with internal spaces) is a distinguishing element of Uranite's visual identity. The formatter enforces it automatically, so you do not need to remember it while writing code — write with or without spaces, and the formatter normalizes on save.

### The Linter

The linter is a separate static analysis pass that enforces coding standards beyond formatting. While the formatter fixes layout, the linter reports semantic and structural issues that require human judgment to resolve.

Linter diagnostics use GNU-style error format:

```
src/models/user.urn:42:5: warning [cryptic-variable] Variable name "x" is too short (minimum 3 characters)
src/services/auth.urn:15:1: warning [missing-doccomment] Public function "authenticate" is missing a doccomment
src/utils.urn:87:1: error [long-function] Function "processData" body is 215 lines (maximum 200)
```

Each diagnostic includes file path, line number, column number, severity ("warning" or "error"), rule identifier in brackets, and a message explaining the violation.

### Lint Rules

The linter enforces these rules:

| Rule ID | Default | Severity | Description |
|---|---|---|---|
| `cryptic-variable` | Enabled, min 3 chars | Warning | Flags variable names shorter than the configured minimum length. Enforces Uranite's naming convention of descriptive identifiers. |
| `cryptic-parameter` | Enabled, min 3 chars | Warning | Flags function parameter names shorter than the configured minimum length. |
| `missing-doccomment` | Enabled | Warning | Flags public classes, interfaces, enums, structs, and functions that lack a `"""..."""` doccomment. |
| `missing-complexity` | Enabled | Warning | Flags doccomments that lack a "Complexity:" section documenting algorithmic complexity. |
| `invalid-complexity-format` | Enabled | Warning | Validates that "Complexity:" values use valid Big-O notation (e.g., "O(n)", "O(n log n)", "O(1)"). |
| `at-param-style` | Enabled | Warning | Flags doccomments that use `@param` tags instead of Uranite's "Parameters:" section format. |
| `long-function` | Enabled, max 200 lines | Error | Flags functions whose body exceeds the configured maximum line count. |
| `deep-nesting` | Enabled, max 6 levels | Warning | Flags control-flow blocks (`if`, `for`, `while`, `match`, `try`) nested deeper than the configured maximum. |
| `missing-return-type` | Enabled | Warning | Flags function declarations that lack an explicit return type annotation. |

All rule thresholds are configurable. The linter maintains an allowlist of short names that bypass the cryptic-variable check (e.g., "id").

### CI Pipeline Integration

Use the `--check` flag to enforce formatting in continuous integration pipelines. This mode compares formatted output against the original source and exits with code 1 if any file differs:

```yaml
steps:
  - name: Check formatting
    run: |
      ./build/uranite-fmt --check src/
      if [ $? -ne 0 ]; then
        echo "Formatting check failed. Run 'uranite-fmt --write src/' to fix."
        exit 1
      fi

  - name: Run linter
    run: |
      ./build/uranite-fmt --lint src/
```

`--check` prints "Would reformat: <filepath>" for each file that is not properly formatted, followed by a summary line:

```
Would reformat: src/models/user.urn
Would reformat: src/services/auth.urn
15 files checked: 13 formatted, 2 need formatting
```

For a combined formatting and linting gate in a single CI step:

```bash
#!/bin/bash
set -e

echo "Checking formatting..."
./build/uranite-fmt --check src/
FORMAT_EXIT=$?

echo "Running linter..."
./build/uranite-fmt --lint src/
LINT_EXIT=$?

if [ $FORMAT_EXIT -ne 0 ] || [ $LINT_EXIT -ne 0 ]; then
    echo "Quality gate failed."
    exit 1
fi

echo "All checks passed."
```

`--check` is a read-only operation. It never modifies files, making it safe for CI environments where the source tree should remain unmodified. Exit code convention (0 for pass, 1 for failure) integrates with standard CI tools.

For pre-commit hooks, add a Git pre-commit script at `.git/hooks/pre-commit`:

```bash
#!/bin/bash

STAGED_FILES=$(git diff --cached --name-only --diff-filter=ACM | grep '\.urn$')

if [ -z "$STAGED_FILES" ]; then
    exit 0
fi

FAILED=0
for FILE in $STAGED_FILES; do
    ./build/uranite-fmt --check "$FILE"
    if [ $? -ne 0 ]; then
        FAILED=1
    fi
done

if [ $FAILED -ne 0 ]; then
    echo "Pre-commit check failed: some .urn files need formatting."
    echo "Run 'uranite-fmt --write <file>' to fix."
    exit 1
fi
```

This checks only staged `.urn` files, preventing unformatted code from being committed.
