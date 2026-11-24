# Fixed TLV fuzz tests

From the repository root, build the standalone test project with Clang 18,
Clang++, compiler-rt (libFuzzer, ASan and UBSan), CMake, Ninja and Python 3.
The production C sources and both harnesses receive sanitizer and coverage
instrumentation. Only the two fuzz executables link the libFuzzer driver.

```sh
CC=clang-18 CXX=clang++-18 cmake -S tests/fuzz -B build-fuzz -G Ninja -DCMAKE_BUILD_TYPE=RelWithDebInfo
cmake --build build-fuzz
python3 tests/fuzz/run.py build-fuzz fuzz-failures
```

The runner first replays each checked-in seed. It then gives each target a
fresh writable corpus, a fixed random seed of 31 and a 30-second mutation
budget. Inputs are limited to 1,024 bytes, each invocation has a five-second
timeout, and the process RSS limit is 512 MiB. The harnesses also enforce
the input bound when a file is supplied directly. Neither harness allocates
memory based on a wire length.

## Seed corpus

Files in `corpus/reader` and `corpus/writer` are raw bytes. The examples
are literal test vectors independent of the production encoder.

| Target | Input layout | Checked-in examples |
| --- | --- | --- |
| Reader | Tag width, length width, order selector modulo 3 (big, little, unknown), then wire bytes. Zero tag width and length widths outside 1–8 exercise format rejection. | `big-stream` contains adjacent records, `little-two` uses a two-byte little-endian length, `partial-value` ends inside a value, `empty-stream` has no record, and `invalid-width` has a zero-length field width. |
| Writer | Two width selectors (1 + byte modulo 8), a format selector, a capacity selector, tag bytes, then the value. | `exact`, `short`, `spare`, and `empty` cover an exact write, a failed append, unused capacity, and a zero-length value with an eight-byte length field. `large-spare` appends a 256-byte value with 1,053 bytes available. |

For the writer, bit 0 of the format selector chooses big (0) or little (1)
endian. After the first record, the writer tries an invalid format. Values
0 through 3 in bits 1–2 select zero tag width, zero length width, length
width 9, or unknown byte order, respectively.
Capacity selector 0 gives an exact fit for the next record
and 1 gives one byte too few. Selectors 2 through 255 scale from zero to
all remaining space in the test buffer.

The reader compares cursor and one-shot results to an independent length
decoder, checks borrowed pointers and consumed bytes, and re-encodes every
complete record. On truncation or end it checks the cursor and output
sentinels. The writer appends after a committed empty record and checks
the entire destination against independently constructed bytes. It checks
that a rejected reinitialization preserves the writer and its buffer, and
that a one-shot write with the same invalid format preserves the byte count
and the buffer. It then appends using the original format and checks
the count on short writes, state preservation on errors, and decoding back
to the original tag and value on success.

## Replay and regressions

The runner saves libFuzzer failure inputs under `fuzz-failures/reader` or
`fuzz-failures/writer`. A failed CI run uploads available inputs in the
`fixed-tlv-fuzz-failures` artifact. To replay one case, pass the file to the
same executable built from the same revision and toolchain, for example:

```sh
build-fuzz/fuzz_reader tests/fuzz/corpus/reader/partial-value
```

For a confirmed regression, fix or track the finding and keep the smallest
reviewed input that demonstrates the contract in the corresponding seed
directory. The next CI run replays it before mutation.
