/* v0.1.0 IO C ABI. Define OPENTLV_ABI_FUNCTION before including this list. */
OPENTLV_ABI_FUNCTION(tlv_result_t, tlv_read,
                     (const uint8_t *, size_t, const tlv_fixed_format_t *, tlv_element_t *,
                      size_t *))
OPENTLV_ABI_FUNCTION(tlv_result_t, tlv_read_with_diagnostic,
                     (const uint8_t *, size_t, const tlv_fixed_format_t *, tlv_element_t *,
                      size_t *, tlv_diagnostic_t *))
OPENTLV_ABI_FUNCTION(tlv_result_t, tlv_reader_init,
                     (tlv_reader_t *, const uint8_t *, size_t, const tlv_fixed_format_t *))
OPENTLV_ABI_FUNCTION(tlv_result_t, tlv_reader_next, (tlv_reader_t *, tlv_element_t *))
OPENTLV_ABI_FUNCTION(tlv_result_t, tlv_reader_next_with_diagnostic,
                     (tlv_reader_t *, tlv_element_t *, tlv_diagnostic_t *))
OPENTLV_ABI_FUNCTION(tlv_result_t, tlv_write,
                     (uint8_t *, size_t, const tlv_fixed_format_t *, const tlv_element_t *,
                      size_t *))
OPENTLV_ABI_FUNCTION(tlv_result_t, tlv_write_with_diagnostic,
                     (uint8_t *, size_t, const tlv_fixed_format_t *, const tlv_element_t *,
                      size_t *, tlv_diagnostic_t *))
OPENTLV_ABI_FUNCTION(tlv_result_t, tlv_writer_init,
                     (tlv_writer_t *, uint8_t *, size_t, const tlv_fixed_format_t *))
OPENTLV_ABI_FUNCTION(tlv_result_t, tlv_writer_write, (tlv_writer_t *, const tlv_element_t *))
OPENTLV_ABI_FUNCTION(tlv_result_t, tlv_writer_write_with_diagnostic,
                     (tlv_writer_t *, const tlv_element_t *, tlv_diagnostic_t *))
