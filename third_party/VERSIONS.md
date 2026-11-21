# Third-party copies

Each directory of `third_party/` is a copy of an outside project, taken at a
recorded commit. A local change is listed here, one line per change, so that
it can be carried over to the next version. `ci/check_third_party.sh` checks
this file against the copies.

SDL3 is not copied: `ci/deps.sh` fetches the MinGW build of the pinned
release and checks its SHA-256.

## doctest

| Field | Value |
|---|---|
| Directory | `doctest/` |
| Origin | https://github.com/doctest/doctest |
| Version | 2.5.3 (tag `v2.5.3`) |
| Commit | `2d0a9359a60c51affe2a9bebb1be1dca47868151` |
| Licence | MIT (`doctest/LICENSE.txt`) |
| Use | unit tests (`tests/`) |
| Local changes | none |

| File | Taken from | SHA-256 |
|---|---|---|
| `doctest/doctest/doctest.h` | `doctest/doctest.h` | `cfd518a3ef90f67e1f3ba514df23fb3627437de1a2feeba78cf5062a40021421` |
| `doctest/LICENSE.txt` | `LICENSE.txt` | `0fe0b331fa1513dcce8604ff1fa925f32d1cea17d8aeb1c2471fad40d291adc5` |

## nlohmann/json

| Field | Value |
|---|---|
| Directory | `nlohmann_json/` |
| Origin | https://github.com/nlohmann/json |
| Version | 3.12.0 (tag `v3.12.0`) |
| Commit | `55f93686c01528224f448c19128836e7df245f72` |
| Licence | MIT (`nlohmann_json/LICENSE.MIT`) |
| Use | JSON outside the core: session, server, wire, benches |
| Local changes | none |

| File | Taken from | SHA-256 |
|---|---|---|
| `nlohmann_json/nlohmann/json.hpp` | release asset `json.hpp` (single header) | `aaf127c04cb31c406e5b04a63f1ae89369fccde6d8fa7cdda1ed4f32dfc5de63` |
| `nlohmann_json/LICENSE.MIT` | `LICENSE.MIT` | `46a65cffd1ea955132d95a8dd921640714a8d6b537d2e4e482d31145ae95b603` |
