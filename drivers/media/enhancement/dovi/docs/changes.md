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

## Color and transfer functions

### Matrices, primaries and narrowing

YUV offsets, scalar Kr/Kg/Kb conversion matrices, element copies,
white-vector row sums, and the fixed LMS/IPT scale getters share their
unchanged calculations. Bytewise access supports unaligned buffers;
row-by-row pointer reloads preserve overlapping output behavior. The
Kr/Kg/Kb calculation retains coefficients four and five when `kg` is zero.

LMS/IPT narrowing changes from 219 luma and 224 chroma factors in STB 2.4
to uniform 219 factors in STB 2.6. Final matrix packing also changes from
truncation to saturation. The factor change is observable at 10-bit depth
independently of saturation. General full/narrow RGB/YUV helpers retain
their respective signed scale/depth interpretation and unusual-depth
branches; STB 2.6 adds narrow ICtCp input/output matrices.

Full RGB/YUV matrices retain different coefficient sources and signed or
unsigned division. STB 2.4 uses seven RGB-to-XYZ definitions; STB 2.6 uses
separate Kr/Kg/Kb constants, whose low coefficients can differ, and 19
primary records. STB 2.6 adds descending sorting, xy/CCT and xyY/XYZ
conversion, primary lookup and custom-primary matrix construction, with
the required determinant masks, result widths and bounds traps.

STB 2.6 adds cross-talk and inverse-cross-talk matrices, XYZ-to-LMS
constants, and forward/inverse ICtCp input/output and display-management
bases. Arbitrary-scale inverse construction preserves AArch64
zero-divisor behavior. Its color-temperature helpers add cross-talk
removal/reapplication, primary wrappers and diagonal getters; their
coefficient data includes 64-bit XYZ definitions and white-point/CCT
tables.

Custom-primary XYZ/LMS construction retains each version's determinant
truncation, operand selection, division order, intermediate rounding,
row/column scaling and saturation. `GetLms2RgbByDefM33Fxp(7)` illustrates
the distinct inverse result domains: signed 16-bit 32 in STB 2.4 versus
raw 32-bit `0xffff0020` in STB 2.6. Forward matrix scale results remain
bounded positive values. `GetRgb2LmsWpByDefV3Fxp` returns its scale in both
versions. STB 2.6 `Lms2RgbCtWp` retains a reserved 64-bit argument slot
before its white-point pointer.

White-point matrix construction shares the once-per-row signed division
core, including explicit zero-divisor handling. Its scale/result domain
and truncate-versus-saturate finish remain version-specific.

### Ranges, gamma and PQ

Range fallback and exceptional-depth handling change:
`GetRange(10, 3)` returns minimum/extent 0/1023 in STB 2.4 versus 64/959
in STB 2.6. Minimum/extent alias reloads remain ordered. Bypass maps
retain their own divisors, signed-depth handling and zero/full-range
rules. `CommitKsUs` uses different inputs and packed-store sequences.
STB 2.6 adds level-bit operations, stable ascending sorting, downscale
and filter setup, inverse/interpolated LUTs, and display/primary lookups,
with signed comparisons and inclusive last-index behavior preserved.

`GenerateL2GLut` shares its sampling and store sequence and consumes a
32-bit gamma value. The slope denominator is zero-extended in STB 2.4
and sign-extended in STB 2.6. STB 2.4 additionally supplies 97-point
target-EOTF LUTs, interpolation and gamma alpha/beta calculation. STB 2.6
uses its CCT/xy/XYZ/ICtCp conversion, intended white-point CCT and gamma
parameter preparation, with their state writes. Common gamma breakpoint,
Pulsar and SDR table values remain unchanged.

The 12/16-bit inverse PQ interpolation shares its calculation and explicit
low-16 input masking. Control-path `LToPQ12` permits rounded 4096 in
STB 2.4 and caps at 4095 in STB 2.6; both decoder conversions cap at 4095.
Signed/extended `L2PqX` retains different range branches, rounding and
result widths: raw input `0xffffffffffffffff` produces signed 32-bit -108
in STB 2.4 versus full 64-bit `0x7ffffffffff94` in STB 2.6. STB 2.6 adds
16/18-bit LUT conversions, signed forward/inverse extended PQ and
table-interpolated extended conversions, including their fatal bounds.
Common inverse table values remain shared; extended tables support the
additional operations.

`DeGamma` shares its calculation and table values. Gain, offset, exponent
and sample consume their low 16 bits; overflow scale consumes all 32 bits.
The calculation preserves unsigned log interpolation/wrap, conditional
64-bit overflow scaling with 32-bit truncation, and the final 64-bit
product. STB 2.4 returns its low 32 bits; STB 2.6 returns all 64 bits.
STB 2.4 selects singleton tone direction and separate graphics state;
STB 2.6 selects primary/indexed state by current and committed mode,
normalizes index zero, retains reachable bounds traps and propagates EOTF.
