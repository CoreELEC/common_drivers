# Fixed-point arithmetic

## Representation and compatibility

Color, tone and trim stages use explicit integer widths. Keep 32-bit wrapping,
signed versus unsigned shifts, narrowing and 64-bit intermediate products
visible: an algebraic rewrite can change the emitted registers.
`fixed_access.h` supplies signed-width views and bytewise little-endian access;
`fixed_bits.h` supplies shared arithmetic primitives. Complete backing is
required for every documented input, scratch and output buffer. Unaligned
storage and permitted aliases retain their original access/reload order.

Public declarations share raw input types when their native register bits are
equivalent; each algorithm introduces explicit signed views where required.
Fixed and narrow color scales, matrix precision, PQ outputs and square root
results use common types within their bounded result domains.
General scale and inverse-result domains remain distinct where narrowing or
full register bits carry meaning. Some STB 2.6 arithmetic helpers preserve
full 64-bit results, including high bits on early-return paths. These are
library contracts, independent of the kernel release number.

## Matrices and color ranges

Matrices have nine row-major coefficients, with 16-bit or 32-bit elements as
specified by their entry points. Exponents belong to each matrix operation;
there is no universal Q format. The shared matrix precision/narrowing helper
reloads each coefficient after the previous output store so overlapping
buffers retain their native behavior. STB 2.4 truncates signed 16-bit matrix
results; STB 2.6 saturates them and also provides rounded narrowing.

The common YUV offset helper writes three 32-bit little-endian words: the luma
offset followed by equal chroma offsets. Its stores target disjoint slots and
perform no intervening source reads or callbacks. The common YUV-to-RGB offset
calculation sign-extends 16-bit matrix coefficients, multiplies by unsigned
32-bit offsets in 64 bits and shifts the summed result arithmetically by 16.
It reloads the next matrix row after each output store.

STB 2.4 LMS/IPT narrow conversion uses 219 for the luma row/column and 224 for
chroma. STB 2.6 uses 219 for all LMS/IPT coefficients. This changes ordinary
10-bit matrices even when no coefficient saturates. RGB/YUV narrowing retains
separate luma/chroma factors. Each version's unusual bit-depth and signed
16-bit interpretation also remains explicit.

`GetRange(bits, range)` retains different fallback rules. With bits=10 and
range=3, STB 2.4 writes minimum=0, extent=1023; STB 2.6 writes minimum=64,
extent=959. Known range modes also preserve their original output alias/reload
semantics. Bypass normalization shares the YUV offset helper while retaining
its own divisor and exceptional-depth rules.

Color-definition tables, custom-primary determinants, cross-talk composition
and white-point scaling retain their version-specific coefficients,
intermediate widths, rounding and order. STB 2.6 adds ICtCp IO/DM bases,
primary-coordinate lookup, color-temperature conversion and separate
cross-talk reapplication helpers. Replacing the STB 2.4 fixed XYZ-to-LMS
operand with a general matrix multiply would change that entry point's API.

Matrix inversion shares one cofactor/determinant core and low-16 scale
interpretation. A singular matrix still returns signed16 -1 in STB 2.4 and
raw32 65535 in STB 2.6. For `GetLms2RgbByDefM33Fxp(7, output)`, the
STB 2.4 result is 32 while STB 2.6 returns 0xffff0020; those inverse-result
domains remain distinct. Fixed forward getters and precision have bounded
positive results and share declarations. GetRgb2LmsWpByDefV3Fxp also returns
the bounded scale in both versions.

## Integer arithmetic and edge inputs

Shared primitives implement saturating addition/subtraction, absolute values,
normalization, multiply and raw AArch64 division. Raw divide handles zero and
INT64_MIN/-1 by their instruction result, distinct from the
saturating `sdiv` helper. STB 2.6 adds signed/unsigned high-product and wide
shift helpers used by its extended power/PQ calculations.

A negative left-shift argument is a real version difference: `sshl_ssu(1,-1)`
returns 0x7fffffff in STB 2.4 and zero in STB 2.6. Taylor log/power and square-root
wrappers preserve their own invalid-scale paths and signed/logical shifts.
The square-root approximation uses normalization in 0..30, a positive polynomial
accumulator below 2^63, and an optional positive multiply that preserves the
0..INT32_MAX output bound. Logarithm and power polynomials also
share explicit low32 multiply/add cores; scale and finishing branches remain
version-specific. The first product of the shared-purpose repeated power chain
uses an arithmetic right shift; subsequent loop products use logical shifts.
STB 2.6 adds general exponent, extended-range power and angle lookup functions,
including fatal lookup bounds checks.

## Luminance, PQ and gamma

`luminance_pq.c` and `pq_conversion.c` expose decoder and control luminance-to-PQ
entry points. Their 12-bit paths share the 128-entry table values in
`pq_luminance_*.inc`. Both decoder conversions cap at 4095. STB 2.4 control
conversion can return the rounded 4096 endpoint; STB 2.6 control caps at 4095.
Inverse 12-bit table values also match; both inverse wrappers explicitly mask
low 16 input bits and now share 32-bit declarations. Interpolation and endpoint
rules remain explicit. Extended 15/16/18-bit and signed PQ
conversions are separate APIs with different scaling and range contracts.

Gamma inputs use a 32-bit declaration. Gamma generation selects its output form through
the slope pointer: null
writes the 16-bit LUT; nonnull also writes positions and 64-bit slopes. Scratch
and output writes can affect later loads through aliases. STB 2.4 zero-extends
the 32-bit slope denominator before division; STB 2.6 sign-extends it. The native STB 2.6 breakpoint guard is unreachable for the immutable table:
section advances only at levels 16,12,9,2, so indices remain 1..5. The sampler
omits that dead guard.

The STB 2.4 target-EOTF helper writes 97 value/slope pairs, filling all values
and the first 96 slopes while preserving the last slope word. Its interpolation
helper consumes that layout and leaves its maximum argument unused. STB 2.6
uses its native color-temperature, XYZ/ICtCp and gamma-parameter stages instead
of substituting this older helper family.

## Tone-curve generation

`tone_controls.c` prepares controls, `tone_curve.c` coordinates regeneration,
`tone_parameters.c` derives parameters, `tone_curve_lut.c` samples and packs,
and `tone_maps.c` generates auxiliary tone and saturation maps. These units
retain different native algorithms and storage contracts across versions.

STB 2.4 compares 160 bytes of a 168-byte control object before regeneration.
Its two-point path uses `dovi_CalcMappedParams2pxx` in modes 0/1; mode 1 replaces
minimum, maximum and diagonal with source values. Mode 3 returns without writes;
other modes use existing parameters. Null final-tone output requires 515 work
elements; nonnull requires 4096 output elements and 515 work elements.

The STB 2.4 three-point path clamps source fields in place and generates the
intensity LUT. Null ratio output also fills auxiliary maps; nonnull receives
the ratio and leaves those maps untouched. LUT sampling uses
`step=floor(4096/lut_size)` and `sample_count=ceil(4096/step)`. A finite call
requires a positive size no greater than 4096. With no 4K output the work
buffer needs `sample_count` elements; with 4K output it needs
`max(515,sample_count+1)` elements and output needs 4096 elements. Samples start
at work index 1 in that mode; interpolation reads indices 0 through 514. Sizes
other than 512 may leave work cells unsampled. The x_min/x_max inputs are unused.

STB 2.6 compares a 0x1a0-byte control prefix and a 0xf8-byte parameter block at
control offset 0x1650. Regeneration prepares parameters, builds LUTs, packs them,
copies the first sample into four control slots and returns native change flags.
Its HLG EOTF stage writes 256 words from output offset 0x30 and may reach a fatal
bounds path after partial writes. Local saturation records use strides 14 and
42 bytes. Tone score may write an optional square-root value before later
configuration loads, so aliases must preserve that order.

## Trims and display maps

STB 2.4 reduction/commit sorts packed trims, selects brackets and interpolates
its native level-2 controls. Equal keys swap during its shrinking bubble sort,
reversing duplicate order. Fixed work arrays support sixteen records per trim
level. STB 2.6 adds level-8/level-10 selection, default-trim construction,
separate luminance/chroma coordinate builders, Q12 interpolation weights and
inverse-LUT uniqueness. Its level-2/level-8 records use native 14/42-byte strides;
equality cases and default exclusion preserve their ordering.

Chroma distance keeps each version's primary/XYZ/ICtCp pipeline and persistent
writes, then uses the same saturated absolute-value sum. STB 2.6 additionally
returns zero for null primaries. Both declarations take 32-bit luminance.
This does not establish a null-input contract for
STB 2.4.

Input/output maps retain native configuration, context and kernel-record
layouts. STB 2.4 commits singleton graphics/video state; STB 2.6 has indexed
signals, additional ICtCp/bypass decisions, tone-map updates and DM3 bypass
builders. Bypass paths still populate output records. Pointer reloads, packed
widths, partial writes, assertions and fatal bounds remain part of each
entry point's contract.

## Changes between versions

The [changelog](changes.md) records the STB 2.4-to-2.6 arithmetic and ABI changes.
The [module README](../README.md#validation) describes the validation scope for
each version.
