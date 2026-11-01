// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Marek Cingel

#ifndef OPENTLV_TLVPP_QUERY_DOCUMENT_HPP
#define OPENTLV_TLVPP_QUERY_DOCUMENT_HPP

/** @file
 * @brief Query operations on owned Documents; link OpenTLV::QueryDocument.
 */
#include "tlv++/document/document.hpp"
#include "tlv++/query/query.hpp"
#include "tlv++/query/program.hpp"
#include "tlv/query/document.h"

namespace tlv {
inline const_node document::find(const query& path) const {
    return const_cast<document*>(this)->find(path);
}

inline std::vector<const_node> document::select(const query& path) const {
    auto values = const_cast<document*>(this)->select(path);
    return std::vector<const_node>(values.begin(), values.end());
}

inline std::vector<const_node> document::select(const char* text) const {
    return select(query::compile(text));
}

inline expected<std::vector<const_node>, query_failure>
document::select(const query_program& program, query_capabilities environment = nullptr,
                 size_t max_work) const {
    auto values = const_cast<document*>(this)->select(program, environment, max_work);
    if (!values) return unexpected<query_failure>(values.error());
    return std::vector<const_node>(values->begin(), values->end());
}

inline expected<const_node, query_failure> document::next(query_execution& execution) const {
    auto value = const_cast<document*>(this)->next(execution);
    if (!value) return unexpected<query_failure>(value.error());
    return const_node(*value);
}

inline node document::find(const query& path) {
    tlv_node_t* result = nullptr;
    const auto  rc = tlv_document_find_path(impl_->handle.get(), &path.c_query(), &result);
    if (rc != TLV_OK) throw query_error(rc, tlv_diagnostic_t{});
    return node(result, impl_->lifetime);
}

inline std::vector<node> document::select(const query& path) {
    struct collection {
        const document*           owner;
        std::vector<node>         results;
        std::exception_ptr        failure;
        static tlv_visit_result_t append(tlv_node_t* raw, void* context) {
            auto& state = *static_cast<collection*>(context);
            try {
                state.results.push_back(node(raw, state.owner->impl_->lifetime));
                return TLV_VISIT_CONTINUE;
            } catch (...) {
                state.failure = std::current_exception();
                return TLV_VISIT_ERROR;
            }
        }
    };
    collection             state{this, {}, {}};
    tlv_query_diagnostic_t d{};
    auto rc = tlv_document_query_visit(impl_->handle.get(), &path.c_query(), &collection::append,
                                       &state, &d);
    if (state.failure) std::rethrow_exception(state.failure);
    if (rc != TLV_OK) throw query_error(rc, d.diagnostic);
    return std::move(state.results);
}

inline std::vector<node> document::select(const char* text) {
    return select(query::compile(text));
}

inline expected<std::vector<node>, query_failure>
document::select(const query_program& program, query_capabilities environment = nullptr,
                 size_t max_work) {
    if (program.info().result_kind != TLV_QUERY_RESULT_NODES)
        return unexpected<query_failure>(detail::query_failed(TLV_ERR_INVALID_ARG));
    auto execution = query_execution::create(program, impl_->max_depth, size() ? size() : 1,
                                             max_work, environment);
    if (!execution) return unexpected<query_failure>(execution.error());
    std::vector<uint8_t> values;
    auto                 rc = evaluate_owned(program, *execution, values);
    if (!rc) return unexpected<query_failure>(rc.error());
    std::vector<node>      results;
    tlv_query_diagnostic_t d{};
    for (;;) {
        tlv_node_t* pointer = nullptr;
        auto        code = tlv_document_query_next(execution->c_exec(), &pointer, &d);
        if (code == TLV_END) break;
        if (code != TLV_OK) return unexpected<query_failure>(detail::query_failed(code, d));
        results.push_back(node(pointer, impl_->lifetime));
    }
    return results;
}

inline expected<void, query_failure> document::evaluate(query_execution&        execution,
                                                        span<byte>              values,
                                                        const writer_workspace* staging,
                                                        node                    context) const {
    // cppcheck-suppress-end passedByValue
    tlv_tree_writer_workspace_t raw{};
    if (staging) {
        raw.frames = staging->frames.data();
        raw.frame_capacity = staging->frames.size();
        raw.scratch = reinterpret_cast<uint8_t*>(staging->scratch.data());
        raw.scratch_capacity = staging->scratch.size();
    }
    return evaluate(execution, reinterpret_cast<uint8_t*>(values.data()), values.size(),
                    staging ? &raw : nullptr, context);
}

inline expected<void, query_failure> document::evaluate(query_execution& execution, uint8_t* values,
                                                        size_t                       capacity,
                                                        tlv_tree_writer_workspace_t* staging,
                                                        node context) const {
    if (context.identity_ && !context)
        return unexpected<query_failure>(detail::query_failed(TLV_ERR_INVALID_STATE));
    if (context.identity_ && context.owner_.lock() != impl_->lifetime)
        return unexpected<query_failure>(detail::query_failed(TLV_ERR_INVALID_ARG));
    tlv_query_diagnostic_t d{};
    auto rc = tlv_document_query_evaluate(c_document(), execution.c_exec(), context.c_node(),
                                          values, capacity, staging, &d);
    if (rc != TLV_OK) return unexpected<query_failure>(detail::query_failed(rc, d));
    execution.document_lifetime_ = impl_->lifetime;
    execution.has_document_ = true;
    return {};
}

inline expected<node, query_failure> document::next(query_execution& execution) {
    if (!execution.has_document_ || execution.document_lifetime_.expired())
        return unexpected<query_failure>(detail::query_failed(TLV_ERR_INVALID_STATE));
    if (execution.document_lifetime_.lock() != impl_->lifetime)
        return unexpected<query_failure>(detail::query_failed(TLV_ERR_INVALID_ARG));
    tlv_node_t*            pointer = nullptr;
    tlv_query_diagnostic_t d{};
    auto                   rc = tlv_document_query_next(execution.c_exec(), &pointer, &d);
    if (rc == TLV_END) return unexpected<query_failure>(detail::query_failed(rc));
    if (rc != TLV_OK) return unexpected<query_failure>(detail::query_failed(rc, d));
    return node(pointer, impl_->lifetime);
}

inline expected<size_t, query_failure> document::query_remove(const query_program& program) {
    return edit_query(program, TLV_DOCUMENT_QUERY_REMOVE, tlv_tag(nullptr, 0), bytes(), nullptr);
}

inline expected<size_t, query_failure> document::query_replace(const query_program& program,
                                                               bytes value, size_t* applied) {
    return edit_query(program, TLV_DOCUMENT_QUERY_REPLACE, tlv_tag(nullptr, 0), value, applied);
}

inline expected<size_t, query_failure> document::query_insert_after(const query_program& program,
                                                                    tlv::tag tag, bytes value,
                                                                    size_t* applied) {
    return edit_query(program, TLV_DOCUMENT_QUERY_INSERT_AFTER, detail::semantic_access::get(tag),
                      value, applied);
}

inline expected<size_t, query_failure> document::edit_query(const query_program& program,
                                                            int edit_kind, tlv_tag_t tag,
                                                            bytes value, size_t* applied) {
    size_t count = 0;
    if (applied) *applied = 0;
    if (program.info().result_kind != TLV_QUERY_RESULT_NODES)
        return unexpected<query_failure>(detail::query_failed(TLV_ERR_INVALID_ARG));
    auto execution =
        query_execution::create(program, impl_->max_depth, size() ? size() : 1, 100000000);
    if (!execution) return unexpected<query_failure>(execution.error());
    std::vector<uint8_t> values;
    auto                 evaluated = evaluate_owned(program, *execution, values);
    if (!evaluated) return unexpected<query_failure>(evaluated.error());
    std::vector<tlv_node_t*> targets(size());
    tlv_query_diagnostic_t   d{};
    auto rc = tlv_document_query_edit(c_document(), execution->c_exec(),
                                      static_cast<tlv_document_query_edit_kind_t>(edit_kind), tag,
                                      reinterpret_cast<const uint8_t*>(value.data()), value.size(),
                                      targets.data(), targets.size(), &count, &d);
    if (applied) *applied = count;
    if (rc != TLV_OK) return unexpected<query_failure>(detail::query_failed(rc, d));
    return count;
}

inline expected<void, query_failure> document::evaluate_owned(const query_program&  program,
                                                              query_execution&      execution,
                                                              std::vector<uint8_t>& values) const {
    if (!program.info().constructed_values_required)
        return evaluate(execution, nullptr, 0, nullptr);
    std::vector<tlv_tree_writer_frame_t> frames(impl_->max_depth + 1);
    std::vector<uint8_t>                 scratch;
    tlv_tree_writer_workspace_t          staging{};
    staging.frames = frames.data();
    staging.frame_capacity = frames.size();
    size_t bytes = 0;
    for (;;) {
        staging.data = values.data();
        staging.data_capacity = values.size();
        staging.scratch = scratch.data();
        staging.scratch_capacity = scratch.size();
        tlv_query_diagnostic_t d{};
        auto rc = tlv_document_query_value_size(c_document(), &staging, &bytes, &d);
        if (rc == TLV_OK) break;
        if (rc != TLV_ERR_BUFFER_TOO_SHORT)
            return unexpected<query_failure>(detail::query_failed(rc, d));
        if (staging.required_data <= values.size() && staging.required_scratch <= scratch.size())
            return unexpected<query_failure>(detail::query_failed(rc, d));
        if (staging.required_data > values.size()) values.resize(staging.required_data);
        if (staging.required_scratch > scratch.size()) scratch.resize(staging.required_scratch);
    }
    return evaluate(execution, values.data(), values.size(), &staging);
}
} // namespace tlv
#endif
