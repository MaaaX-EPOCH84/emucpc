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

// Flat bus for tests: 64 KB of RAM, no wait state, inputs served from a
// list, every port access and (optionally) every machine cycle logged.
#pragma once

#include <array>
#include <cstddef>
#include <cstdint>
#include <vector>

#include <emucpc/z80/cpu.hpp>

namespace emucpc::z80 {

struct PortAccess {
    std::uint16_t port = 0;
    std::uint8_t value = 0;
    bool write = false;

    friend constexpr bool operator==(const PortAccess&, const PortAccess&) = default;
};

struct BusAccess {
    Cycle cycle;
    std::uint16_t address = 0;
    std::uint8_t value = 0;

    friend constexpr bool operator==(const BusAccess&, const BusAccess&) = default;
};

class FlatBus {
public:
    std::array<std::uint8_t, 0x10000> memory{};
    std::vector<std::uint8_t> inputs;  // values of the next inputs, in order
    std::uint8_t default_input = 0xFF;  // when `inputs` is exhausted
    std::vector<PortAccess> ports;      // every input and output
    bool record_cycles = false;
    std::vector<BusAccess> cycles;  // every machine cycle, when record_cycles

    bool int_active = false;         // level of INT
    std::uint8_t int_vector = 0xFF;  // data bus during the acknowledge
    bool nmi_pending = false;        // consumed by the CPU

    bool ready(Tick /*t*/) const noexcept { return true; }

    std::uint8_t fetch(const Cycle& c, std::uint16_t a) noexcept { return log(c, a, memory[a]); }
    std::uint8_t read(const Cycle& c, std::uint16_t a) noexcept { return log(c, a, memory[a]); }
    void write(const Cycle& c, std::uint16_t a, std::uint8_t v) noexcept {
        memory[a] = v;
        static_cast<void>(log(c, a, v));
    }

    std::uint8_t io_read(const Cycle& c, std::uint16_t port) noexcept {
        std::uint8_t v = default_input;
        if (next_input_ < inputs.size()) {
            v = inputs[next_input_++];
        }
        ports.push_back(PortAccess{port, v, false});
        return log(c, port, v);
    }

    void io_write(const Cycle& c, std::uint16_t port, std::uint8_t v) noexcept {
        ports.push_back(PortAccess{port, v, true});
        static_cast<void>(log(c, port, v));
    }

    std::uint8_t int_ack(const Cycle& c) noexcept { return log(c, 0, int_vector); }
    bool int_line(Tick /*t*/) const noexcept { return int_active; }
    bool nmi_edge(Tick /*t*/) noexcept {
        const bool edge = nmi_pending;
        nmi_pending = false;
        return edge;
    }

private:
    std::size_t next_input_ = 0;

    std::uint8_t log(const Cycle& c, std::uint16_t a, std::uint8_t v) {
        if (record_cycles) {
            cycles.push_back(BusAccess{c, a, v});
        }
        return v;
    }
};

static_assert(Bus<FlatBus>);

// Instantiated once, in the library.
extern template void Cpu::step<FlatBus>(FlatBus&) noexcept;

}  // namespace emucpc::z80
