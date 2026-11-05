// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Marek Cingel

#include "opentlv/builtin/dhcp/options.h"

static const uint8_t dhcp_option_identifiers[] = {0,  1,  3,  6,  12, 15, 50, 51, 53,
                                                  54, 55, 57, 58, 59, 60, 61, 255};

static const tlv_definition_t dhcp_option_definitions[] = {
    {{dhcp_option_identifiers + 0, 1}, "Pad"},
    {{dhcp_option_identifiers + 1, 1}, "Subnet Mask"},
    {{dhcp_option_identifiers + 2, 1}, "Router"},
    {{dhcp_option_identifiers + 3, 1}, "Domain Name Server"},
    {{dhcp_option_identifiers + 4, 1}, "Host Name"},
    {{dhcp_option_identifiers + 5, 1}, "Domain Name"},
    {{dhcp_option_identifiers + 6, 1}, "Requested IP Address"},
    {{dhcp_option_identifiers + 7, 1}, "IP Address Lease Time"},
    {{dhcp_option_identifiers + 8, 1}, "DHCP Message Type"},
    {{dhcp_option_identifiers + 9, 1}, "Server Identifier"},
    {{dhcp_option_identifiers + 10, 1}, "Parameter Request List"},
    {{dhcp_option_identifiers + 11, 1}, "Maximum DHCP Message Size"},
    {{dhcp_option_identifiers + 12, 1}, "Renewal Time Value"},
    {{dhcp_option_identifiers + 13, 1}, "Rebinding Time Value"},
    {{dhcp_option_identifiers + 14, 1}, "Vendor Class Identifier"},
    {{dhcp_option_identifiers + 15, 1}, "Client Identifier"},
    {{dhcp_option_identifiers + 16, 1}, "End"},
};

const tlv_definition_registry_t tlv_dhcpv4_options = {
    dhcp_option_definitions, sizeof(dhcp_option_definitions) / sizeof(dhcp_option_definitions[0])};
