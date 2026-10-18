# STB 2.6 changes

This changelog describes the changes from STB 2.4 to STB 2.6, including the
host integration required to use the newer library. STB versions identify
the processing library. Linux 4.9 and 5.15 identify its host interfaces and
build environments; changes to color arithmetic, parsing and tone mapping
belong to the library.

## Scalar arithmetic

### Fixed-point operations

Saturating addition, subtraction, absolute value, normalization, logarithm
and power polynomial cores, and the bounded square-root calculation use
shared implementations. Explicit signed views preserve signed consumers
while raw input words preserve every input bit pattern.

STB 2.6 retains full 64-bit results where STB 2.4 returns narrowed words.
For example, its `smpy(0xffffffff, 0x7fffffff)` result is `0x1ffffffff`,
`sdiv_sls(1, 0xffffffff)` returns `0xffffffffffffffff`, and normalization of
a value with a zero low word preserves the original high result bits.
STB 2.6 also adds saturating division returning 0x7fffffff for a zero divisor,
signed and unsigned 128-bit products, and wide-shift helpers. Product
outputs are stored low word before high word, including overlapping slots.

Negative shift counts retain different behavior. `sshl_ssu(1, -1)` gives
`0x7fffffff` in STB 2.4 and zero in STB 2.6. For `sshr_sss`, a count of
`INT_MIN` gives 1 versus zero for input 1, and `0xffffffff` versus zero
for input -1. Masking, signed interpretation and saturation follow the
respective library operations.

### Logarithms, powers and angles

The Taylor logarithm/power, exponential and repeated-power entry points
retain different invalid-scale tests, arithmetic versus logical shifts,
and loop bounds around the shared polynomial arithmetic.
`fxLOG2Taylor(0x40000000, 31)` returns zero in STB 2.4 and `0xffffffff`
in STB 2.6. STB 2.6 `log10Taylor(0)` retains the full result `0x1dbe05bc9`.

STB 2.6 repeated power uses `(scale - 2)` modulo 32 with a do/while loop;
STB 2.4 uses a bounded signed loop. Both consume only the low 32 scale
bits. STB 2.6 adds extended exponent/domain operations and angle
interpolation with their original bounds traps.

`fx_assert` remains an empty function in both versions. Its call sites in
the older tone algorithms do not impose an assertion policy.

### Byte buffers

Both versions perform forward byte copies with overlap propagation.
For a negative signed count, STB 2.4 copies nothing and compares equal.
STB 2.6 sign-extends the 32-bit count into a 64-bit countdown, then proceeds
until zero, a mismatch for comparison, or a backing fault. This behavior
also determines the required readable and writable backing for abnormal
lengths.

### Matrix scaling

Precision calculation remains bounded to 0..64. General matrix scaling
retains STB 2.4 signed 16-bit scale/results versus STB 2.6 raw 32-bit
scale/results and their truncation versus saturation policies. STB 2.6
adds rounded narrowing. The common inverse calculation consumes the low
16 scale bits and reads all inputs before storing outputs, preserving
unaligned and overlapping use. A singular matrix returns signed -1 in
STB 2.4 versus raw 65535 in STB 2.6; final coefficient packing also
differs. Only STB 2.4 exposes `M33TimesM33`, with 32-bit operand reads and
16-bit stores.
