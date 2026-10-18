# STB 2.4 fixed-point arithmetic

## Representation

Color, tone and trim stages use explicit integer widths. Keep 32-bit wrapping,
signed versus unsigned shifts, narrowing and 64-bit intermediate products
visible: an algebraic rewrite can change the emitted registers.
`fixed_access.h` supplies signed-width views and bytewise little-endian access;
`fixed_bits.h` supplies arithmetic primitives. Complete backing is required
for every input, scratch and output buffer. Unaligned storage and permitted
aliases retain their original access/reload order.

Public scalar declarations describe raw argument bits; algorithms introduce
signed views where required. Scale and inverse-result domains retain explicit
narrowing. A returned scale belongs to its particular matrix or transfer
operation, rather than a universal Q format.

## Matrices and color ranges

Matrices have nine row-major coefficients, with 16-bit or 32-bit elements as
specified by their entry points. Matrix precision/narrowing reloads each
coefficient after the previous output store so overlapping buffers retain
their native behavior. Signed 16-bit matrix narrowing truncates results.

The YUV offset helper writes three 32-bit little-endian words: the luma offset
followed by equal chroma offsets. The YUV-to-RGB offset calculation sign-extends
16-bit matrix coefficients, multiplies by unsigned 32-bit offsets in 64 bits
and shifts the summed result arithmetically by 16. It reloads the next matrix
row after each output store.

LMS/IPT and RGB/YUV narrow conversion use 219 for the luma row/column and 224
for chroma. Bit-depth inputs retain their signed 16-bit interpretation where
declared. `GetRange(10, 3)` takes the fallback path and writes minimum 0 and
extent 1023. Known range modes preserve their output alias/reload semantics.
Bypass normalization retains its own divisor and exceptional-depth rules.

Color-definition tables, custom-primary determinants, cross-talk composition
and white-point scaling retain their coefficients, intermediate widths,
rounding and evaluation order. The fixed XYZ-to-LMS operand in
`m33_times_m33_as` is part of that entry point's API.

Matrix inversion uses cofactor/determinant arithmetic and low-16 scale
interpretation. A singular matrix returns signed 16-bit `-1`.
`GetLms2RgbByDefM33Fxp(7, output)` returns scale 32. Forward getters,
white-point getters and inverse getters retain the result widths declared in
[numeric_color_matrix.h](../core/numeric_color_matrix.h).

## Integer arithmetic and edge inputs

The primitives implement saturating addition/subtraction, absolute values,
normalization, multiply and raw AArch64 division. Raw divide handles zero and
`INT64_MIN / -1` by their instruction result, distinct from the
saturating `sdiv` helper.

Negative shift arguments retain the native masked-shift behavior:
`dovi_sshl_ssu(1, -1)` returns `0x7fffffff`. Taylor log/power and square-root
wrappers preserve their invalid-scale paths and signed/logical shifts.
The square-root approximation normalizes in 0..30, uses a positive polynomial
accumulator below `2^63`, and returns a value in 0..`INT32_MAX`.

Logarithm and power polynomials use explicit low-32 multiply/add operations.
The first product of the repeated power chain uses an arithmetic right shift;
subsequent loop products use logical shifts. Scale and finishing branches
remain explicit in [fixed_transcendentals.c](../core/fixed_transcendentals.c).

## Luminance, PQ and gamma

[luminance_pq.c](../core/luminance_pq.c) exposes decoder luminance-to-PQ
conversion, capped at 4095. The control conversion `dovi_LToPQ12` in
[pq_conversion.c](../core/pq_conversion.c) takes Q18 luminance and can return
the rounded 4096 endpoint. The 12-bit paths use the 128-entry table values in
`pq_luminance_*.inc`. Inverse 12-bit wrappers mask the low 16 input bits.
15-bit/16-bit and extended conversions retain their separate scales and ranges.

Gamma generation selects its output form through the slope pointer: null
writes 129 16-bit LUT entries; nonnull writes 128 entries with positions and
64-bit slopes. Scratch and output writes can affect later loads through
aliases. The 32-bit slope denominator is zero-extended before division.

The target-EOTF helper writes 97 value/slope pairs, filling all values and the
first 96 slopes while preserving the last slope word. Its interpolation helper
consumes that layout and leaves its maximum argument unused. These functions
are declared in [numeric_transfer.h](../core/numeric_transfer.h).

## Tone-curve generation

`tone_controls.c` prepares controls, `tone_curve.c` coordinates regeneration,
`tone_parameters.c` derives parameters, `tone_curve_lut.c` samples and packs,
and `tone_maps.c` generates auxiliary tone and saturation maps.

Regeneration compares 160 bytes of a 168-byte control object. Its two-point
path uses `dovi_CalcMappedParams2pxx` in modes 0/1; mode 1 replaces minimum,
maximum and diagonal with source values. Mode 3 returns without writes; other
modes use existing parameters. Null final-tone output requires 515 work
elements; nonnull requires 4096 output elements and 515 work elements.

The three-point path clamps source fields in place and generates the intensity
LUT. Null ratio output also fills auxiliary maps; nonnull receives the ratio
and leaves those maps untouched. LUT sampling uses
`step = floor(4096 / lut_size)` and `sample_count = ceil(4096 / step)`.
A finite call requires a positive size no greater than 4096. With no 4K output
the work buffer needs `sample_count` elements; with 4K output it needs
`max(515, sample_count + 1)` elements and output needs 4096 elements. Samples
start at work index 1 in that mode; interpolation reads indices 0 through 514.
Sizes other than 512 may leave work cells unsampled. The `x_min`/`x_max`
inputs are unused. See [tone_curve_lut.h](../core/tone_curve_lut.h).

## Trims and display maps

Trim reduction/commit sorts packed trims, selects brackets and interpolates
level-2 controls. Equal keys swap during the shrinking bubble sort, reversing
duplicate order. Fixed work arrays support sixteen records per trim level.

Chroma distance retains its primary/XYZ/ICtCp pipeline and persistent writes,
then adds three saturating absolute values with unsigned 32-bit wrapping.
It requires valid primaries; there is no null-input contract. Its luminance
argument is 32 bits.

Input/output maps retain their configuration, context and kernel-record
layouts and commit singleton graphics/video state. Bypass paths still populate
output records. Pointer reloads, packed widths, partial writes and assertion
behavior remain part of each entry point's contract.
