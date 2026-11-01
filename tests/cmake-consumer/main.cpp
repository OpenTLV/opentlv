// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Marek Cingel
#include <cstring>
#include <tlv++/version.hpp>

int main(int argc, char** argv) {
    if (argc != 2) return 1;
    return std::strcmp(tlv::version(), argv[1]) != 0;
}
