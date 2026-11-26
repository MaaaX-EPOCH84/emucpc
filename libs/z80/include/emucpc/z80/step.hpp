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

// Z80 instruction execution: definition of Cpu::step().
//
// Included by the code that instantiates step() for its bus type (the
// machine, the flat test bus in src/cpu.cpp).
//
// Decoder: one switch per prefix, written by hand; the register operands
// are decoded from the bits of the opcode (x = bits 7-6, y = 5-3, z = 2-0,
// p = y >> 1, q = y & 1). DD and FD reuse the unprefixed decoder through the
// Index parameter: HL becomes IX or IY, H and L become their halves, except
// beside an (IX+d) operand, where H and L stay H and L.
//
// Each machine cycle is a call to the bus with its dated edges; the T
// states inside an instruction are placed as in the Zilog data sheet
// (M1 4 T, memory 3 T, I/O 4 T, internal T states where the instruction
// has them).
//
// Undocumented behaviour followed (NMOS Zilog): flags X (bit 3) and Y
// (bit 5) everywhere, WZ (MEMPTR), Q for SCF and CCF, P/V cleared when an
// interrupt is taken right after LD A,I or LD A,R, OUT (C),0, the copy of
// the result into a register by DDCB/FDCB, SLL, mirrors of the ED opcodes.
#pragma once

#include <array>
#include <concepts>
#include <cstdint>

#include <emucpc/z80/cpu.hpp>

namespace emucpc::z80::detail {

inline constexpr std::uint8_t CF = 0x01;
inline constexpr std::uint8_t NF = 0x02;
inline constexpr std::uint8_t PF = 0x04;
inline constexpr std::uint8_t XF = 0x08;
inline constexpr std::uint8_t HF = 0x10;
inline constexpr std::uint8_t YF = 0x20;
inline constexpr std::uint8_t ZF = 0x40;
inline constexpr std::uint8_t SF = 0x80;

// Truncations to 8 and 16 bits, written once: the arithmetic below is done
// on promoted values.
template <std::integral T>
constexpr std::uint8_t u8(T v) noexcept {
    return static_cast<std::uint8_t>(v);
}
template <std::integral T>
constexpr std::uint16_t u16(T v) noexcept {
    return static_cast<std::uint16_t>(v);
}

// S, Z, Y, X of a byte; the same with P/V as even parity.
struct FlagTables {
    std::array<std::uint8_t, 256> sz53{};
    std::array<std::uint8_t, 256> sz53p{};
};

constexpr FlagTables make_flag_tables() noexcept {
    FlagTables t;
    for (unsigned v = 0; v < 256; ++v) {
        const unsigned szxy = (v & (SF | YF | XF)) | (v == 0 ? ZF : 0U);
        unsigned ones = 0;
        for (unsigned bit = 0; bit < 8; ++bit) {
            ones += (v >> bit) & 1U;
        }
        t.sz53[v] = u8(szxy);
        t.sz53p[v] = u8(szxy | ((ones & 1U) == 0 ? PF : 0U));
    }
    return t;
}

inline constexpr FlagTables flag_tables = make_flag_tables();

constexpr std::uint8_t sz53(std::uint8_t v) noexcept { return flag_tables.sz53[v]; }
constexpr std::uint8_t sz53p(std::uint8_t v) noexcept { return flag_tables.sz53p[v]; }
constexpr std::uint8_t parity(unsigned v) noexcept { return flag_tables.sz53p[v & 0xFFU] & PF; }

enum class Index : std::uint8_t { hl, ix, iy };

template <Bus B>
class Executor {
public:
    Executor(Cpu& cpu, B& bus) noexcept : c_(cpu), bus_(bus) {}

    void step() noexcept {
        const bool after_ei = c_.ei_just;
        const bool after_ld_air = c_.p;
        c_.ei_just = false;
        c_.p = false;
        // NMI and INT as sampled at the end of the previous instruction.
        if (c_.nmi_latched) {
            c_.nmi_latched = false;
            nmi(after_ld_air);
        } else if (c_.int_sampled && c_.iff1 && !after_ei) {
            irq(after_ld_air);
        } else if (c_.halted) {
            // HALT: M1 cycles at PC (the byte after HALT), opcode ignored.
            static_cast<void>(fetch());
            c_.pc = u16(c_.pc - 1U);
        } else {
            exec_main<Index::hl>(fetch());
        }
        sample_interrupts();
        c_.q = flags_written_ ? c_.f : std::uint8_t{0};
        c_.p = ld_air_;
    }

private:
    Cpu& c_;
    B& bus_;
    bool flags_written_ = false;
    bool ld_air_ = false;

    // ---- Machine cycles --------------------------------------------------

    void internal(unsigned t_states) noexcept { c_.now = c_.now + t_states * ticks_per_t; }

    // INT (a level) and NMI (an edge, latched until served) are sampled on
    // the rising edge of the last T state of the instruction.
    void sample_interrupts() noexcept {
        const Tick last_t{c_.now.value - ticks_per_t};
        if (bus_.nmi_edge(last_t)) {
            c_.nmi_latched = true;
        }
        c_.int_sampled = bus_.int_line(last_t);
    }

    // A machine cycle that starts now: its nominal length in T states, the
    // falling edge where WAIT is first sampled (ticks after T1), and one
    // Tw of 4 ticks inserted for each sample where READY is false. A bus
    // that holds READY forever would hang the CPU, as on the real chip; past
    // 65535 Tw the cycle goes on (a fault of the bus, not a CPU state).
    Cycle begin(CycleKind kind) noexcept {
        constexpr std::uint64_t half = ticks_per_t / 2;
        unsigned t_states = 3;
        std::uint64_t sample = ticks_per_t + half;  // falling edge of T2
        switch (kind) {
        case CycleKind::opcode:
            t_states = 4;
            break;
        case CycleKind::read:
        case CycleKind::write:
            break;
        case CycleKind::io_read:
        case CycleKind::io_write:
            t_states = 4;
            sample = 2 * ticks_per_t + half;  // falling edge of the automatic wait state
            break;
        case CycleKind::int_ack:
            t_states = 6;
            sample = 3 * ticks_per_t + half;  // falling edge of the second automatic wait state
            break;
        }
        unsigned waits = 0;
        while (waits < 0xFFFF && !bus_.ready(c_.now + sample + std::uint64_t{waits} * ticks_per_t)) {
            ++waits;
        }
        Cycle cy;
        cy.kind = kind;
        cy.t1 = c_.now;
        cy.end = c_.now + (t_states + waits) * std::uint64_t{ticks_per_t};
        switch (kind) {
        case CycleKind::opcode:
        case CycleKind::read:
            cy.mreq = cy.rd = c_.now + half;  // falling edge of T1
            break;
        case CycleKind::write:
            cy.mreq = c_.now + half;
            cy.wr = c_.now + ticks_per_t + half;  // falling edge of T2
            break;
        case CycleKind::io_read:
            cy.iorq = cy.rd = c_.now + ticks_per_t;  // rising edge of T2
            break;
        case CycleKind::io_write:
            cy.iorq = cy.wr = c_.now + ticks_per_t;
            break;
        case CycleKind::int_ack:
            cy.iorq = c_.now + 2 * ticks_per_t + half;  // first automatic wait state
            break;
        }
        return cy;
    }

    void bump_r() noexcept { c_.r = u8((c_.r & 0x80U) | ((c_.r + 1U) & 0x7FU)); }

    std::uint8_t fetch() noexcept {
        const Cycle cy = begin(CycleKind::opcode);
        const std::uint8_t op = bus_.fetch(cy, c_.pc);
        c_.pc = u16(c_.pc + 1U);
        bump_r();
        c_.now = cy.end;
        return op;
    }

    std::uint8_t read(std::uint16_t address) noexcept {
        const Cycle cy = begin(CycleKind::read);
        const std::uint8_t v = bus_.read(cy, address);
        c_.now = cy.end;
        return v;
    }

    void write(std::uint16_t address, std::uint8_t v) noexcept {
        const Cycle cy = begin(CycleKind::write);
        bus_.write(cy, address, v);
        c_.now = cy.end;
    }

    std::uint8_t in(std::uint16_t port) noexcept {
        const Cycle cy = begin(CycleKind::io_read);
        const std::uint8_t v = bus_.io_read(cy, port);
        c_.now = cy.end;
        return v;
    }

    void out(std::uint16_t port, std::uint8_t v) noexcept {
        const Cycle cy = begin(CycleKind::io_write);
        bus_.io_write(cy, port, v);
        c_.now = cy.end;
    }

    std::uint8_t imm8() noexcept {
        const std::uint8_t v = read(c_.pc);
        c_.pc = u16(c_.pc + 1U);
        return v;
    }

    std::uint16_t imm16() noexcept {
        const std::uint8_t lo = imm8();
        const std::uint8_t hi = imm8();
        return u16((hi << 8) | lo);
    }

    std::uint16_t read16(std::uint16_t address) noexcept {
        const std::uint8_t lo = read(address);
        const std::uint8_t hi = read(u16(address + 1U));
        return u16((hi << 8) | lo);
    }

    void write16(std::uint16_t address, std::uint16_t v) noexcept {
        write(address, u8(v));
        write(u16(address + 1U), u8(v >> 8));
    }

    void push(std::uint16_t v) noexcept {
        c_.sp = u16(c_.sp - 1U);
        write(c_.sp, u8(v >> 8));
        c_.sp = u16(c_.sp - 1U);
        write(c_.sp, u8(v));
    }

    std::uint16_t pop() noexcept {
        const std::uint8_t lo = read(c_.sp);
        c_.sp = u16(c_.sp + 1U);
        const std::uint8_t hi = read(c_.sp);
        c_.sp = u16(c_.sp + 1U);
        return u16((hi << 8) | lo);
    }

    // ---- Interrupts ------------------------------------------------------

    void nmi(bool after_ld_air) noexcept {
        c_.halted = false;
        if (after_ld_air) {
            c_.f = u8(c_.f & ~unsigned{PF});
        }
        c_.iff1 = false;
        static_cast<void>(fetch());  // M1, opcode ignored
        c_.pc = u16(c_.pc - 1U);
        internal(1);
        push(c_.pc);
        c_.pc = c_.wz = 0x0066;
    }

    void irq(bool after_ld_air) noexcept {
        c_.halted = false;
        if (after_ld_air) {
            c_.f = u8(c_.f & ~unsigned{PF});
        }
        c_.iff1 = c_.iff2 = false;
        const Cycle cy = begin(CycleKind::int_ack);  // M1 with two automatic wait states
        const std::uint8_t v = bus_.int_ack(cy);
        bump_r();
        c_.now = cy.end;
        switch (c_.im) {
        case 0:
            // The byte on the bus is executed as an opcode (RST on the CPC,
            // whose bus reads FFh). The operands of a longer instruction
            // would be read from memory at PC: not modelled.
            exec_main<Index::hl>(v);
            break;
        case 1:
            internal(1);
            push(c_.pc);
            c_.pc = c_.wz = 0x0038;
            break;
        default: {
            internal(1);
            push(c_.pc);
            c_.pc = c_.wz = read16(u16((c_.i << 8) | v));
            break;
        }
        }
    }

    // ---- Registers -------------------------------------------------------

    void set_f(unsigned v) noexcept {
        c_.f = u8(v);
        flags_written_ = true;
    }

    template <Index I>
    std::uint8_t& reg_h() noexcept {
        if constexpr (I == Index::ix) {
            return c_.ixh;
        } else if constexpr (I == Index::iy) {
            return c_.iyh;
        } else {
            return c_.h;
        }
    }

    template <Index I>
    std::uint8_t& reg_l() noexcept {
        if constexpr (I == Index::ix) {
            return c_.ixl;
        } else if constexpr (I == Index::iy) {
            return c_.iyl;
        } else {
            return c_.l;
        }
    }

    // r[n], n != 6: B C D E H L - A, H and L substituted under DD/FD.
    template <Index I>
    std::uint8_t& reg8(unsigned n) noexcept {
        switch (n) {
        case 0: return c_.b;
        case 1: return c_.c;
        case 2: return c_.d;
        case 3: return c_.e;
        case 4: return reg_h<I>();
        case 5: return reg_l<I>();
        default: return c_.a;
        }
    }

    template <Index I>
    std::uint16_t idx() noexcept {
        return u16((reg_h<I>() << 8) | reg_l<I>());
    }

    template <Index I>
    void set_idx(std::uint16_t v) noexcept {
        reg_h<I>() = u8(v >> 8);
        reg_l<I>() = u8(v);
    }

    // rp[n]: BC DE HL SP.
    template <Index I>
    std::uint16_t rp(unsigned n) noexcept {
        switch (n) {
        case 0: return c_.bc();
        case 1: return c_.de();
        case 2: return idx<I>();
        default: return c_.sp;
        }
    }

    template <Index I>
    void set_rp(unsigned n, std::uint16_t v) noexcept {
        switch (n) {
        case 0: c_.set_bc(v); break;
        case 1: c_.set_de(v); break;
        case 2: set_idx<I>(v); break;
        default: c_.sp = v; break;
        }
    }

    // rp2[n]: BC DE HL AF.
    template <Index I>
    std::uint16_t rp2(unsigned n) noexcept {
        return n == 3 ? c_.af() : rp<I>(n);
    }

    template <Index I>
    void set_rp2(unsigned n, std::uint16_t v) noexcept {
        if (n == 3) {
            c_.set_af(v);
        } else {
            set_rp<I>(n, v);
        }
    }

    // Address of the memory operand (HL), or (IX+d) / (IY+d): displacement
    // read, then `extra` internal T states (5 for most instructions).
    template <Index I>
    std::uint16_t mem_operand(unsigned extra = 5) noexcept {
        if constexpr (I == Index::hl) {
            return c_.hl();
        } else {
            const auto displacement = static_cast<std::int8_t>(imm8());
            internal(extra);
            const std::uint16_t address = u16(static_cast<unsigned>(idx<I>() + displacement));
            c_.wz = address;
            return address;
        }
    }

    bool condition(unsigned n) const noexcept {
        switch (n) {
        case 0: return (c_.f & ZF) == 0;
        case 1: return (c_.f & ZF) != 0;
        case 2: return (c_.f & CF) == 0;
        case 3: return (c_.f & CF) != 0;
        case 4: return (c_.f & PF) == 0;
        case 5: return (c_.f & PF) != 0;
        case 6: return (c_.f & SF) == 0;
        default: return (c_.f & SF) != 0;
        }
    }

    // ---- ALU -------------------------------------------------------------

    void add8(std::uint8_t v, unsigned carry) noexcept {
        const unsigned res = c_.a + v + carry;
        const unsigned overflow = ((c_.a ^ ~unsigned{v}) & (c_.a ^ res) & 0x80U) >> 5;
        set_f(sz53(u8(res)) | ((c_.a ^ v ^ res) & HF) | overflow | ((res >> 8) & CF));
        c_.a = u8(res);
    }

    // A - v - carry; the result is kept unless `compare`. CP takes X and Y
    // from the operand.
    void sub8(std::uint8_t v, unsigned carry, bool compare) noexcept {
        const unsigned res = c_.a - v - carry;
        const unsigned overflow = ((c_.a ^ v) & (c_.a ^ res) & 0x80U) >> 5;
        const unsigned xy = compare ? (v & (XF | YF)) : (res & (XF | YF));
        set_f((sz53(u8(res)) & ~unsigned{XF | YF}) | xy | NF | ((c_.a ^ v ^ res) & HF) | overflow |
              ((res >> 8) & CF));
        if (!compare) {
            c_.a = u8(res);
        }
    }

    void alu(unsigned op, std::uint8_t v) noexcept {
        switch (op) {
        case 0: add8(v, 0); break;
        case 1: add8(v, c_.f & CF); break;
        case 2: sub8(v, 0, false); break;
        case 3: sub8(v, c_.f & CF, false); break;
        case 4:
            c_.a &= v;
            set_f(sz53p(c_.a) | HF);
            break;
        case 5:
            c_.a ^= v;
            set_f(sz53p(c_.a));
            break;
        case 6:
            c_.a |= v;
            set_f(sz53p(c_.a));
            break;
        default: sub8(v, 0, true); break;
        }
    }

    std::uint8_t inc8(std::uint8_t v) noexcept {
        const std::uint8_t res = u8(v + 1U);
        set_f((c_.f & CF) | sz53(res) | (v == 0x7F ? PF : 0U) | ((res & 0x0FU) == 0 ? HF : 0U));
        return res;
    }

    std::uint8_t dec8(std::uint8_t v) noexcept {
        const std::uint8_t res = u8(v - 1U);
        set_f((c_.f & CF) | NF | sz53(res) | (v == 0x80 ? PF : 0U) | ((v & 0x0FU) == 0 ? HF : 0U));
        return res;
    }

    std::uint16_t add16(std::uint16_t a, std::uint16_t v) noexcept {
        const unsigned res = unsigned{a} + v;
        c_.wz = u16(a + 1U);
        set_f((c_.f & (SF | ZF | PF)) | ((res >> 8) & (XF | YF)) | (((a ^ v ^ res) >> 8) & HF) |
              ((res >> 16) & CF));
        return u16(res);
    }

    std::uint16_t adc16(std::uint16_t a, std::uint16_t v) noexcept {
        const unsigned res = unsigned{a} + v + (c_.f & CF);
        const unsigned overflow = ((a ^ ~unsigned{v}) & (a ^ res) & 0x8000U) >> 13;
        c_.wz = u16(a + 1U);
        set_f(((res >> 8) & (SF | XF | YF)) | ((res & 0xFFFFU) == 0 ? ZF : 0U) |
              (((a ^ v ^ res) >> 8) & HF) | overflow | ((res >> 16) & CF));
        return u16(res);
    }

    std::uint16_t sbc16(std::uint16_t a, std::uint16_t v) noexcept {
        const unsigned res = unsigned{a} - v - (c_.f & CF);
        const unsigned overflow = ((a ^ v) & (a ^ res) & 0x8000U) >> 13;
        c_.wz = u16(a + 1U);
        set_f(((res >> 8) & (SF | XF | YF)) | ((res & 0xFFFFU) == 0 ? ZF : 0U) |
              (((a ^ v ^ res) >> 8) & HF) | overflow | ((res >> 16) & CF) | NF);
        return u16(res);
    }

    // CB rotations and shifts: RLC RRC RL RR SLA SRA SLL SRL.
    std::uint8_t rot(unsigned op, std::uint8_t v) noexcept {
        unsigned res = 0;
        unsigned carry = 0;
        switch (op) {
        case 0:
            carry = v >> 7;
            res = (unsigned{v} << 1) | carry;
            break;
        case 1:
            carry = v & 1U;
            res = (v >> 1) | (carry << 7);
            break;
        case 2:
            carry = v >> 7;
            res = (unsigned{v} << 1) | (c_.f & CF);
            break;
        case 3:
            carry = v & 1U;
            res = (v >> 1) | ((unsigned{c_.f} & CF) << 7);
            break;
        case 4:
            carry = v >> 7;
            res = unsigned{v} << 1;
            break;
        case 5:
            carry = v & 1U;
            res = (v >> 1) | (v & 0x80U);
            break;
        case 6:
            carry = v >> 7;
            res = (unsigned{v} << 1) | 1U;
            break;
        default:
            carry = v & 1U;
            res = v >> 1;
            break;
        }
        const std::uint8_t r8 = u8(res);
        set_f(sz53p(r8) | carry);
        return r8;
    }

    // BIT n: X and Y from `xy` (the operand, or WZ high byte for memory).
    void bit(unsigned n, std::uint8_t v, std::uint8_t xy) noexcept {
        const unsigned tested = v & (1U << n);
        unsigned flags = (c_.f & CF) | HF | (xy & (XF | YF));
        if (tested == 0) {
            flags |= ZF | PF;
        }
        if (n == 7 && tested != 0) {
            flags |= SF;
        }
        set_f(flags);
    }

    // RLCA RRCA RLA RRA.
    void rot_a(unsigned op) noexcept {
        const std::uint8_t a = c_.a;
        unsigned res = 0;
        unsigned carry = 0;
        switch (op) {
        case 0:
            carry = a >> 7;
            res = (unsigned{a} << 1) | carry;
            break;
        case 1:
            carry = a & 1U;
            res = (a >> 1) | (carry << 7);
            break;
        case 2:
            carry = a >> 7;
            res = (unsigned{a} << 1) | (c_.f & CF);
            break;
        default:
            carry = a & 1U;
            res = (a >> 1) | ((unsigned{c_.f} & CF) << 7);
            break;
        }
        c_.a = u8(res);
        set_f((c_.f & (SF | ZF | PF)) | (c_.a & (XF | YF)) | carry);
    }

    void daa() noexcept {
        const std::uint8_t a = c_.a;
        unsigned diff = 0;
        unsigned carry = c_.f & CF;
        if ((c_.f & HF) != 0 || (a & 0x0FU) > 9) {
            diff = 0x06;
        }
        if (carry != 0 || a > 0x99) {
            diff |= 0x60;
            carry = CF;
        }
        unsigned half = 0;
        if ((c_.f & NF) != 0) {
            half = ((c_.f & HF) != 0 && (a & 0x0FU) < 6) ? HF : 0U;
            c_.a = u8(a - diff);
        } else {
            half = (a & 0x0FU) > 9 ? HF : 0U;
            c_.a = u8(a + diff);
        }
        set_f(sz53p(c_.a) | (c_.f & NF) | carry | half);
    }

    // ---- Unprefixed opcodes, and DD / FD through I ------------------------

    template <Index I>
    void exec_main(std::uint8_t op) noexcept {
        const unsigned x = op >> 6;
        const unsigned y = (op >> 3) & 7U;
        const unsigned z = op & 7U;
        if (x == 1) {
            if (op == 0x76) {
                c_.halted = true;
            } else if (z == 6) {
                const std::uint16_t address = mem_operand<I>();
                reg8<Index::hl>(y) = read(address);
            } else if (y == 6) {
                const std::uint16_t address = mem_operand<I>();
                write(address, reg8<Index::hl>(z));
            } else {
                reg8<I>(y) = reg8<I>(z);
            }
            return;
        }
        if (x == 2) {
            if (z == 6) {
                alu(y, read(mem_operand<I>()));
            } else {
                alu(y, reg8<I>(z));
            }
            return;
        }
        const unsigned p = y >> 1;
        switch (op) {
        case 0x00:
            break;
        case 0x08: {
            const std::uint16_t af = c_.af();
            c_.set_af(c_.af2);
            c_.af2 = af;
            break;
        }
        case 0x10: {  // DJNZ
            internal(1);
            const auto e = static_cast<std::int8_t>(imm8());
            c_.b = u8(c_.b - 1U);
            if (c_.b != 0) {
                internal(5);
                c_.pc = c_.wz = u16(static_cast<unsigned>(c_.pc + e));
            }
            break;
        }
        case 0x18:
        case 0x20:
        case 0x28:
        case 0x30:
        case 0x38: {  // JR, JR cc
            const auto e = static_cast<std::int8_t>(imm8());
            if (op == 0x18 || condition(y - 4)) {
                internal(5);
                c_.pc = c_.wz = u16(static_cast<unsigned>(c_.pc + e));
            }
            break;
        }
        case 0x01:
        case 0x11:
        case 0x21:
        case 0x31:
            set_rp<I>(p, imm16());
            break;
        case 0x09:
        case 0x19:
        case 0x29:
        case 0x39:
            internal(7);
            set_idx<I>(add16(idx<I>(), rp<I>(p)));
            break;
        case 0x02:
        case 0x12: {  // LD (BC),A  LD (DE),A
            const std::uint16_t address = p == 0 ? c_.bc() : c_.de();
            write(address, c_.a);
            c_.wz = u16((unsigned{c_.a} << 8) | ((address + 1U) & 0xFFU));
            break;
        }
        case 0x0A:
        case 0x1A: {  // LD A,(BC)  LD A,(DE)
            const std::uint16_t address = p == 0 ? c_.bc() : c_.de();
            c_.a = read(address);
            c_.wz = u16(address + 1U);
            break;
        }
        case 0x22: {
            const std::uint16_t address = imm16();
            write16(address, idx<I>());
            c_.wz = u16(address + 1U);
            break;
        }
        case 0x2A: {
            const std::uint16_t address = imm16();
            set_idx<I>(read16(address));
            c_.wz = u16(address + 1U);
            break;
        }
        case 0x32: {
            const std::uint16_t address = imm16();
            write(address, c_.a);
            c_.wz = u16((unsigned{c_.a} << 8) | ((address + 1U) & 0xFFU));
            break;
        }
        case 0x3A: {
            const std::uint16_t address = imm16();
            c_.a = read(address);
            c_.wz = u16(address + 1U);
            break;
        }
        case 0x03:
        case 0x13:
        case 0x23:
        case 0x33:
            internal(2);
            set_rp<I>(p, u16(rp<I>(p) + 1U));
            break;
        case 0x0B:
        case 0x1B:
        case 0x2B:
        case 0x3B:
            internal(2);
            set_rp<I>(p, u16(rp<I>(p) - 1U));
            break;
        case 0x34: {
            const std::uint16_t address = mem_operand<I>();
            const std::uint8_t v = read(address);
            internal(1);
            write(address, inc8(v));
            break;
        }
        case 0x35: {
            const std::uint16_t address = mem_operand<I>();
            const std::uint8_t v = read(address);
            internal(1);
            write(address, dec8(v));
            break;
        }
        case 0x04:
        case 0x0C:
        case 0x14:
        case 0x1C:
        case 0x24:
        case 0x2C:
        case 0x3C: {
            std::uint8_t& reg = reg8<I>(y);
            reg = inc8(reg);
            break;
        }
        case 0x05:
        case 0x0D:
        case 0x15:
        case 0x1D:
        case 0x25:
        case 0x2D:
        case 0x3D: {
            std::uint8_t& reg = reg8<I>(y);
            reg = dec8(reg);
            break;
        }
        case 0x36: {
            const std::uint16_t address = mem_operand<I>(0);
            const std::uint8_t v = imm8();
            if constexpr (I != Index::hl) {
                internal(2);
            }
            write(address, v);
            break;
        }
        case 0x06:
        case 0x0E:
        case 0x16:
        case 0x1E:
        case 0x26:
        case 0x2E:
        case 0x3E:
            reg8<I>(y) = imm8();
            break;
        case 0x07:
        case 0x0F:
        case 0x17:
        case 0x1F:
            rot_a(y);
            break;
        case 0x27:
            daa();
            break;
        case 0x2F:
            c_.a = u8(~unsigned{c_.a});
            set_f((c_.f & (SF | ZF | PF | CF)) | HF | NF | (c_.a & (XF | YF)));
            break;
        case 0x37:
            set_f((c_.f & (SF | ZF | PF)) | CF | (((c_.q ^ c_.f) | c_.a) & (XF | YF)));
            break;
        case 0x3F:
            set_f((c_.f & (SF | ZF | PF)) | ((c_.f & CF) != 0 ? HF : CF) |
                  (((c_.q ^ c_.f) | c_.a) & (XF | YF)));
            break;

        case 0xC0:
        case 0xC8:
        case 0xD0:
        case 0xD8:
        case 0xE0:
        case 0xE8:
        case 0xF0:
        case 0xF8:  // RET cc
            internal(1);
            if (condition(y)) {
                c_.pc = c_.wz = pop();
            }
            break;
        case 0xC9:
            c_.pc = c_.wz = pop();
            break;
        case 0xC1:
        case 0xD1:
        case 0xE1:
        case 0xF1:
            set_rp2<I>(p, pop());
            break;
        case 0xC5:
        case 0xD5:
        case 0xE5:
        case 0xF5:
            internal(1);
            push(rp2<I>(p));
            break;
        case 0xC2:
        case 0xCA:
        case 0xD2:
        case 0xDA:
        case 0xE2:
        case 0xEA:
        case 0xF2:
        case 0xFA:  // JP cc
            c_.wz = imm16();
            if (condition(y)) {
                c_.pc = c_.wz;
            }
            break;
        case 0xC3:
            c_.pc = c_.wz = imm16();
            break;
        case 0xC4:
        case 0xCC:
        case 0xD4:
        case 0xDC:
        case 0xE4:
        case 0xEC:
        case 0xF4:
        case 0xFC:  // CALL cc
            c_.wz = imm16();
            if (condition(y)) {
                internal(1);
                push(c_.pc);
                c_.pc = c_.wz;
            }
            break;
        case 0xCD:
            c_.wz = imm16();
            internal(1);
            push(c_.pc);
            c_.pc = c_.wz;
            break;
        case 0xC6:
        case 0xCE:
        case 0xD6:
        case 0xDE:
        case 0xE6:
        case 0xEE:
        case 0xF6:
        case 0xFE:
            alu(y, imm8());
            break;
        case 0xC7:
        case 0xCF:
        case 0xD7:
        case 0xDF:
        case 0xE7:
        case 0xEF:
        case 0xF7:
        case 0xFF:
            internal(1);
            push(c_.pc);
            c_.pc = c_.wz = u16(y * 8U);
            break;
        case 0xD3: {  // OUT (n),A
            const std::uint8_t n = imm8();
            out(u16((c_.a << 8) | n), c_.a);
            c_.wz = u16((unsigned{c_.a} << 8) | ((n + 1U) & 0xFFU));
            break;
        }
        case 0xDB: {  // IN A,(n)
            const std::uint16_t port = u16((c_.a << 8) | imm8());
            c_.a = in(port);
            c_.wz = u16(port + 1U);
            break;
        }
        case 0xE3: {  // EX (SP),HL
            const std::uint8_t lo = read(c_.sp);
            const std::uint8_t hi = read(u16(c_.sp + 1U));
            internal(1);
            write(u16(c_.sp + 1U), reg_h<I>());
            write(c_.sp, reg_l<I>());
            internal(2);
            set_idx<I>(u16((hi << 8) | lo));
            c_.wz = idx<I>();
            break;
        }
        case 0xEB: {  // EX DE,HL (never IX or IY)
            const std::uint16_t de = c_.de();
            c_.set_de(c_.hl());
            c_.set_hl(de);
            break;
        }
        case 0xD9: {  // EXX
            const std::uint16_t bc = c_.bc();
            const std::uint16_t de = c_.de();
            const std::uint16_t hl = c_.hl();
            c_.set_bc(c_.bc2);
            c_.set_de(c_.de2);
            c_.set_hl(c_.hl2);
            c_.bc2 = bc;
            c_.de2 = de;
            c_.hl2 = hl;
            break;
        }
        case 0xE9:
            c_.pc = idx<I>();
            break;
        case 0xF9:
            internal(2);
            c_.sp = idx<I>();
            break;
        case 0xF3:
            c_.iff1 = c_.iff2 = false;
            break;
        case 0xFB:
            c_.iff1 = c_.iff2 = true;
            c_.ei_just = true;
            break;
        case 0xCB:
            if constexpr (I == Index::hl) {
                exec_cb(fetch());
            } else {
                exec_index_cb<I>();
            }
            break;
        case 0xED:
            exec_ed(fetch());
            break;
        case 0xDD:
            c_.q = 0;  // the prefix changes no flag: SCF and CCF after it see Q = 0
            exec_main<Index::ix>(fetch());
            break;
        default:  // 0xFD
            c_.q = 0;
            exec_main<Index::iy>(fetch());
            break;
        }
    }

    // ---- CB --------------------------------------------------------------

    void exec_cb(std::uint8_t op) noexcept {
        const unsigned x = op >> 6;
        const unsigned y = (op >> 3) & 7U;
        const unsigned z = op & 7U;
        if (z == 6) {
            const std::uint16_t address = c_.hl();
            const std::uint8_t v = read(address);
            internal(1);
            if (x == 1) {
                bit(y, v, u8(c_.wz >> 8));
                return;
            }
            write(address, cb_result(x, y, v));
            return;
        }
        std::uint8_t& reg = reg8<Index::hl>(z);
        if (x == 1) {
            bit(y, reg, reg);
            return;
        }
        reg = cb_result(x, y, reg);
    }

    std::uint8_t cb_result(unsigned x, unsigned y, std::uint8_t v) noexcept {
        switch (x) {
        case 0: return rot(y, v);
        case 2: return u8(v & ~(1U << y));
        default: return u8(v | (1U << y));
        }
    }

    // DDCB d op / FDCB d op: the displacement and the opcode are plain
    // memory reads; the result of a rotation, RES or SET is also copied
    // into register z when z != 6.
    template <Index I>
    void exec_index_cb() noexcept {
        const auto displacement = static_cast<std::int8_t>(imm8());
        const std::uint8_t op = imm8();
        internal(2);
        const std::uint16_t address = u16(static_cast<unsigned>(idx<I>() + displacement));
        c_.wz = address;
        const unsigned x = op >> 6;
        const unsigned y = (op >> 3) & 7U;
        const unsigned z = op & 7U;
        const std::uint8_t v = read(address);
        internal(1);
        if (x == 1) {
            bit(y, v, u8(address >> 8));
            return;
        }
        const std::uint8_t res = cb_result(x, y, v);
        write(address, res);
        if (z != 6) {
            reg8<Index::hl>(z) = res;
        }
    }

    // ---- ED --------------------------------------------------------------

    void exec_ed(std::uint8_t op) noexcept {
        const unsigned x = op >> 6;
        const unsigned y = (op >> 3) & 7U;
        const unsigned z = op & 7U;
        const unsigned p = y >> 1;
        if (x == 2) {
            if (z <= 3 && y >= 4) {
                block(y, z);
            }
            return;
        }
        if (x != 1) {
            return;  // ED 00-3F and C0-FF: NOP of 8 T states
        }
        switch (z) {
        case 0: {  // IN r,(C); IN (C) for y = 6
            const std::uint16_t port = c_.bc();
            const std::uint8_t v = in(port);
            c_.wz = u16(port + 1U);
            set_f((c_.f & CF) | sz53p(v));
            if (y != 6) {
                reg8<Index::hl>(y) = v;
            }
            break;
        }
        case 1: {  // OUT (C),r; OUT (C),0 for y = 6 (NMOS)
            const std::uint16_t port = c_.bc();
            out(port, y == 6 ? std::uint8_t{0} : reg8<Index::hl>(y));
            c_.wz = u16(port + 1U);
            break;
        }
        case 2:
            internal(7);
            if ((y & 1U) == 0) {
                c_.set_hl(sbc16(c_.hl(), rp<Index::hl>(p)));
            } else {
                c_.set_hl(adc16(c_.hl(), rp<Index::hl>(p)));
            }
            break;
        case 3: {
            const std::uint16_t address = imm16();
            if ((y & 1U) == 0) {
                write16(address, rp<Index::hl>(p));
            } else {
                set_rp<Index::hl>(p, read16(address));
            }
            c_.wz = u16(address + 1U);
            break;
        }
        case 4: {  // NEG and its mirrors
            const std::uint8_t v = c_.a;
            c_.a = 0;
            sub8(v, 0, false);
            break;
        }
        case 5:  // RETN, RETI and mirrors: both copy IFF2 into IFF1
            c_.iff1 = c_.iff2;
            c_.pc = c_.wz = pop();
            break;
        case 6: {
            constexpr std::array<std::uint8_t, 8> modes{0, 0, 1, 2, 0, 0, 1, 2};
            c_.im = modes[y];
            break;
        }
        default:
            misc_ed(y);
            break;
        }
    }

    // ED 47-7F, z = 7: LD I,A  LD R,A  LD A,I  LD A,R  RRD  RLD, NOPs.
    void misc_ed(unsigned y) noexcept {
        switch (y) {
        case 0:
            internal(1);
            c_.i = c_.a;
            break;
        case 1:
            internal(1);
            c_.r = c_.a;
            break;
        case 2:
        case 3:
            internal(1);
            c_.a = y == 2 ? c_.i : c_.r;
            set_f((c_.f & CF) | sz53(c_.a) | (c_.iff2 ? PF : 0U));
            ld_air_ = true;
            break;
        case 4:
        case 5: {
            const std::uint16_t address = c_.hl();
            const std::uint8_t v = read(address);
            internal(4);
            std::uint8_t m = 0;
            if (y == 4) {  // RRD
                m = u8((c_.a << 4) | (v >> 4));
                c_.a = u8((c_.a & 0xF0U) | (v & 0x0FU));
            } else {  // RLD
                m = u8((unsigned{v} << 4) | (c_.a & 0x0FU));
                c_.a = u8((c_.a & 0xF0U) | (v >> 4));
            }
            write(address, m);
            c_.wz = u16(address + 1U);
            set_f((c_.f & CF) | sz53p(c_.a));
            break;
        }
        default:
            break;
        }
    }

    // LDI CPI INI OUTI (y = 4), LDD CPD IND OUTD (5), and the repeated
    // forms (6, 7). A repeated instruction that goes on rewinds PC and
    // takes 5 more T states.
    void block(unsigned y, unsigned z) noexcept {
        const bool down = (y & 1U) != 0;
        const bool repeat = y >= 6;
        const unsigned delta = down ? 0xFFFFU : 1U;
        switch (z) {
        case 0: {  // LDI
            const std::uint8_t v = read(c_.hl());
            write(c_.de(), v);
            internal(2);
            c_.set_hl(u16(c_.hl() + delta));
            c_.set_de(u16(c_.de() + delta));
            c_.set_bc(u16(c_.bc() - 1U));
            const unsigned n = v + unsigned{c_.a};
            unsigned flags = (c_.f & (SF | ZF | CF)) | (n & XF) | ((n << 4) & YF);
            if (c_.bc() != 0) {
                flags |= PF;
            }
            set_f(flags);
            if (repeat && c_.bc() != 0) {
                rewind();
            }
            break;
        }
        case 1: {  // CPI
            const std::uint8_t v = read(c_.hl());
            internal(5);
            c_.set_hl(u16(c_.hl() + delta));
            c_.set_bc(u16(c_.bc() - 1U));
            c_.wz = u16(c_.wz + delta);
            const unsigned res = (c_.a - unsigned{v}) & 0xFFU;
            const unsigned half = (c_.a ^ v ^ res) & HF;
            const unsigned n = res - (half != 0 ? 1U : 0U);
            unsigned flags = (c_.f & CF) | NF | half | (res & SF) | (res == 0 ? ZF : 0U) |
                             (n & XF) | ((n << 4) & YF);
            if (c_.bc() != 0) {
                flags |= PF;
            }
            set_f(flags);
            if (repeat && c_.bc() != 0 && res != 0) {
                rewind();
            }
            break;
        }
        case 2: {  // INI
            internal(1);
            const std::uint16_t port = c_.bc();
            const std::uint8_t v = in(port);
            write(c_.hl(), v);
            c_.wz = u16(port + delta);
            c_.b = u8(c_.b - 1U);
            c_.set_hl(u16(c_.hl() + delta));
            io_block_flags(v, (c_.c + delta) & 0xFFU, repeat);
            break;
        }
        default: {  // OUTI
            internal(1);
            const std::uint8_t v = read(c_.hl());
            c_.b = u8(c_.b - 1U);
            const std::uint16_t port = c_.bc();
            out(port, v);
            c_.wz = u16(port + delta);
            c_.set_hl(u16(c_.hl() + delta));
            io_block_flags(v, c_.l, repeat);
            break;
        }
        }
    }

    // Flags of INI IND OUTI OUTD and of their repeated forms. `k_base` is
    // C +/- 1 for the inputs, L after its update for the outputs.
    void io_block_flags(std::uint8_t v, unsigned k_base, bool repeat) noexcept {
        const unsigned k = v + k_base;
        unsigned flags = sz53(c_.b) | ((v & 0x80U) != 0 ? NF : 0U) | parity((k & 7U) ^ c_.b);
        if (k > 0xFF) {
            flags |= HF | CF;
        }
        if (repeat && c_.b != 0) {
            // A repeated instruction that goes on: flags of the extra
            // T states (X, Y from PC; H and P/V from the B update).
            rewind();
            flags = (flags & ~unsigned{XF | YF | HF | PF}) | ((c_.pc >> 8) & (XF | YF));
            unsigned pv = parity((k & 7U) ^ c_.b);
            if ((flags & CF) != 0) {
                if ((v & 0x80U) != 0) {
                    pv ^= unsigned{parity((c_.b - 1U) & 7U)} ^ PF;
                    flags |= (c_.b & 0x0FU) == 0x00 ? HF : 0U;
                } else {
                    pv ^= unsigned{parity((c_.b + 1U) & 7U)} ^ PF;
                    flags |= (c_.b & 0x0FU) == 0x0F ? HF : 0U;
                }
            } else {
                pv ^= unsigned{parity(c_.b & 7U)} ^ PF;
            }
            flags |= pv & PF;
            set_f(flags);
            return;
        }
        set_f(flags);
    }

    // The repeated block instruction goes on: PC back on its ED prefix,
    // 5 more T states, WZ = PC + 1, X and Y from the high byte of PC.
    void rewind() noexcept {
        internal(5);
        c_.pc = u16(c_.pc - 2U);
        c_.wz = u16(c_.pc + 1U);
        c_.f = u8((c_.f & ~unsigned{XF | YF}) | ((c_.pc >> 8) & (XF | YF)));
    }
};

}  // namespace emucpc::z80::detail

namespace emucpc::z80 {

template <Bus B>
void Cpu::step(B& bus) noexcept {
    detail::Executor<B>(*this, bus).step();
}

}  // namespace emucpc::z80
