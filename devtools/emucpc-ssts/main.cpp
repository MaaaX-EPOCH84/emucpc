// emucpc-ssts: runs the Z80 against SingleStepTests (github.com/SingleStepTests/z80).
//
// Each data file holds 1000 cases of one opcode: initial state, final state,
// RAM, port accesses and bus activity T state by T state. A case runs one
// step() on the flat bus and compares the final registers (WZ, Q, P, EI
// included), the RAM and the port accesses; with --cycles, also the number
// of T states and the position, address and value of every bus access.
//
// Usage: emucpc-ssts [--dir <dir>] [--cases <n>] [--jobs <n>] [--cycles]
//                    [--match <text>] [--report <n>]
//   --dir     data files (*.json); default $EMUCPC_SSTS_DIR, else
//             $HOME/dev/emucpc-refs/tests/singlestep_z80/v1
//   --cases   first n cases of each file (0: all, the default); the reading
//             stops at the n-th case
//   --jobs    files run in parallel (default: hardware threads)
//   --match   only the files whose name contains <text>
//   --report  failing cases described per file (default 1)
//
// Exit status: 0 all cases pass, 1 a case fails or a file is unreadable,
// 2 usage, 77 no data (ctest reports the test as skipped).
#include <algorithm>
#include <atomic>
#include <chrono>
#include <cstdint>
#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <memory>
#include <sstream>
#include <string>
#include <string_view>
#include <thread>
#include <utility>
#include <vector>

#include <nlohmann/json.hpp>

#include <emucpc/z80/flat_bus.hpp>

namespace {

namespace fs = std::filesystem;
using json = nlohmann::json;
using emucpc::z80::BusAccess;
using emucpc::z80::Cpu;
using emucpc::z80::CycleKind;
using emucpc::z80::FlatBus;
using emucpc::z80::PortAccess;
using emucpc::z80::Tick;
using emucpc::z80::ticks_per_t;

// SAX reader that builds the first `want` elements of the top-level array
// (all of them when want is 0) and stops reading there.
class FirstElements final : public nlohmann::json_sax<json> {
public:
    explicit FirstElements(std::size_t want) : want_(want) {}

    std::vector<json> elements;
    std::string error;

    bool null() override { return place(json(nullptr)); }
    bool boolean(bool v) override { return place(json(v)); }
    bool number_integer(number_integer_t v) override { return place(json(v)); }
    bool number_unsigned(number_unsigned_t v) override { return place(json(v)); }
    bool number_float(number_float_t v, const string_t& /*s*/) override { return place(json(v)); }
    bool string(string_t& v) override { return place(json(v)); }
    bool binary(binary_t& v) override { return place(json::binary(v)); }
    bool key(string_t& v) override {
        key_ = v;
        return true;
    }

    bool start_object(std::size_t /*n*/) override {
        if (!in_top_) {
            error = "the top level is not an array";
            return false;
        }
        return open(json::object());
    }
    bool end_object() override { return close(); }

    bool start_array(std::size_t /*n*/) override {
        if (!in_top_) {
            in_top_ = true;
            return true;
        }
        return open(json::array());
    }
    bool end_array() override {
        if (stack_.empty()) {
            return true;  // end of the top-level array
        }
        return close();
    }

    bool parse_error(std::size_t position, const std::string& /*token*/,
                     const nlohmann::detail::exception& e) override {
        error = "byte " + std::to_string(position) + ": " + e.what();
        return false;
    }

    [[nodiscard]] bool stopped_on_purpose() const { return want_ != 0 && elements.size() == want_; }

private:
    std::size_t want_;
    bool in_top_ = false;
    std::vector<json*> stack_;
    std::string key_;

    json* insert(json&& v) {
        if (stack_.empty()) {
            elements.push_back(std::move(v));
            return &elements.back();
        }
        json& parent = *stack_.back();
        if (parent.is_array()) {
            parent.push_back(std::move(v));
            return &parent.back();
        }
        json& slot = parent[key_];
        slot = std::move(v);
        return &slot;
    }

    bool place(json&& v) {
        if (stack_.empty()) {
            error = "a scalar at the top level";
            return false;
        }
        insert(std::move(v));
        return true;
    }

    bool open(json&& v) {
        stack_.push_back(insert(std::move(v)));
        return true;
    }

    bool close() {
        stack_.pop_back();
        return !(stack_.empty() && want_ != 0 && elements.size() == want_);
    }
};

std::uint8_t byte_of(const json& j, const char* name) { return j.at(name).get<std::uint8_t>(); }
std::uint16_t word_of(const json& j, const char* name) { return j.at(name).get<std::uint16_t>(); }

void load_state(Cpu& cpu, const json& s) {
    cpu.a = byte_of(s, "a");
    cpu.f = byte_of(s, "f");
    cpu.b = byte_of(s, "b");
    cpu.c = byte_of(s, "c");
    cpu.d = byte_of(s, "d");
    cpu.e = byte_of(s, "e");
    cpu.h = byte_of(s, "h");
    cpu.l = byte_of(s, "l");
    cpu.i = byte_of(s, "i");
    cpu.r = byte_of(s, "r");
    cpu.af2 = word_of(s, "af_");
    cpu.bc2 = word_of(s, "bc_");
    cpu.de2 = word_of(s, "de_");
    cpu.hl2 = word_of(s, "hl_");
    cpu.set_ix(word_of(s, "ix"));
    cpu.set_iy(word_of(s, "iy"));
    cpu.pc = word_of(s, "pc");
    cpu.sp = word_of(s, "sp");
    cpu.wz = word_of(s, "wz");
    cpu.iff1 = s.at("iff1").get<int>() != 0;
    cpu.iff2 = s.at("iff2").get<int>() != 0;
    cpu.im = byte_of(s, "im");
    cpu.ei_just = s.at("ei").get<int>() != 0;
    cpu.p = s.at("p").get<int>() != 0;
    cpu.q = byte_of(s, "q");
    cpu.halted = false;
}

// Appends "name expected X got Y" for each differing field.
class Diff {
public:
    template <class T>
    void check(const char* name, T expected, T got) {
        if (expected != got) {
            out_ << ' ' << name << ' ' << +expected << "!=" << +got;
            ok_ = false;
        }
    }
    void note(const std::string& text) {
        out_ << ' ' << text;
        ok_ = false;
    }
    [[nodiscard]] bool ok() const { return ok_; }
    [[nodiscard]] std::string text() const { return out_.str(); }

private:
    std::ostringstream out_;
    bool ok_ = true;
};

void compare_state(Diff& diff, const Cpu& cpu, const json& s) {
    diff.check("a", byte_of(s, "a"), cpu.a);
    diff.check("f", byte_of(s, "f"), cpu.f);
    diff.check("b", byte_of(s, "b"), cpu.b);
    diff.check("c", byte_of(s, "c"), cpu.c);
    diff.check("d", byte_of(s, "d"), cpu.d);
    diff.check("e", byte_of(s, "e"), cpu.e);
    diff.check("h", byte_of(s, "h"), cpu.h);
    diff.check("l", byte_of(s, "l"), cpu.l);
    diff.check("i", byte_of(s, "i"), cpu.i);
    diff.check("r", byte_of(s, "r"), cpu.r);
    diff.check("af_", word_of(s, "af_"), cpu.af2);
    diff.check("bc_", word_of(s, "bc_"), cpu.bc2);
    diff.check("de_", word_of(s, "de_"), cpu.de2);
    diff.check("hl_", word_of(s, "hl_"), cpu.hl2);
    diff.check("ix", word_of(s, "ix"), cpu.ix());
    diff.check("iy", word_of(s, "iy"), cpu.iy());
    diff.check("pc", word_of(s, "pc"), cpu.pc);
    diff.check("sp", word_of(s, "sp"), cpu.sp);
    diff.check("wz", word_of(s, "wz"), cpu.wz);
    diff.check("iff1", s.at("iff1").get<int>() != 0, cpu.iff1);
    diff.check("iff2", s.at("iff2").get<int>() != 0, cpu.iff2);
    diff.check("im", byte_of(s, "im"), cpu.im);
    diff.check("ei", s.at("ei").get<int>() != 0, cpu.ei_just);
    diff.check("p", s.at("p").get<int>() != 0, cpu.p);
    diff.check("q", byte_of(s, "q"), cpu.q);
}

// One bus access as SingleStepTests samples it: T state of the sample where
// the control signals are active, address, value, and pins ("r-m-"...).
struct Sample {
    std::size_t t = 0;
    std::uint16_t address = 0;
    std::uint8_t value = 0;
    std::string pins;

    friend bool operator==(const Sample&, const Sample&) = default;
};

std::string to_string(const Sample& s) {
    std::ostringstream o;
    o << 'T' << s.t << ':' << s.pins << '@' << s.address << '=' << +s.value;
    return o.str();
}

// From the "cycles" array: the data of a read is on the sample after the
// active one, the data of a write on the active one.
std::vector<Sample> expected_samples(const json& cycles) {
    std::vector<Sample> out;
    for (std::size_t t = 0; t < cycles.size(); ++t) {
        const json& entry = cycles[t];
        const auto pins = entry.at(2).get<std::string>();
        if (pins == "----") {
            continue;
        }
        Sample s;
        s.t = t;
        s.address = entry.at(0).get<std::uint16_t>();
        s.pins = pins;
        const json& data = pins[0] == 'r' && t + 1 < cycles.size() ? cycles[t + 1].at(1) : entry.at(1);
        s.value = data.is_null() ? std::uint8_t{0} : data.get<std::uint8_t>();
        out.push_back(std::move(s));
    }
    return out;
}

std::vector<Sample> actual_samples(const std::vector<BusAccess>& accesses, Tick start) {
    std::vector<Sample> out;
    for (const BusAccess& a : accesses) {
        Sample s;
        const std::uint64_t t1 = (a.cycle.t1.value - start.value) / ticks_per_t;
        switch (a.cycle.kind) {
        case CycleKind::opcode:
        case CycleKind::read:
            s.t = static_cast<std::size_t>(t1 + 1);
            s.pins = "r-m-";
            break;
        case CycleKind::write:
            s.t = static_cast<std::size_t>(t1 + 1);
            s.pins = "-wm-";
            break;
        case CycleKind::io_read:
            s.t = static_cast<std::size_t>(t1 + 2);
            s.pins = "r--i";
            break;
        case CycleKind::io_write:
            s.t = static_cast<std::size_t>(t1 + 2);
            s.pins = "-w-i";
            break;
        case CycleKind::int_ack:
            s.t = static_cast<std::size_t>(t1 + 2);
            s.pins = "int";
            break;
        }
        s.address = a.address;
        s.value = a.value;
        out.push_back(std::move(s));
    }
    return out;
}

struct Options {
    fs::path dir;
    std::size_t cases = 0;
    unsigned jobs = 1;
    bool cycles = false;
    std::string match;
    std::size_t report = 1;
};

// Runs one case; returns an empty string when it passes.
std::string run_case(const json& test, const Options& options) {
    const json& initial = test.at("initial");
    const json& final_state = test.at("final");
    auto bus = std::make_unique<FlatBus>();
    Cpu cpu;
    load_state(cpu, initial);
    for (const json& cell : initial.at("ram")) {
        bus->memory[cell.at(0).get<std::uint16_t>()] = cell.at(1).get<std::uint8_t>();
    }
    std::vector<PortAccess> expected_ports;
    if (const auto it = test.find("ports"); it != test.end()) {
        for (const json& access : *it) {
            PortAccess pa;
            pa.port = access.at(0).get<std::uint16_t>();
            pa.value = access.at(1).get<std::uint8_t>();
            pa.write = access.at(2).get<std::string>() == "w";
            if (!pa.write) {
                bus->inputs.push_back(pa.value);
            }
            expected_ports.push_back(pa);
        }
    }
    bus->record_cycles = options.cycles;
    const Tick start = cpu.now;
    cpu.step(*bus);

    Diff diff;
    compare_state(diff, cpu, final_state);
    for (const json& cell : final_state.at("ram")) {
        const auto address = cell.at(0).get<std::uint16_t>();
        const auto expected = cell.at(1).get<std::uint8_t>();
        if (bus->memory[address] != expected) {
            diff.note("ram[" + std::to_string(address) + "] " + std::to_string(expected) +
                      "!=" + std::to_string(bus->memory[address]));
        }
    }
    if (bus->ports != expected_ports) {
        diff.note("ports differ (" + std::to_string(expected_ports.size()) + " expected, " +
                  std::to_string(bus->ports.size()) + " done)");
    }
    if (options.cycles) {
        const json& cycles = test.at("cycles");
        const std::uint64_t t_states = (cpu.now.value - start.value) / ticks_per_t;
        diff.check("T", static_cast<std::uint64_t>(cycles.size()), t_states);
        const std::vector<Sample> expected = expected_samples(cycles);
        const std::vector<Sample> actual = actual_samples(bus->cycles, start);
        if (expected != actual) {
            std::string text = "bus:";
            for (const Sample& s : expected) {
                text += ' ' + to_string(s);
            }
            text += " / got:";
            for (const Sample& s : actual) {
                text += ' ' + to_string(s);
            }
            diff.note(text);
        }
    }
    if (diff.ok()) {
        return {};
    }
    return test.at("name").get<std::string>() + ":" + diff.text();
}

struct FileResult {
    std::size_t cases = 0;
    std::size_t failed = 0;
    std::vector<std::string> reports;
    std::string error;
};

FileResult run_file(const fs::path& path, const Options& options) {
    FileResult result;
    std::ifstream in(path, std::ios::binary);
    if (!in) {
        result.error = "cannot open";
        return result;
    }
    FirstElements reader(options.cases);
    const bool complete = json::sax_parse(in, &reader);
    if (!complete && !reader.stopped_on_purpose()) {
        result.error = reader.error.empty() ? "unreadable" : reader.error;
        return result;
    }
    for (const json& test : reader.elements) {
        ++result.cases;
        std::string report;
        try {
            report = run_case(test, options);
        } catch (const json::exception& e) {
            report = "malformed case: " + std::string(e.what());
        }
        if (!report.empty()) {
            ++result.failed;
            if (result.reports.size() < options.report) {
                result.reports.push_back(std::move(report));
            }
        }
    }
    return result;
}

[[noreturn]] void usage(const std::string& message) {
    std::cerr << "emucpc-ssts: " << message << "\n"
              << "usage: emucpc-ssts [--dir <dir>] [--cases <n>] [--jobs <n>] [--cycles] "
                 "[--match <text>] [--report <n>]\n";
    std::exit(2);
}

std::size_t parse_count(const std::string& text, const char* option) {
    std::size_t used = 0;
    unsigned long value = 0;
    try {
        value = std::stoul(text, &used);
    } catch (const std::exception&) {
        used = 0;
    }
    if (used != text.size() || text.empty()) {
        usage(std::string(option) + " expects a number, not '" + text + "'");
    }
    return value;
}

fs::path default_dir() {
    if (const char* dir = std::getenv("EMUCPC_SSTS_DIR"); dir != nullptr && *dir != '\0') {
        return dir;
    }
    if (const char* home = std::getenv("HOME"); home != nullptr && *home != '\0') {
        return fs::path(home) / "dev/emucpc-refs/tests/singlestep_z80/v1";
    }
    return {};
}

Options parse_options(int argc, char** argv) {
    Options options;
    options.jobs = std::max(1U, std::thread::hardware_concurrency());
    bool dir_given = false;
    const std::vector<std::string> args(argv + 1, argv + argc);
    for (std::size_t k = 0; k < args.size(); ++k) {
        const std::string& arg = args[k];
        auto value = [&]() -> const std::string& {
            if (k + 1 >= args.size()) {
                usage(arg + " expects a value");
            }
            return args[++k];
        };
        if (arg == "--dir") {
            options.dir = value();
            dir_given = true;
        } else if (arg == "--cases") {
            options.cases = parse_count(value(), "--cases");
        } else if (arg == "--jobs") {
            options.jobs = static_cast<unsigned>(std::max<std::size_t>(1, parse_count(value(), "--jobs")));
        } else if (arg == "--cycles") {
            options.cycles = true;
        } else if (arg == "--match") {
            options.match = value();
        } else if (arg == "--report") {
            options.report = parse_count(value(), "--report");
        } else {
            usage("unknown option '" + arg + "'");
        }
    }
    if (!dir_given) {
        options.dir = default_dir();
    }
    return options;
}

}  // namespace

int main(int argc, char** argv) {
    const Options options = parse_options(argc, argv);
    std::vector<fs::path> files;
    std::error_code ec;
    if (!options.dir.empty() && fs::is_directory(options.dir, ec)) {
        for (const fs::directory_entry& entry : fs::directory_iterator(options.dir, ec)) {
            const fs::path& path = entry.path();
            if (path.extension() == ".json" &&
                path.filename().string().find(options.match) != std::string::npos) {
                files.push_back(path);
            }
        }
    }
    if (files.empty()) {
        std::cout << "emucpc-ssts: no data file in '" << options.dir.string() << "', skipped\n";
        return 77;
    }
    std::sort(files.begin(), files.end());

    const auto start = std::chrono::steady_clock::now();
    std::vector<FileResult> results(files.size());
    std::atomic<std::size_t> next{0};
    auto worker = [&] {
        for (std::size_t k = next++; k < files.size(); k = next++) {
            results[k] = run_file(files[k], options);
        }
    };
    {
        std::vector<std::jthread> pool;
        const unsigned jobs = std::min<unsigned>(options.jobs, static_cast<unsigned>(files.size()));
        for (unsigned k = 1; k < jobs; ++k) {
            pool.emplace_back(worker);
        }
        worker();
    }
    const double seconds =
        std::chrono::duration<double>(std::chrono::steady_clock::now() - start).count();

    std::size_t cases = 0;
    std::size_t failed = 0;
    std::size_t failed_files = 0;
    std::size_t errors = 0;
    for (std::size_t k = 0; k < files.size(); ++k) {
        const FileResult& r = results[k];
        const std::string name = files[k].filename().string();
        cases += r.cases;
        failed += r.failed;
        if (!r.error.empty()) {
            ++errors;
            std::cout << name << ": " << r.error << '\n';
        }
        if (r.failed != 0) {
            ++failed_files;
            std::cout << name << ": " << r.failed << '/' << r.cases << " failed\n";
            for (const std::string& report : r.reports) {
                std::cout << "  " << report << '\n';
            }
        }
    }
    std::cout << "emucpc-ssts: " << files.size() << " files, " << cases << " cases, " << failed
              << " failed in " << failed_files << " files, " << errors << " unreadable"
              << (options.cycles ? ", bus activity compared" : "") << " (" << seconds << " s)\n";
    return failed == 0 && errors == 0 ? 0 : 1;
}
