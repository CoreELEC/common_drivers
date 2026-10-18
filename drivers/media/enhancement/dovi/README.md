# Dolby Vision STB 2.4 C module

This directory implements the Dolby Vision metadata parser and control path.
The core parses RPU metadata and produces composer registers, display-mapping
registers, lookup tables and HDMI metadata. The host driver applies those outputs
to the video hardware. All module objects are compiled from the included C
sources; no binary module or precompiled processing object is required.

The build profile selects the native host interface and produces the external
module `dovi.ko`.

## Library interface

The module implements the STB 2.4 library interface, with a 48-byte host callback
table, singleton parser storage and single-input control. STB 2.4 is the Dolby
library version, separate from the Linux kernel version and Dolby Vision content
profiles. The implementation uses fixed-point arithmetic and the host's packed
control and output layouts.

## Validation

Validation includes matching-kernel builds, differential checks against the
reference implementation, and physical FEL playback and seeking on the Ugoos
AM6B+ running Linux 4.9.269. Replayed device callbacks match return values and
output buffers for identical inputs and parser-chained processing.

A seek run with recording disabled logged a missing-EL warning; its cause and
repeatability remain unestablished, with no demonstrated output discrepancy.
The tests cover the exercised inputs and lifecycle sequences. Panel luminance
was not measured.

## Module boundary

[module.c](module.c) delegates registration to [host_adapter.c](host_adapter.c).
[callback_registration.c](callback_registration.c) populates the host callback
table in persistent storage. Compile-time assertions in [host_abi.h](host_abi.h)
check its offsets and the control and metadata layouts against the host headers.
[runtime_bridge.c](runtime_bridge.c) connects kernel services to the core.

`host_control.c` translates host signal formats before
calling the control runtime. SDR BT.2020 is host value `6` and core value `4`.
The core writes the 13,468-byte output prefix; the trailing host fields
`vsvdb_len`, `vsvdb_changed` and `mode_changed` remain host-owned, as do the
source and destination format fields.

Parser and control state use static singleton storage. Metadata release is a
no-op. Configuration or registration failure is returned directly to the kernel.

Calls must be serialized across initialization, reset and processing, and
callbacks must not reenter shared state. The module adds no locking and has no
`module_exit` or teardown implementation. Replacing a successfully registered
module requires a reboot. See [Runtime](docs/runtime.md) for the lifecycle and
ownership contract.

## Build

This source has been build-tested without warnings against an AArch64 Linux
4.9.269 configuration, using the in-tree `amdolby_vision` host headers.

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

The profile defaults to `CROSS_COMPILE=aarch64-linux-gnu-` and produces
`dovi.ko`. It selects the 4.9 host declarations from the kernel tree
and enables GNU11 and `-fno-builtin` for the vendor 4.9 compiler interface.

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
