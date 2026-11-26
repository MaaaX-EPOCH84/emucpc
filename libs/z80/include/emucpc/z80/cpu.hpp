// ================================================================
//     ______                 __________  ______
//    / ____/___ ___  __  __/ ____/ __ \/ ____/
//   / __/ / __ `__ \/ / / / /   / /_/ / /
//  / /___/ / / / / / /_/ / /___/ ____/ /___
// /_____/_/ /_/ /_/\__,_/\____/_/    \____/
//
// EmuCPC — Amstrad CPC, CPC Plus and GX4000, down to the 1/16 µs
// ================================================================
// Author: Maxime MARCONATO (MaaaX^^, aka EPOCH84)
// ================================================================
// License: GNU General Public License v3.0 or later — see LICENSE.
// SPDX-License-Identifier: GPL-3.0-or-later
// ================================================================

// Z80 CPU: registers, machine cycles dated in ticks, bus concept.
//
// The CPU knows nothing of the machine around it. It executes one
// instruction (or one interrupt response) per call to step(), as a sequence
// of machine cycles; each cycle that drives the bus is handed to the bus with
// its dated edges (Cycle). Internal T states only advance the time.
//
// Timing: one T state is 4 ticks (Tick = 1/16 us, T = 1/4 us). The edges are
// those of the Zilog data sheet. WAIT is sampled through Bus::ready on the
// falling edge of T2 (M1 and memory cycles), of the automatic wait state
// (I/O) or of the second one (interrupt acknowledge), and on each Tw it
// adds; INT and NMI on the rising edge of the last T state of an
// instruction, which the next step() serves.
#pragma once

#include <compare>
#include <concepts>
#include <cstdint>

namespace emucpc::z80 {

// Time in ticks of the 16 MHz master clock (1/16 us). A strong type: a
// count of microseconds or of T states does not pass for a tick.
struct Tick {
    std::uint64_t value = 0;

    friend constexpr auto operator<=>(Tick, Tick) = default;
    friend constexpr Tick operator+(Tick t, std::uint64_t ticks) noexcept {
        return Tick{t.value + ticks};
    }
};

inline constexpr std::uint64_t ticks_per_t = 4;
inline constexpr Tick never{~std::uint64_t{0}};

enum class CycleKind : std::uint8_t {
    opcode,     // M1: opcode fetch, then refresh
    read,       // memory read
    write,      // memory write
    io_read,    // input
    io_write,   // output
    int_ack,    // interrupt acknowledge (M1 with IORQ)
};

// One machine cycle that drives the bus: tick of the start of T1 and tick of
// the active (falling) edge of each control signal, `never` for a signal
// that stays inactive during the cycle; `end` is the end of its last T state.
// `address` is the address (or port) of the access, the one the bus call
// receives; `refresh` is I·R, put on the address bus in T3 and T4 of an
// opcode fetch or an interrupt acknowledge (0 for the other kinds). The
// level of every pin at any tick follows from these: <emucpc/z80/pins.hpp>.
struct Cycle {
    CycleKind kind = CycleKind::opcode;
    std::uint16_t address = 0;
    std::uint16_t refresh = 0;
    Tick t1;
    Tick mreq = never;
    Tick iorq = never;
    Tick rd = never;
    Tick wr = never;
    Tick end;

    friend constexpr bool operator==(const Cycle&, const Cycle&) = default;
};

// What the CPU needs from the machine. Every call is resolved at compile
// time (no virtual function): step() is instantiated for each bus type.
template <class B>
concept Bus = requires(B& b, const Cycle& c, Tick t, std::uint16_t a, std::uint8_t v) {
    { b.ready(t) } -> std::same_as<bool>;          // READY at the WAIT sampling edge
    { b.fetch(c, a) } -> std::same_as<std::uint8_t>;  // M1
    { b.read(c, a) } -> std::same_as<std::uint8_t>;
    { b.write(c, a, v) };
    { b.io_read(c, a) } -> std::same_as<std::uint8_t>;
    { b.io_write(c, a, v) };
    { b.int_ack(c) } -> std::same_as<std::uint8_t>;  // value on the data bus
    { b.int_line(t) } -> std::same_as<bool>;          // INT active
    { b.nmi_edge(t) } -> std::same_as<bool>;          // falling edge of NMI since the last call
};

// Registers and internal state, as plain values. The power-on values are
// those of "The Undocumented Z80 Documented": AF = SP = FFFFh, the other
// pairs FFFFh too, PC = I = R = 0, IM 0, interrupts disabled.
class Cpu {
public:
    std::uint8_t a = 0xFF, f = 0xFF;
    std::uint8_t b = 0xFF, c = 0xFF, d = 0xFF, e = 0xFF, h = 0xFF, l = 0xFF;
    std::uint8_t ixh = 0xFF, ixl = 0xFF, iyh = 0xFF, iyl = 0xFF;
    std::uint16_t af2 = 0xFFFF, bc2 = 0xFFFF, de2 = 0xFFFF, hl2 = 0xFFFF;  // AF' BC' DE' HL'
    std::uint16_t sp = 0xFFFF, pc = 0, wz = 0;  // WZ: the internal MEMPTR
    std::uint8_t i = 0, r = 0;
    bool iff1 = false, iff2 = false;
    std::uint8_t im = 0;
    bool halted = false;
    bool ei_just = false;  // the last instruction was EI: no interrupt before the next one
    bool p = false;        // the last instruction was LD A,I or LD A,R
    std::uint8_t q = 0;    // F if the last instruction changed the flags, else 0
    bool int_sampled = false;  // INT was active at the end of the last instruction
    bool nmi_latched = false;  // an NMI edge seen and not served yet
    Tick now;              // start of the next machine cycle

    [[nodiscard]] constexpr std::uint16_t af() const noexcept { return pair(a, f); }
    [[nodiscard]] constexpr std::uint16_t bc() const noexcept { return pair(b, c); }
    [[nodiscard]] constexpr std::uint16_t de() const noexcept { return pair(d, e); }
    [[nodiscard]] constexpr std::uint16_t hl() const noexcept { return pair(h, l); }
    [[nodiscard]] constexpr std::uint16_t ix() const noexcept { return pair(ixh, ixl); }
    [[nodiscard]] constexpr std::uint16_t iy() const noexcept { return pair(iyh, iyl); }
    constexpr void set_af(std::uint16_t v) noexcept { split(v, a, f); }
    constexpr void set_bc(std::uint16_t v) noexcept { split(v, b, c); }
    constexpr void set_de(std::uint16_t v) noexcept { split(v, d, e); }
    constexpr void set_hl(std::uint16_t v) noexcept { split(v, h, l); }
    constexpr void set_ix(std::uint16_t v) noexcept { split(v, ixh, ixl); }
    constexpr void set_iy(std::uint16_t v) noexcept { split(v, iyh, iyl); }

    // RESET: PC, I, R, IM and the interrupt flip-flops cleared, HALT left;
    // the other registers keep their value.
    constexpr void reset() noexcept {
        pc = 0;
        i = 0;
        r = 0;
        im = 0;
        iff1 = iff2 = false;
        halted = false;
        ei_just = false;
        p = false;
        q = 0;
        int_sampled = false;
        nmi_latched = false;
    }

    // Executes one instruction, or one interrupt response, or one M1 cycle
    // of HALT. Defined in <emucpc/z80/step.hpp>.
    template <Bus B>
    void step(B& bus) noexcept;

    friend constexpr bool operator==(const Cpu&, const Cpu&) = default;

private:
    static constexpr std::uint16_t pair(std::uint8_t hi, std::uint8_t lo) noexcept {
        return static_cast<std::uint16_t>((hi << 8) | lo);
    }
    static constexpr void split(std::uint16_t v, std::uint8_t& hi, std::uint8_t& lo) noexcept {
        hi = static_cast<std::uint8_t>(v >> 8);
        lo = static_cast<std::uint8_t>(v);
    }
};

}  // namespace emucpc::z80
