# Flecha for Visual Studio Code

This directory contains the syntax-highlighting extension bundled with Flecha.
It recognizes `.fl` files, supplies Flecha-aware comments and brackets, and
highlights the language syntax implemented by the compiler.

For development, copy or link this directory to the VS Code extensions
directory and restart VS Code:

- Linux and macOS: `~/.vscode/extensions/flecha-language-0.1.0`
- Windows: `%USERPROFILE%\\.vscode\\extensions\\flecha-language-0.1.0`

The v0.1 extension provides grammar-based editing support only. Language-server
features such as completion, navigation, and semantic tokens are future work.
