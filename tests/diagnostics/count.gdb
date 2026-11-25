# SPDX-License-Identifier: MIT
# Copyright (c) 2026 Marek Cingel

set pagination off
set confirm off
python
import gdb

operations = ("read", "next", "write", "append")
if int(gdb.parse_and_eval("$native")):
    symbols = ("tlv_read_with_diagnostic", "tlv_reader_next_with_diagnostic",
               "tlv_write_with_diagnostic", "tlv_writer_write_with_diagnostic")
else:
    symbols = tuple("probe_" + name for name in operations)

points = [gdb.Breakpoint("*" + name, internal=True) for name in symbols]
gdb.execute("run", to_string=True)
for mode in ("null", "enabled"):
    for extent in ("complete", "short"):
        for operation, symbol in zip(operations, symbols):
            if int(gdb.parse_and_eval("$pc")) != int(gdb.parse_and_eval("&" + symbol)):
                raise gdb.GdbError("Expected to enter " + symbol)
            # Stop at the first instruction, before the x86-64 prologue.
            # Disable nested breakpoints while counting through the return.
            for point in points:
                point.enabled = False
            caller = int(gdb.parse_and_eval("*(void **)$rsp"))
            count = 0
            while int(gdb.parse_and_eval("$pc")) != caller:
                gdb.execute("stepi", to_string=True)
                count += 1
            print(f"{mode} {extent} {operation} {count}")
            for point in points:
                point.enabled = True
            gdb.execute("continue", to_string=True)
if int(gdb.parse_and_eval("$_exitcode")) != 0:
    raise gdb.GdbError("Workload failed")
end
