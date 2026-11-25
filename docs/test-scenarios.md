# Behavioral scenario inventory

Use the named suites below to find the assertions for each Fixed TLV
contract. Parameterized cases run for length widths 1 through 8 in both
byte orders. The [testing guide](testing.md) shows how to select a suite,
an individual case, or a single wire layout with CTest.

| Core scenario | Where it is checked |
| --- | --- |
| Convert zero and the largest representable count to `size_t`. On x86, reject the first larger count and `UINT64_MAX`. Add through the 64-bit limit, reject overflow and preserve the output on failure. | `CoreSize` in [size_test.cpp](../tests/core/size_test.cpp) |
| Copy a borrowed value, keep the supplied pointer for empty views, reject missing storage and preserve the destination on error, including an unrepresentable size on x86. | `CoreValue` in [value_test.cpp](../tests/core/value_test.cpp) |
| Read and write zero, a patterned value and the field maximum at unaligned addresses. Check literal bytes, consumed and written counts, surrounding bytes and sizing queries. Exercise every short prefix and reject an unencodable value. | `FixedLength` in [fixed_length_test.cpp](../tests/core/fixed_length_test.cpp) |
| Reject zero or overwide length fields, unknown byte order and missing arguments. Distinguish empty input from a null pointer with positive size. | `FixedLengthArguments` in [fixed_length_test.cpp](../tests/core/fixed_length_test.cpp) |
| Measure opaque tags, empty and nonempty payloads, and a header or complete element of exactly `SIZE_MAX`. Reject overflow, invalid formats, mismatched tag widths, missing data, and values beyond the wire or native limit. | `FixedMeasurement` and `FixedElementSize` in [fixed_measure_test.cpp](../tests/core/fixed_measure_test.cpp) |
| Compare C and C++ categories and messages for every named result and for unassigned codes. Round-trip a diagnostic with full-width counts. | `CoreResult` in [result_test.cpp](../tests/core/result_test.cpp) and `CppCore.RoundTripsDiagnosticFieldsAndFullWidthCounts` in [cpp_core_test.cpp](../tests/core/cpp_core_test.cpp) |
| Convert borrowed descriptors between C and C++17, keep views alive after the source descriptor goes out of scope, and compare size limits, format validation, wire bytes and failure outputs across both interfaces. | `CppCore` in [cpp_core_test.cpp](../tests/core/cpp_core_test.cpp) and `CppFixed` in [cpp_fixed_test.cpp](../tests/core/cpp_fixed_test.cpp) |

| IO and integration scenario | Where it is checked |
| --- | --- |
| Compare C++ one-shot and cursor reads with C. Verify borrowed views, independent cursor copies and preserved outputs on end, truncation and invalid initialization. | `CppReader` in [cpp_reader_test.cpp](../tests/io/cpp_reader_test.cpp) |
| Read exactly one element with trailing bytes, borrow its tag and value, and preserve the view, count and cursor on each incomplete prefix or end of input. | `ReaderLayout` in [reader_test.cpp](../tests/io/reader_test.cpp) |
| Iterate adjacent records with opaque and empty payloads. Keep earlier views valid, copy the format on initialization, advance copied cursors independently and restart using the stored format. | `ReaderStream` in [reader_test.cpp](../tests/io/reader_test.cpp) |
| Reject invalid arguments, formats, positions and failed reinitializations. Compare large wire lengths against remaining bytes before native conversion, including 2³², 2³² + 1 and 2⁶⁴ − 1. | `ReaderArguments` and `ReaderBounds` in [reader_test.cpp](../tests/io/reader_test.cpp), and `ReadDiagnostic.ReportsTheFullWireLengthWhenItExceedsTheRemainingInput` in [diagnostic_test.cpp](../tests/io/diagnostic_test.cpp) |
| Encode the tag, length and payload against expected bytes. Report the required capacity for a size query or short write, retry at exact fit and preserve surrounding bytes. | `IOWrite` in [writer_test.cpp](../tests/io/writer_test.cpp) |
| Append with a saved format, retry a short append and borrow input from a committed prefix. Reject a 256-byte value for a one-byte length, then append 255 bytes. Check cursor, count and destination on descriptor, format, range and capacity errors. | `IOWriter` and `IOWrite.SizingQueriesValidateWireAndNativeLimits` in [writer_test.cpp](../tests/io/writer_test.cpp) |
| Report the truncated tag, length or value with required and available counts. Check offsets from a supplied slice and from the start of a stream. Accept every one-byte tag and length value. Clear stale diagnostic fields on success, end and argument errors. | `ReadDiagnostic` in [diagnostic_test.cpp](../tests/io/diagnostic_test.cpp) |
| Report a short output against the whole element, including the offset after an append. Check invalid descriptors, wire and native size errors, cleared details and calls with an omitted diagnostic. | `WriteDiagnostic` in [diagnostic_test.cpp](../tests/io/diagnostic_test.cpp) |
| Check encoder and decoder independently against golden bytes. Round-trip bounded binary values, check borrowed pointers and consume only complete records when a stream ends partway through its final record. | `FixedRoundTrip` in [fixed_round_trip_test.cpp](../tests/integration/fixed_round_trip_test.cpp) |

The [public-header check](../tests/check_public_api.py) compiles headers in
isolation and checks compiler diagnostics. The [installed-package
check](../tests/check_cmake_package.py) builds and runs a C99 Core consumer,
a C++17 IO consumer and the examples after relocating each static or shared
install. These exercises cover language compatibility, transitive linkage,
and the installed target interfaces.

The [production coverage gate](coverage.md) checks the executed code
alongside this inventory. It measures each production translation unit and
requires a passing behavioral test for every public function.
