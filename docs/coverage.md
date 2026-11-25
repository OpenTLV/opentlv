# Production coverage gates

CI measures Core and IO on Ubuntu 24.04 x86-64 with GCC, G++, and gcov
13.3.0. It builds the production libraries at `-O0` with coverage counters.
The Core, IO, and integration GoogleTest suites link those libraries. CTest
runs the cases sequentially against one set of counters. The gate requires
every production `.c` file under
`src/`, including newly added files, plus `src/io/internal/diagnostic.h`.
A missing source or an additional instrumented header under `src/` fails the
scope check. Test code and GoogleTest are outside the measured sources.

Each compiled copy of a repeated source contributes its own executable lines
and branch outcomes to the per-file totals. For example, both the reader and
writer copies of the diagnostic header must exercise both outcomes of its
null check. Missed locations in repeated sources include the object profile
path so the report identifies which copy needs attention.

The gate requires **100% of reachable executable lines, 100% of reachable
branch outcomes, and 100% of public functions mapped to passing behavioral
tests**. The report lists raw per-file and total counts and identifies every
missed line and branch by location. A new gap fails the run. An exclusion
that disappears also fails and requires review. CI compares the reports from
two fresh builds byte for byte and publishes them in the `coverage-reports`
artifact. See the [commands for reproducing the report](testing.md#coverage).

## Reviewed gaps

Branch indices below are zero-based within the source line in gcov JSON.

| File | Uncovered location | Reason |
| --- | --- | --- |
| `src/io/reader/reader.c` | Line 42, branch 0 and line 43 | The length reader is called with validated field settings, valid output pointers, and the entire header available. Its defensive error return cannot be reached with that input. |
| `src/io/writer/writer.c` | Line 37, branch 0 and line 38 | Element measurement already validated the length field and payload size, and the capacity check reserved the entire element. The length writer's error return cannot be reached. |
| `src/io/reader/reader.c` | Line 79, branch 6 | A successfully initialized reader cannot have a null buffer and positive size. The public contract requires callers to treat cursor fields as read-only. |
| `src/io/writer/writer.c` | Line 71, branch 4 | Writer initialization rejects a null buffer with positive capacity. Reaching this outcome would require violating the initialized cursor contract. |

The native-size conversion check in `src/core/size.c` is compiled only when
`SIZE_MAX < TLV_SIZE_MAX`. On x86-64 it contributes zero executable lines
or branches. On 32-bit targets the conditional Core cases check rejection of
the first unrepresentable count and preservation of the caller's output.

## Public API behavior

Each row names one representative CTest case. The gate compares the table
with the public header declarations, checks that each function executed, and
requires the named case to pass in the same coverage run. Function execution
counts cover the entire suite. Reviewers verify that the named test exercises
the mapped function and asserts its expected behavior.

| Public function | Behavioral test |
| --- | --- |
| `tlv_size_to_native` | `CoreSize.ConvertsRepresentableCountsToNative` |
| `tlv_size_add` | `CoreSize.ReportsOverflowAndPreservesOutput` |
| `tlv_value_init` | `CoreValue.AShallowCopyKeepsBorrowingTheOriginalBytes` |
| `tlv_value_validate` | `CoreValue.RejectsMissingDataAndPreservesTheDestination` |
| `tlv_result_category` | `CoreResult.ClassifiesAndDescribesEveryNamedResult` |
| `tlv_result_string` | `CoreResult.ClassifiesAndDescribesEveryNamedResult` |
| `tlv_fixed_length_read` | `WireLayouts/FixedLength.ReadsExactlyOneField/BigEndian1` |
| `tlv_fixed_length_write` | `WireLayouts/FixedLength.MeasuresAndWritesExactlyOneField/LittleEndian3` |
| `tlv_fixed_header_size` | `FixedMeasurement.MeasuresAnExactNativeHeaderAndRejectsTheNextWidth` |
| `tlv_fixed_element_size` | `FixedElementSize.RejectsMismatchedTagWidths` |
| `tlv_read` | `WireLayouts/ReaderLayout.BorrowsOnlyTheFirstElement/BigEndian1` |
| `tlv_read_with_diagnostic` | `ReadDiagnostic.LocatesEachIncompleteFieldFromTheStartOfTheSuppliedBuffer` |
| `tlv_reader_init` | `ReaderArguments.InvalidInitializationPreservesTheCurrentStream` |
| `tlv_reader_next` | `ReaderStream.CopiesTheFormatAndAdvancesIndependentCursors` |
| `tlv_reader_next_with_diagnostic` | `ReadDiagnostic.LocatesEachIncompleteFieldFromTheStartOfTheSuppliedBuffer` |
| `tlv_write` | `IOWrite.EncodesTheTagLengthAndPayloadForEveryLayout` |
| `tlv_write_with_diagnostic` | `WriteDiagnostic.ReportsTheWholeElementAndTheCapacityOfTheSuppliedSlice` |
| `tlv_writer_init` | `IOWriter.RetriesAfterShortAppendAndKeepsItsFormatSnapshot` |
| `tlv_writer_write` | `IOWriter.RetriesAfterShortAppendAndKeepsItsFormatSnapshot` |
| `tlv_writer_write_with_diagnostic` | `WriteDiagnostic.ReportsCommittedOffsetAndRemainingSpaceForAShortAppend` |

The Core suite also exercises the inline C++17 interfaces. Compile-time
assertions check view type distinctions, result code conversions and the
`noexcept` contract. The coverage report measures the underlying C functions.

| C++ Core operations | Behavioral test |
| --- | --- |
| `result_category`, `result_string` and result conversions | `CoreResult.ClassifiesAndDescribesEveryNamedResult`, `CoreResult.ReturnsTheFallbackForUnassignedCodes` |
| `to_c`, `from_c` for diagnostics | `CppCore.RoundTripsDiagnosticFieldsAndFullWidthCounts` |
| `to_c`, `from_c` for borrowed views | `CppCore.CopiedDescriptorsKeepBorrowingTheOriginalStorage` |
| `make_value_view`, `validate` | `CppCore.ValidatesBorrowedValuesAndPreservesTheDestinationOnFailure` |
| `size_add`, `size_to_native` | `CppCore.ChecksLogicalAndNativeSizeLimits` |
| `to_c`, `from_c` for format settings and byte order, and `header_size` | `CppFixed.ConvertsConfigurationByValueAndValidatesHeaderSize` |
| `from_c` and `to_c` for unknown and unnamed byte orders | `CppFixed.ConvertsUnknownByteOrdersBetweenCAndCpp` |
| `element_size` | `CppFixed.MeasuresElementsAndPreservesSizeOnError` |
| `read_length`, `write_length` | `CppFixed.ReadsAndWritesExactlyTheSelectedFieldInBothOrders`, `CppFixed.PreservesFieldOutputsAndReportsTruncation` |

| C++ IO operations | Behavioral test |
| --- | --- |
| `write` results, diagnostics and output state on failure | `CppWriter.OneShotFailuresMatchCResultsAndDiagnostics` |
| `read` with and without diagnostics | `CppReader.OneShotMatchesCAtEveryPrefix` |
| `reader::init`, `reader::format` and reinitialization | `CppReader.PreservesTheCursorOnInvalidInitialization`, `CppReader.CopiesCursorsAndBorrowsTheInput` |
| `reader::next`, `reader::position` and cursor copies | `CppReader.CopiesCursorsAndBorrowsTheInput`, `CppReader.ReportsStreamOffsetsAndPreservesStateOnFailure` |
