// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Marek Cingel

#include "opentlv/builtin/bluetooth/company_ids.h"

static const uint8_t company_identifiers[][2] = {
    {0x00, 0x00}, {0x06, 0x00}, {0x4C, 0x00}, {0x59, 0x00}, {0x75, 0x00}, {0xE0, 0x00},
};

static const tlv_definition_t company_definitions[] = {
    {{company_identifiers[0], 2}, "Ericsson AB"},
    {{company_identifiers[1], 2}, "Microsoft"},
    {{company_identifiers[2], 2}, "Apple, Inc."},
    {{company_identifiers[3], 2}, "Nordic Semiconductor ASA"},
    {{company_identifiers[4], 2}, "Samsung Electronics Co. Ltd."},
    {{company_identifiers[5], 2}, "Google"},
};

const tlv_definition_registry_t tlv_bluetooth_company_ids = {
    company_definitions, sizeof(company_definitions) / sizeof(company_definitions[0])};
