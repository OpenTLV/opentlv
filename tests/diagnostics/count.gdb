# SPDX-License-Identifier: MIT
# Copyright (c) 2026 Marek Cingel

set pagination off
set confirm off
python
import gdb

if gdb.convenience_variable("wrappers"):
    workloads = [(extent, op) for extent in ("valid", "invalid") for op in ("category", "value")]
    workloads.append(("complete", "measure"))
    workloads.extend((extent, op) for extent in ("complete", "short") for op in ("read", "next", "write", "append"))
    cases = [(f"wrapper {extent} {op} {api}", f"{api}_{op}")
             for extent, op in workloads for api in ("c", "cpp")]
elif gdb.convenience_variable("fields"):
    cases = []
    for width in range(1, 9):
        for order in ("big", "little"):
            for offset in (0, 1):
                for op in ("read", "write"):
                    cases.append((f"field {width} {order} {offset} {op}", "tlv_fixed_length_" + op))
            for size in (0, 3, 255 if width == 1 else 513):
                for op in ("read", "write"):
                    cases.append((f"record {width} {order} {size} {op}", "tlv_" + op))
elif gdb.convenience_variable("measure"):
    # The first write measures the same complete element in every build.
    cases = [("measure", "tlv_fixed_element_size")]
else:
    operations = ("read", "next", "write", "append")
    if int(gdb.parse_and_eval("$native")):
        symbols = ("tlv_read_with_diagnostic", "tlv_reader_next_with_diagnostic",
                   "tlv_write_with_diagnostic", "tlv_writer_write_with_diagnostic")
    else:
        symbols = tuple("probe_" + name for name in operations)
    cases = [(f"{mode} {extent} {op}", symbol)
             for mode in ("null", "enabled")
             for extent in ("complete", "short")
             for op, symbol in zip(operations, symbols)]

for index, (label, symbol) in enumerate(cases):
    # Stop at the first instruction, before the x86-64 prologue. The
    # temporary breakpoint is removed before stepping through nested calls.
    gdb.Breakpoint("*" + symbol, internal=True, temporary=True)
    gdb.execute("run" if index == 0 else "continue", to_string=True)
    if int(gdb.parse_and_eval("$pc")) != int(gdb.parse_and_eval("&" + symbol)):
        raise gdb.GdbError("Expected to enter " + symbol)
    caller = int(gdb.parse_and_eval("*(void **)$rsp"))
    count = 0
    while int(gdb.parse_and_eval("$pc")) != caller:
        gdb.execute("stepi", to_string=True)
        count += 1
    print(f"{label} {count}")
gdb.execute("continue", to_string=True)
if int(gdb.parse_and_eval("$_exitcode")) != 0:
    raise gdb.GdbError("Workload failed")
end
