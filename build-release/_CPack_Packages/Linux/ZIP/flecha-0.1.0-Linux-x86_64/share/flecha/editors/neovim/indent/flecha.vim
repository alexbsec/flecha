if exists('b:did_indent')
    finish
endif
let b:did_indent = 1

" Flecha's brace-delimited blocks use C-style indentation. Reindent a closing
" brace as it is typed instead of retaining the preceding body's indentation.
setlocal cindent
setlocal indentexpr=FlechaIndent()
setlocal cinkeys+=0}
" Colons belong to Flecha operators and slices, not C-style labels.
setlocal cinkeys-=:
setlocal cinoptions+=L0

if !exists('*FlechaIndent')
    function! FlechaIndent() abort
        let previous = prevnonblank(v:lnum - 1)
        " C indentation reads `value := expression;` as a label followed by
        " a statement and aligns the next line after the colon. Keep Flecha
        " statements at their existing block indentation instead.
        if getline(v:lnum) !~# '^\s*}'
                    \ && getline(previous) =~# '^\s*\h\w*\s*:=.*;\s*\%(//.*\)\?$'
            return indent(previous)
        endif
        return cindent(v:lnum)
    endfunction
endif

let b:undo_indent = 'setlocal cindent< indentexpr< cinkeys< cinoptions<'
