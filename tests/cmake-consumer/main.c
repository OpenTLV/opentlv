// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Marek Cingel
#include <string.h>
#include <tlv/version.h>

int main(int argc, char** argv) {
    if (argc != 2) return 1;
    if (strcmp(tlv_version_string(), OPENTLV_VERSION_STRING) != 0) return 2;
    return strcmp(tlv_version_string(), argv[1]) != 0;
}
