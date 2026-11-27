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

## z80test

| Field | Value |
|---|---|
| Directory | `z80test/` |
| Origin | https://github.com/raxoft/z80test |
| Version | 1.2a (tag `v1.2a`; release asset `z80test-1.2a.zip`, SHA-256 `7df0443d703e6b3114ea04b4cdef3e13b91421c62e37185c1036d06864cacbaf`) |
| Commit | `c490c0ca94f1021509f8bd656b413861bd7cfb41` |
| Licence | MIT (`z80test/license.txt`) |
| Use | Z80 bench `emucpc-z80test` (`devtools/emucpc-z80test/`) |
| Local changes | none; the files below are extracted, not edited, by `devtools/emucpc-z80test/extract_release.py` |

The `.bin` files are the code blocks of the `.tap` files of the release
(loaded at 8000h, block checksums verified). The `.scr` files are the
bitmaps 4000h-4FFFh shown by the reference images `img/z80ccfscr-*.png` of
the repository at the commit above (the release archive does not hold
them): `zilog.png` SHA-256 `1fc00915371ff23eb0180229c6886e1d105c0a81a19c79bd7aed787bfc322510`,
`nec.png` `cf2c804e7dd7963ea6d6ee3c6b1b1a208198acd82b3a2b3dcd921514989e6ff2`,
`st-cmos.png` `96a3a327825747b077afa9c550211e2af57eba2fef332ff7489a78e36bf813b1`.

| File | Taken from | SHA-256 |
|---|---|---|
| `z80test/z80full.bin` | `z80full.tap`, CODE block | `731f179ec9b0f086440e26e6bae171590e53a3daf55d61fd1729b5b80a197632` |
| `z80test/z80flags.bin` | `z80flags.tap`, CODE block | `d39da3e78b10244c28e2f3e8c015c0fbefd827984a5cc899b44749795706350c` |
| `z80test/z80doc.bin` | `z80doc.tap`, CODE block | `a95c61c7566d21271dbecce1c1064e70bfa8ae99693b8828ab1f68438a25c1a6` |
| `z80test/z80docflags.bin` | `z80docflags.tap`, CODE block | `c09127f4a0804320e1e01fe998eec1886c0325144e6b4182fb612d3b71e760f1` |
| `z80test/z80ccf.bin` | `z80ccf.tap`, CODE block | `6169819c555dde8c06d9c7c87f9ab558d210ee57696b78a1e7dd646f94f27cc6` |
| `z80test/z80memptr.bin` | `z80memptr.tap`, CODE block | `2c396fcea12b2e544e4e6f9307f076d0b89401e348295398e0051dac8a349dd1` |
| `z80test/z80ccfscr.bin` | `z80ccfscr.tap`, CODE block | `bfa212c14ffd3595f0d9cd772c98031ba226b23bd9c15552bb48b66e12b670ab` |
| `z80test/z80ccfscr-zilog.scr` | `img/z80ccfscr-zilog.png`, 4000h-4FFFh | `538fdbc36070eb553ed0d1cb4c1919e2bc176775bc102fbda21b5bf90f100587` |
| `z80test/z80ccfscr-nec.scr` | `img/z80ccfscr-nec.png`, 4000h-4FFFh | `86a0b56657b7c79b4ef46c18157ffbe842970286820992037f436154a807576b` |
| `z80test/z80ccfscr-st.scr` | `img/z80ccfscr-st-cmos.png`, 4000h-4FFFh | `d52fb1fc243c54249aa51ed7775f22a7c342b9c81f48f20c8b68fedd45b7d746` |
| `z80test/license.txt` | `license.txt` | `67e39cd3bbecedf1e406ea9ce99d55fc48c3b497e235f1c7406d1bc4406b7071` |
