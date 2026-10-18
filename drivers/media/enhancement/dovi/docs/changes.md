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

## Metadata parser

### Instance ownership and callback interface

STB 2.4 has a static 0x780-byte application context, contiguous payload
backing, a singleton parser and persistent external counters. STB 2.6
allocates a 0x770-byte application context, a 0x10442-byte sequence and a
queried parser arena. Output sizes and callback context belong to each
instance; the frame counter remains global. Composer storage moves from
application offset 0x30 to 0x18, and sequence/configuration/parser/counter
offsets change. Allocation failure and release preserve the required
free/null order. STB 2.4 keeps its header/RPU/DM payloads, counters and
logger outside the application; STB 2.6 owns its arenas separately.

STB 2.4 initialization accepts the logger and historical flag, rebinds the
singleton and clears static payloads. Reset ignores flags and release is
empty. Processing passes the full end-of-stream byte, publishes lengths
on success and then resets the parser. STB 2.6 initialization accepts
instance state without the parser logger. Its nine-argument process
interface adds explicit context, flags and input kind; kinds 1/2 ingest
VUI/ST2086/T.35 data, flag bit 0 flushes raw processing, and success
retains parser state. Error paths retain their partial length publication.
Release accepts a pointer to the instance pointer and frees/nulls sequence,
parser and application storage in order.

STB 2.6 explicit reset resets the global frame counter after success and
adds flags to both reset APIs. In direct mode, flags equal to 1 clear the
0x136-byte current header, 0x10744-byte DM payload and two payload records
occupying 0xe070 bytes. STB 2.4 resets indices/state/MEL without those deep clears.
Buffered STB 2.6 reset traps for an excessive slot count after writing the
first fifteen payload handles; STB 2.4 retains unchecked copying.

### Queue handling and bounds

Both parsers stop at queue results -20/-21. STB 2.4 reports the first
other nonpositive result once and stops; any nonzero decoder MEL byte
sets flag bit 0. STB 2.6 continues draining under its unsigned loop
condition, retains the last error and ORs the raw MEL byte into flags.

STB 2.6 preserves fatal decoder/query checks at level >=16. Validation
level 16 uses `BRK #1`, larger levels use `BRK #0x5512`, and profile >16
is fatal; profile 16 uses its separate 38-word row. Mapping pivot 15,
payload IDs >=15 and spatial modes >=8 are fatal. Sentinel searches keep
their full row/table bounds. Buffered append increments the length before
trapping when its previous value exceeds 0xfff. STB 2.4 retains validator
results -4/-5 and unchecked pointer/array access, including its
write-before-post-check buffering behavior.

Fallback NLQ initialization changes from complete 28-byte records in
STB 2.4 to sparse overlapping stores over 0x6874..0x68c7 in STB 2.6.
Untouched bytes remain significant. STB 2.6 exposes `set_default_payload`
and the payload-level MEL predicate; the STB 2.4 MEL predicate is private.
STB 2.6 also removes the ignored VUI pointer from decoder/parser VUI calls.

### Extensions, lengths and diagnostics

STB 2.4 extension reading receives configuration, event-callback slot and
reader explicitly; STB 2.6 obtains them from its decoder. Both only read
the byte index. Non-first payload diagnostics test kind ==2 in STB 2.4
versus kind !=1 in STB 2.6. Unknown extension copy has no 256-byte union
limit in STB 2.4; STB 2.6 stops after byte 255.

STB 2.4 adds both initial extension reads and second incremental-update
reads to the returned DM bit count; STB 2.6 omits those adjustments.
Unknown packed DM extensions use signed 32-bit wrapping
`position + 5 + offset` addressing in STB 2.4. STB 2.6 first selects the
block pointer, then addresses `block[5 + offset]`.

NLQ sign masks use `1u << ((width + 1) & 31)` in STB 2.4 versus
`2u << (width & 31)` in STB 2.6. At width modulo 32 equal to 31, the
former selects bit 0 and the latter drops the mask to zero. After the
same signed capacity comparison, metadata copy length widens through
unsigned 32 bits in STB 2.4 and sign-extends to 64 bits in STB 2.6;
abnormal nonpositive lengths retain their distinct partial-output behavior.

Parser API text changes from 0.4.1 to 0.4.3; algorithm text remains 1.1.2.
STB 2.4 stores algorithm text at API text +8; STB 2.6 uses separate string
objects. Diagnostics retain the selected logger/import behavior and
STB 2.6 supplies the previously omitted partition-count printf argument.

The common parser code shares byte alignment, residual-coefficient
traversal, ST2094 inclusive target windows, synthetic polynomial defaults
and byte-published extension bookkeeping. Separate policies select
application allocation and metadata-copy sign extension.

### Resampling coefficients

Prediction-filter reading shares coefficient sign extension but uses
signed component/direction address arithmetic in STB 2.4 versus unsigned
arithmetic in STB 2.6. Diagnostic capture timing, prediction flag/delta
reloads and cached/reloaded payload owners remain distinct. Only STB 2.4
exposes the spatial phase/filter reader in this API family.

## Tone mapping and trim

### Tone controls, curves and LUTs

STB 2.4 maps source/target displays and level-2 trims into its 168-byte
tone control. STB 2.6 prepares indexed source/target color state,
metadata overrides, adaptive backlight, PQ bounds, secondary-signal
propagation and tone-LUT sizes from different context/configuration
records. HLG, luminance/chroma defaults, repeated reads after packed
stores, per-signal update order and LUT bounds traps remain explicit.

STB 2.4 compares 160 bytes of tone control and builds two/three-point
parameters, ratio maps and auxiliary maps. A two-point mode-3 call can
leave every output untouched; ratio-pointer selection can leave three
auxiliary maps untouched. STB 2.6 compares 0x1a0 control bytes and 0xf8
parameter bytes at 0x1650, prepares/builds/packs its curve pipeline,
copies the first sample into four control slots and returns its change
flags. Mode/context slots remain part of its interface.

The STB 2.4 parameter builder derives mapped points and curve segments
from source/display records with ordered output aliases. STB 2.6 copies
its control block into a different parameter record, derives low/mid/high
points and applies HLG, native-luminance, white-point and trim scaling.
The shifts, clamps, denominator handling and destination stores differ.

STB 2.4 samples a 4096-step log-coordinate table with floor/ceiling
strides and optional interpolation from a padded 515-word work buffer
into 4096 outputs, retaining finite-size and untouched-cell behavior.
STB 2.6 samples 512 primary/identity values, applies HLG where selected
and packs register LUTs and control state with the required source reloads.
It builds a primary and three auxiliary tone/saturation maps using
different parameter blocks, square-root scratch, rounding/clamping and
511/512 endpoint rules.

STB 2.6 adds local-saturation patching, HLG inverse-OETF parameter
generation, Hermite tone/saturation cores and cutoff scoring with signed
128-bit product reduction. HLG writes 256 words at output +0x30 and can
trap after partial writes. `TMScore` may publish its square-root output
before later configuration reads. Forward/backward tone/saturation
wrappers use the parameter block at 0x1650 and Q11 conversion, retaining
32/full-64 result widths, cached square roots and ratio behavior. HLG
OOTF composition uses extended PQ/power helpers and preserves repeated
loads and zero/negative-ratio behavior.

### Trim records and interpolation

STB 2.4 reduces packed big-endian level 2/8/10/254 records, sorts keys,
selects display brackets and commits clamped/interpolated level-2 values
with default/configuration adjustments. Its fixed work arrays hold 16
records; equal keys swap and can reverse duplicate order.

STB 2.6 selects up to four retained luminance/chroma indices, constructs
level-2/8 defaults and resolves target maxima from display/level-10
records. The 14/42-byte record strides, duplicate ordering, native array
limits, default injection and partial writes follow these new APIs.
New interpolation operations select brackets, sort coordinates with
their associated indices and return Q12 weights/default-use flags.

Luminance coordinates use display/level-10 fallback or level-2 target PQ,
scale into the working domain, suppress rounded/upper duplicates and
inject/exclude defaults before publishing target PQ. Chroma coordinates
use display/level-10/metadata primaries and suppress duplicates/defaults.
Both retain 17-entry workspaces, index/count store order, diagnostics
and bounds traps after earlier writes.

The final chroma-distance absolute sum is shared, including `INT32_MIN`
saturation and unsigned 32-bit addition. STB 2.4 computes its own primary,
XYZ/LMS/cross-talk path and writes 116 bytes of persistent state.
STB 2.6 calls XYZ/ICtCp helpers and returns zero for null primaries.
Luminance arithmetic consumes only the low 32 input bits.

Inverse-LUT uniqueness publishes the end of each run and reloads the
mutable last index after writes. Primary backward-map concatenation
copies tone parameters before checking the signed last index, iterates
inclusively through that mutable index, and traps at entry 512 after
earlier outputs. Its inverse-luma plane narrows to short without clamping;
the other three planes use tone/saturation and signed-ratio calculations
with separate rounding and 0..0x2000 clamps. A negative initial last index
skips map writes after the parameter copy. The entry returns 1.

## Display management and metadata

### Configuration, frames and state

STB 2.4 initializes separate source/graphics/target records, pending and
active configuration pointers, tone controls and trim arrays with sparse
stores, retained pitches/Chrm fields and self-pointers. STB 2.6 initializes
indexed 0x120-byte signals, 0x34-byte alpha records, 0x64-byte control and
0x1b0-byte tone control in a 0x8e4-byte configuration prefix. Five full
signals plus alpha storage remain distinct from the legacy index-5 byte
view. Clearing, primary/transfer defaults, primaries calls, manager stores,
repeated assignments and untouched fields preserve their order.

STB 2.4 getters select pending before active state, including a null pending
pointer. STB 2.6 configuration initialization ignores its first LP64
argument and returns its word-backed storage pointer. Direct/indexed
getters retain null-context failure, nonnull out-of-range traps and
zero-extended `0xffffffff` status. `GetTmCtrl`/`GetDmCtrl` expose signed
32-bit status in both versions. STB 2.6 extension initialization ignores
configuration; STB 2.4 reads it. STB 2.6 also appends fixed 0x108-byte
extension records after the current byte count, then wraps the count,
without added capacity checking.

Gray encoding changes from 4 to 5. STB 2.6 omits chroma for planar gray;
STB 2.4 includes it. Minimum-frame sizing uses wrapped 32-bit products
and signed quarter division. Compact row-pitch sizing instead halves
each signed dimension before multiplying and uses the component count
for packed pitch. Other common component/sample encodings remain equal.
STB 2.6 configuration-to-frame conversion writes geometry, location and
pitches before its flags-bit-1 early return, then updates sample type,
weave, bit depth and column pitch. Metadata conversion gates on validity,
uses 8-bit/wider sample sizes and retains reload-after-write aliasing.
STB 2.4 keeps its compact/singleton/secondary-input frame updates.

Kernel-state initialization changes allocation from 0x32a0000 to
0xca80000 and row pitch from 0x1000 to 0x2000, with different manager,
configuration, context and maxima fields. Metadata classification compares
520/514 bytes and returns 0/1/2 in STB 2.4 versus 700/120 bytes and 0/4/1
in STB 2.6. The STB 2.6 0x78-byte metadata base view sits within the
700-byte storage; it does not encompass every extension.

STB 2.4 configuration commit reads the pending pointer before its nominal
null check, copies 984 bytes and updates singleton frame addresses.
STB 2.6 selects indexed kernel/metadata state, initializes dimming/filter
timing and square-root state, uses frame planes at +0x4380000/+0x8700000,
and commits input/alpha/target frames and the output-map pointer.
Finalization publishes first/primary/last input bits, marks initialization,
copies 0x8f8 configuration bytes, stores the mode and may disable dimming.

### Metadata decoding and output

STB 2.4 decodes the 71-byte base into 520-byte metadata using caller-owned
trim arrays, signed remaining lengths and levels 1/2/3/5/255. STB 2.6
resets flags/scales and primary-input unknown-record cursors before
decoding its indexed 700-byte format. It adds levels 4/6/8/9/10/11/254,
per-level capacities and fatal trim-array bounds. Level-8 optional fields
depend on declared lengths and local-saturation defaults; level-9/10
custom primaries and unknown tails have separate pointer/length arrays.
Both retain count and pointer reloads, duplicate/order behavior, skipped
bytes, signed/wrapped size predicates, diagnostics and partial updates.
`ParseMds` changes from signed length without context to unsigned declared
size with explicit context.

STB 2.6 `GetMdsOutput` requires initialized context, applies configured
mode/capacity fallback and can measure before writing. `GenerateOutputMd`
selects indexed/primary state, exclusions and target/signal versions.
Header matrix, EOTF, primaries and min/max derivation can update context
before capacity failure. Identity-prefix initialization traps beyond 16
entries after the first 16 writes.

Output supports levels 1/2/3/4 with target-version constraints and trim
filtering; level 8 with its 13-byte core, optional 16-bit/byte fields and
unknown tails; indexed/custom level-9 primaries; 6/22-byte level-10 records
with unknown fields; source/override level 11; level 254; and aggregate
unknown records. Gates, payload lengths, duplicate selection, extension
count timing, signed capacity checkpoints, staged endian stores and
trap-after-prior-write behavior remain ordered. Null-output measurement
is supported. Level 255 is decoded but has no dedicated output emitter.

### Indexed input consolidation and maps

STB 2.4 commits singleton default/external LMS/IPT matrices, video/graphics
tone targets, output maps and active metadata. STB 2.6 consolidates indexed
or primary signals, crops exclusive active-area endpoints with dimension
reloads, clamps user contrast/chroma/saturation/luminance and signed
white-point adjustments, and selects/builds primaries and color-temperature
transforms. Level 9/11 primaries, intended CCT, source/target scaling and
per-signal white-point state feed tone preparation and two-pass level-2/8
luma/chroma interpolation with configuration/default fallbacks.

Level-254 mode/version comes from metadata +0x6f/+0x70; version clamps
value +2 to 4. Selector +0xd8 comes from metadata +0x71, configuration or
primary state. Selector 2 returns after active-area consolidation.
Signed control word +0xdc >127 clears flags 0x210. Tone preparation
precedes trim consolidation; Q12 weights, anchor order and partial stores
remain observable.

STB 2.6 separates metadata, signal and indexed IPT/ICtCp input-map stages,
with independent color/range decisions, HLG and DM3 bypass. Output maps
select indexed owners and mode-dependent target ranges, add ICtCp and
cross-talk reapplication, `UpdateKsOMapTmm` and DM3 output bypass.
STB 2.4 uses singleton configuration/target-update arguments and separate
graphics/video commits. Both preserve matrix/offset calls, gamma presets,
packed store widths, cached/reloaded owners and partial-write order.
STB 2.6 overlapping consolidation/output workspaces occupy 384/288 bytes;
their reserved spans and 16/32/64-bit views preserve intermediate stores.

### Commit coordination and adaptive backlight

STB 2.4 `CommitMds` selects forced/current metadata, commits pending
color/graphics/video/tone/chroma state, swaps active/pending configuration
and dispatches dynamic metadata. STB 2.6 `Commit2DmKs` distinguishes null
from all-ones metadata, classifies full records/prefixes, maintains per-input
flags and primary-mode timing, consolidates input, chooses bypass or maps
and tone curves, compacts inverse maps and conditionally publishes
700-byte metadata. Diagnostics, temporal transitions, return bits and
partial updates retain their sequencing.

STB 2.6 adds adaptive-backlight filtering: `ApplyAblu` selects indexed or
primary state, restores target windows when inactive, derives PQ/linear
anchors, clamps model inputs, applies asymmetric temporal weights and
frame thresholds, and publishes target/output ranges. Getters require
active state, tolerate null output pointers, return zero or zero-extended
`0xffffffff`, and expose filtered anchors/means, target windows and model
input/output. No STB 2.4 standalone equivalents exist.

### Kernel-map streams

STB 2.4 serializes control/resampling/primary records, optional secondary,
graphics and color-selected alpha records, then output map/frame data.
It requires a 10928-byte kernel object and produces up to 10872 stream
bytes or 10850 hardware-interface bytes. STB 2.6 uses a 0x1608-byte kernel
object, a fixed 0x15d0-byte stream and a 112-byte map reached through
kernel +0x15f8. Only STB 2.4 exposes `dovi_SetHardwareInterface`.

Loading STB 2.4 restores four geometry/pitch sets and chooses optional
alpha storage from the existing graphics-map color. STB 2.6 preserves
primary/secondary geometry and pitches, secondary type/depth and output
pitches, clears the existing indirect map, restores map bytes at +0x1568
and retargets +0x15f8 to that internal map. Both restore output dimensions
from the primary input. Copy/store and sparse-restore order remain
version-specific. Dump requires the old map readable; load requires it
writable. Neither API accepts a capacity or validates generic backing.
Unaligned little-endian storage is supported; source/destination must be
distinct and every accessed region complete. Byte counts are unsigned
32-bit values.

## Control and output

### Multi-input state and lifetime

STB 2.4 controls one source through 17 arguments, separate graphics/target
descriptors and explicitly bound runtime storage. STB 2.6 receives three
0x68-byte private input records in 0x4f58-byte settings, selects a primary,
stages multiple signal/kernel contexts and publishes two Core 1 slots.
There are two metadata/composer staging slots despite three host inputs.
The packed metadata/primaries, packet headers, source flags/sizes and
overlapping HDR scratch retain their phase-specific views.

STB 2.4 resets at input format -1 and calls its no-op destroy helper on
ordinary completion without clearing initialization. STB 2.6 resets at
zero input count, clears staging and initialization/content bytes and
keeps shared control state across ordinary calls. `Init_cp` receives
instance/configuration inputs, rejects counts >=5 and retains zero and
negative-count behavior. Destroy remains argument-free and returns zero.

Memory requests change from 32 signed, unchecked singleton slots to 24
slots with count reloads, an exact -1 threshold, unsigned bounds traps and
partial stores. STB 2.6 sizes per-instance kernel/extension arenas.
`InitMmg` returns the manager pointer in both versions; request flags
remain 32-bit words. STB 2.6 adds OS-backed byte clearing, metadata/frame
synchronization, run-mode indexing and display/default records.

STB 2.4 configuration getters select pending/active pointers and its kernel
getter returns the singleton. STB 2.6 returns context +8 for nonnull
configuration, uses signed index-minus-one kernel slots and indexed
run/signal/tone-map/context getters with their null/trap behavior.
Kernel publication accepts an instance index and traps at index >=4.

STB 2.6 owns contiguous 210856-byte control storage, signature storage and
constant/table aggregates with asserted offsets and reserved spans.
STB 2.4 owns separately aligned runtime and table objects. Relative aliases
and shared initializer values preserve distinct native objects. CLI
priority moves from byte 52 in a 112-byte record to 0x1d14 in a 7468-byte
record. The newer tail adds selected count/primary, initialization/input
flags and level-11 controls; unknown fields remain reserved.

### Target, graphics and transport

STB 2.4 VSVDB conversion writes an 88-byte target descriptor and returns
-1 for null or invalid identification. STB 2.6 updates persistent capability
fields, treats null as retained-state success and returns -8 for invalid
identification/version/length. Partial capability/luminance writes remain
ordered. Decimal versus fixed-point luminance and rounding differ; the
STB 2.6 version-1 path squares/shifts a 32-bit word, reinterprets it as
signed and divides toward zero. Output target, low-latency and RGB setup
follow the newer capability state.

STB 2.4 derives a separate 64-byte graphics descriptor from CLI fields and
stored luminance offsets. STB 2.6 uses input mode 2, format/primary
defaults, graphics luminance words and a separate Core 2 slot. Gamma,
matrix, source/default stores and partial-state order differ.

STB 2.6 adds VSEM/VSIF decoding, CRC/header/OUI validation and -5 failures,
transport-derived signals, optional Dolby control packets and content,
white-point and level-11 notifications. Packet/type/status writes and
callback order are preserved. Indexed kernel dumping and multi-kernel
Core 3 packing use the new owners. STB 2.4 instead passes packed metadata
and separate HDR10/source arguments.

### Metadata, registers and publication

STB 2.4 metadata has a 71-byte prefix and fixed 261-byte extension slots
in current/previous 66626-byte backing, optional temporal level-1 filtering,
logger failure for lengths >256, and HDMI trim/removal/compaction.
STB 2.6 consumes contiguous variable-length records in 0x1000-byte slots,
retaining signed offsets, incomplete-header reuse, per-source progress and
fatal capacity checks after partial writes.

Composer packing is shared with a constant-zero unsigned-short result.
Its base/enclosing extent changes from 176/900 bytes to 200/968 bytes.
Mode changes from residual-flag selection to constant zero; width/height
move from source +12/+16 to +0/+4. STB 2.6 may convert selected 8-bit
BL/EL configuration, pivots and NLQ coefficients to 10 bits in place before
packing. Interleaved loads/stores support its overlapping views;
STB 2.4 requires disjoint objects. Synthetic source preparation, register
maps and primary-ordered multi-source commit remain version-specific.

LUT packing shares four paired 15-bit streams and the final 32-bit input
map copy. Tone/input bases change from kernel +1328/+200 to +0x540/+0xd8.
Output order and STB 2.6 overlap behavior are preserved.

STB 2.4 raw HDMI publication checks size before writes, rejects >512 words,
logs and preserves the previous payload after clearing availability.
STB 2.6 interleaves raw/extension writes with fatal capacity checks and
publishes transport/control/content outputs using the selected register
and LUT banks. Host/core register offsets and extents differ.

STB 2.4 builds HDR infoframes from input format/HDR10 arguments and fixed
primaries. STB 2.6 derives primaries from fixed-point targets and selects
primary-input HDR10 passthrough/debug output. Level-6 records are processed
in order in STB 2.4 versus last-record-first selection in STB 2.6. Output
format getters expand required 16-byte records into optional PQ/luminance/
primaries records. HDR publication uses 64-bit little-endian copies at
offsets 0, 8, 16 and 21, including the final overlapping store.
