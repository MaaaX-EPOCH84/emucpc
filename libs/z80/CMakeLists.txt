# Z80 CPU, generic: knows nothing of the CPC. Header-mostly (step() is a
# template over the bus); the library instantiates it for the flat test bus.
emucpc_core_library(z80 src/cpu.cpp)
