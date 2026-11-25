// The Z80 instantiated for the flat test bus.
#include <emucpc/z80/flat_bus.hpp>
#include <emucpc/z80/step.hpp>

namespace emucpc::z80 {

template void Cpu::step<FlatBus>(FlatBus&) noexcept;

}  // namespace emucpc::z80
