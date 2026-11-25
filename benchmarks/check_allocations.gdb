# SPDX-License-Identifier: MIT
# Copyright (c) 2026 Marek Cingel

set pagination off
set confirm off
start
python
import gdb

print("Allocation observer: GDB " + gdb.VERSION)

class Allocation(gdb.Breakpoint):
    def stop(self):
        try:
            if int(gdb.parse_and_eval("opentlv_allocation_active")):
                gdb.execute("set variable opentlv_allocation_seen = 1", to_string=True)
        except Exception as error:
            # Stop the inferior. The exit-status check below then fails the test.
            gdb.write("Cannot observe allocation: " + str(error) + "\n", gdb.STDERR)
            return True
        return False

# Install after startup, once the runtime and OpenTLV shared libraries are loaded.
# Trap the CRT, including calls made through DLL imports or a C++ runtime allocator.
for symbol in ("malloc", "calloc", "realloc", "aligned_alloc", "posix_memalign", "_aligned_malloc"):
    try:
        Allocation(symbol, internal=True)
    except gdb.error:
        if symbol in ("malloc", "calloc", "realloc"):
            raise

gdb.execute("continue")
status = gdb.convenience_variable("_exitcode")
if status is None or int(status) != 0:
    raise gdb.GdbError("Allocation probe did not exit successfully")
end
