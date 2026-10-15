#ifndef FLECHA_LOWERING_BOUND_IR_DUMPER_HPP
#define FLECHA_LOWERING_BOUND_IR_DUMPER_HPP

#include <string>

#include "bound_ir.hpp"

namespace flecha::lowering {

[[nodiscard]] std::string DumpBoundProgram(const BoundProgram& program);

}  // namespace flecha::lowering

#endif  // FLECHA_LOWERING_BOUND_IR_DUMPER_HPP
