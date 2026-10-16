# Flecha support for Neovim

This runtime plugin provides `.fl` filetype detection, syntax highlighting, and
brace-aware indentation, and basic buffer settings for the currently implemented Flecha language.

## Install

For a development checkout, link this directory into Neovim's native package
path:

```sh
mkdir -p ~/.local/share/nvim/site/pack/flecha/start
ln -s /path/to/flecha/editors/neovim \
    ~/.local/share/nvim/site/pack/flecha/start/flecha.nvim
```

Restart Neovim and open a `.fl` file. `:set filetype?` should report
`filetype=flecha`. Enable indentation plugins with `filetype plugin indent on`
if your configuration does not already do so. Closing braces align with their
opening block; indentation width follows your `shiftwidth` setting.

Plugin managers that support local directories can use `editors/neovim` as a
local plugin source instead.

## Test

From the repository root:

```sh
make test-neovim
```

The test runs Neovim headlessly and checks filetype detection, buffer settings,
representative syntax groups, and indentation while typing closing braces.
