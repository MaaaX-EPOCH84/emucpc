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

// emucpc-z80test: runs the Z80 test suite of Patrik Rak (z80test 1.2a,
// github.com/raxoft/z80test) on a bus that stands in for the parts of a
// 48K ZX Spectrum it uses.
//
// The programs are the code blocks of the official .tap files, loaded at
// 8000h and called as RANDOMIZE USR 32768 would. The Spectrum is reduced to:
//   0000h-3FFFh  read-only, zero but for two stubs: CHAN-OPEN (1601h) is a
//                RET, RST 10h prints by OUT (FFh),A then RET;
//   ports        an even port reads BFh (no key, MIC bit low: what the IN
//                tests expect), an odd one FFh;
//   interrupts   none.
// The text printed is decoded (13 new line, 23 TAB, 127 the copyright sign,
// 32 columns) and read test by test.
//
// Verdict by variant (--variant):
//   zilog  every test passes; the NEC and ST tests are skipped;
//   nec/st a test may fail only if it is SCF, CCF, SCF+CCF, CCF+SCF or a
//          test of the other variant; the tests of the variant itself never
//          fail, and pass whenever SCF or CCF failed before them.
// The closing line ("Result: all tests passed." or "Result: N of M tests
// failed.") must agree with the test lines. z80ccf assumes a Zilog and is
// meant for --variant zilog.
//
// z80ccfscr (--screen) never returns: it is run for --steps instructions,
// then 4000h-4FFFh is compared with the expected bitmap, taken from the
// reference image of the variant.
//
// Usage: emucpc-z80test --program <file.bin> [--variant zilog|nec|st]
//                       [--screen <file.scr>] [--steps <n>] [--quiet]
//
// Exit status: 0 verdict met, 1 not met (or the program did not return),
// 2 usage or unreadable file.
#include <algorithm>
#include <chrono>
#include <cstddef>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <fstream>
#include <iostream>
#include <iterator>
#include <optional>
#include <sstream>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

#include <emucpc/z80/cpu.hpp>
#include <emucpc/z80/step.hpp>

namespace {

using emucpc::z80::Cpu;
using emucpc::z80::Cycle;
using emucpc::z80::Tick;
using emucpc::z80::ticks_per_t;
using emucpc::z80::Variant;

constexpr std::uint16_t load_address = 0x8000;
constexpr std::uint16_t return_address = 0x0030;  // where the program's RET lands
constexpr std::uint16_t stack_top = 0x7FF8;       // below RAMTOP after CLEAR 32767
constexpr std::uint16_t rom_end = 0x4000;
constexpr std::uint16_t drawn_start = 0x4000;  // z80ccfscr draws 4000h-4FFFh
constexpr std::size_t drawn_size = 0x1000;

// The 48K Spectrum as z80test sees it.
class SpectrumBus {
public:
    SpectrumBus() : memory_(0x10000, 0) {
        // RST 10h: OUT (FFh),A ; RET
        memory_[0x0010] = 0xD3;
        memory_[0x0011] = 0xFF;
        memory_[0x0012] = 0xC9;
        memory_[0x1601] = 0xC9;  // CHAN-OPEN: RET
        printed.reserve(16384);
    }

    std::string printed;  // bytes sent to RST 10h

    void load(std::uint16_t address, const std::vector<std::uint8_t>& bytes) {
        std::copy(bytes.begin(), bytes.end(), memory_.begin() + address);
    }
    [[nodiscard]] std::uint8_t peek(std::uint16_t address) const { return memory_[address]; }
    void poke(std::uint16_t address, std::uint8_t value) { memory_[address] = value; }

    // Bus concept.
    [[nodiscard]] bool ready(Tick /*t*/) const noexcept { return true; }
    [[nodiscard]] std::uint8_t fetch(const Cycle& /*c*/, std::uint16_t a) const noexcept { return memory_[a]; }
    [[nodiscard]] std::uint8_t read(const Cycle& /*c*/, std::uint16_t a) const noexcept { return memory_[a]; }
    void write(const Cycle& /*c*/, std::uint16_t a, std::uint8_t v) noexcept {
        if (a >= rom_end) {
            memory_[a] = v;
        }
    }
    [[nodiscard]] std::uint8_t io_read(const Cycle& /*c*/, std::uint16_t port) const noexcept {
        return (port & 1U) == 0 ? std::uint8_t{0xBF} : std::uint8_t{0xFF};
    }
    void io_write(const Cycle& /*c*/, std::uint16_t port, std::uint8_t v) noexcept {
        if ((port & 0xFFU) == 0xFF) {
            printed.push_back(static_cast<char>(v));
        }
    }
    [[nodiscard]] std::uint8_t int_ack(const Cycle& /*c*/) const noexcept { return 0xFF; }
    [[nodiscard]] bool int_line(Tick /*t*/) const noexcept { return false; }
    [[nodiscard]] bool nmi_edge(Tick /*t*/) noexcept { return false; }

private:
    std::vector<std::uint8_t> memory_;
};

// The printed bytes as lines of at most 32 columns, as the Spectrum shows
// them (the copyright sign is one column, written as UTF-8).
std::vector<std::string> screen_lines(std::string_view printed) {
    std::vector<std::string> lines(1);
    std::size_t column = 0;
    auto new_line = [&] {
        lines.emplace_back();
        column = 0;
    };
    for (std::size_t i = 0; i < printed.size(); ++i) {
        const auto ch = static_cast<unsigned char>(printed[i]);
        if (ch == 13) {
            new_line();
            continue;
        }
        if (ch == 23) {  // TAB n (two bytes; the column is n mod 32)
            if (i + 2 >= printed.size()) {
                break;
            }
            const std::size_t target = static_cast<unsigned char>(printed[i + 1]) % 32U;
            i += 2;
            if (column > target) {
                new_line();
            }
            lines.back().append(target - column, ' ');
            column = target;
            continue;
        }
        if (column == 32) {
            new_line();
        }
        if (ch == 127) {
            lines.back() += "\xC2\xA9";
        } else if (ch >= 32 && ch < 127) {
            lines.back() += static_cast<char>(ch);
        } else {
            lines.back() += '?';
        }
        ++column;
    }
    return lines;
}

// The closing line: the counts it gives, or nothing if it is missing.
struct Result {
    int failed = 0;
    int total = -1;  // -1: "all tests passed."
};

std::optional<Result> result_line(const std::vector<std::string>& lines) {
    for (const std::string& line : lines) {
        if (line == "Result: all tests passed.") {
            return Result{};
        }
        Result r;
        char tail[16] = {};
        if (std::sscanf(line.c_str(), "Result: %d of %d tests %15s", &r.failed, &r.total, tail) == 3 &&
            std::string_view{tail} == "failed.") {
            return r;
        }
    }
    return std::nullopt;
}

enum class Status : std::uint8_t { ok, failed, skipped };

struct TestLine {
    int number = 0;
    std::string name;
    Status status = Status::ok;
};

std::string trim(const std::string& s) {
    const auto first = s.find_first_not_of(' ');
    if (first == std::string::npos) {
        return {};
    }
    return s.substr(first, s.find_last_not_of(' ') - first + 1);
}

// Test lines: "NNN <name> ... OK|FAILED|Skipped".
std::vector<TestLine> test_lines(const std::vector<std::string>& lines) {
    std::vector<TestLine> out;
    for (const std::string& line : lines) {
        if (line.size() < 5 || line[3] != ' ' || line.find_first_not_of("0123456789") != 3) {
            continue;
        }
        TestLine t;
        t.number = std::stoi(line.substr(0, 3));
        std::string rest = trim(line.substr(4));
        for (const auto& [word, status] :
             {std::pair{std::string_view{"OK"}, Status::ok}, std::pair{std::string_view{"FAILED"}, Status::failed},
              std::pair{std::string_view{"Skipped"}, Status::skipped}}) {
            if (rest.size() > word.size() && rest.ends_with(word)) {
                t.status = status;
                t.name = trim(rest.substr(0, rest.size() - word.size()));
                out.push_back(t);
                break;
            }
        }
    }
    return out;
}

std::string_view variant_name(Variant v) {
    switch (v) {
    case Variant::nec:
        return "nec";
    case Variant::st:
        return "st";
    case Variant::zilog:
        break;
    }
    return "zilog";
}

// The verdict on the test lines; empty when met, else the reasons.
std::vector<std::string> judge(const std::vector<TestLine>& tests, Variant v) {
    std::vector<std::string> faults;
    const std::string own = v == Variant::nec ? " (NEC)" : " (ST)";
    const std::string other = v == Variant::nec ? " (ST)" : " (NEC)";
    bool scf_ccf_failed = false;
    for (const TestLine& t : tests) {
        const bool is_scf_ccf = t.name == "SCF" || t.name == "CCF" || t.name == "SCF+CCF" || t.name == "CCF+SCF";
        const bool is_nec_st = t.name.ends_with(" (NEC)") || t.name.ends_with(" (ST)");
        const std::string label = std::to_string(t.number) + " " + t.name;
        if (v == Variant::zilog) {
            if (t.status == Status::failed) {
                faults.push_back(label + ": FAILED");
            } else if (is_nec_st && t.status != Status::skipped) {
                faults.push_back(label + ": run, should be skipped");
            }
            continue;
        }
        if (t.status == Status::failed) {
            if (is_scf_ccf) {
                scf_ccf_failed = true;
            } else if (!t.name.ends_with(other)) {
                faults.push_back(label + ": FAILED");
            }
        }
        if (t.name.ends_with(own) && scf_ccf_failed && t.status != Status::ok) {
            faults.push_back(label + ": not OK after SCF or CCF failed");
        }
    }
    if (tests.empty()) {
        faults.emplace_back("no test line printed");
    }
    return faults;
}

struct Options {
    std::string program;
    Variant variant = Variant::zilog;
    std::string screen;
    std::uint64_t steps = 200'000;
    bool quiet = false;
};

[[noreturn]] void usage(std::string_view message) {
    std::cerr << "emucpc-z80test: " << message << "\n"
              << "usage: emucpc-z80test --program <file.bin> [--variant zilog|nec|st]\n"
              << "                      [--screen <file.scr>] [--steps <n>] [--quiet]\n";
    std::exit(2);
}

Options parse_options(int argc, char** argv) {
    Options o;
    const std::vector<std::string> args(argv + 1, argv + argc);
    for (std::size_t i = 0; i < args.size(); ++i) {
        const std::string& a = args[i];
        auto value = [&]() -> const std::string& {
            if (i + 1 >= args.size()) {
                usage(a + " needs a value");
            }
            return args[++i];
        };
        if (a == "--program") {
            o.program = value();
        } else if (a == "--variant") {
            const std::string& v = value();
            if (v == "zilog") {
                o.variant = Variant::zilog;
            } else if (v == "nec") {
                o.variant = Variant::nec;
            } else if (v == "st") {
                o.variant = Variant::st;
            } else {
                usage("unknown variant " + v);
            }
        } else if (a == "--screen") {
            o.screen = value();
        } else if (a == "--steps") {
            const std::string& v = value();
            if (v.empty() || v.find_first_not_of("0123456789") != std::string::npos) {
                usage("--steps needs a number");
            }
            o.steps = std::stoull(v);
        } else if (a == "--quiet") {
            o.quiet = true;
        } else {
            usage("unknown option " + a);
        }
    }
    if (o.program.empty()) {
        usage("--program is required");
    }
    return o;
}

std::optional<std::vector<std::uint8_t>> read_file(const std::string& path) {
    std::ifstream in(path, std::ios::binary);
    if (!in) {
        return std::nullopt;
    }
    return std::vector<std::uint8_t>(std::istreambuf_iterator<char>(in), std::istreambuf_iterator<char>());
}

}  // namespace

int main(int argc, char** argv) {
    const Options options = parse_options(argc, argv);
    const auto program = read_file(options.program);
    if (!program || program->empty() || program->size() > 0x10000U - load_address) {
        std::cerr << "emucpc-z80test: cannot use " << options.program << "\n";
        return 2;
    }
    std::optional<std::vector<std::uint8_t>> expected_screen;
    if (!options.screen.empty()) {
        expected_screen = read_file(options.screen);
        if (!expected_screen || expected_screen->size() != drawn_size) {
            std::cerr << "emucpc-z80test: " << options.screen << " is not a " << drawn_size << "-byte bitmap\n";
            return 2;
        }
    }

    SpectrumBus bus;
    bus.load(load_address, *program);
    Cpu cpu;
    cpu.variant = options.variant;
    cpu.pc = load_address;
    cpu.sp = stack_top - 2;
    bus.poke(cpu.sp, return_address & 0xFF);
    bus.poke(static_cast<std::uint16_t>(cpu.sp + 1), return_address >> 8);
    cpu.set_iy(0x5C3A);  // what BASIC leaves for USR
    cpu.hl2 = 0x2758;

    const auto start = std::chrono::steady_clock::now();
    std::uint64_t steps = 0;
    bool returned = false;
    if (expected_screen) {
        for (; steps < options.steps; ++steps) {
            cpu.step(bus);
        }
    } else {
        // Bounded only to stop a runaway program: z80full takes far fewer.
        constexpr std::uint64_t limit = 100'000'000'000ULL;
        for (; steps < limit; ++steps) {
            if (cpu.pc == return_address) {
                returned = true;
                break;
            }
            cpu.step(bus);
        }
    }
    const double seconds = std::chrono::duration<double>(std::chrono::steady_clock::now() - start).count();
    const std::uint64_t t_states = cpu.now.value / ticks_per_t;

    std::cout << "emucpc-z80test: " << options.program << ", variant " << variant_name(options.variant) << "\n";
    std::cout << "emucpc-z80test: " << steps << " steps, " << t_states << " T states ("
              << static_cast<double>(t_states) / 3.5e6 << " s of a 3.5 MHz Spectrum) in " << seconds << " s\n";

    if (expected_screen) {
        std::size_t differing = 0;
        for (std::size_t i = 0; i < drawn_size; ++i) {
            if (bus.peek(static_cast<std::uint16_t>(drawn_start + i)) != (*expected_screen)[i]) {
                ++differing;
            }
        }
        if (differing != 0) {
            std::cout << "emucpc-z80test: screen differs from " << options.screen << " in " << differing
                      << " of " << drawn_size << " bytes\n";
            return 1;
        }
        std::cout << "emucpc-z80test: screen identical to " << options.screen << "\n";
        return 0;
    }

    const std::vector<std::string> lines = screen_lines(bus.printed);
    if (!options.quiet) {
        for (const std::string& line : lines) {
            std::cout << "| " << line << "\n";
        }
    }
    if (!returned) {
        std::cout << "emucpc-z80test: the program did not return\n";
        return 1;
    }
    const std::vector<TestLine> tests = test_lines(lines);
    std::vector<std::string> faults = judge(tests, options.variant);
    const auto failed_lines = static_cast<std::size_t>(
        std::count_if(tests.begin(), tests.end(), [](const TestLine& t) { return t.status == Status::failed; }));
    const bool in_fe_failed = bus.printed.find("IN FE:") != std::string::npos;
    const std::optional<Result> result = result_line(lines);
    if (!result) {
        faults.emplace_back("no Result line");
    } else if (result->total < 0 ? failed_lines != 0
                                 : std::cmp_not_equal(result->failed, failed_lines) ||
                                       std::cmp_not_equal(result->total, tests.size())) {
        faults.push_back("the Result line disagrees with the " + std::to_string(tests.size()) + " test lines, " +
                         std::to_string(failed_lines) + " FAILED");
    }
    if (in_fe_failed) {
        faults.emplace_back("IN FE mismatch: the IN tests were skipped");
    }
    std::cout << "emucpc-z80test: " << tests.size() << " tests, " << failed_lines << " failed\n";
    for (const std::string& f : faults) {
        std::cout << "emucpc-z80test: verdict not met: " << f << "\n";
    }
    return faults.empty() ? 0 : 1;
}
