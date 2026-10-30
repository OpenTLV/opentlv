// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Marek Cingel

#include "tlv/diagnostic/cause.h"
#include <string.h>

void tlv_diagnostic_causes_init(tlv_diagnostic_causes_t* causes) {
    if (causes) memset(causes, 0, sizeof *causes);
}

tlv_result_t tlv_diagnostic_causes_append(tlv_diagnostic_causes_t* causes,
                                          const tlv_diagnostic_domain_t* domain, uint16_t type,
                                          const void* detail, size_t size) {
    if (!causes || !domain || !domain->name || !detail) return TLV_ERR_NULL_ARG;
    if (!domain->id || !domain->name[0] || !type || !size || size > TLV_DIAGNOSTIC_DETAIL_MAX ||
        causes->count > TLV_DIAGNOSTIC_CAUSES_MAX)
        return TLV_ERR_INVALID_ARG;
    if (causes->count == TLV_DIAGNOSTIC_CAUSES_MAX || causes->omitted) {
        if (causes->omitted != SIZE_MAX) ++causes->omitted;
        return TLV_ERR_BUFFER_TOO_SHORT;
    }
    tlv_diagnostic_cause_t* frame = &causes->frames[causes->count++];
    memset(frame, 0, sizeof *frame);
    frame->domain = domain;
    frame->type = type;
    frame->size = size;
    memcpy(frame->payload.bytes, detail, size);
    return TLV_OK;
}

tlv_result_t tlv_diagnostic_cause_copy(const tlv_diagnostic_cause_t* cause,
                                       const tlv_diagnostic_domain_t* domain, uint16_t type,
                                       void* detail, size_t size) {
    if (!cause || !domain || !domain->name || !detail) return TLV_ERR_NULL_ARG;
    if (!cause->domain || !cause->domain->name || !domain->id || !domain->name[0] || !type ||
        !size || size > TLV_DIAGNOSTIC_DETAIL_MAX || cause->size != size ||
        cause->domain->id != domain->id || strcmp(cause->domain->name, domain->name) ||
        cause->type != type)
        return TLV_ERR_INVALID_ARG;
    memcpy(detail, cause->payload.bytes, size);
    return TLV_OK;
}
