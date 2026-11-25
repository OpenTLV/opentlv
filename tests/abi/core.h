/* v0.1.0 Core C ABI. Define OPENTLV_ABI_FUNCTION before including this list. */
OPENTLV_ABI_FUNCTION(tlv_result_t, tlv_fixed_element_size,
                     (const tlv_fixed_format_t *, const tlv_element_t *, size_t *))
OPENTLV_ABI_FUNCTION(tlv_result_t, tlv_fixed_header_size,
                     (const tlv_fixed_format_t *, size_t *))
OPENTLV_ABI_FUNCTION(tlv_result_t, tlv_fixed_length_read,
                     (const tlv_fixed_length_t *, const uint8_t *, size_t, tlv_size_t *, size_t *))
OPENTLV_ABI_FUNCTION(tlv_result_t, tlv_fixed_length_write,
                     (const tlv_fixed_length_t *, tlv_size_t, uint8_t *, size_t, size_t *))
OPENTLV_ABI_FUNCTION(tlv_result_category_t, tlv_result_category, (tlv_result_t))
OPENTLV_ABI_FUNCTION(const char *, tlv_result_string, (tlv_result_t))
OPENTLV_ABI_FUNCTION(tlv_result_t, tlv_size_add, (tlv_size_t, tlv_size_t, tlv_size_t *))
OPENTLV_ABI_FUNCTION(tlv_result_t, tlv_size_to_native, (tlv_size_t, size_t *))
OPENTLV_ABI_FUNCTION(tlv_result_t, tlv_value_init, (const uint8_t *, tlv_size_t, tlv_value_t *))
OPENTLV_ABI_FUNCTION(tlv_result_t, tlv_value_validate, (const tlv_value_t *))
