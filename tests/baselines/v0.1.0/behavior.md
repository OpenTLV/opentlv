# v0.1.0 Fixed TLV behavior

The release vectors supply literal bytes for all 16
combinations of length width (1–8) and byte order. With a two-byte length,
tag `00 A7` and payload `00 FF 80`, the big-endian record is
`00 A7 00 03 00 FF 80` and the little-endian record is `00 A7 03 00 00 FF 80`.
The writer emits seven bytes. The reader consumes seven and returns views
into the input at offsets 0 and 4. The C and C++17 stream tests also exercise
an empty record before the binary record at every cut of that second record.
[Release vectors](fixed-behavior.hpp) · [Wire comparisons](../../integration/fixed_round_trip_test.cpp)

| Operation | v0.1.0 result | Executable comparison |
| --- | --- | --- |
| Convert 2³² or `UINT64_MAX` to a native count | x86 returns `TLV_ERR_NATIVE_SIZE` and preserves the output. x64 returns `TLV_OK` and the exact count. | [CoreSize](../../core/size_test.cpp) |
| Read an incomplete fixed-length field | Return `TLV_ERR_TRUNCATED`, report the available prefix in `consumed` and preserve the decoded count. An eight-byte field of `FF` decodes to `UINT64_MAX` on both architectures. | [FixedLength](../../core/fixed_length_test.cpp) |
| Write a one-byte length | 255 encodes as `FF`. A payload of 256 returns `TLV_ERR_INVALID_LENGTH` and preserves the destination and count. | [FixedLength](../../core/fixed_length_test.cpp), [IOWriter](../../io/writer_test.cpp) |
| Read a short record or reach the end of a stream | A nonempty incomplete record returns `TLV_ERR_TRUNCATED`. A read at the end of a complete record stream returns `TLV_END`. Both preserve the previous view, consumed count and cursor. | [ReaderLayout and ReaderStream](../../io/reader_test.cpp), [CppReader](../../io/cpp_reader_test.cpp) |
| Report a truncated field | In the release stream, a seven-byte prefix of the second record has two of three value bytes. The diagnostic reports `TLV_FIELD_VALUE`, offset 5 in a one-shot read and 11 from the stream start, required 3 and available 2. | [ReadDiagnostic](../../io/diagnostic_test.cpp) |
| Retry a short write | Writing `D7 02 00 FF` into three bytes returns `TLV_ERR_BUFFER_TOO_SHORT` and sets `written` to 4. The destination retains its bytes. At capacity 4 the retry writes the exact record. On a short append, the cursor retains its committed offset and the diagnostic reports capacity from that offset. | [WriteDiagnostic](../../io/diagnostic_test.cpp), [CppWriter](../../io/cpp_writer_test.cpp) |
