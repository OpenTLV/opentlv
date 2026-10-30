// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Marek Cingel
#include "tlv/query/features.h"
/* Query features */
int tlv_config_query_frontend(void) {
    return OPENTLV_QUERY_FRONTEND;
}

int tlv_config_query_set_operations(void) {
    return OPENTLV_QUERY_SET_OPERATIONS;
}
