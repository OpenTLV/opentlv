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
