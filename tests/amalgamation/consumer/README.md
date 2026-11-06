# Standalone OpenTLV consumers

These samples use Core and IO with the ASN.1 and LLDP builtins. The C++ sample
also validates an LLDP packet and decodes its TTL through the existing Codec
facade. They return nonzero on failure and print the Library version on success.

After extracting a release amalgamation archive, run these commands from its
`OpenTLV-<version>-amalgamation` directory. For the installed `Amalgamation`
component, use `share/OpenTLV/amalgamation` (or your configured data directory).
No CMake, Python, prebuilt OpenTLV libraries or additional link libraries are
needed to compile these consumers.

## GCC or Clang

```sh
cc -std=c99 -pedantic-errors -c opentlv.c -o opentlv.o
cc -std=c99 -pedantic-errors -I. examples/consumer.c opentlv.o -o consumer-c
./consumer-c
c++ -std=c++17 -pedantic-errors -Iinclude examples/consumer.cpp opentlv.o -o consumer-cpp
./consumer-cpp
```

The C-only program needs just `opentlv.c` and `opentlv.h`. C++ also needs the
provided `include/` tree and a C++17 standard library. Compile `opentlv.c` once
with the C driver; link C++ applications with the C++ driver. The public
headers provide C linkage; do not wrap the C++ headers in `extern "C"`.

## MSVC

From an x64 Visual Studio Developer shell, use MSVC's C11 mode for the C99
source (MSVC has no C99 mode). Use the same architecture and runtime for all
objects:

```bat
cl /nologo /MD /TC /std:c11 /c opentlv.c /Foopentlv.obj
cl /nologo /MD /TC /std:c11 /I. /c examples/consumer.c /Foconsumer-c.obj
cl /nologo consumer-c.obj opentlv.obj /Feconsumer-c.exe
consumer-c.exe
cl /nologo /MD /EHsc /TP /std:c++17 /Iinclude /c examples/consumer.cpp /Foconsumer-cpp.obj
cl /nologo consumer-cpp.obj opentlv.obj /Feconsumer-cpp.exe
consumer-cpp.exe
```

Do not compile `opentlv.c` as C++, link another OpenTLV implementation into the
same program, or redefine the generated Query feature/version macros. No
`OPENTLV_*_STATIC_DEFINE` or DLL import/export definitions are needed. Query
features are fixed when the sources are generated, even if the conventional
build uses shared libraries.

## Optional dead stripping

`opentlv.c` is one translation unit, so by default a program links all of it,
several hundred kilobytes of code, even when it calls only a few functions.
Placing each function and object in its own section lets the linker drop the
unreferenced ones. These flags are optional: OpenTLV does not require them,
and the default commands above remain the reference.

With GCC or Clang and a GNU-compatible linker (GNU ld, gold or LLD), compile
every object with `-ffunction-sections -fdata-sections` and link with
`-Wl,--gc-sections`:

```sh
cc -std=c99 -pedantic-errors -ffunction-sections -fdata-sections -c opentlv.c -o opentlv.o
cc -std=c99 -pedantic-errors -ffunction-sections -fdata-sections -I. examples/consumer.c opentlv.o -Wl,--gc-sections -o consumer-c
./consumer-c
c++ -std=c++17 -pedantic-errors -ffunction-sections -fdata-sections -Iinclude examples/consumer.cpp opentlv.o -Wl,--gc-sections -o consumer-cpp
./consumer-cpp
```

With MSVC, compile with `/Gy /Gw` and link with `/OPT:REF /OPT:ICF`:

```bat
cl /nologo /MD /TC /std:c11 /Gy /Gw /c opentlv.c /Foopentlv.obj
cl /nologo /MD /TC /std:c11 /Gy /Gw /I. /c examples/consumer.c /Foconsumer-c.obj
cl /nologo consumer-c.obj opentlv.obj /Feconsumer-c.exe /link /OPT:REF /OPT:ICF
consumer-c.exe
cl /nologo /MD /EHsc /TP /std:c++17 /Gy /Gw /Iinclude /c examples/consumer.cpp /Foconsumer-cpp.obj
cl /nologo consumer-cpp.obj opentlv.obj /Feconsumer-cpp.exe /link /OPT:REF /OPT:ICF
consumer-cpp.exe
```

These commands are tested with GCC and Clang 18 on Linux and with MSVC x64.
There both samples shrink to about a fifth of their default size or less; the
exact result depends on the compiler, linker, optimization level and the
functions the program uses. Notes:

- The linker removes only what nothing references, so the gain applies to the
  final executable. A static library containing `opentlv.o` is not smaller
  itself: compile the object with the section flags and pass the linker flag
  when linking the program.
- A shared library keeps every function it exports. Building one from the
  amalgamation reduces little unless it exports only the functions it needs.
- `/OPT:REF` is the MSVC linker's default unless `/DEBUG` is given; without
  `/Gy` the linker can only keep or drop the whole object. Release builds with
  `/O1` or `/O2` already imply `/Gy`.
- Apple's linker uses `-Wl,-dead_strip` instead of `-Wl,--gc-sections`.
  Neither it nor link-time optimization (`-flto`, or `/GL` with `/LTCG`) is
  tested here.
