# Plan: splitting nanobind into headers (`nanobind`) and ABI providers

Status: final plan for review
Date: 2026-06-18

This document is the authoritative implementation plan. It records the
architecture, the decisions taken in design discussion, the one genuinely hard
sub-problem (domain/`internals` resolution), the frozen-ABI rules, and a phased
migration where every phase is independently testable.

---

## 1. Goal

Stop shipping `libnanobind` inside user wheels. Split nanobind into:

- **`nanobind`** — header-only. What users build against.
- **ABI providers** — Python packages shipping compiled backends (today's
  `src/*.cpp`), exposing them as function-pointer tables inside `PyCapsule`s.
  The official provider is the `nanobind-abi` distribution / `nanobind_abi`
  import module. Providers are built per (Python version x OS/arch x
  C++-runtime ABI x free-threaded).
- **Interpreter ABI registry** — a private entry in
  `PyInterpreterState_GetDict(PyInterpreterState_Get())`. Official and
  third-party ABI providers register their ABI-tagged capsules here so multiple
  providers can coexist.

The split is **universal**: every extension reaches the backend through the
capsule table. There is no surviving "statically link `libnanobind`" path.

`Py_LIMITED_API` on the extension is an **orthogonal, per-project flag**:

- With it: one wheel per (OS/arch x C++-runtime ABI), valid for CPython 3.9+.
- Without it: a per-Python-version build that keeps a little inline speed
  (inline `Py_INCREF`/`Py_DECREF`, the fast container accessors). Since all
  heavy machinery now lives in the backend either way, the gap is small and the
  choice is low-stakes.

In both cases the user wheel no longer contains `libnanobind`.

---

## 2. Architecture

Three runtime roles:

```
  extension wheel (header-only nanobind)     interpreter ABI registry
  --------------------------------------     ------------------------
  templated casters + impl trampolines       tag/version -> PyCapsule
  frozen construction structs                stored in PyInterpreterState dict
  inline forwarders -> nb_abi-> ...
  bootstrap (limited-API safe)        asks
  per-DSO: nb_abi*, internals handle

                         ABI provider package(s), e.g. nanobind_abi
                         -------------------------------------------
                         libnanobind (src/*.cpp), built without
                         Py_LIMITED_API, per py/os/c++-abi/ft
                         registers NB_ABI_TAG -> nb_abi table capsule
                         nb_internals (private)
```

At load the extension's bootstrap imports one or more ABI provider modules, then
looks in the interpreter ABI registry for the capsule matching the extension's
`NB_ABI_TAG` and minimum `NB_ABI_VERSION`. The extension validates the capsule,
stores the table pointer plus a per-domain `internals` handle, then calls the
backend to finish init. Thereafter every `detail::nb_*` call that the linker
used to resolve is an indirect call through the table.

This is NumPy's distribution model (`PyArray_API`: an append-only, version-
stamped function-pointer table fetched from a capsule). We adopt the same shape.

The audit of `include/nanobind/**` (recorded in `docs/design/abi_split.md`)
found **zero** header sites that read the type-object tail, dereference
`internals`, or read `func_data` storage after construction. The storage structs
are already backend-private in practice, so this is a freeze-and-rewire effort,
not a clawback of leaked accesses.

---

## 3. The function table

### 3.1 Single source of truth

The full backend interface is already centralized in `nb_lib.h` (the former
`NB_CORE detail::` surface). Drive both the table type and the backend's
table-population from one X-macro list so order and signatures cannot drift.
The Phase-2 static-linked table starts at `NB_ABI_VERSION == 1`. While this
development branch is not released, keep `NB_ABI_VERSION` at `1` even when
rewiring the in-progress table/trampoline ABI. Only start incrementing it after
the first public ABI boundary exists; then any later slot append increments it
to 2, 3, ...

```cpp
#define NB_ABI_FUNCTIONS(F)                                                 \
    F(nb_func_new,  PyObject *, (nb_internals *, const func_data_init_base *)) \
    F(nb_type_new,  PyObject *, (nb_internals *, const type_data_init *))      \
    F(nb_type_get,  bool,       (nb_internals *, const std::type_info *,       \
                                 PyObject *, uint8_t, cleanup_list *, void **))\
    F(raise_python_error_impl, void, ())  /* stateless: no internals */        \
    /* ... one line per current nb_lib.h entry, append-only ... */
```

The struct (header) and the populated instance (backend) both expand the list.
New revisions append lines only.

### 3.2 Table type and per-DSO pointer

```cpp
namespace nanobind::detail {

struct nb_abi {
    uint32_t struct_size;   // sizeof(nb_abi) as the backend built it
    uint32_t version;       // highest table revision the backend implements
    // function pointers, expanded from NB_ABI_FUNCTIONS, never reordered
};

// One per extension shared object. The whole namespace is hidden-visibility
// (NB_NAMESPACE), so this inline variable merges to a single definition within
// each DSO and is NOT shared across DSOs.
inline const nb_abi *nb_abi = nullptr;

// This extension's domain/interpreter state, set at bootstrap. Opaque here:
// nb_internals is forward-declared only, so headers never dereference it.
struct nb_internals;
inline nb_internals *nb_abi_internals = nullptr;

}
```

### 3.3 Forwarders keep call sites unchanged

`nb_lib.h` keeps every `detail::nb_*` name, reimplemented as an inline forwarder
so the hundreds of downstream call sites do not change:

```cpp
inline PyObject *nb_func_new(const func_data_init_base *f) noexcept {
    return nb_abi->nb_func_new(nb_abi_internals, f);
}
```

**Every `NB_CORE` function the extension calls must become a table slot.** In the
final split there is no `libnanobind` to static-link; the extension reaches the
backend only through the capsule table. So the forwarder rewrite covers the whole
`nb_lib.h` surface, not just the functions that consult `internals`. The only
`NB_CORE` entries that stay header-inline are trivial ones with no backend body
(`none_ref`, `true_ref`, `false_ref`).

Routing the stateless conversion/accessor functions (`load_*`, `str_*`,
`bytes_*`, `getattr`, the container and number accessors, ...) through the table
is also a speed win for limited-API extensions: their fast/slow split lives in
the backend (e.g. `load_int`'s `PyUnstable_Long_IsCompact` fast path under
`#if !defined(Py_LIMITED_API)`), and the backend is always compiled non-limited,
so a `Py_LIMITED_API` extension gets the fast path through the table instead of
the slow stable-API fallback it compiles inline today.

Two mechanical wrinkles in the rewrite:

- **Stateless functions sometimes need a distinct backend symbol.** A function
  that takes no `internals` handle and still has a public inline wrapper cannot
  also keep the same backend definition name (ODR clash with the wrapper). The
  current implementation uses private slot names for those cases, e.g.
  `raise_python_error_impl`, `type_get_slot_impl`,
  `trampoline_release_impl`, and `trampoline_leave_impl`. Stateless functions
  without a same-named public wrapper can keep their backend name (e.g.
  `load_cmplx`). Functions that *do* take the handle avoid this: the extra
  leading `nb_internals *` makes the backend impl a natural overload of the
  forwarder, so they keep their name.
- **Variadic functions** (`raise`, `raise_type_error`, `fail`) cannot forward
  `...` through a function pointer. Their slots take a `va_list` (`raise_v`,
  `raise_type_error_v`, `fail_v`, `chain_error_v`), while the public inline
  wrapper does `va_start`/`va_end`.

### 3.3a Wrappers are the public boundary only; the backend never self-calls through the table

The inline `nb_abi->...` forwarders exist for **one** purpose: to let header-only
extension code cross into the backend. Backend-internal code must **not** call
them. When one backend function needs another, it calls the implementation
directly and threads its own `internals` handle, e.g.
`nb_type_put(internals, cpp_type, ...)`, never `nb_type_put(cpp_type, ...)` (the
forwarder).

Two reasons this matters:

- **Re-entrancy.** The forwarder sources `nb_abi_internals`, the per-DSO cached
  handle. A backend function that routed through it would use that cached handle
  instead of the one threaded into it, which is exactly the cross-domain
  corruption section 4 forbids. Calling the impl with the threaded handle keeps
  the backend re-entrant.
- **Cost.** A self-call through the table is a needless indirect call on paths
  that already hold the handle.

Practical rule: inside `src/`, a bare `foo(args)` that resolves to a forwarder is
a bug; it must be `foo(internals, args)` (the impl overload) or the private
stateless backend helper (`foo_impl`, `foo_v`, or another slot target named in
`NB_ABI_FUNCTIONS`). The Phase-2 work that introduced the forwarders left some
backend-internal calls resolving to public forwarders (correct only because the
static build's `nb_abi` currently points at the same in-DSO table); a follow-up
pass must convert every such site to call the backend helper directly.

### 3.4 `cleanup_list` becomes public

`cleanup_list` moves into a public header with its layout frozen (because
`append()` is inlined and touches `m_size`/`m_capacity`/`m_data`/`m_local`
directly). It intentionally does **not** carry an `internals` pointer; section
4.1 chooses explicit `internals` threading through the function trampoline and
casters instead. Its two out-of-line methods forward through the table:

```cpp
void cleanup_list::expand()  noexcept { nb_abi->cleanup_list_expand(this); }
void cleanup_list::release() noexcept { nb_abi->cleanup_list_release(this); }
```

### 3.5 Bootstrap (resolves the chicken-and-egg)

Current Phase-2 state is deliberately still static-linked: `nb_module_exec()`
sets `detail::nb_abi = &nb_abi_storage` and resolves `nb_abi_internals` from the
interpreter dictionary. That keeps the test suite exercising the table
indirection before changing packaging. Phase 3 replaces this bootstrap with the
dynamic capsule load below.

The first call cannot use the table, so `NB_MODULE`'s `Py_mod_exec` function
fetches it first, using only limited-API-safe C-API. `PyInterpreterState_Get()`
and `PyInterpreterState_GetDict()` are part of the Stable ABI since Python 3.8,
so the bootstrap can use the interpreter dictionary directly even for abi3
extensions.

Extensions import one or more provider modules, then ask the interpreter ABI
registry for the capsule matching their `NB_ABI_TAG`. The registry has exactly
one active nanobind table version per interpreter; registering a different table
version fails. Older extensions are served by the active table when
`NB_ABI_VERSION <= registry.table_version`.

```cpp
static int nanobind_<name>_exec(PyObject *m) {
    // nb_abi_load(): import configured providers, then read
    //   PyInterpreterState_GetDict(PyInterpreterState_Get()),
    //   fetch tag NB_ABI_TAG, require registry.table_version >= NB_ABI_VERSION,
    //   PyCapsule_GetPointer(capsule, NB_ABI_TAG), set detail::nb_abi.
    if (!nanobind::detail::nb_abi_load())
        return -1;  // actionable error already set
    // nb_module_init does today's nb_module_exec work in the backend and
    // returns this domain's internals handle (section 5).
    nb_abi_internals = nb_abi->nb_module_init(NB_DOMAIN_STR, m);
    if (!nb_abi_internals) return -1;
    /* ... existing try/catch around the user's exec_impl ... */
}
```

`PyModuleDef.m_free` is a compile-time static, so it points at a tiny
extension-side trampoline `static void nb_module_free_tramp(void *m) {
nb_abi->nb_module_free(nb_abi_internals, m); }`.

The capsule name carries `NB_ABI_TAG`, and `PyCapsule_GetPointer` requires an
exact name match, so a C++-runtime-ABI or free-threaded mismatch fails cleanly
at this point instead of calling through an incompatible table.

### 3.5a ABI provider registry

The registry lives in `PyInterpreterState_GetDict(PyInterpreterState_Get())`
under a private key, e.g. `__nanobind_abi_registry__`. ABI backend packages are
providers. A provider can be the official `nanobind_abi` wheel, a
project-vendored wheel, or a local source build for a non-canonical C++ runtime.
Providers do not compete for the same module name; on import, they register
their capsule in the interpreter registry.

Logical registry contents:

```text
table_version: 1
providers:
  NB_ABI_TAG -> { capsule, provider_name }
```

Registration rules:

- First provider initializes `table_version`.
- Re-registering the same provider/capsule/tag is idempotent.
- Registering the same `NB_ABI_TAG` with a different capsule fails.
- Registering any provider with a different `nb_abi::version` fails. There is
  only one active nanobind table version per interpreter.
- Registering a different `NB_ABI_TAG` with the same table version is OK.

Lookup rules:

- Missing tag fails with an actionable import error listing the requested tag,
  requested table version, active table version (if any), and registered tags.
- Requested `NB_ABI_VERSION > table_version` fails cleanly and asks the user to
  install or import a newer provider.

Provider discovery is separate from registry storage. The default loader imports
the official provider (`nanobind_abi`) first. It may also import provider modules
named by a compile-time/runtime override (for example `NB_ABI_PROVIDERS`). This
makes custom ABI providers possible without taking over the official package
name while keeping the registry itself independent of the Python package
namespace.

### 3.6 Exception specifications

Each slot carries the **exact** exception specification of the function it points
to. The many `noexcept` entries (`nb_func_new`, `nb_type_new`, `nb_type_get`,
`nb_type_put`, ...) are declared `noexcept` in the table:

- It keeps the inline forwarders sound: they re-expose the same `noexcept`
  signature, and a throw out of a non-`noexcept` slot would hit `std::terminate`.
- A call through a `noexcept` function pointer is non-throwing, so the compiler
  emits no exception-handling landing pad at the (many, inlined) call sites in
  the extension. Declaring slots non-`noexcept` and relying on the implicit
  `noexcept`->non-`noexcept` pointer conversion would keep the runtime guarantee
  but lose this size/speed win.
- It is also a compile-time check: a `noexcept` slot can only be populated with a
  genuinely `noexcept` function (the reverse conversion is ill-formed). Since the
  struct and the backend's population both expand from `NB_ABI_FUNCTIONS`, slot
  and target match exactly.

The functions that throw by design stay non-`noexcept`: `raise`,
`raise_type_error`, `fail`, `raise_python_error` are `[[noreturn]]` and throw
`nb::python_error` / `std::exception`. Their exceptions unwind across the table
from backend into extension, which is why Section 8 mandates a shared,
dynamically-linked C++ runtime. The X-macro carries the spec in the declarator
argument, so no macro change is needed:
`F(nb_func_new, PyObject *, (nb_internals *, const func_data_init_base *) noexcept)`.

---

## 4. The hard sub-problem: domain / `internals` resolution

**This is the one place the shared-backend model genuinely diverges from
today, and it must be settled before the table signatures freeze.**

Today `libnanobind` is statically linked into each extension, so each extension
has its **own** global `internals` pointer, set to the right per-domain
`nb_internals` (the interpreter-dict capsule is keyed by `abi_tag` + `NB_DOMAIN`).
A single shared backend has only one code image and cannot have a per-extension
global, yet named domains must stay isolated (default domain shares across all
same-ABI extensions; `NB_DOMAIN=foo` is a separate registry).

**Resolution (decided): resolve domain to an `internals` handle once, then pass
it on every call.**

- `nb_module_init(domain, m)` performs the existing per-domain lookup/create in
  the interpreter dict and **returns** the `nb_internals *` for that domain. The
  extension stores it in the per-DSO static `nb_abi_internals` (section 3.2).
- **A table function takes the handle only if it consults per-domain state**
  (the type registry, the instance/keep-alive maps, the exception translators,
  the lifeline). Functions that obviously do not — error raising (`raise`,
  `raise_type_error`, `fail`, `raise_python_error`), the object conversions and
  int/float loaders, attribute/item access, call building, refcount helpers
  (`incref_checked`/`decref_checked`), `cleanup_list_expand`/`release` — omit it.
  Table signatures freeze, but the table is append-only, so a function later
  found to need `internals` is handled by **appending a new slot** that takes it
  and routing new headers there. The old slot keeps serving old extensions.
  A wrong "stateless" guess is therefore a cheap append, not a wall.
- The backend therefore holds **no global `internals`**. The pointer is threaded
  from each entry point through the backend's internal call chains. This is what
  makes the backend re-entrant: cross-domain re-entrancy (domain A calls into
  Python, which calls domain B, which returns, and A resumes) is correct with no
  save/restore, because each extension always passes its own handle.
- **Carve-out — interpreter-initiated entry points.** The `nb_func`/`nb_type`
  vectorcall slots (and `tp_init`, `tp_dealloc`, ...) are invoked by the
  interpreter, not through the table, so they cannot receive `internals` as an
  argument. They read it from the object: store an `nb_internals *` back-pointer
  in the private `func_data` / `type_data` storage at creation time (private
  storage, no ABI cost). Rule of thumb: every call the **extension** makes passes
  `internals`; every call **Python** makes reaches `internals` through the object.
  - *Why not the per-interpreter module-state APIs* (`PyType_GetModule`,
    `PyType_GetModuleState`, `PyType_GetModuleByDef`): wrong granularity
    (`internals` is per-domain, and a domain spans multiple modules, whereas
    module state is per-module), and for the `nb_func` vectorcall path
    `PyType_GetModule(Py_TYPE(callable))` returns nanobind's own metatype module,
    not the domain module, so a back-pointer is needed there regardless. The
    stash is also one load versus an instance->type->module->state chain (or an
    MRO walk). Stashing is subinterpreter-safe on its own because nanobind
    objects do not cross interpreters, so each interpreter's module-init sets its
    own object's pointer.
- Headers keep `nb_internals` opaque (forward-declared) and only pass the handle,
  so the clean-boundary audit result (no header dereference of `internals`) is
  preserved.

This keeps domain semantics exactly as today and is strictly cleaner than the
status quo (no global, re-entrant). The implementation cost is the mechanical
refactor that turns the backend's pervasive global `internals` into a threaded
parameter. Do not shortcut it with a backend-side global set at entry: that
corrupts under cross-domain re-entry.

Alternatives, considered and rejected:

- **Thread-local "current internals"**: keeps table signatures clean, but adds a
  TLS read on the hot cast path and needs save/restore for cross-domain
  re-entrancy.
- **Single global `internals` with domain-partitioned maps**: still needs the
  domain threaded to `nb_type_get`/`put`, so it has the same signature impact
  with more backend complexity.

Multiple-interpreters stays unsupported
(`Py_MOD_MULTIPLE_INTERPRETERS_NOT_SUPPORTED`, already set), which keeps the
per-DSO handle valid for the lifetime of the one interpreter. **Phase 0
validates the refactor cost and the hot-path impact; the frozen table signatures
already include the handle.**

### 4.1 Subinterpreter readiness (non-goal now, but keep the door open)

Removing the backend's global `internals` is precisely what subinterpreter
support requires, so this redesign turns nanobind from structurally incompatible
into structurally ready, at no extra cost. The backend becomes pure functions of
their `internals` argument, and the function table is immutable code that is safe
to share across interpreters.

The remaining hazard is that an extension `.so` is loaded once per process, so
any mutable C global in the extension (including the cached `nb_abi_internals`)
is shared across every subinterpreter that imports it, and under a per-interpreter
GIL they run concurrently. The discipline that avoids this is: **never rely on
the cached global on a path running under a live interpreter; source `internals`
from call context instead.**

- Python-initiated calls resolve `internals` from the stashed object pointer
  (per-object, hence per-interpreter-correct).
- The caster hot path should take `internals` from the already-resolved context
  rather than a global, threaded explicitly through the call chain.

**Frozen-ABI decision (settle before Phase 2): thread `internals` explicitly
through the function-call trampoline and the casters, and free the slot for it by
moving `rv_policy` to a compile-time parameter.** (Decided over the alternative of
having `cleanup_list` carry the handle; see "why not the carrier" below.)

Concretely:

- **`internals` becomes an explicit argument of `impl`**, passed by the
  dispatcher (from the object's `internals` back-pointer in the Python-initiated
  case) and forwarded into every `from_python`/`from_cpp` caster, which hand it to
  `nb_type_get`/`put`/etc. as their leading argument. The handle stays
  register-resident across the per-argument caster calls on x86_64 (SysV),
  aarch64, and Windows x64 (the compiler hoists the value parameter into a
  callee-saved register at `impl` entry; it is not reloaded, unlike a
  `cleanup->internals` field read that aliases through the `append()`-mutated
  `cleanup_list`).

- **`rv_policy` moves off the `impl` runtime signature to keep the argument count
  at five** (so the new `internals` slot does not become a 6th argument that
  spills to the stack under the 4-integer-register Windows x64 ABI). The chosen
  mechanism is a **compile-time NTTP**: `rv_policy` is known at the `def` site
  (`m.def("f", &f, nb::rv_policy::reference)`), so bake it into the trampoline
  instantiation rather than passing it at runtime. `impl` is a captureless lambda
  (it must decay to a function pointer for the type-erased dispatcher), so it
  cannot capture the policy; an NTTP is the only way to make it compile-time.
  This requires encoding the policy in the **type system** so `func_create` can
  recover it as a constant: turn the `rv_policy` constants into tag types
  (`policy_tag<P>` with an implicit `operator rv_policy()` so the pervasive
  runtime uses — `from_cpp(value, policy, ...)`, `nb::cast(value, policy)`, the
  dispatcher's `(rv_policy)(f->flags & 0b111)`, pybind11-compat surface — keep
  working unchanged), while preserving the `nb::rv_policy::reference` call syntax.
  Bonus beyond freeing the slot: a constant policy lets the compiler specialize
  each function's return conversion, pruning the dead `rv_policy` branches in
  `from_cpp` per trampoline. (A runtime-`rv_policy` escape hatch, if kept, needs a
  fallback trampoline that still takes the policy at runtime.)

This redesign is an internals/API break and should ship as **nanobind 3.0**.
Do not add compatibility wrappers that silently accept old type-caster
signatures. Built-in and user type casters must be updated to the new explicit
handle shape:

```cpp
bool from_python(handle, uint8_t, nb_internals *, cleanup_list *) noexcept;
static handle from_cpp(T &&, nb_internals *, rv_policy, cleanup_list *) noexcept;
```

The old `from_python(handle, uint8_t, cleanup_list *)` and
`from_cpp(T &&, rv_policy, cleanup_list *)` forms are intentionally not part of
the 3.0 boundary.

The resulting frozen `impl` shape:

```cpp
PyObject *(*impl)(void *, PyObject **, uint8_t, nb_internals *, cleanup_list *);
//                                      ^^^^^^^ call_flags byte
//                                               ^^^^^^^^^^^^ was rv_policy
```

The `uint8_t` argument is not a per-argument flag pointer. It is a compact
dispatch-context byte (`call_flags`) carrying only dynamic facts:

- whether this is the conversion-accepting overload pass,
- whether argument 0 is constructor `self`,
- whether constructor `self` came from the trusted vectorcall fast path,
- whether this is a copy constructor whose source argument must not convert.

Per-argument convert/accepts-none information is now encoded in the annotation
types themselves (`arg_tag<Flags>`, `arg_v_tag<Flags>`, locked variants). The
trampoline reconstructs the `cast_flags` byte for each argument from:

- the compile-time annotation flag byte,
- the C++ argument type (`none_disallowed`, optional/default handling),
- the single runtime `call_flags` byte.

This removes the dispatcher's per-overload `args_flags` allocation and
initialization. The dispatcher still passes `PyObject **args` because complex
and medium calls must sometimes build a normalized argument vector: keyword
resolution, defaults, `*args`, `**kwargs`, constructor-self-by-keyword, and
pre/post-call hooks all need a mutable/normalized `PyObject **`. The simple
dispatchers already pass `args_in` directly.

The annotation implementation deliberately uses simple top-level constexpr
values rather than recursive type plumbing:

- `arg` annotations carry one `static constexpr uint8_t flags`.
- `func_extra_apply()` has one overload for all arg/default/locked annotation
  forms and copies that byte into `arg_data_init`.
- `func_arg_info<I, IsMethod, Arg, Extra...>` names the few derived facts
  (`is_self`, `converts`, `accepts_none`, `dispatch_mask`) and
  `func_load_arg()` is the single load helper used by both hook and non-hook
  trampolines.

*Why not the `cleanup_list` carrier.* Having `cleanup_list` hold `internals` and
`nb_type_get` read `cleanup->internals` keeps the change localized (no `impl`
signature change, no `from_python` ripple), but `cleanup` is `nullptr` on the
common `nb::cast` paths (C++->Python `cast(value, policy)`; Python->C++ to a
pointer/reference, or `convert=false`) — it is only allocated for the dispatcher,
for `cast(value, policy, parent)`, and for `convert`-value casts. So the carrier
would need a `cleanup ? cleanup->internals : nb_abi_internals` fallback (branch +
global read) precisely on the busy `nb::cast` paths, and the field read aliases
through `append()`. Explicit threading is uniform across dispatch and `nb::cast`
(the latter feeds `nb_abi_internals` straight into `from_python`) and
register-friendlier, at the cost of the wider source change.

This keeps the subinterpreter door open with no future ABI break and removes the
global read from the dispatch hot path. The only surface still needing genuine
per-interpreter resolution is user-initiated C++ entry points outside a dispatch
(`nb::cast`, constructing objects from C++), which have no object to inherit from.
Until subinterpreters are targeted, the cached `nb_abi_internals` serves them;
afterwards they resolve via the interpreter-state dict (optionally cached in
module state) with no ABI change. Remaining per-DSO caches (`static_pyobjects` /
lifeline) are private storage and can be made per-interpreter if and when
subinterpreters become a goal.

Status: implemented in the development branch. The `impl` signature,
`rv_policy` tag-type change, explicit caster `internals` parameter, and
`call_flags` trampoline byte are all frozen-boundary moves that must settle
before the table/struct freeze (Phase 2). `nb_type_get`/`put`/`put_p`/
`put_unique[_p]`/`ndarray_export` take `internals` as a threaded argument rather
than reading a global; `cleanup_list` does **not** gain an `internals` field.
Because this branch is not public, `NB_ABI_VERSION` stays at `1` while these
development ABI changes are being refined.

---

## 5. Struct decoupling and the flag split

### 5.1 Naming

| Concept | Frozen construction record (public header) | Backend-private storage |
|---|---|---|
| Types | `type_data_init` | `type_data` |
| Functions | `func_data_init_base` / `func_data_init<N>` | `func_data` |
| Enums | `enum_data_init` | backend's own |
| Args | `arg_data_init` | `arg_data` (or backend's own) |
| Table | `nb_abi` (type), `nb_abi` (per-DSO pointer) | n/a |

Storage structs move out of `nb_class.h` / `nb_attr.h` into `src/` (private).

### 5.2 Decoupling

`type_data_init` stops inheriting from `type_data` and becomes a flat, standalone
record holding only construction inputs. Provisional partition (confirm against
`nb_type.cpp` in Phase 1):

- **In `type_data_init`**: `size`, `align`, construction `flags`, `name`,
  `type`, `destruct`, `copy`, `move`, `set_self_py`,
  `keep_shared_from_this_alive`, `pool_capacity`, and the init-only fields
  `scope`, `base`, `base_py`, `doc`, `type_slots`, `supplement_size`.
- **Backend storage only** (set during creation, not supplied): `type_py`,
  `alias_chain`, `vectorcall`, `dictoffset`, `weaklistoffset`, the
  `implicit`/`enum_tbl` union, the `supplement` pointer, the instance pool
  (`pool` / `pool_index`), `init` (the constructor `nb_func`, bound later), and
  the new `internals` back-pointer from section 4.

`func_data` already adds storage fields over the prelim base, so that split is
mostly a rename.

### 5.3 Construction flags vs internal flags

Keep flags **packed in a single field per init struct** — binding code expands
into one initialization sequence per `class_` / `def`, so field count
multiplies across the binary and `func_data_init` / `arg_data_init` are
high-multiplicity. Do not widen into multiple flag words.

Instead, separate the flag **namespaces**:

- `type_data_init` carries only **construction** flags: the 14 persistent
  construction inputs (`is_destructible`, `is_copy_constructible`,
  `is_move_constructible`, `has_destruct`, `has_copy`, `has_move`, `is_final`,
  `has_dynamic_attr`, `intrusive_ptr`, `has_shared_from_this`,
  `is_weak_referenceable`, `has_signature`, `is_generic`, `pooled`) plus the 5
  init-only flags (`has_supplement`, `has_doc`, `has_base`, `has_base_py`,
  `has_type_slots`) = 19 bits in the packed `flags:24` field, 5 free.
- The 5 flags that the backend sets at runtime are **dropped** from the init
  struct: `has_gc` (`nb_type.cpp`), `has_implicit_conversions` (`implicit.cpp`),
  `is_python_type` (`nb_type.cpp`), `has_new` / `has_nullary_new` (`nb_func.cpp`).
  Confirmed by grep: these are never set in headers.
- Storage `type_data` keeps the full internal flag set in its own private
  numbering, free to grow.
- `nb_type_new` translates construction flags into internal flags **once, in the
  backend**, costing zero extension binary size.

`func_data_init` flags stay a full `uint32_t` (already ~14 free bits, no
change). `enum_data_init` flags (`is_signed`, ...) are few — verify headroom and
apply the same construction/internal split if it carries any backend-only bits.

`arg_data_init.flag` widens `uint8_t` -> `uint16_t` (same single store, free
headroom).

If more than ~5 spare construction bits are wanted as freeze-once insurance, the
only single-field option is a full `uint32_t flags` (pull `align` to a
neighboring slot) for 13 free. Default to staying at `align:8 / flags:24` unless
we expect construction flags to grow fast.

---

## 6. Versioning and compatibility rules

- **Capsule name** = `NB_ABI_TAG`, encoding what must match for the table to be
  safe to call: a **boundary major** version + `NB_PLATFORM_ABI_TAG` (C++
  runtime/standard-library/debug ABI) + free-threaded marker. Drop `_stable`
  (the backend is never limited-API, and the extension's limited-API-ness does
  not change the boundary). Do not include `NB_INTERNALS_VERSION` in this public
  tag; internals storage is backend-private after the split. The tag macros move
  from `src/nb_abi.h` to a **public** header so the extension can compute its
  expected tag.
- **`nb_abi::version`** is the monotonic table revision (function count). The
  extension bakes in `NB_ABI_VERSION` (the revision its headers target). The
  backend serves any extension with `NB_ABI_VERSION <= nb_abi->version`. The
  initial table introduced in the static-linked Phase 2 work is version 1.
- **Forward-only guarantee**: the single active table version in the interpreter
  must serve older extensions. Provider too old + newer extension is allowed to
  fail, but cleanly ("upgrade or import a provider with a newer table version").
- **Frozen structs grow by appending fields gated by an existing flag bit.**
  `type_init_flags` already signals optional-field presence. Never reorder or
  resize an existing field, never repurpose a flag bit.
- **`NB_INTERNALS_VERSION`** stops gating cross-module boundary compatibility
  (storage no longer crosses any boundary). It remains internal to the backend,
  governing the private storage layout and the interpreter-dict internals key.
- **Freeze guards (CI)**: `static_assert` on `sizeof`/`offsetof` of every frozen
  struct (`type_data_init`, `func_data_init_base`, `arg_data_init`,
  `enum_data_init`, `cleanup_list`, the `nb_abi` prefix); a lint over
  `include/nanobind/**` that fails if a header ever reads the type tail,
  dereferences `internals`, or reads `func_data` storage (audit categories 1, 2,
  5 must stay empty).

---

## 7. Backend simplification (it is never limited-API)

Because the backend is always compiled **without** `Py_LIMITED_API`, delete from
`src/`:

- the `NB_SLOT` indirection and `internals->type_data_offset`,
- the cached `PyType_Type_tp_*` / `PyProperty_Type_tp_*` slot pointers,
- every `#if defined(Py_LIMITED_API)` branch in `src/`.

The backend always has full struct access. Header limited-API fallbacks (the ~45
guarded category-3 sites) **stay**, because the extension may be limited-API.
Net: limited-API conditionals live only in headers, never in the backend.

---

## 8. Lowering the limited-API floor to 3.9

The 3.12 requirement existed because `libnanobind` itself was limited-API. With
the backend non-limited and version-specific, only the **header** path must stay
within 3.9 limited API.

- Audit header limited-API usage for 3.9 availability (e.g. `PyType_GetSlot` on
  heap types, any post-3.9 stable additions). Move anything newer into a table
  call handled by the (version-specific) backend.
- Change `STABLE_ABI` to set `Py_LIMITED_API=0x03090000` and relax the
  `nb_defs.h:97-100` assertion.
- Test across 3.9 - 3.13+.

---

## 9. Build system and packaging

- `nanobind_add_module` no longer creates or links a
  `nanobind[-static|-abi3|-ft]` support library. It compiles the extension
  against header-only nanobind, bakes in `NB_ABI_VERSION` and the expected
  `NB_ABI_TAG`, and records a runtime dependency on at least one ABI provider
  (the official default is `nanobind_abi`).
- `STABLE_ABI` becomes a thin switch (sets `Py_LIMITED_API`, `.abi3` suffix). It
  no longer spins up a separate backend library.
- **Dynamic C++ runtime linkage becomes the enforced default.** Cross-binary C++
  exception propagation between extension and backend works only with a shared
  unwinder and RTTI, which is also why `std::type_info*` may cross the table and
  why `NB_PLATFORM_ABI_TAG` stays. Reject static libstdc++/libc++ for extensions
  in this model (generalize today's `MUSL_DYNAMIC_LIBCPP` intent).
- The nanobind project gains a build that produces official `nanobind-abi`
  provider wheels across (Python minor x OS/arch x C++-runtime ABI x ft). PyPI
  (manylinux/macOS/Windows) and conda-forge are separate C++-ABI ecosystems and
  need separate backend builds. Initial Windows scope is narrower: publish only
  the MSVC dynamic-runtime (`/MD`) `nanobind-abi` provider for now. Other MSVC
  runtime variants remain out of scope until there is demand, and mismatches
  still fail via `NB_PLATFORM_ABI_TAG`.
- Multiple ABI providers can be installed and imported in the same environment.
  The interpreter registry decides by `NB_ABI_TAG` and the single active table
  version, not by provider package name. A mismatch is detected at bootstrap and
  reported, not crashed. A provider may register more than one capsule if it
  bundles several backend variants, but all registered capsules must share the
  same `nb_abi::version`.

---

## 10. Migration phases (each shippable and testable)

**Phase 0 — De-risk (no shipped artifact).**
- Prototype the domain/`internals` handle-threading model (section 4) end to
  end on a minimal binding. This gates the frozen table signatures.
- Implement the context-threading shape that keeps the subinterpreter door open
  (section 4.1): thread `internals` explicitly through `impl` and the casters,
  and move `rv_policy` to a compile-time NTTP to free the `impl` argument slot.
  This changes the frozen `impl` signature and the `rv_policy` type, so settle it
  pre-freeze.
- Microbenchmark one added indirection on the cast/dispatch hot path
  (`nb_type_get`/`put`, `nb_func` dispatch) to retire the indirect-call risk.

**Phase 1 — Decouple structs (still static-linked, behavior unchanged).**
- Rename to `type_data_init` / `func_data_init*` / `enum_data_init` /
  `arg_data_init`. Make `type_data_init` standalone; move storage structs into
  `src/`. Confirm the field partition (section 5.2) against `nb_type.cpp`.
- Separate construction flags from internal flags; add the backend remap in
  `nb_type_new`. Widen `arg_data_init.flag`.
- Add freeze-guard `static_assert`s and the header lint.
- Run the full test suite. This phase is a pure cleanup.

**Phase 2 — Introduce the table indirection (still static-linked).**
- Define `nb_abi` from the `NB_ABI_FUNCTIONS` X-macro. Reimplement `nb_lib.h` as
  inline forwarders. Make `cleanup_list` public with forwarding methods.
- Backend populates the table; for now set `detail::nb_abi` and
  `nb_abi_internals` from a still-static init so the full suite exercises the
  indirection before distribution changes.
- Current status: table version 1 is in place, stateless equal-signature slots
  use private `_impl`/`_v` names, and public `NB_CORE` use is reduced to the
  static bootstrap pair `nb_module_exec`/`nb_module_free`.
- Before Phase 3, finish the backend self-call pass from section 3.3a so `src/`
  never routes through public forwarders.

**Phase 3 — Split distribution.**
- Build `src/` into an ABI provider module publishing an `NB_ABI_TAG`-named
  capsule, and make the official `nanobind_abi` provider register that capsule
  in the interpreter registry. Move ABI-tag macros to a public header.
- Rework `NB_MODULE` to the bootstrap (section 3.5); `nb_module_exec` ->
  `nb_module_init` (returns the internals handle); add the `m_free` trampoline.
  `nb_abi_load()` imports configured providers, then asks the interpreter
  registry for `NB_ABI_TAG` and `NB_ABI_VERSION`.
- `nanobind_add_module` stops linking the static library and adds the runtime
  dependency. Keep the old static path behind a transition flag until parity is
  proven.

**Phase 4 — Backend simplification.** Remove all limited-API paths from `src/`
(section 7); drop `_stable` from keys.

**Phase 5 — Lower the floor to 3.9** (section 8): header audit, then flip the
`STABLE_ABI` macro and the assertion; test 3.9 - 3.13+.

**Phase 6 — Packaging and rollout.** Build and publish official `nanobind-abi`
provider wheels (PyPI + conda-forge); document third-party provider registration
into the interpreter registry; wire actionable bootstrap errors; docs,
changelog, migration guide. Remove the transition flag.

**Phase 7 — Free-threading and future.** Keep ft a compile dimension (the
capsule tag carries `_ft`); keep frozen boundary structs ft-neutral so a single
limited-API extension can bind an ft backend once CPython supports limited-API
on free-threaded builds.

---

## 11. Implementation conventions

Comment sparingly. Prefer no comment when the code is self-evident, and a single
short line when it is not. Do not write paragraph-length explanations in the
sources. Reserve longer rationale for this plan or the commit message.

A large amount of code will need to be migrated as part of this work. Prefer
copying (with ``cp``) files into the ABI provider tree and then editing
in-place, over manual copying of text.

## 12. Risks and open questions

- **Domain/`internals` resolution (section 4)** — decided: resolve domain to a
  handle at bootstrap, pass it on every table call, read it from the object on
  interpreter-initiated entry points. Phase 0 validates the mechanical refactor
  cost (removing the backend's global `internals`) and the hot-path impact.
- **Indirect-call overhead** — mitigate via the Phase-0 benchmark; the hottest
  inline ops (`cleanup_list::append`, refcounting) stay inline via frozen
  layouts.
- **Frozen-forever layouts** — `type_data_init`, `func_data_init_base`,
  `arg_data_init`, `enum_data_init`, `cleanup_list`, and the `nb_abi` prefix can
  only grow. Review carefully before the first split release; freeze guards
  enforce it after.
- **Header 3.9 limited-API cleanliness** — the floor drop hinges on it; audit
  and push newer surface into the table.
- **Cross-binary exceptions require a shared C++ runtime** — enforce dynamic
  linkage; document.
- **Mixed C++ ABIs / custom providers** — the interpreter registry is the escape
  hatch: multiple provider packages can be imported and register distinct
  `NB_ABI_TAG` capsules in one place, provided they share the active table
  version.
- **Bootstrap failure ergonomics** — "no provider registered tag X / provider
  too old / wrong C++ ABI" must be actionable; this is the new top-of-funnel
  failure.
- **Field-provenance partition and flag remap correctness** — derive from `src`
  usage and cover with tests.
- **PyPy / sub-interpreters** — PyPy uses builtins for internals discovery;
  multiple interpreters stay unsupported. Confirm scope explicitly.
- **Old + new extension coexistence** — pre-split extensions keep their embedded
  `libnanobind` and still work; they interoperate (share types) with new-style
  extensions only when ABI/domain match, otherwise they fall back to separate
  `internals`, which is today's behavior for ABI-mismatched modules.

---

## 13. Naming reference

- Construction records: `type_data_init`, `func_data_init_base` /
  `func_data_init<N>`, `enum_data_init`, `arg_data_init`.
- Storage (private, in `src/`): `type_data`, `func_data`, etc.
- Function table: type `nb_abi`, per-DSO pointer `nb_abi`, per-DSO domain state
  `nb_abi_internals`.
- Versioning: `NB_ABI_TAG` (public boundary/C++ ABI identity and capsule name),
  `NB_ABI_VERSION` (table revision the headers target), `nb_abi::version`
  (revision the provider table supplies), `NB_INTERNALS_VERSION`
  (backend-internal storage version only).
