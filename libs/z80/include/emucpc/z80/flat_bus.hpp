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
//
// It is instantiated in the core library, so it allocates nothing and
// throws nothing while the CPU runs: the logs are fixed-size buffers that
// stop filling when full and say so (overflowed()).
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

// Fixed-capacity log: push() keeps the first N entries, then only counts
// the overflow.
template <class T, std::size_t N>
class FixedLog {
public:
    void push(const T& v) noexcept {
        if (size_ < N) {
            items_[size_++] = v;
        } else {
            overflowed_ = true;
        }
    }
    void clear() noexcept {
        size_ = 0;
        overflowed_ = false;
    }
    [[nodiscard]] std::size_t size() const noexcept { return size_; }
    [[nodiscard]] bool empty() const noexcept { return size_ == 0; }
    [[nodiscard]] bool overflowed() const noexcept { return overflowed_; }
    [[nodiscard]] const T& operator[](std::size_t k) const noexcept { return items_[k]; }
    [[nodiscard]] const T& front() const noexcept { return items_[0]; }
    [[nodiscard]] const T& back() const noexcept { return items_[size_ - 1]; }
    [[nodiscard]] const T* begin() const noexcept { return items_.data(); }
    [[nodiscard]] const T* end() const noexcept { return items_.data() + size_; }

private:
    std::array<T, N> items_{};
    std::size_t size_ = 0;
    bool overflowed_ = false;
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
    FixedLog<PortAccess, 64> ports;  // every input and output
    bool record_cycles = false;
    FixedLog<BusAccess, 256> cycles;  // every machine cycle, when record_cycles

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
        ports.push(PortAccess{port, v, false});
        return log(c, port, v);
    }

    void io_write(const Cycle& c, std::uint16_t port, std::uint8_t v) noexcept {
        ports.push(PortAccess{port, v, true});
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

    std::uint8_t log(const Cycle& c, std::uint16_t a, std::uint8_t v) noexcept {
        if (record_cycles) {
            cycles.push(BusAccess{c, a, v});
        }
        return v;
    }
};

static_assert(Bus<FlatBus>);

// Instantiated once, in the library.
extern template void Cpu::step<FlatBus>(FlatBus&) noexcept;

}  // namespace emucpc::z80
