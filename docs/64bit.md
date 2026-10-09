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

Two encoding details matter in practice:

- A "pointer" below 4 GB encodes as itself. Nothing is mapped there on macOS
  (the 4 GB `__PAGEZERO`), so it is an integer the decompiled code typed as a
  pointer (a byte count, an index) and must survive `(int)Encode(p)`.
- A value in the handle range that no handle was issued for decodes to itself.
  The fastfile loader tests raw on-disk offsets (blocks 4-6 are >= 0x80000000)
  for null and -1 before converting them.

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
   `unsigned long *` (e.g. for `_BitScanReverse`) writes 8 bytes into 4, and a
   `long` counter "decremented" with `0xFFFFFFFF` grows by 4 billion instead
   (this deadlocked every FastCriticalSection).
8. A function called through a cast function pointer must really return what the
   pointer type says: on arm64 an `int` return does not fill a pointer register
   (the hunk allocators returned encoded ints through `void *(*)()`; they now go
   through typed adapters). The reverse breaks too: the FastFile/LoadObj
   dispatchers called pointer-returning functions as `int (*)()` and decoded the
   result, i.e. the low half of a native pointer (right only when the image sits
   at exactly 0x100000000, as under lldb with ASLR off). Call such functions
   directly. Likewise a native pointer read through another union member
   (`Ptr32_Decode(dvar->current.integer)` for a string dvar): read the pointer.
9. The decompiler aliases fields through other fields: negative indices
   (`scene.dynSModelVisBitsCamera[i - 13]` is `scene.dpvs.entVisData[i]`),
   `&objBuf[1758][2]` meaning the constant 0x4000000, `_user[1].flags` meaning a
   field after a `HunkUser` header. Find the real field from the i386 layout
   (`tools/layout_diff.py`, or clang's `-fdump-record-layouts`) and name it.
10. Pointers kept in enum fields are not caught by `-Wint-to-pointer-cast`
   (`(T *)entry->asset.type` was the asset free list).
11. Literal sizes and offsets of native structs are i386 values: byte-pointer
   initialisers (`DObjCreate` wrote `buf[112]`, `*((unsigned int *)buf + 29)`;
   `tagInfo_s` likewise), field offsets passed as arguments (`G_Find(0, 356, ..)`
   is `offsetof(gentity_s, classname)`), strides (`SV_LocateGameData(.., 760, ..)`),
   allocation budgets (`*_AllocateClientMemory_SizeRequired` must cover what the
   matching allocator takes), pool strides (`/ sizeof(GfxImage)` over pool
   entries that are unions with an 8-byte `next`). Name the field or use sizeof.
   `tools/audit_rawofs.py` only sees accesses through struct-typed pointers.
12. The original compiler folded identical functions, and the decompiled tables
   kept whichever name survived: the asset size table used `XAnimTreeSize` for
   the 8-byte asset types. Before changing what such a function returns, check
   every table that references it.
13. Session 1's mechanical rewrite turned `*(unsigned int *)slot = (unsigned int)p`
   and `x.integer = (int)p` into encoded stores. That is right only where the slot
   is 32-bit memory; where the reader takes a native pointer (`UILocalVar::u.string`,
   devgui's free list in `label`, `jqGetWorkercmdParam`), make both sides agree.
   Likewise `(T **)structPtr` out-parameters whose first field is a `Ptr32`
   (`CreateTexture(.., (IDirect3DTexture9 **)image, ..)`): use a local.
14. `setjmp`/`longjmp`: locals changed after `setjmp` and read after the `longjmp`
   are indeterminate. MSVC kept them in memory; clang and GCC keep them in
   registers at -O1 and up. The script VM resumes its interpreter loop after a
   script error this way, so `VM_Execute_0` is compiled without optimization
   (`SCR_VM_SETJMP_SAFE`); it is a small share of server time.

15. Sizes the engine passes to its own queues are i386 sizeofs: render commands
   (`R_GetCommandBuffer(RC_DRAW_FRAMED, 44)`), worker commands (`jqWorkerCmd`'s data
   size), `R_ClearScene`'s `132 * sceneDObjCount`, the flame pools' list strides.
   Use `sizeof`. Their readers often index the data by i386 offsets too
   (`*((unsigned __int16 *)data + 4)` for a `DpvsDynamicCellCmd`): use the fields.
16. Bitfield words addressed as `*((unsigned int *)&s + N)` (`flameGeneric_s` type/id,
   `GfxStaticModelDrawStream::which_lod`): on 64-bit word N is a pointer half. Name
   the bitfield; the masks match its layout.
17. Decompiled loops that walk a list typed as the container (`(PhysGlob *)node`,
   `(phys_free_list<T> *)node` then `->m_ptr_list[244]`) read node data at i386
   offsets: use the list's iterator and the element's fields.
18. A non-format variadic function that `va_arg`s pointers (`DDL_MoveTo`) must get
   pointers, not `operand.internals.intVal`: 4 bytes in, 8 read.
19. Out-of-range float-to-int conversions are undefined: the decompiled
   `((int)-fabs(unsignedDiff) >> 31) & (i + 1)` "next index with wrap-around"
   relied on x87/SSE truncation; clang folded it so a glass loop never ended.
   Write the wrap explicitly.

Runtime structs that the code addresses by raw 32-bit offsets keep their i386
layout too: `centity_s` (205 sites) has `Ptr32` pointer fields and a strict size
assert on every build. `tools/audit_rawofs.py` lists such structs.

The macOS build turns the relevant warnings into errors: pointer <-> 32-bit int
casts both ways, and `Ptr32` objects passed through varargs. clang does not apply
that check to functions with a format attribute (`snprintf`, `sprintf`); run
`tools/audit_format.py` for those. The
macOS and Linux builds compile with `-fno-strict-aliasing -fwrapv`: the
decompiled code type-puns through pointer casts and assumes wrapping signed
arithmetic, as MSVC compiled it.

## Tools

- `tools/layout_diff.py <build_dir> <file.cpp>... [--only A,B]` compiles the files
  for i386 (the original ABI) and natively, and lists structs whose size or field
  offsets differ. All 732 asset structs must match.
- `tools/check_file.py <build_dir> [--i386] <file.cpp>...` syntax-checks files
  with the build's flags, natively or as 32-bit. Safe to run in parallel.
- `tools/fix_ptr_casts.py <build_dir> <file.cpp>...` rewrites the mechanical
  cases (int <-> pointer casts, Ptr32 through varargs) that clang reports, and
  lists the casts to `T **` for a decision.
- Audits for what the compiler accepts (need Homebrew LLVM's clang-query):
  - `tools/audit_64bit.py`: `*(int *)&ptr`, `(T **)&ptr32Slot`, `(T *)enumValue`.
  - `tools/audit_rawofs.py`: structs addressed as `(int *)p + N`.
  - `tools/audit_allocs.py`, `tools/audit_memlit.py`, `tools/audit_qsort.py`:
    allocations, memset/memcpy and qsort/bsearch sized with a 32-bit sizeof
    (audit_memlit looks through casts on the destination and matches
    `literal * count`; `64 * sizeof(x)` shows up as a false positive).
  When the dedicated server first booted, all but `audit_rawofs.py` (a listing,
  not a check) were clean apart from three allocation hits in commented-out or
  `#if 0` code (mem_track.cpp, jobqueue_all.cpp).
