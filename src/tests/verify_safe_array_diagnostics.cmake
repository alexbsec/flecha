file(MAKE_DIRECTORY "${FLECHA_OUTPUT}")

function(reject_array name code body)
    set(source "${FLECHA_OUTPUT}/${name}.fl")
    set(object "${FLECHA_OUTPUT}/${name}.o")
    file(WRITE "${source}" "module ${name};\n@imports :: lib::std::containers::array;\n${body}\n")
    file(REMOVE "${object}")
    execute_process(
        COMMAND "${FLECHA_COMPILER}" --emit-object "${source}"
            --lib "std=${FLECHA_STD}" -o "${object}"
        RESULT_VARIABLE result ERROR_VARIABLE errors TIMEOUT 30)
    if(result EQUAL 0 OR EXISTS "${object}" OR NOT errors MATCHES "${code}")
        message(FATAL_ERROR "${name}: expected ${code} without object emission: ${errors}")
    endif()
endfunction()

reject_array(copy FLECHA-S0046 [=[
void main() {
    values := array::Array<i32>::init();
    copy := values;
}
]=])
reject_array(private_field FLECHA-S0044 [=[
void main() {
    values := array::Array<i32>::init();
    values.inner.clear();
}
]=])
reject_array(return_owner FLECHA-S0046 [=[
array::Array<i32> make() {
    values := array::Array<i32>::init();
    return values;
}
void main() {}
]=])
reject_array(borrow FLECHA-S0029 [=[
void main() {
    values := array::Array<i32>::init();
    &const array::Array<i32> borrowed = &values;
    values.push(1);
    borrowed->get(0);
}
]=])
reject_array(destructible_element FLECHA-S0054 [=[
structure Owned {
    i32 value;
};
void Owned::tear(&self) override {}
Owned Owned::zero() override {
    return build Owned {};
}
void main() {
    values := array::Array<Owned>::init();
    values.reserve(1);
}
]=])
