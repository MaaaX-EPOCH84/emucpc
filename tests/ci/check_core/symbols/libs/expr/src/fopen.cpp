// Fixture of ci/check_core.sh: no include, yet a reference to fopen.
// SPDX-License-Identifier: GPL-3.0-or-later
extern "C" void* fopen(const char* path, const char* mode);

void* fixture_open()
{
    return fopen("x", "r");
}
