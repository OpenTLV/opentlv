# Fixed TLV diagnostic instruction counts

The [probe](probe.cpp) runs one-shot reads and writes and their cursor
equivalents on the same seven-byte record, `12 34 00 03 AA BB CC`. The
reader sees either the full record or its first three bytes, ending inside
the two-byte length. The writer gets seven or three bytes of output capacity.
Each cursor starts at offset zero. The probe checks
return codes, counts, borrowed payload pointers, cursor positions and output
bytes outside the measured calls.

## Optimized paths

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

## Measurements

Each cell gives the executed instruction count at baseline `911da4969dfd`
and with the diagnostic changes. C counts start at the public
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

Run in Bash from the repository root with the selected GCC, G++, Python-enabled
GDB, and GNU objdump on `PATH`. The same [GDB script](count.gdb) single-steps
both binaries on x86-64. The baseline build uses the current probe with the
baseline library sources and headers.

```bash
set -eu
work=$(mktemp -d)
mkdir "$work/base"
git archive 911da4969dfd include src | tar -x -C "$work/base"
for version in base current; do
    src=.
    if [ "$version" = base ]; then src="$work/base"; fi
    build="$work/$version-obj"
    mkdir "$build"
    for file in "$src"/src/core/*.c "$src"/src/io/{reader,writer}/*.c; do
        gcc -std=c99 -O2 -g -I"$src/include" -c "$file" -o "$build/$(basename "${file%.c}").o"
    done
    g++ -std=c++17 -O2 -g -I"$src/include" tests/diagnostics/probe.cpp "$build"/*.o -o "$build/probe.exe"
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
callees in the two assembly files. In the readers, stores to the temporary
native *element* serve the result view and occupy different stack slots from
the optional diagnostic.
