// Z80: what SingleStepTests does not cover (interrupts, HALT, EI delay,
// repeated block instructions to their end, prefix chains, cycle edges).
#define DOCTEST_CONFIG_IMPLEMENT_WITH_MAIN
#include <cstdint>
#include <initializer_list>

#include <doctest/doctest.h>

#include <emucpc/z80/flat_bus.hpp>

namespace {

using emucpc::z80::Cpu;
using emucpc::z80::CycleKind;
using emucpc::z80::FlatBus;
using emucpc::z80::Tick;
using emucpc::z80::ticks_per_t;

constexpr std::uint8_t PF = 0x04;

struct Machine {
    Cpu cpu;
    FlatBus bus;

    void load(std::uint16_t address, std::initializer_list<std::uint8_t> bytes) {
        for (const std::uint8_t b : bytes) {
            bus.memory[address++] = b;
        }
    }

    // Runs one step; returns its length in T states.
    std::uint64_t step() {
        const Tick before = cpu.now;
        cpu.step(bus);
        return (cpu.now.value - before.value) / ticks_per_t;
    }

    std::uint16_t stacked() const {
        return static_cast<std::uint16_t>(bus.memory[cpu.sp] | (bus.memory[(cpu.sp + 1) & 0xFFFF] << 8));
    }
};

}  // namespace

TEST_CASE("power-on and reset values") {
    Cpu cpu;
    CHECK(cpu.af() == 0xFFFF);
    CHECK(cpu.sp == 0xFFFF);
    CHECK(cpu.pc == 0);
    CHECK(cpu.i == 0);
    CHECK(cpu.r == 0);
    CHECK(cpu.im == 0);
    CHECK_FALSE(cpu.iff1);
    cpu.pc = 0x1234;
    cpu.im = 2;
    cpu.iff1 = cpu.iff2 = true;
    cpu.halted = true;
    cpu.set_bc(0x5678);
    cpu.reset();
    CHECK(cpu.pc == 0);
    CHECK(cpu.im == 0);
    CHECK_FALSE(cpu.iff1);
    CHECK_FALSE(cpu.iff2);
    CHECK_FALSE(cpu.halted);
    CHECK(cpu.bc() == 0x5678);
}

TEST_CASE("IM 1: no interrupt right after EI, then RST 38h in 13 T") {
    Machine m;
    m.cpu.im = 1;
    m.cpu.sp = 0x8000;
    m.load(0x0000, {0xFB, 0x00, 0x00});  // EI, NOP, NOP
    m.bus.int_active = true;
    CHECK(m.step() == 4);  // EI
    CHECK(m.cpu.iff1);
    CHECK(m.step() == 4);  // NOP: INT not taken after EI
    CHECK(m.cpu.pc == 2);
    const std::uint8_t r = m.cpu.r;
    CHECK(m.step() == 13);
    CHECK(m.cpu.pc == 0x0038);
    CHECK(m.cpu.wz == 0x0038);
    CHECK(m.stacked() == 2);
    CHECK_FALSE(m.cpu.iff1);
    CHECK_FALSE(m.cpu.iff2);
    CHECK(m.cpu.r == r + 1);
}

TEST_CASE("INT is ignored while IFF1 is reset") {
    Machine m;
    m.cpu.im = 1;
    m.load(0x0000, {0x00});
    m.bus.int_active = true;
    CHECK(m.step() == 4);
    CHECK(m.cpu.pc == 1);
}

TEST_CASE("IM 2: vector read from I and the data bus, 19 T") {
    Machine m;
    m.cpu.im = 2;
    m.cpu.i = 0x80;
    m.cpu.sp = 0x8000;
    m.cpu.pc = 0x4000;
    m.cpu.iff1 = m.cpu.iff2 = true;
    m.load(0x80FE, {0x34, 0x12});
    m.bus.int_active = true;
    m.bus.int_vector = 0xFE;
    m.bus.record_cycles = true;
    CHECK(m.step() == 19);
    CHECK(m.cpu.pc == 0x1234);
    CHECK(m.cpu.wz == 0x1234);
    CHECK(m.stacked() == 0x4000);
    REQUIRE_FALSE(m.bus.cycles.empty());
    CHECK(m.bus.cycles.front().cycle.kind == CycleKind::int_ack);
}

TEST_CASE("IM 0: the byte on the bus is executed, FFh is RST 38h in 13 T") {
    Machine m;
    m.cpu.im = 0;
    m.cpu.sp = 0x8000;
    m.cpu.pc = 0x4000;
    m.cpu.iff1 = m.cpu.iff2 = true;
    m.bus.int_active = true;
    m.bus.int_vector = 0xFF;
    CHECK(m.step() == 13);
    CHECK(m.cpu.pc == 0x0038);
    CHECK(m.stacked() == 0x4000);
}

TEST_CASE("NMI: 11 T, IFF1 reset and IFF2 kept, RETN restores IFF1") {
    Machine m;
    m.cpu.sp = 0x8000;
    m.cpu.pc = 0x4000;
    m.cpu.iff1 = m.cpu.iff2 = true;
    m.load(0x0066, {0xED, 0x45});  // RETN
    m.bus.nmi_pending = true;
    CHECK(m.step() == 11);
    CHECK(m.cpu.pc == 0x0066);
    CHECK_FALSE(m.cpu.iff1);
    CHECK(m.cpu.iff2);
    CHECK(m.stacked() == 0x4000);
    CHECK(m.step() == 14);
    CHECK(m.cpu.pc == 0x4000);
    CHECK(m.cpu.iff1);
}

TEST_CASE("NMI is taken right after EI, INT is not") {
    Machine m;
    m.cpu.im = 1;
    m.cpu.sp = 0x8000;
    m.load(0x0000, {0xFB, 0x00});
    m.bus.int_active = true;
    CHECK(m.step() == 4);
    m.bus.nmi_pending = true;
    CHECK(m.step() == 11);
    CHECK(m.cpu.pc == 0x0066);
}

TEST_CASE("HALT: M1 cycles at the next byte, PC held, R counted; INT resumes after HALT") {
    Machine m;
    m.cpu.im = 1;
    m.cpu.sp = 0x8000;
    m.cpu.iff1 = m.cpu.iff2 = true;
    m.load(0x0000, {0x76, 0x3C});  // HALT, INC A
    m.bus.record_cycles = true;
    CHECK(m.step() == 4);
    CHECK(m.cpu.halted);
    CHECK(m.cpu.pc == 1);
    const std::uint8_t a = m.cpu.a;
    const std::uint8_t r = m.cpu.r;
    for (int k = 0; k < 3; ++k) {
        CHECK(m.step() == 4);
    }
    CHECK(m.cpu.pc == 1);
    CHECK(m.cpu.a == a);  // the INC A under PC is not executed
    CHECK(m.cpu.r == r + 3);
    CHECK(m.bus.cycles.back().address == 1);
    m.bus.int_active = true;
    CHECK(m.step() == 13);
    CHECK_FALSE(m.cpu.halted);
    CHECK(m.stacked() == 1);
}

TEST_CASE("NMI ends HALT") {
    Machine m;
    m.cpu.sp = 0x8000;
    m.load(0x0000, {0x76});
    CHECK(m.step() == 4);
    m.bus.nmi_pending = true;
    CHECK(m.step() == 11);
    CHECK_FALSE(m.cpu.halted);
    CHECK(m.stacked() == 1);
}

TEST_CASE("LD A,I copies IFF2 into P/V; an interrupt taken right after clears it") {
    Machine m;
    m.cpu.im = 1;
    m.cpu.sp = 0x8000;
    m.cpu.iff1 = m.cpu.iff2 = true;
    m.load(0x0000, {0xED, 0x57, 0xED, 0x57});  // LD A,I twice
    CHECK(m.step() == 9);
    CHECK((m.cpu.f & PF) != 0);
    CHECK(m.cpu.p);
    m.bus.int_active = true;
    CHECK(m.step() == 13);
    CHECK((m.cpu.f & PF) == 0);
}

TEST_CASE("LDIR runs to its end: 21 T per repeated byte, 16 T for the last") {
    Machine m;
    m.load(0x0000, {0xED, 0xB0});
    m.load(0x1000, {1, 2, 3, 4});
    m.cpu.set_hl(0x1000);
    m.cpu.set_de(0x2000);
    m.cpu.set_bc(4);
    std::uint64_t t = 0;
    int steps = 0;
    while (m.cpu.pc == 0) {
        t += m.step();
        ++steps;
    }
    CHECK(steps == 4);
    CHECK(t == 3 * 21 + 16);
    CHECK(m.cpu.bc() == 0);
    CHECK((m.cpu.f & PF) == 0);
    CHECK(m.cpu.hl() == 0x1004);
    CHECK(m.cpu.de() == 0x2004);
    for (std::uint16_t k = 0; k < 4; ++k) {
        CHECK(m.bus.memory[0x2000U + k] == k + 1);
    }
}

TEST_CASE("prefix chain: the last DD or FD applies, one step, 4 T per extra prefix") {
    Machine m;
    m.load(0x0000, {0xDD, 0xFD, 0x21, 0x34, 0x12});  // DD FD LD IY,1234h
    m.cpu.set_ix(0);
    CHECK(m.step() == 18);
    CHECK(m.cpu.iy() == 0x1234);
    CHECK(m.cpu.ix() == 0);
    CHECK(m.cpu.pc == 5);
    CHECK(m.cpu.r == 3);
}

TEST_CASE("nominal edges of a memory write and of an output") {
    Machine m;
    m.load(0x0000, {0x77, 0xD3, 0x10});  // LD (HL),A ; OUT (10h),A
    m.cpu.set_hl(0x4000);
    m.bus.record_cycles = true;
    CHECK(m.step() == 7);
    REQUIRE(m.bus.cycles.size() == 2);
    const auto& w = m.bus.cycles[1].cycle;
    CHECK(w.kind == CycleKind::write);
    CHECK(w.t1.value == 16);
    CHECK(w.mreq.value == 18);
    CHECK(w.wr.value == 22);
    CHECK(w.end.value == 28);
    CHECK(w.iorq == emucpc::z80::never);
    m.bus.cycles.clear();
    CHECK(m.step() == 11);
    REQUIRE(m.bus.cycles.size() == 3);
    const auto& o = m.bus.cycles[2].cycle;
    CHECK(o.kind == CycleKind::io_write);
    CHECK(o.iorq.value == o.t1.value + 4);
    CHECK(o.wr == o.iorq);
    CHECK(o.end.value == o.t1.value + 16);
    CHECK(m.bus.ports.back().port == ((m.cpu.a << 8) | 0x10));
}
