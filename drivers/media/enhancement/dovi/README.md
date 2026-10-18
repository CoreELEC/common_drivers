# Dolby Vision STB 2.6 C module

This directory implements the Dolby Vision metadata parser and control path.
The core parses RPU metadata and produces composer registers, display-mapping
registers, lookup tables and HDMI metadata. The host driver applies those outputs
to the video hardware. All module objects are compiled from the included C
sources; no binary module or precompiled processing object is required.

The build profile selects the native host interface and produces the external
module `dovi.ko`.

## Library interface

The module implements the STB 2.6 library interface, with allocated parser
instances and multi-input control settings. STB 2.6 is the Dolby library version,
separate from the Linux kernel version and Dolby Vision content profiles.
The [STB 2.4 to STB 2.6 changelog](docs/changes.md) describes the parser,
arithmetic, display-management, control and host-interface changes.

Control luminance-to-PQ now caps at 4095 rather than allowing 4096, and signed
16-bit matrix narrowing saturates rather than truncating. Parser bounds,
extension handling, partial writes and control sequencing retain the supplied
STB 2.6 behavior. The host adapter retains the module after registration errors
and exposes their result through `registration_status`.

The external build switches from GCC to Clang/LLVM, using `LLVM=1` and the
integrated assembler (`LLVM_IAS=1`). The build uses the selected kernel's stack
protection and instrumentation; the direct STB 2.6 callback interface requires
`CONFIG_CFI_CLANG=n`. The Clang profile also enables wrapping signed arithmetic
and disables strict-aliasing optimizations.

## Validation

STB 2.6 has differential checks against the reference implementation and
warning-free builds against CoreELEC Linux 5.15.170 and 5.15.196. Hardware
playback with the matching STB 2.6 host has not been validated.

STB 2.4 was separately tested with matching-kernel builds, differential checks,
and physical FEL playback and seeking on the Ugoos AM6B+ running Linux 4.9.269.
Replayed device callbacks match return values and output buffers for identical
inputs and parser-chained processing. A seek run with recording disabled logged
a missing-EL warning; its cause and repeatability remain unestablished.
These tests cover the exercised inputs and lifecycle sequences; they do not
establish STB 2.6 playback equivalence. Panel luminance was not measured.

## Module boundary

[module.c](module.c) delegates registration to [host_adapter.c](host_adapter.c).
[callback_registration.c](callback_registration.c) populates the host callback
table in persistent storage. Compile-time assertions in [host_abi.h](host_abi.h)
check its offsets and the control and metadata layouts against the host headers.
[runtime_bridge.c](runtime_bridge.c) connects kernel services to the core.

The settings record contains three private input records and two
Core 1 result slots. The callback table has a 96-byte STB 2.6 prefix and two trailing
HW5 pointers; unsupported callbacks remain null.

Each parser instance owns vmalloc-backed state, which release frees. Control
state remains shared. The module stays resident after a registration attempt;
the read-only `registration_status` parameter reports the result. A failed
registration requires reboot.

Calls must be serialized across initialization, reset and processing, and
callbacks must not reenter shared state. The module adds no locking and has no
`module_exit` or teardown implementation. Replacing a successfully registered
module requires a reboot. See [Runtime](docs/runtime.md) for the lifecycle and
ownership contract.

## Build

This source has been build-tested without warnings against CoreELEC AArch64
configurations for Linux 5.15.170 and 5.15.196, using matching common-drivers
headers.

No module source changes are required between these configurations. The public
host layouts and compiled processing code match; rebuild for the selected
kernel because its release metadata and symbol versions differ.

Build on Linux with an AArch64 compiler and a completed kernel build for the
device configuration. Its generated headers, configuration and `Module.symvers`
must match the host kernel. `modules_prepare` alone does not generate the symbol
CRCs required by `CONFIG_MODVERSIONS`.

From this module directory, with separate kernel source and output directories:

```sh
make KDIR=/path/to/kernel/source KOUT=/path/to/kernel/output ARCH=arm64
```

Omit `KOUT` when `KDIR` already provides the configured kernel build entry.
The module is an explicit external `M=` target; the parent directory does not
select it through Kconfig.

[Kbuild](Kbuild) links the module entry, host adapter, callback registration,
kernel services and the core units listed in [core_objects.mk](core_objects.mk).
[build_profile.mk](build_profile.mk) selects the host headers, module name and
compiler interface. The build emits stack-usage records and warns about
frames exceeding 2,048 bytes. Kbuild supplies the target kernel's symbol
versions and instrumentation.

The profile defaults to `LLVM=1` with the integrated assembler and produces
`dovi.ko`. Host declarations default to this common-drivers checkout; for a
standalone copy, also pass `COMMON_DRIVERS=/path/to/common_drivers`. The STB 2.6
callback interface requires a non-CFI kernel (`CONFIG_CFI_CLANG=n`). The C
module supplies direct callback addresses.

## Code guide

- [Core source map](core/README.md): entry points, interfaces and source groups.
- [Runtime](docs/runtime.md): host registration, ownership and serialization.
- [Parser and metadata](docs/parser.md): lifecycle, buffer ownership, decoder
  storage and the packed metadata format.
- [Control and register output](docs/control.md): persistent state, output
  ownership, display capabilities and register commits.
- [Fixed-point arithmetic](docs/numerics.md): integer widths, matrix layouts,
  PQ conversion and tone-curve buffer requirements.

## Interface requirements

The layout assertions in `host_abi.h` and the core headers detect incompatible
structure sizes and offsets during compilation. Callers must provide sufficient
buffer extents, serialize shared state and keep callbacks and buffers live for
their required lifetime. Low-level readers require padded input beyond nominal
payload lengths. The parser and control contracts specify ownership, aliasing
and storage requirements for each entry path.
