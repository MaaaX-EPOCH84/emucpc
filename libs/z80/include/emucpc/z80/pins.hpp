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

// Z80 bus pins rebuilt from the machine cycles, at any tick.
//
// The CPU hands each cycle to the bus as a Cycle; the level of the address
// bus and of M1, MREQ, IORQ, RD, WR and RFSH at any tick follows from it,
// with the edges of the Zilog data sheet (T = 4 ticks, a falling edge two
// ticks after the rising one; T3 is the third T state from the end of an M1
// cycle and the last one of the others, wait states being inserted before
// it):
//
//   opcode   M1 T1..T3, MREQ and RD from the falling edge of T1 to T3,
//            address PC; then RFSH T3..end, MREQ from the falling edge of
//            T3 to the falling edge of T4, address I·R
//   read     MREQ and RD from the falling edge of T1 to that of T3
//   write    MREQ likewise, WR from the falling edge of T2 to that of T3
//   io_read  IORQ and RD from T2 to the falling edge of T3
//   io_write IORQ and WR likewise
//   int_ack  M1 T1..T3, IORQ from the falling edge of the first automatic
//            wait state to T3, address PC; then the refresh, as an opcode
//
// Between cycles (internal T states) every control is inactive and the
// address bus keeps the last address it carried: I·R after an M1, the
// access address otherwise. The data bus is not rebuilt.
#pragma once

#include <cstdint>
#include <span>

#include <emucpc/z80/cpu.hpp>

namespace emucpc::z80 {

// Levels at one tick; true = active (the pins are active low).
struct Pins {
    std::uint16_t address = 0;
    bool m1 = false;
    bool mreq = false;
    bool iorq = false;
    bool rd = false;
    bool wr = false;
    bool rfsh = false;

    friend constexpr bool operator==(const Pins&, const Pins&) = default;
};

namespace detail {

constexpr bool within(Tick t, Tick from, Tick to) noexcept { return t >= from && t < to; }
constexpr Tick minus(Tick t, std::uint64_t ticks) noexcept { return Tick{t.value - ticks}; }

constexpr bool has_refresh(CycleKind kind) noexcept {
    return kind == CycleKind::opcode || kind == CycleKind::int_ack;
}

}  // namespace detail

// The address the bus keeps after the cycle, through the internal T states
// that follow it.
[[nodiscard]] constexpr std::uint16_t held_address(const Cycle& c) noexcept {
    return detail::has_refresh(c.kind) ? c.refresh : c.address;
}

// Levels during the cycle, for c.t1 <= t < c.end.
[[nodiscard]] constexpr Pins pins_at(const Cycle& c, Tick t) noexcept {
    using detail::minus;
    using detail::within;
    constexpr std::uint64_t half = ticks_per_t / 2;
    Pins p;
    p.address = c.address;
    if (detail::has_refresh(c.kind)) {
        const Tick t3 = minus(c.end, 2 * ticks_per_t);
        if (t >= t3) {
            p.address = c.refresh;
            p.rfsh = true;
            p.mreq = within(t, t3 + half, minus(c.end, half));
            return p;
        }
        p.m1 = true;
        if (c.kind == CycleKind::opcode) {
            p.mreq = p.rd = within(t, c.mreq, t3);
        } else {
            p.iorq = within(t, c.iorq, t3);
        }
        return p;
    }
    const Tick off = minus(c.end, half);  // falling edge of T3
    p.mreq = within(t, c.mreq, off);
    p.iorq = within(t, c.iorq, off);
    p.rd = within(t, c.rd, off);
    p.wr = within(t, c.wr, off);
    return p;
}

// Levels at tick t from cycles in time order; `idle_address` is on the bus
// before the first one.
[[nodiscard]] constexpr Pins pins_at(std::span<const Cycle> cycles, Tick t,
                                     std::uint16_t idle_address) noexcept {
    Pins p;
    p.address = idle_address;
    for (const Cycle& c : cycles) {
        if (t < c.t1) {
            break;
        }
        if (t < c.end) {
            return pins_at(c, t);
        }
        p.address = held_address(c);
    }
    return p;
}

}  // namespace emucpc::z80
