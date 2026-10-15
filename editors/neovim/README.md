# Flecha support for Neovim

This runtime plugin provides `.fl` filetype detection, syntax highlighting, and
basic buffer settings for the currently implemented Flecha language.

## Install

For a development checkout, link this directory into Neovim's native package
path:

```sh
mkdir -p ~/.local/share/nvim/site/pack/flecha/start
ln -s /path/to/flecha/editors/neovim \
    ~/.local/share/nvim/site/pack/flecha/start/flecha.nvim
```

Restart Neovim and open a `.fl` file. `:set filetype?` should report
`filetype=flecha`.

Plugin managers that support local directories can use `editors/neovim` as a
local plugin source instead.

## Test

From the repository root:

```sh
make test-neovim
```

The test runs Neovim headlessly and checks filetype detection, buffer settings,
and representative syntax groups.
