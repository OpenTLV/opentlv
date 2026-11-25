// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Marek Cingel

#include "adapter.h"
#include <opentlv/tlv.hpp>

namespace core = opentlv::core;
namespace io = opentlv::io;

extern "C" void bench_run_cpp(bench_operation op, const bench_input *in, uint8_t *destination,
                              bench_output *out)
{
    const auto format = core::from_c(in->format);
    const auto source = core::from_c(in->source);
    const core::byte_view input{in->data, in->available};
    const core::mutable_byte_view output{destination, in->available};
    auto diagnostic = core::from_c(out->diagnostic);
    auto *details = in->diagnostic ? &diagnostic : nullptr;

    switch (op) {
    case BENCH_FIELD_READ:
        out->result = core::to_c(core::read_length(format.length, {input.data, input.size},
                                                   out->length, out->count));
        return;
    case BENCH_FIELD_WRITE:
        out->result = core::to_c(core::write_length(format.length, in->length, output, out->count));
        return;
    case BENCH_HEADER_SIZE:
        out->result = core::to_c(core::header_size(format, out->count));
        return;
    case BENCH_ELEMENT_SIZE:
        out->result = core::to_c(core::element_size(format, source, out->count));
        return;
    case BENCH_READ: {
        auto view = core::from_c(out->view);
        out->result = core::to_c(io::read(format, input, view, out->count, details));
        out->view = core::to_c(view);
        break;
    }
    case BENCH_NEXT: {
        io::reader reader;
        out->result = core::to_c(reader.init(format, input));
        if (out->result != TLV_OK) {
            return;
        }
        auto view = core::from_c(out->view);
        out->result = core::to_c(reader.next(view, details));
        out->view = core::to_c(view);
        out->position = reader.position();
        break;
    }
    case BENCH_WRITE:
        out->result = core::to_c(io::write(format, output, source, out->count, details));
        break;
    case BENCH_APPEND: {
        io::writer writer;
        out->result = core::to_c(writer.init(format, output));
        if (out->result != TLV_OK) {
            return;
        }
        out->result = core::to_c(writer.write(source, details));
        out->position = writer.position();
        break;
    }
    }
    if (details) {
        out->diagnostic = core::to_c(diagnostic);
    }
}
