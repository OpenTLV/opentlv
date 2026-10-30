# Reader, Format and Field boundary

This page fixes the module boundary of the shared Reader → Format → Field
execution path, the hottest path of OpenTLV, before the
[module contracts](README.md) split it between IO and Core. It is written for
contributors of [#647](https://github.com/OpenTLV/opentlv/issues/647) and of
the performance epic [#562](https://github.com/OpenTLV/opentlv/issues/562).

**Status: proposed** in [#644](https://github.com/OpenTLV/opentlv/issues/644).
It records the current path and the rules for keeping it; nothing on the path
changes in #644, and the modularization epic does not optimize it.

## Path

Reading one element crosses these functions on `main`:

```text
tlv_reader_next_diag()           IO   reader/reader.c
  -> tlv_read_impl()             IO   static, maps errors to Reader detail
  -> tlv_format_decode()         Core format.c, validates the provider result
  -> format->decode()            Format provider, indirect call
       Core:    tlv_fields_decode(), tlv_binary_decode(), tlv_variable_decode(),
                tlv_escaped_decode(), tlv_packed_decode()   formats/
       builtin: BER, DER, CER, EMV, Bluetooth, NFC, DHCP, LLDP decoders
  -> Field Encoding reads        Core field/, such as tlv_fixed_identifier_read(),
                                 tlv_variable_length_read(), tlv_packed_field_read()
```

The Tree Reader adds one layer above (`tlv_tree_reader_next_event_diag()` →
`tlv_reader_next_source_diag()`), and Query, Schema, Document and builtin
validators reach the path through the Reader or Tree Reader. Writing mirrors
it: Writer → `tlv_format_measure()` / `tlv_format_encode()` → provider →
Field Encoding writes.

## Ownership

| Part | Owner | Interface |
| --- | --- | --- |
| Reader, incremental input, Tree Reader, Reader diagnostics | IO | Public IO API |
| `tlv_format_t`, the decode/measure/encode callback types, `tlv_decoded_t`, `tlv_source_t`, `tlv_format_error_t` | Core | Public Format contract |
| `tlv_format_decode()`, `tlv_format_measure()`, `tlv_format_encode()`, `tlv_source_preserve()` | Core | Public; the only entry points IO uses |
| Field composition and generic Format constructors (`formats/`) | Core | Public Format construction API |
| Field Encodings (`field/`) | Core | Public Field Encoding API; `fixed_internal.h`, `variable_internal.h` and `escaped_internal.h` are Core private |
| Builtin Format providers | Each builtin | Public builtin API; they call Core composition and Field Encoding through public API |

## Rules

1. IO calls a Format only through `tlv_format_decode()`, `tlv_format_measure()`
   and `tlv_format_encode()`. It never calls `format->decode` or another
   callback directly, because the Core entry points validate provider results
   (callback normalization, Source ranges, Tag binding) and produce the
   `tlv_format_error_t` that Reader detail is built from.
2. Format providers, generic or builtin, depend only on the public Format
   contract and public Field Encoding API. They know nothing about the Reader.
3. Reader detail, locations and incremental-input policy stay in IO. Core
   reports failures only through its return value and `tlv_format_error_t`.
4. Core private headers (`field/*_internal.h`) are used only by Core.
   Callback-result validation is public in `tlv/callback.h` since #647.

## Call boundaries and symbol visibility

| Call | Boundary | Static build | Shared build |
| --- | --- | --- | --- |
| Reader → `tlv_format_decode()` | IO → Core | Direct call to an archive member | Exported Core function, called through the PLT or import table |
| `tlv_format_decode()` → `format->decode` | Core → provider | Indirect call, as today | Indirect call, as today |
| Core composition → Field Encoding | Within Core | Direct call | Within the Core library; may stay internal to Core |
| Builtin provider → Core composition and Field Encoding | Builtin → Core | Direct call | Exported Core functions |

Today's shared library already exports these functions and makes the
provider call indirect, so the module split adds no indirection inside a
Format provider. The new cost in shared builds is that a call from IO into Core
crosses a library boundary. Static builds keep today's code generation. This is
recorded by the informational comparison of
[#655](https://github.com/OpenTLV/opentlv/issues/655).

Each library exports only its declared API (see
[Static and shared builds](README.md#static-and-shared-builds)). Core exports
the Format entry points, composition and Field Encoding functions it exports
today; it gains no export just to serve IO.

## Inlining constraints

- No function is inlined across module boundaries except `static inline`
  functions in public headers, which are API of the header's owning module and
  compile into every caller. Core may add such helpers only when they are part
  of the documented contract, because changing them requires recompiling
  every consumer.
- Link-time optimization across modules is a consumer choice for static
  builds. No module may require it for correctness or for its documented
  performance.
- Moving the Reader into IO must keep `tlv_read_impl()` and its callers in one
  translation unit, so its current inlining within IO stays possible.

## Performance epic implications

The epic [#562](https://github.com/OpenTLV/opentlv/issues/562) works inside
these boundaries:

| Issue | Owner after modularization | Constraint |
| --- | --- | --- |
| [#563](https://github.com/OpenTLV/opentlv/issues/563) performance contracts | Core and IO benchmarks | Measure static and shared builds separately, because only shared builds pay the IO → Core library boundary |
| [#564](https://github.com/OpenTLV/opentlv/issues/564) construction-time decisions | Core | Prepared or specialized Format state is part of the Core Format contract, created by Core constructors; IO does not inspect provider configuration |
| [#565](https://github.com/OpenTLV/opentlv/issues/565) per-element dispatch | Core | Dispatch reductions happen behind `tlv_format_decode()` and within providers; a fast path that IO calls is a new public Core entry point, not access to Core internals |
| [#566](https://github.com/OpenTLV/opentlv/issues/566) decode-result and Source metadata | Core and IO | On-demand Source metadata is a Format contract change negotiated through public Core API; IO keeps building Reader detail lazily |

Do not optimize or change this path during the modularization epic. A
regression found by the informational comparisons is documented and evaluated
under #562.
