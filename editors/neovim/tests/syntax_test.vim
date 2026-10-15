filetype plugin on
syntax enable

execute 'edit ' . fnameescape(
            \ expand('<sfile>:p:h') . '/fixtures/highlighting.fl')

function! s:HighlightAt(text, offset) abort
    call cursor(1, 1)
    let position = searchpos('\V' . escape(a:text, '\'), 'W')
    call assert_notequal([0, 0], position, 'missing fixture text: ' . a:text)
    if position ==# [0, 0]
        return ''
    endif

    let column = position[1] + a:offset
    return synIDattr(synID(position[0], column, 1), 'name')
endfunction

call assert_equal('flecha', &filetype)
call assert_equal('flecha', get(b:, 'current_syntax', ''))
call assert_equal('// %s', &l:commentstring)
call assert_equal('flechaType', s:HighlightAt('stringv', 0))
call assert_equal('flechaFunction', s:HighlightAt('Echo', 0))
call assert_equal('flechaLineComment', s:HighlightAt('// NOTE', 0))
call assert_equal('flechaNumber', s:HighlightAt('42', 0))
call assert_equal('flechaFloat', s:HighlightAt('4.2', 0))
call assert_equal('flechaCharacter', s:HighlightAt("'\\n'", 0))
call assert_equal('flechaString', s:HighlightAt('"flecha\n"', 0))
call assert_equal('flechaRepeat', s:HighlightAt('while', 0))
call assert_equal('flechaBoolean', s:HighlightAt('true', 0))
call assert_equal('flechaWordOperator', s:HighlightAt('and', 0))
call assert_equal('flechaConditional', s:HighlightAt('also', 0))
call assert_equal('flechaOperator', s:HighlightAt(':=', 0))
call assert_equal('flechaOperator', s:HighlightAt('&&', 0))
call assert_equal('flechaOperator', s:HighlightAt('||', 0))
call assert_equal('flechaCastMethod', s:HighlightAt('.cast', 0))
call assert_equal('flechaCast', s:HighlightAt(' as ', 1))
call assert_equal('flechaJump', s:HighlightAt('jumpto', 0))
call assert_equal('flechaJumpTarget', s:HighlightAt('final_position;', 0))
call assert_equal('flechaLabel', s:HighlightAt('#final_position', 0))
call assert_equal('flechaStatement', s:HighlightAt('proceed', 0))
call assert_equal('flechaStatement', s:HighlightAt('break', 0))
call assert_equal('flechaStatement', s:HighlightAt('return', 0))

call assert_equal('flechaModifier', s:HighlightAt('constptr', 0))
call assert_equal('flechaModifier', s:HighlightAt('constref', 0))
call assert_equal('flechaModifier', s:HighlightAt('&const', 1))
call assert_equal('flechaOperator', s:HighlightAt('&const', 0))
call assert_notequal('flechaModifier', s:HighlightAt('constval', 0))

if !empty(v:errors)
    for error in v:errors
        echomsg error
    endfor
    cquit 1
endif

quit
