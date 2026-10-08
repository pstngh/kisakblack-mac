# 64-bit builds

The engine is decompiled 32-bit x86 code. The macOS build (Apple Silicon) is the
first 64-bit target. This page is how 64-bit works here and the rules for code that
touches pointers. On the 32-bit builds (Windows, Linux) everything below compiles
to what it was before.

## What can't simply become 64-bit

- **Fastfile assets.** `db_load.cpp` reads asset structs straight out of the zone
  stream, with their 32-bit on-disk layout (4-byte pointers).
- **Network fields.** `msg_mp.cpp`'s field tables address `playerState_s`,
  `entityState_s`, `hudelem_s`, `objective_t`, `clientState_s`,
  `archivedEntity_s` and the match state by literal byte offsets.
- **Pointers kept in ints.** The script compiler walks parse-tree nodes as
  `*(sval_u *)(node.stringValue + 4)`, the VM stores code positions and anim
  trees in int unions, bytecode embeds 4-byte code positions, the physics free
  lists compare-and-swap pointers through `uint32`, and so on, across the codebase.

## The mechanism: `src/universal/ptr32.h`

- **`Ptr32_Encode(p)` / `Ptr32_Decode(v)`** convert between a pointer and a
  32-bit value. The executable image (code, globals) and a 1.5 GB zero-fill heap
  array inside it lie within 2 GB of the image's Mach-O header; a pointer into
  any of them encodes as its offset from that header, so encodings are linear
  (`Encode(p) + n == Encode(p + n)`). Any other pointer gets a handle-table slot:
  it round-trips exactly but is not linear. Every encoding keeps the pointer's
  low 4 bits, so alignment checks on an encoded value still hold.
- **The heap.** `Z_Malloc`, the `VirtualAlloc` shim (hunk, physical-memory pool,
  zone blocks) and the asset pools allocate from the in-image heap
  (`Ptr32_RegionAlloc`, `Ptr32_HeapAlloc`), so engine memory is linear.
- **`Ptr32<T>`** is a 4-byte pointer field holding such a value. It converts
  implicitly to and from `T*`. It is used for:
  - every pointer field of a struct the fastfile loader reads (found from the
    loader's types with clang's AST; 639 fields),
  - pointer members of unions that mix ints and pointers (`VariableUnion`,
    `sval_u`, `scr_anim_s`), so code that writes one member and reads another
    keeps working,
  - `objective_t::alt_3D_text`, which keeps `playerState_s` at its network layout.

On 32-bit builds `Ptr32<T>` is `T*` and the two functions are casts.

## Rules for code that touches pointers

1. Keep 32-bit builds compiling and identical. Never write `.v` or `.get()` on a
   Ptr32 (they don't exist when it is `T*`). Use `Ptr32_Raw(field)` /
   `Ptr32_SetRaw(field, value)` for raw contents, `(T *)field` for a pointer.
2. Don't change the layout of asset or networked structs. Check with
   `tools/layout_diff.py` (below).
3. A pointer stored in, or read back from, a 32-bit int: `Ptr32_Encode` /
   `Ptr32_Decode`.
4. A cast to `T **`: decide what the memory holds. 32-bit-laid-out memory (asset
   data, the script parse tree, bytecode, any buffer of 4-byte slots) is
   `Ptr32<T> *`; a native runtime array of pointers stays `T **`. Byte counts
   that assumed 4-byte pointers (`Hunk_Alloc(2048)` for 512 pointers, `memcpy(.., 4)`,
   `+= 4` strides over pointer arrays) must say what they mean.
5. `&ptr32Field` where `T **` is expected: make the callee take `Ptr32<T> *` if
   it only reads/writes that slot; otherwise use a local and write it back.
6. Atomics on pointers through 32-bit integers truncate. Use one representation
   per variable: always-encoded 32-bit values, or pointer-sized atomics (8-byte
   aligned on arm64).
7. Plain `long` is 8 bytes on macOS (4 on Windows). Casting an `int *` to
   `unsigned long *` (e.g. for `_BitScanReverse`) writes 8 bytes into 4.

The macOS build turns the relevant warnings into errors: pointer <-> 32-bit int
casts both ways, and `Ptr32` objects passed through varargs (`printf`).

## Tools

- `tools/layout_diff.py <build_dir> <file.cpp>... [--only A,B]` compiles the files
  for i386 (the original ABI) and natively, and lists structs whose size or field
  offsets differ. All 732 asset structs must match.
- `tools/check_file.py <build_dir> [--i386] <file.cpp>...` syntax-checks files
  with the build's flags, natively or as 32-bit. Safe to run in parallel.
- `tools/fix_ptr_casts.py <build_dir> <file.cpp>...` rewrites the mechanical
  cases (int <-> pointer casts, Ptr32 through varargs) that clang reports, and
  lists the casts to `T **` for a decision.
