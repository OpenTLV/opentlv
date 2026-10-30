# Module diagnostics

This page specifies how diagnostics are owned and propagated once OpenTLV is
split into the functional modules of the
[module contracts](README.md). It extends the
[failure model](../../concepts/error-model.md), which stays normative for
results, detail, locations and propagation, and is written for contributors
of [#647](https://github.com/OpenTLV/opentlv/issues/647),
[#650](https://github.com/OpenTLV/opentlv/issues/650),
[#651](https://github.com/OpenTLV/opentlv/issues/651) and
[#652](https://github.com/OpenTLV/opentlv/issues/652).

**Implementation status:** #647 publishes `tlv/diagnostic/cause.h` with static
domain descriptors, two inline 160-byte payload slots, saturating omission counts,
a delegation envelope and validated append/copy helpers. Payloads are copied out
to typed objects, avoiding alignment and C aliasing assumptions. Descriptors and
pointers within detail retain their documented lifetimes. The IO consumer asserts
at compile time that Reader leaf detail fits the payload.

Query adopts this transport in #650. Conversion callbacks take the generic
envelope; Codec adapters export a Codec leaf followed by its optional IO or
Schema leaf. Schema Query attaches its finding through the Schema-owned helper.
Query retains raw callback results and conversion violations itself, without
including Codec or Schema headers. Existing domain/family values stay mapped;
Query conversion violations have their own registered family.
That family is marked `baseline: false` in the mapping: its declarations are
new in #650, while the values preserve the corresponding Codec violations.
Reference capture excludes it from the immutable v0.8.0 vocabulary; candidate
ownership and vocabulary checks still validate every member.

Measured Query diagnostic sizes are 1072 bytes on Linux x64 (alignment 16),
1040 bytes on MSVC x64 (alignment 8), 696 bytes on Linux x86 and 704 bytes on
MSVC x86. Clang wasm32 layout is 720 bytes (alignment 16). The old x64 layout
was 840 bytes. Preserving the unchanged 592-byte common diagnostic, expression
and resource context plus two full producer frames exceeds the proposed 1 KiB
budget on x64; no detail is discarded to meet that estimate. The direct Reader
detail and delegated chain share discriminated storage. ABI snapshots include
diagnostic alignment, because platform payload alignment differs.

Reader/Writer name functions belong to IO; Query names and feature mirrors now
belong to Query. Codec (#651) and Schema (#652) own their diagnostic names and
transport delegated detail as producer-owned frames. Their public
attach/copy/export helpers bridge native detail to generic frames without exposing
private headers. Query never
interprets foreign payload bytes. All frames remain copyable by value, with the
original producer's borrowed-pointer lifetimes.

## Baseline coupling

The monolithic library lets every layer embed every other layer's detail:

- Core `diagnostic.c` defines `tlv_codec_diagnostic_init()`, and Core
  `diagnostic_strings.c` holds the name tables of Reader, Writer, Query,
  Schema and Codec, so Core includes all their headers.
- `tlv_codec_detail_t` embeds `tlv_reader_detail_t` and `tlv_schema_detail_t`,
  while `tlv_schema_diagnostic_t` embeds `tlv_codec_detail_t`: Codec and
  Schema headers need each other.
- `tlv_query_diagnostic_t` embeds Reader, Codec and Schema detail, so Query
  headers include Codec and Schema headers, and Query conversion hooks take a
  `tlv_codec_diagnostic_t*`.

These embeddings are why Codec, Schema and Query cannot build without each
other. The detail itself is correct and must keep its precision; only its
ownership and transport change.

The Codec extraction (#651) resolves the Codec side of this cycle: its detail is
now a leaf and its diagnostic carries the Core cause chain. Structure Codec and
Schema number integrations attach IO/Schema frames through producer-owned APIs.
Codec exports and imports its own detail without linking either producer. The
legacy `tlv_codec_cause_t` vocabulary is retained for baseline interpretation,
but no longer discriminates a union. Its mapping remains `none`, `frame:io`,
and `frame:schema`; no result, operation, reason or violation is renumbered.
Schema extraction (#652) replaces both delegated `detail.reader` and
`detail.codec` with `detail.causes`, while keeping its finding in `detail.schema`.
The legacy cause discriminator remains numerically unchanged: Reader maps to an
IO frame and Codec to a Codec frame, as specified below. Generic Schema links only
Core and IO. DER Schema exports the complete Codec chain before preserving its
primary location and path. Schema's descriptor resolves issue family 1 and
definition-kind family 2. C++ Schema failures retain complete evidence in
`schema_failure`; common `error` no longer embeds Schema types.

On Linux x64, the Schema native diagnostic measures 1008 bytes with alignment 16.
The C++ `schema_failure` measures 1632 bytes: it retains the complete native
diagnostic alongside its common `error` projection (624 bytes). These are measured
layouts, not portable ABI constants; consumers must rebuild for this change.

## Ownership

Core owns only generic diagnostic infrastructure:

| Core owns | Current declaration |
| --- | --- |
| Result taxonomy | `tlv_result_t`, `tlv_result_string()` |
| Common base | `tlv_diagnostic_t`, initialization, location, path and context helpers |
| Severity and locations | `tlv_diagnostic_severity_t`, `tlv_location_t`, `tlv_location_domain_t`, `tlv_location_kind_t` and their name functions |
| Diagnostic domains | New: `tlv_diagnostic_domain_t` descriptors and the domain id ranges |
| Cause chains | New: `tlv_diagnostic_cause_t`, `tlv_diagnostic_causes_t` and their helpers |
| Delegation envelope | New: the common base paired with a cause chain, for callback outputs whose producer is unknown |

Each module owns its detail vocabulary, detail types, initialization and name
functions, and none of them moves to Core:

| Module | Owns |
| --- | --- |
| IO | `tlv_reader_operation_t`, `tlv_reader_detail_t`, `tlv_reader_diagnostic_t`, `tlv_writer_operation_t`, `tlv_writer_diagnostic_t` |
| Query | `tlv_query_error_kind_t`, `tlv_query_diagnostic_t`, Query limit names |
| Schema | `tlv_schema_issue_kind_t`, `tlv_schema_definition_kind_t`, `tlv_schema_detail_t`, `tlv_schema_diagnostic_t`; `schema-query` owns `tlv_schema_query_diagnostic_t` |
| Codec | `tlv_codec_operation_t`, `tlv_codec_violation_t`, `tlv_codec_reason_t`, `tlv_codec_detail_t`, `tlv_codec_diagnostic_t` |
| Builtins | Their own detail types if they add any; today they report generic results with Schema or Codec detail |

`tlv_result_t` stays a Core enum because every result class is generic by the
[admission rule](../../concepts/error-model.md#admission-rule). A module never
adds a result value for its own conditions; it adds detail.

## Code namespaces

A diagnostic code is identified by three parts:

- **Domain**: the module that defines it, as a registered 16-bit id.
- **Family**: one vocabulary of that domain, as an id unique within the
  domain, such as Query error kinds or Codec reasons.
- **Value**: the enumerator value within the family.

Two modules can therefore reuse the same numeric value without collision.
Domain ids come from fixed ranges:

| Range | Use |
| --- | --- |
| `0x0000` | No domain |
| `0x0001`–`0x00FF` | Generic modules: Core 1, IO 2, Document 3, Query 4, Schema 5, Codec 6, Generator 7 |
| `0x0100`–`0x01FF` | Builtin protocol families: ASN.1 0x101, EMV 0x102, Bluetooth 0x103, NFC 0x104, DHCP 0x105, LLDP 0x106 |
| `0x0200`–`0x7FFF` | Reserved for future OpenTLV modules |
| `0x8000`–`0xFFFF` | Producers outside OpenTLV; not guaranteed unique, so consumers compare them together with the domain name |

The registry is [diagnostic-mapping.json](diagnostic-mapping.json) `domains`.
Each module declares its own id constant in its own header; Core declares no
module ids. `scripts/check_module_contracts.py` rejects reused domain or family
ids and ids outside their range, which keeps ownership collision-free without
a Core table of modules. A new module or family registers its id there in the
pull request that adds it.

Values of existing families are unchanged. A family may append values; it does
not reuse or renumber them within a release line, and before 1.0 a renumbering
updates the mapping in the same change, as the failure model already requires
for enum changes.

## Cause chains

A diagnostic that delegated work keeps the lower layer's detail as a *cause
frame*. A frame records:

- the producing module's domain, as a pointer to its static
  `tlv_diagnostic_domain_t` descriptor (id, stable name such as
  `"opentlv.codec"`, and name lookups for its detail types and codes);
- a detail type id within that domain, such as Codec detail or Reader detail;
- the producer's typed detail, copied by value into bounded, suitably aligned
  inline storage.

Frames are ordered from the outermost delegated layer to the innermost. A
Query conversion failure whose Structure Codec was rejected by the Reader has
the Query detail itself, then a Codec frame, then an IO frame. The chain:

- is inline and bounded, so diagnostics stay allocation-free, caller-owned,
  copyable without pointer rebasing, and FFI-friendly;
- holds at least two frames, the deepest delegation today
  (Query → Codec → Reader); when it is full it keeps the outermost frames and
  counts the omitted ones, as the diagnostic path does;
- stores each delegated cause once, so a cause stays reachable through one
  path only;
- is only part of envelopes that can delegate: Query, Schema and Codec
  diagnostics and the Core delegation envelope. Reader and Writer diagnostics
  stay leaves.

Frame storage is sized by Core for the largest leaf detail type (currently
`tlv_reader_detail_t`, 160 bytes on 64-bit), and each module asserts at
compile time that its detail fits. A detail type that holds pointers keeps its
existing borrowing rules. The current 840-byte Query diagnostic should stay
below 1 KiB on 64-bit targets with two frames; #647 records the actual sizes.

### Detail payloads

The module that owns a detail type provides the only typed access to it:

- an accessor that returns the typed detail of a frame, or NULL when the
  frame's domain or type is not its own (for example the Codec detail of a
  Codec frame);
- an attach function that appends its detail as a frame to a chain, used by
  the integration targets that delegate to it.

A consumer that knows a module calls its accessor; a consumer that does not
can still print the frame through the domain descriptor. Programs must not
interpret payload bytes of a foreign domain directly.

### Names

The domain descriptor makes every frame self-describing: its name lookups
return the canonical names of the frame's detail type and codes, using the
same static strings as the module's own name functions. Since the producing
module created the frame, it is loaded whenever the frame exists, so a
consumer can name a foreign cause without linking that module. The module name
functions (`tlv_reader_operation_string()` and so on) move with their
vocabularies and stay available wherever their module is linked.

## Propagation

Every rule of [propagation and callbacks](../../concepts/error-model.md#propagation-and-callbacks)
still applies. For modules:

1. A delegating operation returns the lower result unchanged, sets its own
   common base as the failure model requires, and appends the lower detail as
   a frame followed by the lower diagnostic's own frames.
2. A module appends only frames of modules it depends on, or frames it
   received through the Core delegation envelope. A frame of a module it does
   not know is copied unchanged.
3. A callback whose provider may belong to any module uses the Core
   delegation envelope as its diagnostic output, never a type of a module the
   caller does not depend on. Query conversion hooks therefore take the
   envelope instead of `tlv_codec_diagnostic_t*`; the `query-codec` adapters
   call Codec with a Codec diagnostic and attach its detail as a Codec frame.
4. The primary location, path and expected/actual text stay in the common
   base and keep their current propagation and translation rules, including
   the VALUE-to-INPUT resolution of #589. Frames carry only layer detail.
5. Kinds and cause frames agree as the current kind/cause rules require:
   Query kind `READER` has an IO first frame, `CODEC` a Codec first frame,
   `SCHEMA` a Schema first frame, and kinds without a delegated cause have no
   frame.

### Examples

| Failure | Before | After |
| --- | --- | --- |
| Reader rejects a node during Query execution | Query `cause = READER`, `detail.reader` | Query kind `READER`; frames: IO Reader detail |
| Strict Query conversion through a Structure Codec hits malformed input | Query `cause = CODEC`, `detail.codec` with `codec.cause = READER` and `codec.detail.reader` | Query kind `CODEC`; frames: Codec detail, IO Reader detail |
| DER Schema rejects a non-canonical INTEGER leaf | Schema `cause = CODEC`, `detail.codec` | Schema base with result `INVALID_VALUE`; frames: Codec detail with reason `CANONICAL` |
| Schema validation of malformed wire input | Schema `cause = READER`, `detail.reader` | Schema base; frames: IO Reader detail |
| False Schema Query assertion | Query kind and cause `SCHEMA`, `detail.schema` | Query kind `SCHEMA`; frames: Schema detail with issue `ASSERTION` |

## Preserved precision

The change is a representation change of how detail is carried, not of what
is reported. These stay equivalent and are what #655 compares:

- every returned result and the invariant `diagnostic.code == rc` after
  initialization;
- primary location domain, anchor and coordinates, including unknown versus
  zero, EOF spans and VALUE locations;
- enclosing path and omitted count, expected and actual text;
- every layer's typed detail fields, now reached through its frame;
- the order of delegation and which layer each detail came from;
- Query expression span, limit name and configured bound;
- distinctions between results, kinds and causes: no two baseline codes map
  to one new code, which the checker enforces.

## Code mapping

[diagnostic-mapping.json](diagnostic-mapping.json) maps every public
diagnostic code of the baseline to its new identity. The baseline is the
public diagnostic vocabulary of the
[error reference](../../reference/errors.md#diagnostic-enum-names) plus the
Query limit names; #645 ties it to the reference release. Each entry records
the previous enumerator and value, the new code, the owning module, the
documented meaning and the intentional change.

| Family | Codes | Owner | Change |
| --- | ---: | --- | --- |
| `core.result` | 22 | Core | None |
| `core.severity`, `core.location-domain`, `core.location-kind` | 14 | Core | None |
| `io.reader-operation`, `io.writer-operation` | 14 | IO | Name functions move from Core to IO |
| `query.error-kind` | 18 | Query | Name function moves to Query; delegated detail of `READER`, `CODEC`, `CALLBACK` and `SCHEMA` moves to frames |
| `query.cause` | 4 | Query | Discriminator retired; replaced by the first frame's domain |
| `query.limit` | 15 | Query; `schema-contexts` Schema | None; `document-*` names are produced by `query-document` |
| `schema.issue-kind`, `schema.definition-kind` | 16 | Schema | Name functions move to Schema |
| `schema.cause` | 4 | Schema | Discriminator retired; Schema findings stay own detail, Reader and Codec causes move to frames |
| `codec.operation`, `codec.violation`, `codec.reason` | 16 | Codec | Name functions move to Codec |
| `codec.cause` | 3 | Codec | Discriminator retired; Structure Codec causes move to frames |
| **Total** | **126** | | |

A retired cause value maps to the frame that replaces it (`none`,
`frame:io`, `frame:codec`, `frame:schema`, or `own:finding` for Schema's own
finding). These new codes are distinct within each family, so no baseline
meaning merges. The checker parses the current headers and fails when an
enumerator or limit name is missing from the mapping or a mapped one no longer
exists, so the mapping stays complete until #655 verifies it against the
modular implementation.

`scripts/generate_diagnostic_vocabulary.py` reads the same enums and name
tables; the issues that move a name function update its `NAME_TABLES` and
header paths. Binding vocabularies follow in the downstream alignment epic.

## Alternatives considered

| Alternative | Why not |
| --- | --- |
| Keep every vocabulary and detail type in Core or one shared diagnostic target | Puts module-specific diagnostics into a common place that every module changes, which the epic excludes. |
| One header-only vocabulary target per module, keeping the typed unions | Codec detail needs Schema detail and Schema diagnostics need Codec detail, so the vocabulary targets form a cycle; Query would still depend on Codec and Schema vocabularies. |
| One global numeric code space or ranges inside `tlv_result_t` | Mixes detail into the result taxonomy against the admission rule and needs a central allocation for every new value. |
| Domains identified by pointer only (like C++ `std::error_category`) | Not stable across processes, bindings and serialized output; the registered id gives a stable identity and the pointer adds names. |
| Caller-allocated or heap cause chains | Diagnostics must stay allocation-free and copyable; inline bounded storage already serves the path. |
| Text contexts for foreign causes | Loses machine-readable detail; the failure model forbids parsing context strings. |
