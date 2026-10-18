# Runtime contract

## Host registration and parser ownership

| Contract | STB 2.4 / Linux 4.9 | STB 2.6 / CoreELEC 5.15 |
| --- | --- | --- |
| Host callback storage | 48 bytes | 96-byte library prefix plus two HW5 pointers: 112 bytes |
| Parser ownership | Static application and parser singletons | vmalloc-backed context, decoder arena and serialization scratch per instance |
| Parser release | Empty; does not free or reset the singleton | Takes an instance-pointer address, frees owned allocations and clears the pointer |
| Initialization/registration failure | Returns the failure to the kernel | Records the attempted registration result in read-only `registration_status`; insertion succeeds to keep potentially retained callbacks resident |

Registration fills the named fields of the matching host's
`struct dolby_vision_func_s` in persistent storage. Unsupported callbacks
remain null. `host_abi.h` checks callback, settings and nested metadata layouts
against the selected host headers. The STB 2.6 version string begins after
its stored `AML_DV_INFO1:` prefix.

There is no unload entry or callback teardown. Replacing a successfully
registered module requires reboot. STB 2.6 also requires reboot after a failed
registration attempt because the host may have retained its callback addresses.
The event frame counter is shared, including across STB 2.6 parser instances.

## Serialization and buffer lifetime

Control state is shared. Callers must serialize initialization, reset and
processing, including parser callbacks, and avoid concurrent reentry. The
module adds no locking. Loggers and service callbacks must remain valid
throughout their use.

Low-level readers require accessible padded input and output storage.
Caller-owned buffers must remain live through processing and its callbacks;
errors can leave state and partial output changed. The [parser](parser.md) and
[control](control.md) contracts state their required extents and alias rules.

## Kernel build contract

Each module uses its target kernel's generated headers, configuration,
symbol versions and compiler instrumentation. The host declarations must match
the provider's callback and control layouts. See the [build instructions](../README.md#build)
for the selected compiler and host-header profile.

The 4.9 profile uses AArch64 GCC and the in-tree host headers. The 5.15
profile uses Clang/LLVM and matching common-drivers headers. The STB 2.6 module
supplies direct callback addresses and requires `CONFIG_CFI_CLANG=n`.
These integration requirements are separate from Dolby library algorithm changes.
