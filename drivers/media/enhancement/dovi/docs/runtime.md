# STB 2.4 runtime contract

## Host registration and parser ownership

[callback_registration.c](../callback_registration.c) supplies the persistent
48-byte `struct dolby_vision_func_s` consumed by the 4.9 host. Its six fields
are the version string, parser initialize/reset/process/release callbacks and
control callback. [host_abi.h](../host_abi.h) checks their offsets and the host
settings and nested metadata layouts against the selected headers.

[host_adapter.c](../host_adapter.c) configures runtime services before
registration and returns an initialization or registration failure to the
kernel. The module has no unload entry or callback teardown. Replacing a
successfully registered module requires reboot.

The metadata application and low-level parser/decoder use static singleton
storage. Initialization replaces their callback context and logger. Parser
release does nothing; it neither frees nor resets the singleton. The event
frame counter persists across initialize, reset and process calls.

## Serialization and buffer lifetime

Parser and control state are shared. Callers must serialize initialization,
reset and processing, including callbacks, and avoid concurrent reentry.
The module adds no locking. Loggers and service callbacks must remain valid
throughout their use.

Low-level readers require accessible padded input and output storage.
Caller-owned buffers must remain live through processing and its callbacks;
errors can leave state and partial output changed. The [parser](parser.md) and
[control](control.md) contracts state their required extents and alias rules.

## Kernel build contract

Build against the target 4.9 kernel's generated headers, configuration and
symbol versions using the matching AArch64 GCC toolchain. The in-tree host
declarations must match the callback and control layouts checked by
`host_abi.h`. Compiler instrumentation also comes from the selected kernel
configuration. See the [build instructions](../README.md#build) for the
compiler and host-header profile.
