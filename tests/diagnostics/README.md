# Fixed TLV instruction counts

## C++17 wrapper costs

The C99 and C++17 callers take the same pointer and size arguments and their
respective API's descriptors. Both call the same separately compiled C
implementation. Each IO caller selects a null diagnostic at compile time.
Counts begin at the first instruction of each optimized caller and include
nested calls through the return to the fixture. Cursor initialization and
fixture checks take place outside that interval. [C callers](wrapper_probe.c)
and [C++ callers and fixture](wrapper_probe.cpp)

The record is `12 34 00 03 AA BB CC`. The short case supplies three bytes
of input or output capacity. The value-view case uses the record's seven
bytes or a null pointer with the same positive size. The probe checks result
categories, byte counts, pointer identity, encoded bytes, cursor positions
and preservation of sentinel outputs on failure.

| Operation | Input or result | C99 `-O2` | C++17 `-O2` | C99 `-O3` | C++17 `-O3` |
| --- | --- | ---: | ---: | ---: | ---: |
| Result classification | OK | 21 | 24 | 10 | 13 |
| Result classification | Null argument | 22 | 25 | 10 | 13 |
| Value view | Valid | 9 | 22 | 9 | 22 |
| Value view | Null argument | 9 | 18 | 9 | 18 |
| Element measurement | Complete | 46 | 66 | 46 | 66 |
| Read | Complete | 100 | 122 | 96 | 118 |
| Read | Short | 74 | 88 | 71 | 85 |
| Next | Complete | 116 | 133 | 93 | 110 |
| Next | Short | 90 | 99 | 67 | 76 |
| Write | Complete | 167 | 184 | 164 | 181 |
| Write | Short | 100 | 117 | 97 | 114 |
| Append | Complete | 184 | 199 | 162 | 177 |
| Append | Short | 117 | 132 | 93 | 108 |

Two runs at each optimization level produced identical counts on Windows 10
x86-64 build 19045, with WinLibs GCC/G++ 13.3.0 r1, MSVCRT, separate
translation units, default C++ exception handling and GDB 18.1. These are
executed instructions for this ABI and dataset. Elapsed time and
cross-platform thresholds require separate measurements.

- **Result classification.** The enum casts generate no instructions. The C
  caller tail-jumps to `tlv_result_category`. The C++ caller reserves shadow
  space, calls the same function, restores the stack and returns. The C++
  wrapper is `noexcept` and the C declaration has no C++ exception
  specification. In a separate `-O2 -fno-exceptions` assembly check, GCC
  emits the same tail jump for the C++ caller.
- **Descriptors.** A value or element read uses a native output descriptor.
  The wrapper tests the result and publishes pointers and counts on success.
  Compared with `2ea1b98d785a`, omitting the initial clearing of these
  temporaries saves two instructions per value-view call and three per
  one-shot or cursor read, on successes and failures at both levels. The C
  functions assign every output member before returning success.
- **Measurement and writes.** The C++ caller materializes a native element
  descriptor. Measurement and one-shot IO also convert the supplied format,
  mapping unsupported byte orders to unknown. Each typed append checks that
  the writer has been initialized. Both APIs reach the same tag and
  payload copies on successful writes. The optimized probe and library
  objects reference no heap allocation functions.

## Fixed-field encoding and decoding

The length codec selects the wire order once per field. Decoding starts with
the most significant octet and shifts the accumulated unsigned count left by
eight bits for each remaining octet. Encoding stores the least significant
octet first and shifts right by eight bits between stores. The existing field
and element checks establish a width of 1–8 and sufficient space before either
helper accesses a byte. Both helpers use byte accesses on unaligned buffers.

The field probe exercises both byte orders at every width.
It reads and writes each field at offsets zero and one from a 64-byte-aligned
base. Complete records have a two-byte tag, a length at offset three and a
payload of zero, three or 513 bytes (255 for a one-byte length). The probe
checks golden bytes, guards, counts and borrowed views outside the measured
calls. The record writes include the C runtime's payload copy. [Field probe](field_probe.cpp)

Each cell below gives the change in executed x86-64 instructions per public
call from baseline `011519122050` to the selected codec. Negative numbers
indicate fewer instructions. Two runs of each build produced identical counts
on Windows 10 build 19045 with WinLibs GCC/G++ 13.3.0 r1, MSVCRT and GDB 18.1.

| Public operation | Cases | GCC `-O2` | GCC `-O3` |
| --- | ---: | ---: | ---: |
| `tlv_fixed_length_read` | 32 | −43 to −10 | +3 |
| `tlv_fixed_length_write` | 32 | −56 to −6 | +2 |
| `tlv_read` | 48 | −56 to −16 | −18 to −11 |
| `tlv_write` | 48 | −70 to −13 | −31 to −8 |

Accumulating with a fixed eight-bit shift and retaining the per-octet wire
index took an eight-byte big-endian field read from 124 to 122 instructions at `-O2`.
Selecting the traversal direction once and starting from the first octet
brought that call to 82. At `-O3` GCC already unrolls the baseline standalone
field operations. We retained the shared codec for its gains across all 96
complete-record cases at both levels and all 64 standalone cases at `-O2`.
For example, writing a 513-byte payload with an eight-byte big-endian length
took 361 → 291 instructions at `-O2` and 313 → 282 at `-O3`. These counts
describe this build. Elapsed time and throughput require separate measurements.

## Validation in measurement, reads and writes

The diagnostic probe runs one-shot reads and writes and their cursor
equivalents on the same seven-byte record, `12 34 00 03 AA BB CC`. The
reader sees either the full record or its first three bytes, ending inside
the two-byte length. The writer gets seven or three bytes of output capacity.
Each cursor starts at offset zero. The probe checks
return codes, counts, borrowed payload pointers, cursor positions and output
bytes outside the measured calls. [Diagnostic probe](probe.cpp)

The table compares baseline `957341c69a76` with the private validation and
length codec helpers at `011519122050`. Counts include nested calls and the
return instruction.
C IO counts start at the public `_with_diagnostic` entry point. C++ counts
include the caller of its inline wrapper. Two runs on the x86-64 toolchain
described below gave identical results.

| Entry and operation | Null, complete | Null, short | Enabled, complete | Enabled, short |
| --- | ---: | ---: | ---: | ---: |
| C read | 148 → 113 | 67 → 71 | 153 → 117 | 78 → 82 |
| C next | 180 → 134 | 99 → 92 | 185 → 138 | 114 → 107 |
| C write | 246 → 184 | 118 → 95 | 251 → 188 | 123 → 100 |
| C append | 277 → 206 | 149 → 117 | 282 → 210 | 158 → 126 |
| C++ read | 189 → 154 | 100 → 104 | 200 → 164 | 117 → 121 |
| C++ next | 208 → 162 | 119 → 112 | 218 → 171 | 139 → 132 |
| C++ write | 278 → 216 | 150 → 127 | 290 → 227 | 162 → 139 |
| C++ append | 298 → 227 | 170 → 138 | 309 → 237 | 185 → 153 |

- **Measurement.** The first complete write calls `tlv_fixed_element_size`
  for a two-byte tag, a two-byte length and a three-byte value. That call
  dropped from **73 to 45 instructions**. The element already contains its
  value descriptor. Checking its data pointer and logical size directly avoids
  rechecking the descriptor address and writing a native-size temporary.
  Native representability still precedes wire fit and header arithmetic.
- **Read.** The call to `tlv_fixed_length_read` repeated the width, byte-order
  and buffer checks. Header measurement validates the layout and checks for
  native overflow. Once the entire header is available, the private decoder
  can read exactly the selected width. The reader still compares the decoded
  64-bit value against the remaining bytes before narrowing it or committing
  the view. A short header returns before decoding. The one-shot short path
  costs four more instructions in this build to forward arguments to the
  shared reader.
- **Write.** The call to `tlv_fixed_length_write` rechecked the field settings,
  wire fit and space for the length. Element measurement validates the tag,
  value, wire fit and total size before the capacity check for the whole
  element. The private encoder therefore needs only the
  field, length and destination. A short write reports the full requirement
  before the first destination byte is touched.
- **Cursors.** After checking their buffer and position, cursor operations
  pass the remaining slice to the same private read or write routine used by
  one-shot operations. Each operation still validates its format. Cursor
  advancement and stream-relative diagnostic offsets use the same commit and
  error paths.

## Diagnostic paths

The C reader checks the validated header size before decoding a length. For an
incomplete header it returns truncation immediately when the diagnostic pointer
is null. The GCC disassembly places the tag-width comparison, field selection
and available-length subtraction behind that null check. The writer measures
the required size in both modes because the short-buffer contract returns it.
All diagnostic stores, count conversions and cursor offset adjustments are
conditional on a supplied record.

The C++ wrappers create a native diagnostic only in the enabled branch. The C
entry point assigns every field on every return, so the wrapper can convert
it directly. In the measured build this removes two 16-byte stores per call
that cleared a temporary before testing the pointer. Runtime-selected modes
still require a pointer test and may share a stack frame sized for the enabled
path. The public C functions that omit the diagnostic forward a null pointer,
and the compiler can inline that forwarding step. C++ calls the diagnostic
entry point directly with either `nullptr` or the address of its native record.

### Diagnostic measurements

Each cell gives the executed instruction count at baseline `911da4969dfd`
and at `957341c69a76` with the diagnostic changes. C counts start at the public
`_with_diagnostic` function. C++ counts start at the caller of the inline
wrapper and include that same C path. Both include nested calls through the
return instruction. The caller supplies a runtime-selected diagnostic pointer.

| Entry and operation | Null, complete | Null, short | Enabled, complete | Enabled, short |
| --- | ---: | ---: | ---: | ---: |
| C read | 150 → 148 | 73 → 67 | 155 → 153 | 79 → 78 |
| C next | 182 → 180 | 107 → 99 | 187 → 185 | 115 → 114 |
| C write | 246 → 246 | 118 → 118 | 251 → 251 | 123 → 123 |
| C append | 277 → 277 | 151 → 149 | 282 → 282 | 158 → 158 |
| C++ read | 192 → 189 | 107 → 100 | 205 → 200 | 120 → 117 |
| C++ next | 211 → 208 | 128 → 119 | 223 → 218 | 142 → 139 |
| C++ write | 281 → 278 | 153 → 150 | 293 → 290 | 165 → 162 |
| C++ append | 301 → 298 | 175 → 170 | 312 → 309 | 188 → 185 |

Two runs produced identical counts on Windows 10 x64 build 19045 with
WinLibs GCC/G++ 13.3.0 r1 (`x86_64-w64-mingw32`, MSVCRT, POSIX, SEH),
`-O2 -g`, separate translation units, and GDB 18.1. Successful writes
include the C runtime's `memcpy` instructions. Compiler, ABI, optimization
and runtime changes can affect these totals.

## Reproduce

Run the setup below in Bash from the repository root with GCC, G++,
Python-enabled GDB and GNU objdump on `PATH`. Each comparison builds the
same probe against both sets of library sources in separate translation units.
The [GDB script](count.gdb) single-steps each x86-64 binary.

```bash
set -euo pipefail
work=$(mktemp -d)

build_probe() {
    local version=$1 build=$2 level=$3 probe=$4 src=. file
    mkdir "$build"
    if [ "$version" != current ]; then
        src="$build/source"
        mkdir "$src"
        git archive "$version" include src | tar -x -C "$src"
    fi
    for file in "$src"/src/core/*.c "$src"/src/io/{reader,writer}/*.c; do
        gcc -std=c99 "$level" -g -I"$src/include" -c "$file" -o "$build/$(basename "${file%.c}").o"
    done
    if [ -f "tests/diagnostics/$probe.c" ]; then
        gcc -std=c99 "$level" -g -I"$src/include" -c "tests/diagnostics/$probe.c" -o "$build/${probe}.o"
    fi
    g++ -std=c++17 "$level" -g -I"$src/include" "tests/diagnostics/$probe.cpp" "$build"/*.o -o "$build/probe.exe"
}
```

### C99 and C++17 wrapper comparison

The same fixture runs against baseline `2ea1b98d785a` and the working
tree. Each count file has 13 matched C and C++ pairs. The final command
shows instruction counts before and after the wrapper changes for each
caller, operation and case.

```bash
for level in O2 O3; do
    for version in 2ea1b98d785a current; do
        build="$work/wrappers-$version-$level"
        build_probe "$version" "$build" "-$level" wrapper_probe
        for run in 1 2; do
            log="$build/run-$run"
            gdb -nx -batch -ex 'set $wrappers=1' -x tests/diagnostics/count.gdb "$build/probe.exe" > "$log.txt"
            awk '/^wrapper / { sub(/\r$/, ""); print }' "$log.txt" > "$log.counts"
            test "$(wc -l < "$log.counts")" -eq 26
        done
        cmp "$build/run-1.counts" "$build/run-2.counts"
        objdump -d -M intel "$build/probe.exe" > "$build/assembly.txt"
    done
    echo "$level (baseline, current)"
    paste "$work/wrappers-2ea1b98d785a-$level/run-1.counts" "$work/wrappers-current-$level/run-1.counts" |
        awk '{print $2, $3, $4, $5, $10}'
done
```

Inspect external references with GNU nm and the result-classification
caller with exceptions disabled using the same sources.

```bash
build="$work/wrappers-current-O2"
g++ -std=c++17 -O2 -g -Iinclude -c tests/diagnostics/wrapper_probe.cpp -o "$work/typed.o"
nm -C -u "$build"/*.o "$work/typed.o"
g++ -std=c++17 -O2 -fno-exceptions -Iinclude -S tests/diagnostics/wrapper_probe.cpp -o "$work/no-exceptions.s"
grep -A4 '^cpp_category:' "$work/no-exceptions.s"
```

### Fixed-field counts

Run both optimization levels twice. Each output row identifies the field
offset or record payload size, followed by the operation and the instruction
counts for baseline `011519122050` and the working tree.

```bash
for level in O2 O3; do
    for version in 011519122050 current; do
        build="$work/fields-$version-$level"
        build_probe "$version" "$build" "-$level" field_probe
        for run in 1 2; do
            log="$build/run-$run"
            gdb -nx -batch -ex 'set $fields=1' -x tests/diagnostics/count.gdb "$build/probe.exe" > "$log.txt"
            awk '/^(field|record) / { sub(/\r$/, ""); print }' "$log.txt" > "$log.counts"
            test "$(wc -l < "$log.counts")" -eq 160
        done
        cmp "$build/run-1.counts" "$build/run-2.counts"
    done
    echo "$level (baseline, current)"
    paste "$work/fields-011519122050-$level/run-1.counts" "$work/fields-current-$level/run-1.counts" |
        awk '{print $1, $2, $3, $4, $5, $6, $12}'
done
```

### Validation and diagnostic counts

```bash
for version in 911da4969dfd 957341c69a76 011519122050; do
    build="$work/diagnostics-$version"
    build_probe "$version" "$build" -O2 probe
    gdb -nx -batch -ex 'set $measure=1' -x tests/diagnostics/count.gdb "$build/probe.exe" > "$build/measure.txt"
    echo "$version (Core)"
    grep '^measure ' "$build/measure.txt"
    for native in 0 1; do
        log="$build/trace-$native.txt"
        gdb -nx -batch -ex "set \$native=$native" -x tests/diagnostics/count.gdb "$build/probe.exe" > "$log"
        echo "$version (native=$native)"
        grep -E '^(null|enabled) ' "$log"
    done
    objdump -d -M intel "$build/probe.exe" > "$build/assembly.txt"
done
```

Compare `probe_read`, `probe_next`, `probe_write`, `probe_append` and their C
callees in the assembly files. In the readers, stores to the temporary
native *element* serve the result view and occupy different stack slots from
the optional diagnostic.
