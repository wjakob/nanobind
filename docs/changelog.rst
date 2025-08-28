.. _changelog:

.. cpp:namespace:: nanobind

Changelog
#########

nanobind uses a `semantic versioning <http://semver.org>`__ policy for its API.
It also has a separate ABI version that is *not* subject to semantic
versioning.

The ABI version is relevant whenever a type binding from one extension module
should be visible in another nanobind-based extension module. In this
case, both modules must use the same nanobind ABI version, or they will be
isolated from each other. Releases that don't explicitly mention an ABI version
below inherit that of the preceding release.

Version 2.8.0 (July 16, 2025)
-----------------------------

- Added :cpp:class:`nb::fallback <fallback>` wrapper type, which is a
  :cpp:class:`nb::handle <handle>` that always requires implicit conversion
  during casting. This is convenient when adding catch-all overloads that must
  handle arbitrary Python objects, without interfering with implicit conversion
  of arguments in other overloads.

- The ``nanobind::literals`` namespace now includes ``_s`` to create a Python string
  from source code literals. (PR `#1051
  <https://github.com/wjakob/nanobind/pull/1051>`__).

- Added :cpp:func:`nb::dict::empty() <dict::empty>`,
  :cpp:func:`nb::list::empty() <list::empty>`, :cpp:func:`nb::set::empty()
  <set::empty>`, and :cpp:func:`nb::tuple::empty() <tuple::empty>` convenience
  methods. (PR `#1052 <https://github.com/wjakob/nanobind/pull/1052>`__)

- Added a :cpp:func:`nb::dict::get() <dict::get>` function to perform
  dictionary lookups with a fallback value in case of failures. (commit `d38284
  <https://github.com/wjakob/nanobind/commit/d38284e573e404503862a550804e4c2413e11e01>`__).

- Nanobind now uses multi-phase (as opposed to single-phase) initialization API
  when registering modules. However, multi-interpreter extensions remain
  unsupported. (PR `#1059 <https://github.com/wjakob/nanobind/pull/1059>`__).

- Added :cpp:class:`nb::frozenset` that wraps the Python ``frozenset`` type.
  (PR `#1068 <https://github.com/wjakob/nanobind/pull/1068>`__)

- Miscellaneous fixes and improvements (
  commits
  `d4b245 <https://github.com/wjakob/nanobind/commit/d4b245ad69f729c3d2095be4c1cb5b94810dae26>`__,
  `667451 <https://github.com/wjakob/nanobind/commit/667451fb4566dcd7151d64d81e118f9ba194a889>`__,
  `62fc99 <https://github.com/wjakob/nanobind/commit/62fc996018d9ea4d51af9c86cf008c2562b4eeab>`__,
  `8497f7 <https://github.com/wjakob/nanobind/commit/8497f778d006b60e44e99530f241e22f9603bb04>`__).

Version 2.7.0 (Apr 18, 2025)
----------------------------

- nanobind now provides a zero-copy type caster for
  ``Eigen::Map<Eigen::SparseMatrix>``. (PRs `#1003
  <https://github.com/wjakob/nanobind/pull/1003>`__, `#782
  <https://github.com/wjakob/nanobind/pull/782>`__).

- Made handling of return value policies in Eigen type casters more consistent
  with the rest of nanobind. (Issue `#971
  <https://github.com/wjakob/nanobind/issues/971>`__, commit `5cdf59
  <https://github.com/wjakob/nanobind/commit/5cdf58984e7a8b520935c3771029fe0e87edee73>`__).

- The Eigen sparse matrix caster now correctly handles ``scipy.sparse`` objects
  with unsorted indices. (PR `#981
  <https://github.com/wjakob/nanobind/pull/981>`__).

- Nanobind's CMake stub generation command :cmake:command:`nanobind_add_stub`
  now detects when an extension uses sanitizers (TSAN, ASAN, UBSAN). It then
  injects the sanitizer library into the Python process ahead of time so that
  the extension can be loaded. Previously, stub generation failed in
  such cases. (PR `#1000 <https://github.com/wjakob/nanobind/pull/1000>`__).

- The entries of stub files are now sorted in their original definition order.
  Previously, they were alphabetically sorted, which caused issues with
  external tooling. (PR `#938
  <https://github.com/wjakob/nanobind/pull/938>`__).

- Fixed detection and handling of imports and types in external modules in
  stubgen that could lead to incorrect declarations in some cases. (PRs `#939
  <https://github.com/wjakob/nanobind/pull/939>`__, `#940
  <https://github.com/wjakob/nanobind/pull/940>`__).

- The stub generator now detects method aliases and preserves this information
  instead of duplicating the definition. (PR `#735
  <https://github.com/wjakob/nanobind/pull/735>`__).

- Corrected a flaw in the recommended implementation of ``tp_traverse`` in
  garbage-collected bindings. (PRs `#1015
  <https://github.com/wjakob/nanobind/pull/1015>`__).

- Added support for binding functions that accept a ``std::variant<...>`` that
  is not default-constructible (because its first alternative isn't). (PR `#987
  <https://github.com/wjakob/nanobind/pull/987>`__).

- Added support for casting const-qualified ``std::unique_ptr<T>`` values. (PR
  `#988 <https://github.com/wjakob/nanobind/pull/988>`__).

- ``nb::typed<T, ...>`` now supports construction from ``T``, making it more
  ergonomic to return values with type annotations. (PR `#1012
  <https://github.com/wjakob/nanobind/pull/1012>`__

- Miscellaneous fixes and improvements (PRs
  `#1014 <https://github.com/wjakob/nanobind/pull/1014>`__,
  `#1005 <https://github.com/wjakob/nanobind/pull/1005>`__,
  `#1004 <https://github.com/wjakob/nanobind/pull/1004>`__,
  `#990 <https://github.com/wjakob/nanobind/pull/990>`__,
  `#997 <https://github.com/wjakob/nanobind/pull/997>`__, commits
  `f2b08c <https://github.com/wjakob/nanobind/commit/f2b08c936ec4b1dd06d374fef2637d89daa905f4>`__,
  `eef931 <https://github.com/wjakob/nanobind/commit/eef93122ad49ea6ef02d645497d426a4dfc303bd>`__,
  `f1b2f5 <https://github.com/wjakob/nanobind/commit/f1b2f579adb538671c985c9c86a878f6e82de597>`__,
  `dbd602 <https://github.com/wjakob/nanobind/commit/dbdb602cfa00d46600e048f992cd3f81540c777d>`__,
  `2c83fb <https://github.com/wjakob/nanobind/commit/2c83fbbbed3b82edc6491efbdc83e56f98da0db2>`__,
  `87de84 <https://github.com/wjakob/nanobind/commit/87de84d3afad270c02099af82132240b3216ac3b>`__).

Version 2.6.1 (Mar 28, 2025)
----------------------------

- nanobind assigns an ABI tag to compiled extensions and uses it to isolate
  incompatible extensions from each other. This tag was unnecessarily
  fine-grained, often causing isolation where an actual ABI compatibility was
  not present. This release updates the tagging scheme to address this
  long-standing inconvenience. (PR `#778
  <https://github.com/wjakob/nanobind/pull/778>`__).

- Added specialized function dispatchers to accelerate calls to 0 and
  1-argument functions. (PR `#944
  <https://github.com/wjakob/nanobind/pull/944>`__).

- Improved the efficiency of :cpp:func:`nb::getattr(obj, key,
  default) <getattr>` in cases where ``obj[key]`` does not exist. (commit
  `bb05f5
  <https://github.com/wjakob/nanobind/commit/bb05f5503aef9b70498302bf30bf958e8cc605c7>`__).

- ABI version 16.

- Miscellaneous fixes and improvements (PRs `#913
  <https://github.com/wjakob/nanobind/pull/913>`__, `#914
  <https://github.com/wjakob/nanobind/pull/914>`__, `#916
  <https://github.com/wjakob/nanobind/pull/916>`__, `#931
  <https://github.com/wjakob/nanobind/pull/931>`__, `#978
  <https://github.com/wjakob/nanobind/pull/978>`__, commit `1595d2
  <https://github.com/wjakob/nanobind/commit/1595d2d40717d65835ed984b06cfc2b4da0e4858>`__).

Version 2.6.0 (Mar 28, 2025)
----------------------------

- This release was yanked due to a regression.

Version 2.5.0 (Feb 2, 2025)
---------------------------

- Added :cpp:class:`nb::def_visitor\<..\> <def_visitor>`, which can be used to
  define your own binding logic that operates on a :cpp:class:`nb::class_\<..\>
  <class_>` when an instance of the visitor object is passed to
  :cpp:func:`class_::def()`. This generalizes the mechanism used by
  :cpp:class:`init`, :cpp:class:`new_`, etc, so that you can create binding
  abstractions that "feel like" the built-in ones. (PR `#884
  <https://github.com/wjakob/nanobind/pull/884>`__)

- Added some special forms for :cpp:class:`nb::typed\<T, Ts...\> <typed>`
  (PR `#835 <https://github.com/wjakob/nanobind/pull/835>`__):

  - ``nb::typed<nb::object, T>`` or ``nb::typed<nb::handle, T>`` produces
    a parameter or return value that will be described like ``T`` in function
    signatures but accepts any Python object at runtime.

  - ``nb::typed<nb::callable, R(Args...)>`` produces a Python callable signature
    ``Callable[[Args...], R]``; similarly, ``nb::typed<nb::callable, R(...)>``
    (with a literal ellipsis) produces the Python ``Callable[..., R]``.

- It is now possible to create Python subclasses of C++ classes that define
  their constructor bindings using :cpp:struct:`nb::new_() <new_>`. Previously,
  attempting to instantiate such a Python subclass would instead produce an
  instance of the base C++ type. Note that it is still not possible to override
  virtual methods in such a Python subclass, because the object returned by the
  :cpp:struct:`new_() <new_>` constructor will generally not be an instance of
  the alias/trampoline type. (PR `#859
  <https://github.com/wjakob/nanobind/pull/859>`__)

- Fixed the :cpp:class:`nb::int_ <int_>` constructor so that it casts to
  an integer when invoked with a floating point argument.

- Multi-level inheritance (e.g., ``A → B → C``) previously did not work on Python
  3.12+ when a base class (e.g., ``A``) provided a trampoline implementation.
  This is now fixed. (commit `92d9cb
  <https://github.com/wjakob/nanobind/commit/92d9cb3d62b743a9eca2d9d9d8e5fb14a1e00a2a>`__).

- A new ``NB_SUPPRESS_WARNINGS`` parameter of
  :cmake:command:`nanobind_add_module` that marks the nanobind and Python
  include directories as
  `SYSTEM <https://cmake.org/cmake/help/latest/command/include_directories.html>`__
  include directories, which suppresses any potential warning messages
  originating there. This is mainly of relevance for projects that artificially
  raise the warning level using flags like ``-pedantic``, ``-Wcast-qual``,
  ``-Wsign-conversion``. (PR `#868
  <https://github.com/wjakob/nanobind/pull/868>`__).

- Fixed (benign) reference leaks that could occur when ``std::shared_ptr<T>``
  instances were still alive at interpreter shutdown time. (commit `fb8157
  <https://github.com/wjakob/nanobind/commit/fb815762fdb8476cfd293e3717ca41c8bb890437>`__).

- The floating-point type caster now only performs value-changing narrowing
  conversions during the implicit conversion phase. They can be entirely
  avoided by passing the :cpp:func:`.noconvert() <arg::noconvert>` argument
  annotation. (PR `#829 <https://github.com/wjakob/nanobind/pull/829>`__)

- The ``std::complex`` type caster now only performs value-changing narrowing
  conversions during the implicit conversion phase.  They can be entirely
  avoided by passing the :cpp:func:`.noconvert() <arg::noconvert>` argument
  annotation.  Also, during the implicit conversion phase, if the Python object
  is not a complex number object but has a ``__complex__()`` method, it will be
  called. (PR `#854 <https://github.com/wjakob/nanobind/pull/854>`__)

- Fixed an overly strict check that could cause a function taking an
  :cpp:class:`nb::ndarray\<...\> <ndarray>` to refuse specific types of
  column-major input without implicit conversion. (PR `#847
  <https://github.com/wjakob/nanobind/pull/847>`__, commit `b95eb7
  <https://github.com/wjakob/nanobind/commit/b95eb755b5a651a40562002be9ca8a4c6bf0acb9>`__).

Fixes for free-threaded builds
^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^

- Fixed a race condition in free-threaded extensions that could occur when
  :cpp:func:`nb::make_iterator <make_iterator>` was concurrently used by
  multiple threads. (PR `#832 <https://github.com/wjakob/nanobind/pull/832>`__).

- Fixed a race condition in free-threaded extensions that could occur when
  multiple threads access the Python object associated with the same C++
  instance, which does not exist yet and therefore must be created. (issue
  `#867 <https://github.com/wjakob/nanobind/issues/867>`__, PR `#887
  <https://github.com/wjakob/nanobind/pull/887>`__).

- Removed double-checked locking patterns in accesses to internal data
  structures to ensure correct free-threaded behavior on architectures with
  weak memory ordering such as ARM (PR `#819
  <https://github.com/wjakob/nanobind/pull/819>`__).

Version 2.4.0 (Dec 6, 2024)
---------------------------

- Added a function annotation :cpp:class:`nb::call_policy\<Policy\>()
  <call_policy>` which supports custom function wrapping logic,
  calling ``Policy::precall()`` before the bound function and
  ``Policy::postcall()`` after. This is a low-level interface intended
  for advanced users. The precall and postcall hooks are able to
  observe the Python objects forming the function arguments and return
  value, and the precall hook can change the arguments.  See the linked
  documentation for more details, important caveats, and an example policy.
  (PR `#767 <https://github.com/wjakob/nanobind/pull/767>`__)

- :cpp:func:`nb::make_iterator <make_iterator>` now accepts its iterator
  arguments by value, rather than by forwarding reference, in order to
  eliminate the hazard of storing a dangling C++ iterator reference in the
  returned Python iterator object. (PR `#788
  <https://github.com/wjakob/nanobind/pull/788>`__)

- The ``std::variant`` type_caster now does two passes when converting from Python.
  The first pass is done without implicit conversions. This fixes an issue where
  ``std::variant<U, T>`` might cast a Python object wrapping a ``T`` to a ``U`` if
  there is an implicit conversion available from ``T`` to ``U``.
  (issue `#769 <https://github.com/wjakob/nanobind/issues/769>`__)

- Restored support for constructing types with an overloaded ``__new__`` that
  takes no arguments, which regressed with the constructor vector call
  acceleration that was added in nanobind 2.2.0.
  (issue `#786 <https://github.com/wjakob/nanobind/issues/786>`__)

- Bindings for augmented assignment operators (as generated, for example, by
  ``.def(nb::self += nb::self)``) now return the same object in Python in the
  typical case where the C++ operator returns a reference to ``*this``.
  Previously, after ``a += b``, ``a`` would be replaced with a copy.
  (PR `#803 <https://github.com/wjakob/nanobind/pull/803>`__)

- Added an overload to :cpp:func:`nb::isinstance <isinstance>` which tests if a
  Python object is an instance of a Python class. This is in addition to the
  existing overload, which tests if a Python object is an instance of a bound
  C++ class. (PR `#805 <https://github.com/wjakob/nanobind/pull/805>`__).

- Added support for overriding static properties, such as those defined using
  ``def_prop_ro_static``, in subclasses. Previously this would fail with an
  error. (PR `#806 <https://github.com/wjakob/nanobind/pull/806>`__).

- Other minor fixes and improvements. (PRs `#771
  <https://github.com/wjakob/nanobind/pull/771>`__, `#772
  <https://github.com/wjakob/nanobind/pull/772>`__, `#748
  <https://github.com/wjakob/nanobind/pull/748>`__, and `#753
  <https://github.com/wjakob/nanobind/pull/753>`__)

Version 2.3.0
-------------

There is no version 2.3.0 due to a deployment mishap.

- Added casters for `Eigen::Map<Eigen::SparseMatrix<...>` types from the `Eigen library
  <https://eigen.tuxfamily.org/index.php?title=Main_Page>`__. (PR `#782
  <https://github.com/wjakob/nanobind/pull/782>`_).

Version 2.2.0 (October 3, 2024)
-------------------------------

- nanobind can now target `free-threaded Python
  <https://py-free-threading.github.io>`__, which replaces the `Global
  Interpreter Lock (GIL)
  <https://en.wikipedia.org/wiki/Global_interpreter_lock>`__ with a
  fine-grained locking scheme (see `PEP 703
  <https://peps.python.org/pep-0703/>`__) to better leverage multi-core
  parallelism. A :ref:`separate documentation page <free-threaded>` explains this in
  detail (PRs `#695 <https://github.com/wjakob/nanobind/pull/695>`__, `#720
  <https://github.com/wjakob/nanobind/pull/720>`__)

- nanobind has always used `PEP 590 vector calls
  <https://www.python.org/dev/peps/pep-0590>`__ to efficiently dispatch calls
  to function and method bindings, but it lacked the ability to do so for
  constructors (e.g., ``MyType(arg1, arg2, ...)``).

  Version 2.2.0 adds this missing part, which accelerates object
  construction by up to a factor of 2×. The difference is
  especially pronounced when passing keyword arguments to
  constructors. Note that this improvement only applies to
  Python version 3.9 and newer (PR
  `#706 <https://github.com/wjakob/nanobind/pull/706>`__, commits
  `#e24d7f <https://github.com/wjakob/nanobind/commit/e24d7f3434a6bbcc33cd8965632dc47f943fb2f8>`__,
  `#0acecb <https://github.com/wjakob/nanobind/commit/0acecb474874f286119dce2b97b84142b6ada1a8>`__,
  `#77f910 <https://github.com/wjakob/nanobind/commit/77f910f2a92c88f2c5512f3c375b4fe94369558e>`__,
  `#2c96d5 <https://github.com/wjakob/nanobind/commit/2c96d5ae2fdbca030dccb1d01c457c7c5df29a0d>`__).

* A new :cpp:class:`nb::is_flag() <is_flag>` annotation in
  :cpp:class:`nb::enum_\<T\>() <enum_>` produces enumeration
  bindings deriving from :py:class:`enum.Flag`, which enables
  bit-wise combination using compatible operators (``&``, ``|``,
  ``^``, and ``~``). Further combining the annotation with
  :cpp:class:`nb::is_arithmetic() <is_flag>` creates
  enumerations deriving from :py:class:`enum.IntFlag`. (PRs
  `#599 <https://github.com/wjakob/nanobind/pull/599>`__,
  `#688 <https://github.com/wjakob/nanobind/pull/688>`__,
  `#688 <https://github.com/wjakob/nanobind/pull/688>`__,
  `#727 <https://github.com/wjakob/nanobind/pull/727>`__,
  `#732 <https://github.com/wjakob/nanobind/pull/732>`__)

* A refactor of :cpp:class:`nb::ndarray\<...\> <ndarray>` was an opportunity to
  realize three usability improvements:

  1. The constructor used to return new nd-arrays from C++ now considers
     all template arguments:

     - **Memory order**: :cpp:class:`c_contig`, :cpp:class:`f_contig`.
     - **Shape**: :cpp:class:`nb::shape\<3, 4, 5\> <shape>`, etc.
     - **Device type**: :cpp:class:`nb::device::cpu <device::cpu>`,
       :cpp:class:`nb::device::cuda <device::cuda>`, etc.
     - **Framework**: :cpp:class:`nb::numpy <numpy>`,
       :cpp:class:`nb::pytorch <pytorch>`, etc.
     - **Data type**: ``uint64_t``, ``std::complex<double>``, etc.

     Previously, only the **framework** and **data type** annotations were
     taken into account when returning nd-arrays, while all of them were
     examined when *accepting* arrays during overload resolution. This
     inconsistency was a repeated source of confusion among users.

     To give an example, the following now works out of the box without the
     need to redundantly specify the shape and strides to the ``Array``
     constructor below:

     .. code-block:: cpp

        using Array = nb::ndarray<float, nb::numpy, nb::shape<4, 4>, nb::f_contig>;

        struct Matrix4f {
            float m[4][4];
            Array data() { return Array(m); }
        };

        nb::class_<Matrix4f>(m, "Matrix4f")
            .def("data", &Matrix4f::data, nb::rv_policy::reference_internal);

  2. A new nd-array :cpp:func:`.cast() <ndarray::cast>` method forces the
     immediate creation of a Python object with the specified target framework
     and return value policy, while preserving the type signature in return
     values. This is useful to :ref:`return temporaries (e.g. stack-allocated
     memory) <ndarray-temporaries>` from functions.

  3. Added a new and more general mechanism ``nanobind::detail::dtype_traits<T>``
     to declare custom ndarray data types like ``float16`` or ``bfloat16``. The old
     interface (``nanobind::ndarray_traits<T>``) still exists but is deprecated
     and will be removed in the next major release. See the :ref:`documentation
     <ndarray-nonstandard>` for details.

  There are two minor but potentially breaking changes:

  1. The nd-array type caster now interprets the
     :cpp:enumerator:`nb::rv_policy::automatic_reference
     <rv_policy::automatic_reference>` return value policy analogously to the
     :cpp:enumerator:`nb::rv_policy::automatic <rv_policy::automatic>`, which
     means that it references a memory region when the user specifies an
     ``owner``, and it otherwise copies. This makes it safe to use the
     :cpp:func:`nb::cast() <cast>` and :cpp:func:`nb::ndarray::cast()
     <ndarray::cast>` functions that use this policy as a default.

  2. The :cpp:class:`nb::any_contig <any_contig>` memory order annotation,
     which previously did nothing, now accepts C- or F-contiguous arrays and
     rejects non-contiguous ones.

  For further details on the nd-array changes, see PR `#721
  <https://github.com/wjakob/nanobind/pull/721>`__, For further details on the
  nd-array changes, see PR `#742
  <https://github.com/wjakob/nanobind/pull/742>`__, and commit `4647ef
  <https://github.com/wjakob/nanobind/commit/4647efcc45d96e530d41a3461cd9727656bc2ca3>`__.

- The NVIDIA CUDA compiler (``nvcc``) is now explicitly supported and included
  in nanobind's CI test suite (PR `#710
  <https://github.com/wjakob/nanobind/pull/710>`__).

* Added support for return value policy customization to the type casters of
  ``Eigen::Ref<...>`` and ``Eigen::Map<...>`` (commit `67316e
  <https://github.com/wjakob/nanobind/commit/67316eb88955a15e8e89a57ce9a53d8d66263287>`__).

* Added the :cpp:class:`bytearray` wrapper type. (PR `#654
  <https://github.com/wjakob/nanobind/pull/654>`__)

* The :cpp:class:`nb::ellipsis <ellipsis>` type now renders as ``...`` when
  used in :cpp:class:`nb::typed\<...\> <typed>` (PR `#705
  <https://github.com/wjakob/nanobind/pull/705>`__).

* The :cpp:class:`nb::sig("...") <sig>` annotation now supports `inline type
  parameter lists
  <https://docs.python.org/3/reference/compound_stmts.html#type-params>`__ such
  as ``def first[T](l: Sequence[T]) -> T`` (PR `#704
  <https://github.com/wjakob/nanobind/pull/704>`__).

* Fixed implicit conversion of complex nd-arrays. (issue `#709
  <https://github.com/wjakob/nanobind/issues/709>`__)

* Casting via :cpp:func:`nb::cast <cast>` can now specify an owner object for
  use with the :cpp:enumerator:`nb::rv_policy::reference_internal
  <rv_policy::reference_internal>` return value policy (PR `#667
  <https://github.com/wjakob/nanobind/pull/667>`__).

* The ``std::optional<T>`` type caster is now implemented in such a way that it
  can also accommodate non-STL frameworks, such as Boost, Abseil, etc. (PR
  `#675 <https://github.com/wjakob/nanobind/pull/675>`__)

* ABI version 15.

* Minor fixes and improvements (PRs
  `#703 <https://github.com/wjakob/nanobind/pull/703>`__,
  `#724 <https://github.com/wjakob/nanobind/pull/724>`__,
  `#723 <https://github.com/wjakob/nanobind/pull/723>`__,
  `#722 <https://github.com/wjakob/nanobind/pull/722>`__,
  `#715 <https://github.com/wjakob/nanobind/pull/715>`__,
  `#696 <https://github.com/wjakob/nanobind/pull/696>`__,
  `#693 <https://github.com/wjakob/nanobind/pull/693>`__,
  commit `75d259 <https://github.com/wjakob/nanobind/commit/75d259c7c16db9586e5cd3aa4715e09a25e76d83>`__).

Version 2.1.0 (Aug 11, 2024)
----------------------------

* Temporary workaround for a internal compiler error in version 17.10 of the MSVC
  compiler. This workaround will be removed once fixed versions are deployed on
  GitHub actions. (issue `#613
  <https://github.com/wjakob/nanobind/issues/613>`__, commit `f2438b
  <https://github.com/wjakob/nanobind/commit/f2438bb73a1673e4ad9d0c84d353a88cf54e55bf>`__).

* nanobind no longer prevents casting to a C++ container of pointers ``T*``
  where ``T`` is a type with a user-defined type caster if the caster seems to
  operate by extracting a ``T*`` from the Python object rather than a ``T``.
  This change was prompted by discussion `#605
  <https://github.com/wjakob/nanobind/discussions/605>`__.

* Switched nanobind wheel generation from `setuptools
  <https://github.com/pypa/setuptools>`__ to `scikit-build-core
  <https://github.com/scikit-build/scikit-build-core>`__ (PR `#618
  <https://github.com/wjakob/nanobind/discussions/618>`__).

* Improved handling of ``const``-ness in :cpp:class:`nb::ndarray <ndarray>` (PR
  `#491 <https://github.com/wjakob/nanobind/discussions/491>`__).

* Keyword argument annotations are now properly supported with
  :cpp:struct:`nb::new_ <new_>`, passed in the same way they would be with
  :cpp:struct:`nb::init <init>`. (issue `#668
  <https://github.com/wjakob/nanobind/issues/668>`__)

* Ability to use :cpp:func:`nb::cast <cast>` to create object with the
  :cpp:enumerator:`nb::rv_policy::reference_internal
  <rv_policy::reference_internal>` return value policy (PR `#667
  <https://github.com/wjakob/nanobind/pull/667>`__).

* Enable ``char`` type caster to produce ``'\0'`` (PR `#661
  <https://github.com/wjakob/nanobind/pull/661>`__).

* Added ``.def_static()`` member to :cpp:class:`nb::enum_ <enum_>`, which had
  been lost in a redesign of the enumeration implementation in nanobind version
  2.0.0. (commit `38990e
  <https://github.com/wjakob/nanobind/commit/38990ea33bb499bcc23607147555bf5bb00dcf62>`__).

* Fixes for two minor sources of memory leaks (PR
  `#595 <https://github.com/wjakob/nanobind/pull/595>`__,
  `#647 <https://github.com/wjakob/nanobind/pull/647>`__).

* The nd-array wrapper :cpp:class:`nb::ndarray <ndarray>` now properly handles
  CuPy arrays (`#594 <https://github.com/wjakob/nanobind/pull/594>`__).

* Added :cpp:func:`nb::hash() <hash>`, a wrapper for the Python ``hash()``
  function (commit `91fafa5
  <https://github.com/wjakob/nanobind/commit/01fafa5b9e1de0f1ab2a9d108cd0fce20ab9568f>`__).

* Various minor ``stubgen`` fixes (PRs
  `#667 <https://github.com/wjakob/nanobind/pull/667>`__,
  `#658 <https://github.com/wjakob/nanobind/pull/658>`__,
  `#632 <https://github.com/wjakob/nanobind/pull/632>`__,
  `#620 <https://github.com/wjakob/nanobind/pull/620>`__,
  `#592 <https://github.com/wjakob/nanobind/pull/592>`__).

Version 2.0.0 (May 23, 2024)
----------------------------

The 2.0.0 release of nanobind is entirely dedicated to *types* [#f1]_! The
project has always advertised seamless Python ↔ C++ interoperability, and this
release tries to bring a similar level of interoperability to static type
checkers like `MyPy <https://github.com/python/mypy>`__, `PyRight
<https://github.com/microsoft/pyright>`__, `PyType
<https://github.com/google/pytype>`__, and editors with interactive
autocompletion like `Visual Studio Code <https://code.visualstudio.com>`__,
`PyCharm <https://www.jetbrains.com/pycharm/>`__, and many other `LSP
<https://en.wikipedia.org/wiki/Language_Server_Protocol>`__-compatible IDEs.

This required work on three fronts:

1. **Stub generation**: the above tools all analyze Python code statically
   without running it. Because the import mechanism of compiled extensions
   depends the Python interpreter, these tools weren't able to inspect the
   contents of nanobind-based extensions.

   The usual solution involves writing `stubs
   <https://typing.readthedocs.io/en/latest/source/stubs.html>`__ that expose
   the module contents to static analysis tools. However, writing stubs by hand
   is tedious and error-prone.

   This release adds tooling to automatically extract stubs from existing
   extensions. The process is fully integrated into the CMake-based build
   system and explained in a :ref:`new documentation section <stubs>`.

2. **Better default annotations**: once stubs were available, this revealed the
   next problem: the default nanobind-provided function and class signatures
   were too rudimentary, and this led to a user poor experience.

   The release therefore improves many builtin type caster so that they produce
   more accurate type signatures. For example, the STL ``std::vector<T>``
   caster now renders as ``collections.abc.Sequence[T]`` in stubs when it is
   used as an *input*, and ``list[T]`` when it is used as part of a return
   value. The :cpp:func:`nb::make_*_iterator() <make_iterator>` family of
   functions return typed iterators, etc.

3. **Advanced customization**: a subset of the type signatures in larger
   binding projects will generally require further customization. The features
   listed below aim to enable precisely this:

   * In Python, many built-in types are *generic* and can be *parameterized* (e.g.,
     ``list[int]``). The :cpp:class:`nb::typed\<T, Ts...\> <typed>` wrapper
     enables such parameterization within C++ (for example, the
     ``int``-specialized list would be written as ``nb::typed<nb::list,
     int>``). :ref:`Read more <typing_generics_parameterizing>`.

   * The opposite is also possible: passing :cpp:class:`nb::is_generic()
     <is_generic>` to the class binding constructor

     .. code-block:: cpp

        nb::class_<MyType>(m, "MyType", nb::is_generic())

     produces a *generic* type that can be parameterized in Python (e.g.
     ``MyType[int]``). :ref:`Read more <typing_generics_creating>`.

   * The :cpp:class:`nb::sig <sig>` annotation overrides the
     signature of a function or method, e.g.:

     .. code-block:: cpp

        m.def("f", &f, nb::sig("def f(x: Foo = Foo(0)) -> None"), "docstring");

     Each binding of an overloaded function can be customized separately. This
     feature can be used to add decorators or control how default arguments are
     rendered. :ref:`Read more <typing_signature_functions>`.

   * The :cpp:class:`nb::sig <sig>` annotation can also override *class
     signatures* in generated stubs. Stubs often take certain liberties in
     deviating somewhat from the precise type signature of the underlying
     implementation. For example, the following annotation adds an abstract
     base class advertising that the class implements a typed iterator.

     .. code-block:: cpp

        using IntVec = std::vector<int>;

        nb::class_<IntVec>(m, "IntVec",
                           nb::sig("class IntVec(collections.abc.Iterable[int])"));

     Nanobind can't subclass Python types, hence this declaration is
     technically untrue. On the flipside, such a declaration can assist static
     checkers and improve auto-completion in visual IDEs. This is fine since
     these tools only perform a static analysis and never import the actual
     extension. :ref:`Read more <typing_signature_classes>`.

   * The :cpp:struct:`nb::for_setter <for_setter>` and
     :cpp:struct:`nb::for_getter <for_getter>` annotations enable passing
     function binding annotations (e.g., signature overrides) specifically to
     the setter or the getter part of a property.

   * The :cpp:class:`nb::arg("name") <arg>` argument annotation (and
     ``"name"_a`` shorthand) now have a :cpp:func:`.sig("signature")
     <arg::sig>` member to control how a default value is rendered in the stubs
     and docstrings. This provides more targeted control compared to overriding
     the entire function signature.

   * Finally, nanobind's stub generator supports :ref:`pattern files
     <pattern_files>` containing custom stub replacement rules. This catch-all
     solution addresses the needs of advanced binding projects, for which the
     above list of features may still not be sufficient.

Most importantly, it was possible to support these improvements with minimal
changes to the core parts of nanobind.

These release breaks API and ABI compatibility, requiring a new major version
according to `SemVer <http://semver.org>`__. The following changes are
noteworthy:

* The :cpp:class:`nb::enum_\<T\>() <enum_>` binding declaration is now a
  wrapper that creates either a :py:class:`enum.Enum` or :py:class:`enum.IntEnum`-derived type.
  Previously, nanobind relied on a custom enumeration base class that was a
  frequent source of friction for users.

  This change may break code that casts entries to integers, which now only
  works for arithmetic (:py:class:`enum.IntEnum`-derived) enumerations. Replace
  ``int(my_enum_entry)`` with ``my_enum_entry.value`` to work around the issue.

* The :cpp:func:`nb::bind_vector\<T\>() <bind_vector>` and
  :cpp:func:`nb::bind_map\<T\>() <bind_map>` interfaces were found to be
  severely flawed since element access (``__getitem__``) created views into the
  internal state of the STL type that were not stable across subsequent
  modifications.

  This could lead to unexpected changes to array elements and undefined
  behavior when the underlying storage was reallocated (i.e., use-after-free).

  nanobind 2.0.0 improves these types so that they are safe to use, but this
  means that element access must now copy by default, potentially making them
  less convenient. The documentation of :cpp:func:`nb::bind_vector\<T\>()
  <bind_vector>` discusses the issue at length and presents alternative
  solutions.

* The functions :cpp:func:`nb::make_iterator() <make_iterator>`,
  :cpp:func:`nb::make_value_iterator() <make_value_iterator>` and
  :cpp:func:`nb::make_key_iterator() <make_key_iterator>` suffer from the same
  issue as :cpp:func:`nb::bind_vector() <bind_vector>` explained above.

  nanobind 2.0.0 improves these operations so that they are safe to use, but
  this means that iterator access must now copy by default, potentially making
  them less convenient. The documentation of :cpp:func:`nb::make_iterator()
  <make_iterator>` discusses the issue and presents alternative solutions.

* The ``nb::raw_doc`` annotation was found to be too inflexible and was
  removed in this version.

* The ``nb::typed`` wrapper listed above actually already existed in previous
  nanobind versions but was awkward to use, as it required the user to provide
  a custom type formatter. This release makes the interface more convenient.

* The ``nb::any`` placeholder to specify an unconstrained
  :cpp:class:`nb::ndarray <ndarray>` axis was removed. This name was given to a
  new wrapper type :cpp:class:`nb::any` indicating ``typing.Any``-typed
  values.

  All use of ``nb::any`` in existing code must be replaced with ``-1`` (for
  example, ``nb::shape<3, nb::any, 4>`` → ``nb::shape<3, -1, 4>``).

* :ref:`Keyword-only arguments <kw_only>` are now supported, and can be
  indicated using the new :cpp:struct:`nb::kw_only() <kw_only>` function
  annotation. (PR `#448 <https://github.com/wjakob/nanobind/pull/448>`__).

* nanobind classes now permit overriding ``__new__``, in order to
  support C++ singletons, caches, and other types that expose factory
  functions rather than ordinary constructors. Read the section on
  :ref:`customizing Python object creation <custom_new>` for more details.
  (PR `#473 <https://github.com/wjakob/nanobind/pull/473>`__).

* When binding methods on a class ``T``, nanobind will now produce a Python
  function that expects a self argument of type ``T``. Previously, it would
  use the type of the member pointer to determine the Python function
  signature, which could be a base of ``T``, which would create problems
  if nanobind did not know about that base.
  (PR `#471 <https://github.com/wjakob/nanobind/pull/471>`__).

* nanobind can now handle keyword arguments that are not interned, which avoids
  spurious ``TypeError`` exceptions in constructs like
  ``fn(**pickle.loads(...))``. The speed of normal function calls (which
  generally do have interned keyword arguments) should be unaffected. (PR `#469
  <https://github.com/wjakob/nanobind/pull/469>`__).

* The ``owner=nb::handle()`` default value of the :cpp:class:`nb::ndarray
  <ndarray>` constructor was removed since it was bug-prone. You now have to
  specify the owner explicitly. The previous default (``nb::handle()``)
  continues to be a valid argument.

* There have been some changes to the API for type casters in order to
  avoid undefined behavior in certain cases. (PR `#549
  <https://github.com/wjakob/nanobind/pull/549>`__).

  * Type casters that implement custom cast operators must now define a
    member function template ``can_cast<T>()``, which returns false if
    ``operator cast_t<T>()`` would raise an exception and true otherwise.
    ``can_cast<T>()`` will be called only after a successful call to
    ``from_python()``, and might not be called at all if the caller of
    ``operator cast_t<T>()`` can cope with a raised exception.
    (Users of the ``NB_TYPE_CASTER()`` convenience macro need not worry
    about this; it produces cast operators that never raise exceptions,
    and therefore provides a ``can_cast<T>()`` that always returns true.)

  * Many type casters for container types (``std::vector<T>``,
    ``std::optional<T>``, etc) implement their ``from_python()`` methods
    by delegating to another, "inner" type caster (``T`` in these examples)
    that is allocated on the stack inside ``from_python()``. Container casters
    implemented in this way should make two changes in order to take advantage
    of the new safety features:

    * Wrap your ``flags`` (received as an argument of the outer caster's
      ``from_python`` method) in ``flags_for_local_caster<T>()`` before
      passing them to ``inner_caster.from_python()``. This allows nanobind
      to prevent some casts that would produce dangling pointers or references.

    * If ``inner_caster.from_python()`` succeeds, then also verify
      ``inner_caster.template can_cast<T>()`` before you execute
      ``inner_caster.operator cast_t<T>()``. A failure of
      ``can_cast()`` should be treated the same as a failure of
      ``from_python()``.  This avoids the possibility of an exception
      being raised through the noexcept ``load_python()`` method,
      which would crash the interpreter.

  The previous ``cast_flags::none_disallowed`` flag has been removed;
  it existed to avoid one particular source of exceptions from a cast
  operator, but ``can_cast<T>()`` now handles that problem more generally.

* ABI version 14.

.. rubric:: Footnote

.. [#f1] The author of this library had somewhat of a revelation after
   switching to a `new editor <https://neovim.io>`__ and experiencing the
   benefits of interactive Python code completion and type checking for the
   first time. This experience also showed how nanobind-based extension were
   previously a second-class citizen in this typed world, prompting the changes
   in this release.

Version 1.9.2 (Feb 23, 2024)
----------------------------

* Nanobind instances can now be :ref:`made weak-referenceable <weak_refs>` by
  specifying the :cpp:class:`nb::is_weak_referenceable <is_weak_referenceable>` tag
  in the :cpp:class:`nb::class_\<..\> <class_>` constructor. (PR `#335
  <https://github.com/wjakob/nanobind/pull/335>`__, commits `fc7709
  <https://github.com/wjakob/nanobind/commit/fc770930468313e5a69364cfd1bbdab9bc0ab208>`__,
  `3562f6 <https://github.com/wjakob/nanobind/commit/3562f692409f29bd9cef0d9eec2ee7e26e53a055>`__).

* Added a :cpp:class:`nb::bool_ <bool_>` wrapper type. (PR `#382
  <https://github.com/wjakob/nanobind/pull/382>`__, commit `90dfba
  <https://github.com/wjakob/nanobind/commit/90dfbaf4c8c410d819cb9be44a3455898c8c2638>`__).

* Ensure that the GIL is held when releasing :cpp:class:`nb::ndarray
  <ndarray>`. (issue `#377 <https://github.com/wjakob/nanobind/issues/377>`__,
  commit `a968e8
  <https://github.com/wjakob/nanobind/commit/a958e8d966f5af64c84412ca801a405042bbcc0b>`__).

* :cpp:func:`nb::try_cast() <try_cast>` no longer crashes the interpreter when
  attempting to cast a Python ``None`` to a C++ type that was bound using
  :cpp:class:`nb::class_\<...\> <class_>`. Previously this would raise an
  exception from the cast operator, which would result in a call to
  ``std::terminate()`` because :cpp:func:`try_cast() <try_cast>` is declared
  ``noexcept``. (PR `#386 <https://github.com/wjakob/nanobind/pull/386>`__).

* Fixed memory corruption in a PyPy-specific code path in
  :cpp:func:`nb::module_::def_submodule() <module_::def_submodule>` (commit
  `21eaff
  <https://github.com/wjakob/nanobind/commit/21eaffc263c13a5373546d8957e4152e65b1e8ac>`__).

* Don't implicitly convert complex to non-complex nd-arrays. (issue `#364
  <https://github.com/wjakob/nanobind/issues/364>`__, commit `ea2569
  <https://github.com/wjakob/nanobind/commit/ea2569f705b9d12185eea67db399a373d37c75aa>`__).

* Support for non-assignable types in the ``std::optional<T>`` type caster (PR
  `#358 <https://github.com/wjakob/nanobind/pull/358>`__, commit `9c9b64
  <https://github.com/wjakob/nanobind/commit/0c9b6489cd3fe8a0a5a858e364983e99b06101ce>`__).

* nanobind no longer assumes that docstrings provided to function binding (of
  type ``const char *``) have an infinite lifetime and it makes copy. (issue
  `#393 <https://github.com/wjakob/nanobind/pull/393>`__, commit `b3b6f4
  <https://github.com/wjakob/nanobind/commit/b3b6f44e55948986e02cdbf67e04d9cdd11c4aa4>`__).

* Don't pass compiler flags if they may be unsupported by the used compiler.
  This gets NVCC to work out of the box (that said, this change does not
  elevate NVCC to being an *officially* supported compiler). (issue `#383
  <https://github.com/wjakob/nanobind/pull/383>`__, commit `a307ea
  <https://github.com/wjakob/nanobind/commit/a307eacaa9902daa190adc428168cf64007dff9e>`__).

* Added a CMake install target to the nanobind build system. (PR `#356
  <https://github.com/wjakob/nanobind/pull/356>`__, commit `6bde65
  <https://github.com/wjakob/nanobind/commit/5bde6527dc43535982a36ffa02d41275c5e484d9>`__,
  commit `978dbb
  <https://github.com/wjakob/nanobind/commit/978dbb1d6aaeee7530d57cf3e8d558e099a4eec6>`__,
  commit `f5d8de
  <https://github.com/wjakob/nanobind/commit/f5d8defc68a5c6a79b0e64de016ee52dde6ea54d>`__).

* ABI version 13.

* Minor fixes and improvements.

Version 1.9.0-1.9.1 (Feb 18, 2024)
----------------------------------

Releases withdrawn because of a regression. The associated changes are
listed above in the 1.9.2 release notes.

Version 1.8.0 (Nov 2, 2023)
---------------------------

* nanobind now considers two C++ ``std::type_info`` instances to be equal when
  their mangled names match. The previously used pointer comparison was fast
  but fragile and often caused multi-part extensions to not recognize each
  other's types. This version introduces a two-level caching scheme (search by
  pointer, then by name) to fix such problems once and for all, while avoiding
  the cost of constantly comparing very long mangled names. (commit `b515b1
  <https://github.com/wjakob/nanobind/commit/b515b1f7f2f4ecc0357818e6201c94a9f4cbfdc2>`__).

* Fixed casting of complex-valued constant :cpp:class:`nb::ndarray\<T\>
  <ndarray>` instances. (PR `#338
  <https://github.com/wjakob/nanobind/pull/338>`__, commit `ba8c7f
  <https://github.com/wjakob/nanobind/commit/ba8c7fa55f2d0ad748cad1dd4af2b22979ebc46a>`__).

* Added a type caster for ``std::nullopt_t`` (PR `#350
  <https://github.com/wjakob/nanobind/pull/350>`__).

* Added the missing C++ → Python portion of the type caster for
  ``Eigen::Ref<..>`` (PR `#334
  <https://github.com/wjakob/nanobind/pull/334>`__).

* Minor fixes and improvements.

* ABI version 12.


Version 1.7.0 (Oct 19, 2023)
----------------------------

New features
^^^^^^^^^^^^

* The nd-array class :cpp:class:`nb::ndarray\<T\> <ndarray>` now supports
  complex-valued ``T`` (e.g., ``std::complex<double>``). For this, the header
  file ``nanobind/stl/complex.h`` must be included. (PR `#319
  <https://github.com/wjakob/nanobind/pull/319>`__, commit `6cbd13
  <https://github.com/wjakob/nanobind/commit/6cbd1387753ea8f519ac0fe2242f0a54dd670ede>`__).

* Added the function :cpp:func:`nb::del() <del>`, which takes an arbitrary
  accessor object as input and tries to delete the associated entry.
  The C++ statement

  .. code-block:: cpp

     nb::del(o[key]);

  is equivalent to ``del o[key]`` in Python. (commit `4dd745
  <https://github.com/wjakob/nanobind/commit/4dd74596ac7b0f850cb0144f42a438124b91720c>`__).

* Exposed several convenience functions for raising exceptions as public API:
  :cpp:func:`nb::raise <raise>`, :cpp:func:`nb::raise_type_error
  <raise_type_error>`, and :cpp:func:`nb::raise_python_error
  <raise_python_error>`. (commit `0b7f3b
  <https://github.com/wjakob/nanobind/commit/0b7f3b1d2a182bda8b95826a3f98cc3e2d0402db>`__).

* Added :cpp:func:`nb::globals() <globals>`. (PR `#311
  <https://github.com/wjakob/nanobind/pull/311>`__, commit `f0a9eb
  <https://github.com/wjakob/nanobind/commit/f0a9ebd9cd384ac554312247526b120102563e53>`__).

* The ``char*`` type caster now accepts ``nullptr`` and converts it into a
  Python ``None`` object. (PR `#318
  <https://github.com/wjakob/nanobind/pull/317>`__, commit `30a6ba
  <https://github.com/wjakob/nanobind/commit/30a6bac97a89bfafad82c2c5b6ef4516c00c35d6>`__).

* Added the function :cpp:func:`nb::is_alive() <is_alive>`, which returns
  ``false`` when nanobind was destructed by Python (e.g., during interpreter
  shutdown) making further use of the API illegal. (commit `b431d0
  <https://github.com/wjakob/nanobind/commit/b431d040f7b0585e9901856ee6c9b72281a37fa8>`__).

* Minor fixes and improvements.

* ABI version 11.

Bugfixes
^^^^^^^^

* The behavior of the :cpp:class:`nb::keep_alive\<Nurse, Patient\>
  <keep_alive>` function binding annotation was changed as follows: when the
  function call requires the implicit conversion of an argument, the lifetime
  constraint now applies to the newly produced argument instead of the original
  object. The change was rolled into a minor release since the former behavior
  is arguably undesirable and dangerous. (commit `9d4b2e
  <https://github.com/wjakob/nanobind/commit/9d4b2e317dbf32efab4ed41b6c275f9dbbbcf29f>`__).

* STL type casters previously raised an exception when casting a Python container
  containing a ``None`` element into a C++ container that was not able to
  represent ``nullptr`` (e.g., ``std::vector<T>`` instead of
  ``std::vector<T*>``). However, this exception was raised in a context where
  exceptions were not allowed, causing the process to be ``abort()``-ed, which
  is very bad. This issue is now fixed, and such conversions are refused. (PR
  `#318 <https://github.com/wjakob/nanobind/pull/318>`__, commits `d1ad3b
  <https://github.com/wjakob/nanobind/commit/d1ad3b91346a1566f42fdf194a3ed9c3eeec5858>`__
  and `5f25ae
  <https://github.com/wjakob/nanobind/commit/5f25ae0eb9691fbe03a20bcb9f604277ccc1884b>`__).

* The STL sequence casters (``std::vector<T>``, etc.) now refuse to unpack
  ``str`` and ``bytes`` objects analogous to pybind11. (commit `7e4a88
  <https://github.com/wjakob/nanobind/commit/7e4a88b7ccc047ce34ae8ae99492d46b1acf341a>`__).


Version 1.6.2 (Oct 3, 2023)
---------------------------

* Added a missing include file used by the new intrusive reference counting
  sample implementation from v1.6.0. (commit `31d115
  <https://github.com/wjakob/nanobind/commit/31d115fce310475fed0f539b9446cc41ba9ff4d4>`__).

Version 1.6.1 (Oct 2, 2023)
---------------------------

* Added missing namespace declaration to the :cpp:class:`ref` intrusive
  reference counting RAII helper class added in version 1.6.0. (commit `3ba352
  <https://github.com/wjakob/nanobind/commit/3ba3522e99c8f1f4bcc7c172abd2006eeaa8eaf8>`__).


Version 1.6.0 (Oct 2, 2023)
---------------------------

New features
^^^^^^^^^^^^

* Several :cpp:class:`nb::ndarray\<..\> <ndarray>` improvements:

  1. CPU loops involving nanobind nd-arrays weren't getting properly vectorized.
     This release of nanobind adds *views*, which provide an efficient
     abstraction that enables better code generation. See the documentation
     section on :ref:`array views <ndarray-views>` for details.
     (commit `8f602e
     <https://github.com/wjakob/nanobind/commit/8f602e187b0634e1df13ba370352cf092e9042c0>`__).

  2. Added support for nonstandard arithmetic types (e.g., ``__int128`` or
     ``__fp16``) in nd-arrays. See the :ref:`documentation section
     <ndarray-nonstandard>` for details. (commit `49eab2
     <https://github.com/wjakob/nanobind/commit/49eab2845530f84a1f029c5c1c5541ab3c1f9adc>`__).

  3. Shape constraints like :cpp:class:`nb::shape\<nb::any, nb::any, nb::any\>
     <shape>` are tedious to write. Now, there is a shorter form:
     :cpp:class:`nb::ndim\<3\> <ndim>`. (commit `1350a5
     <https://github.com/wjakob/nanobind/commit/1350a5e15b28e80ffc2130a779f3b8c559ddb620>`__).

  4. Added an explicit constructor that can be used to add or remove nd-array
     constraints. (commit `a1ac207
     <https://github.com/wjakob/nanobind/commit/a1ac207ab82206b8e50fe456f577c02270014fb3>`__).

* Added the wrapper class :cpp:class:`nb::weakref <weakref>`. (commit `78887f
  <https://github.com/wjakob/nanobind/commit/78887fc167196a7568a5cef8f8dfbbee09aa7dc4>`__).

* Added the methods :cpp:func:`nb::dict::contains() <dict::contains>` and
  :cpp:func:`nb::mapping::contains() <mapping::contains>` to the Python type
  wrappers. (commit `64d87a
  <https://github.com/wjakob/nanobind/commit/64d87ae01355c247123613f140cef8e71bc98fc7>`__).

* Added :cpp:func:`nb::exec() <exec>` and :cpp:func:`nb:eval() <eval>`. (PR `#299
  <https://github.com/wjakob/nanobind/pull/299>`__).

* Added a type caster for ``std::complex<T>``. (PR `#292
  <https://github.com/wjakob/nanobind/pull/292>`__, commit `dcbed4
  <https://github.com/wjakob/nanobind/commit/dcbed4fe1500383ad1f4dff47cacbf0f2e6b1d3f>`__).

* Added an officially supported sample implementation of :ref:`intrusive
  reference counting <intrusive>` via the :cpp:class:`intrusive_counter`
  :cpp:class:`intrusive_base`, and :cpp:class:`ref` classes. (commit `3fa1af
  <https://github.com/wjakob/nanobind/commit/3fa1af5e9e6fd0b08d13e16bb425a18963854829>`__).

Bugfixes
^^^^^^^^

* Fixed a serious issue involving combinations of bound types (e.g., ``T``) and
  type casters (e.g., ``std::vector<T>``), where nanobind was too aggressive in
  its use of *move semantics*. Calling a bound function from Python taking such
  a list (e.g., ``f([t1, t2, ..])``) would destruct ``t1, t2, ..`` if the type
  ``T`` exposed a move constructor, which is highly non-intuitive and no
  longer happens as of this fix.

  Further investigation also revealed inefficiencies in the previous
  implementation where moves were actually possible but not done (e.g., for
  functions taking an STL vector by value). Some binding projects may see
  speedups as a consequence of this change. (issue `#307
  <https://github.com/wjakob/nanobind/issues/307>`__, commit `122015
  <https://github.com/wjakob/nanobind/commit/1220156961ce2d0c96a525f3c27b88e824b997ce>`__).


Version 1.5.2 (Aug 24, 2023)
----------------------------

* Fixed a severe issue with inheritance of the ``Py_TPFLAGS_HAVE_GC`` flag
  affecting classes that derive from other classes with a
  :cpp:class:`nb::dynamic_attr <dynamic_attr>` annotation. (issue `#279
  <https://github.com/wjakob/nanobind/issues/279>`__, commit `dbedad
  <https://github.com/wjakob/nanobind/commit/dbedadc294a7529bf401f01dbc97d4b47b677bc9>`__).
* Implicit conversion of nd-arrays to conform to contiguity constraints such as
  :cpp:class:`c_contig` and :cpp:class:`f_contig` previously failed in some
  cases that are now addressed. (issue `#278
  <https://github.com/wjakob/nanobind/issues/278>`__ commit `ed929b
  <https://github.com/wjakob/nanobind/commit/ed929b7c6789e7d5e1760d515bc23ce6f7cedf8c>`__).

Version 1.5.1 (Aug 23, 2023)
----------------------------

* Fixed serious reference counting issue introduced in nanobind version 1.5.0,
  which affected the functions :cpp:func:`python_error::traceback()` and
  :cpp:func:`python_error::what()`, causing undefined behavior via
  use-after-free. Also addressed an unrelated minor UB sanitizer warning.
  (issue `#277 <https://github.com/wjakob/nanobind/issues/277>`__, commits
  `30d30c
  <https://github.com/wjakob/nanobind/commit/30d30caaa3e834122944b28833b9c0315ef19a5d>`__
  and `c48b18
  <https://github.com/wjakob/nanobind/commit/c48b180834b4929f2f77ce658f2a50ee78482fb7>`__).
* Extended the internal data structure tag so that it isolates different MSVC
  versions from each other (they are often not ABI compatible, see pybind11
  issue `#4779 <https://github.com/pybind/pybind11/pull/4779>`__). This means
  that nanobind 1.5.1 effectively bumps the ABI version to "10.5" when
  compiling for MSVC, and the internals will be isolated from extensions built
  with nanobind v1.5.0 or older. (commit `c7f3cd
  <https://github.com/wjakob/nanobind/commit/c7f3cd6a7023dec55c63b995ba50c9f5d4b9147a>`__).
* Incorporated fixes so that nanobind works with PyPy 3.10. (commits `fb5508
  <https://github.com/wjakob/nanobind/commit/fb5508955e1b1455adfe1372b49748ba706b4d87>`__
  and `2ed10a
  <https://github.com/wjakob/nanobind/commit/2ed108a73bd5fbe0e1c43a8db07e40a165fc265f>`__).
* Fixed type caster for ``std::vector<bool>``. (PR `#256
  <https://github.com/wjakob/nanobind/pull/256>`__).
* Fixed compilation in debug mode on MSVC. (PR `#253
  <https://github.com/wjakob/nanobind/pull/253>`__).

Version 1.5.0 (Aug 7, 2023)
---------------------------

* Support for creating :ref:`chained exceptions <exception_chaining>` via the
  :cpp:func:`nb::raise_from() <chain_error>` and :cpp:func:`nb::chain_error()
  <chain_error>` functions. (commits `041520
  <https://github.com/wjakob/nanobind/commit/0415208e83885dba038516d86c2f4cca5f81df5f>`__
  and `beb699
  <https://github.com/wjakob/nanobind/commit/beb6999b7ce92ba5e3aaea60cd7f2acc9ba3cdc3>`__).
* Many improvements to the handling of return value policies in
  :cpp:class:`nb::ndarray\<..\> <ndarray>` to avoid unnecessary copies. (commit `ffd22b
  <https://github.com/wjakob/nanobind/commit/ffd22b069ba95a546baeca0bdb6711fb9059cad8>`__,
  `a79575
  <https://github.com/wjakob/nanobind/commit/a79575165134c72c0a26e46772290d0404eae7a3>`__,
  and `6f0c3f
  <https://github.com/wjakob/nanobind/commit/6f0c3feaf088e78c75f2abee90164f20446eba08>`__).
* The :cpp:class:`nb::ndarray\<..\> <ndarray>` class now has an additional
  convenience constructor that takes the shape and (optionally) strides using
  ``std::initializer_list``. (commit `de1117
  <https://github.com/wjakob/nanobind/commit/de111766b21fe893a41cd4614a346b0da251f7f2>`__).
* Added a non-throwing function :cpp:func:`nb::try_cast() <try_cast>` as an
  alternative to :cpp:func:`nb::cast() <cast>`. (commit `6ca852
  <https://github.com/wjakob/nanobind/commit/6ca852cc881ee7cd35b674135030709a6b57b8f6>`__).
* The ``nb::list`` and ``nb::tuple`` default constructors now construct an empty list/tuple instead
  of an invalid null-initialized handle.
  (commit `506185 <https://github.com/wjakob/nanobind/commit/506185dca821c9cc1268c33b4cc867ae20f0fc4b>`__)
* New low-level interface for wrapping existing C++ instances via
  :cpp:func:`nb::inst_take_ownership() <inst_take_ownership>`
  :cpp:func:`nb::inst_reference() <inst_reference>`. Also added convenience
  functions to replace the contents of an instance with that of another.
  :cpp:func:`nb::inst_replace_copy() <inst_replace_copy>` along with
  :cpp:func:`nb::inst_replace_move() <inst_replace_move>` (commit `1c462d
  <https://github.com/wjakob/nanobind/commit/1c462d6e3a112e49686acf33c9cb6e34f996dd6b>`__).
* Added a low-level abstraction around :cpp:func:`nb::type_get_slot()
  <type_get_slot>` around ``PyType_GetSlot``, but with more consistent behavior
  across Python versions. (commit `d555e9
  <https://github.com/wjakob/nanobind/commit/d555e9de1c45394f5be5d62dc999c603d651c8c4>`__).
* The :cpp:func:`nb::list::append() <list::append>` method now performs perfect
  forwarding. (commit `2219d0
  <https://github.com/wjakob/nanobind/commit/2219d0b0fec5e6cc4fce96bc3dbad6bfa148a57d>`__).
* Inference of ``automatic*`` return value policy was entirely moved to the
  base C++ class type caster. (commit `1ff9df
  <https://github.com/wjakob/nanobind/commit/1ff9df03fb56a16f56854b4cecd1f388f73d3b53>`__).
* Switch to the new Python 3.12 error status API if available. (commit `36751c
  <https://github.com/wjakob/nanobind/commit/36751cb05994a96a3801bf511c846a7bc68e2f09>`__).
* Various minor fixes and improvements.
* ABI version 10.

Version 1.4.0 (June 8, 2023)
----------------------------

* Improved the efficiency of the function dispatch loop. (PR `#227
  <https://github.com/wjakob/nanobind/pull/227>`__).
* Significant improvements to the Eigen type casters (generalized stride
  handling to avoid unnecessary copies, support for conversion via
  ``nb::cast()``, many refinements to the  ``Eigen::Ref<T>`` interface). (PR
  `#215 <https://github.com/wjakob/nanobind/pull/215>`__).
* Added a ``NB_DOMAIN`` parameter to :cmake:command:`nanobind_add_module` which
  can isolate extensions from each other to avoid binding clashes. See the
  associated :ref:`FAQ entry <type-visibility>` for details. (commit `977119
  <https://github.com/wjakob/nanobind/commit/977119c4797db7decf8064cf118afde768ff8fab>`__).
* Reduced the severity of nanobind encountering a duplicate type binding
  (commits `f3b0e6
  <https://github.com/wjakob/nanobind/commit/f3b0e6cbd69a4adcdc31dbe0b844370b1b60dbcf>`__,
  and `2c9124
  <https://github.com/wjakob/nanobind/commit/2c9124bbbe736881fa8f9f33ea7817c98b43bf8b>`__).
* Support for pickling/unpickling nanobind objects. (commit `59843e
  <https://github.com/wjakob/nanobind/commit/59843e09bc6e8f2b0338829a44cf71e25f76cba3>`__).
* ABI version 9.

Version 1.3.2 (June 2, 2023)
----------------------------

* Fixed compilation on 32 bit processors (only ``i686`` tested so far).
  (PR `#224 <https://github.com/wjakob/nanobind/pull/224>`__).
* Fixed compilation on PyPy 3.8. (commit `cd8135
  <https://github.com/wjakob/nanobind/commit/cd8135baa1da1213252272b5c9ecbf909e947597>`__).
* Reduced binary bloat of musllinux wheels. (commit `f52513
  <https://github.com/wjakob/nanobind/commit/f525139a80d173feaea5518e842aceeb6ceec5cf>`__).

Version 1.3.1 (May 31, 2023)
----------------------------

* CMake build system improvements for stable ABI wheel generation.
  (PR `#222 <https://github.com/wjakob/nanobind/pull/222>`__).

Version 1.3.0 (May 31, 2023)
----------------------------

This is a big release. The sections below cover added features, efficiency
improvements, and miscellaneous fixes and improvements.

New features
^^^^^^^^^^^^
* nanobind now supports binding types that inherit from
  ``std::enable_shared_from_this<T>``. See the :ref:`advanced section
  on object ownership <enable_shared_from_this>` for more details.
  (PR `#212 <https://github.com/wjakob/nanobind/pull/212>`__).
* Added a type caster between Python ``datetime``/``timedelta`` objects and
  C++ ``std::chrono::duration``/``std::chrono::time_point``, ported
  from pybind11. (PR `#175 <https://github.com/wjakob/nanobind/pull/175>`__).
* The :cpp:class:`nb::ndarray\<..\> <ndarray>` class can now use the buffer
  protocol to receive and return arrays representing read-only memory. (PR
  `#217 <https://github.com/wjakob/nanobind/pull/217>`__).
* Added :cpp:func:`nb::python_error::discard_as_unraisable()
  <python_error::discard_as_unraisable>` as a wrapper around
  ``PyErr_WriteUnraisable()``. (PR `#175
  <https://github.com/wjakob/nanobind/pull/175>`__).

Efficiency improvements:
^^^^^^^^^^^^^^^^^^^^^^^^

* Reduced the per-instance overhead of nanobind by 1 pointer and simplified the
  internal hash table types to crunch ``libnanobind``. (commit `de018d
  <https://github.com/wjakob/nanobind/commit/de018db2d17905564703f1ade4aa201a22f8551f>`__).
* Supplemental type data specified via :cpp:class:`nb::supplement\<T\>()
  <supplement>` is now stored directly within the type object instead of being
  referenced through an indirection. (commit `d82ca9
  <https://github.com/wjakob/nanobind/commit/d82ca9c14191e74dd35dd5bf15fc90f5230319fb>`__).
* Reduced the number of exception-related exports to further crunch
  ``libnanobind``. (commit `763962
  <https://github.com/wjakob/nanobind/commit/763962b8ce76414148089ef6a68cff97d7cc66ce>`__).
* Reduced the size of nanobind type objects by 5 pointers. (PR `#194
  <https://github.com/wjakob/nanobind/pull/194>`__, `#195
  <https://github.com/wjakob/nanobind/pull/195>`__, and commit `d82ca9
  <https://github.com/wjakob/nanobind/commit/d82ca9c14191e74dd35dd5bf15fc90f5230319fb>`__).
* Internal nanobind types (``nb_type``, ``nb_static_property``, ``nb_ndarray``)
  are now constructed on demand. This reduces the size of the ``libnanobind``
  component in static (``NB_STATIC``) builds when those features are not used.
  (commits `95e45a
  <https://github.com/wjakob/nanobind/commit/95e45a4027dcbce935091533f7d41bf59e3e5fe1>`__,
  `375083
  <https://github.com/wjakob/nanobind/commit/37508386a1f8c346d17a0353c8152940aacde9c2>`__,
  and `e033c8
  <https://github.com/wjakob/nanobind/commit/e033c8fab4a14cbb9c5b0e08b1bdf49af2a9cb22>`__).
* Added a small function cache to improve code generation in limited API
  builds. (commit `f0f4aa
  <https://github.com/wjakob/nanobind/commit/f0f42a564995ba3bd573282674d1a6d636a048c8>`__).
* Refined compiler and linker flags across platforms to ensure compact binaries
  especially in ``NB_STATIC`` builds. (commit `5ead9f
  <https://github.com/wjakob/nanobind/commit/5ead9ff348a2ef0df8231e6480607a5b0623a16b>`__)
* nanobind enums now take advantage of :ref:`supplemental data <supplement>`
  to improve the speed of object and name lookups. Note that this prevents
  use of ``nb::supplement<T>()`` with enums for other purposes.
  (PR `#195 <https://github.com/wjakob/nanobind/pull/195>`__).

Miscellaneous fixes and improvements
^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^

* Use the new `PEP-697 <https://peps.python.org/pep-0697/>`__ interface to
  access data in type objects when compiling stable ABI3 wheels. This improves
  forward compatibility (the Python team may at some point significantly
  refactor the layout and internals of type objects). (PR `#211
  <https://github.com/wjakob/nanobind/pull/211>`__):
* Added introspection attributes ``__self__`` and ``__func__`` to nanobind
  bound methods, to make them more like regular Python bound methods.
  Fixed a bug where ``some_obj.method.__call__()`` would behave differently
  than ``some_obj.method()``.
  (PR `#216 <https://github.com/wjakob/nanobind/pull/216>`__).
* Updated the implementation of :cpp:class:`nb::enum_ <enum_>` so it does
  not take advantage of any private nanobind type details. As a side effect,
  the construct ``nb::class_<T>(..., nb::is_enum(...))`` is no longer permitted;
  use ``nb::enum_<T>(...)`` instead.
  (PR `#195 <https://github.com/wjakob/nanobind/pull/195>`__).
* Added the :cpp:class:`nb::type_slots_callback` class binding annotation,
  similar to :cpp:class:`nb::type_slots` but allowing more dynamic choices.
  (PR `#195 <https://github.com/wjakob/nanobind/pull/195>`__).
* nanobind type objects now treat attributes specially whose names
  begin with ``@``. These attributes can be set once, but not
  rebound or deleted.  This safeguard allows a borrowed reference to
  the attribute value to be safely stashed in the type supplement,
  allowing arbitrary Python data associated with the type to be accessed
  without a dictionary lookup while keeping this data visible to the
  garbage collector.  (PR `#195 <https://github.com/wjakob/nanobind/pull/195>`__).
* Fixed surprising behavior in enumeration comparisons and arithmetic
  (PR `#207 <https://github.com/wjakob/nanobind/pull/207>`__):

  * Enum equality comparisons (``==`` and ``!=``) now can only be true
    if both operands have the same enum type, or if one is an enum and
    the other is an ``int``. This resolves some confusing
    results and ensures that enumerators of different types have a
    distinct identity, which is important if they're being put into
    the same set or used as keys in the same dictionary. All of the
    following were previously true but will now evaluate as false:

    * ``FooEnum(1) == BarEnum(1)``
    * ``FooEnum(1) == 1.2``
    * ``FooEnum(1) == "1"``

  * Enum ordering comparisons (``<``, ``<=``, ``>=``, ``>``) and
    arithmetic operations (when using the :cpp:struct:`is_arithmetic`
    annotation) now require that any non-enum operand be a Python number
    (an object that defines ``__int__``, ``__float__``, and/or ``__index__``)
    and will avoid truncating non-integer operands to integers. Note that
    unlike with equality comparisons, ordering and arithmetic operations
    *do* still permit two operands that are enums of different types.
    Some examples of changed behavior:

    * ``FooEnum(1) < 1.2`` is now true (used to be false)
    * ``FooEnum(2) * 1.5`` is now 3.0 (used to be 2)
    * ``FooEnum(3) - "2"`` now raises an exception (used to be 1)

  * Enum comparisons and arithmetic operations with unsupported types
    now return `NotImplemented` rather than raising an exception.
    This means equality comparisons such as ``some_enum == None`` will
    return unequal rather than failing; order comparisons such as
    ``some_enum < None`` will still fail, but now with a more
    informative error.

* ABI version 8.

Version 1.2.0 (April 24, 2023)
------------------------------

* Improvements to the internal C++ → Python instance map data structure to improve
  performance and address type confusion when returning previously registered instances.
  (commit `716354 <https://github.com/wjakob/nanobind/commit/716354f0ed6123d6a19fcabb077b72a17b4ddf79>`__,
  discussion `189 <https://github.com/wjakob/nanobind/discussions/189>`__).
* Added up-to-date nanobind benchmarks on Linux including comparisons to Cython.
  (commit `834cf3
  <https://github.com/wjakob/nanobind/commit/834cf36ce12ffe6470dcffecd21341377c56cee1>`__
  and `39e163
  <https://github.com/wjakob/nanobind/commit/e9e163ec55de995a68a34fafda2e96ff06532658>`__).
* Removed the superfluous ``nb_enum`` metaclass.
  (commit `9c1985 <https://github.com/wjakob/nanobind/commit/9c19850471be70a22114826f6c0edceee99ff40b>`__).
* Fixed a corner case that prevented ``nb::cast<char>`` from working.
  (commit `9ae320 <https://github.com/wjakob/nanobind/commit/9ae32054d9a6ad17af15994dc51138eb88f71f92>`__).

Version 1.1.1 (April 6, 2023)
-----------------------------

* Added documentation on packaging and distributing nanobind modules. (commit
  `0715b2
  <https://github.com/wjakob/nanobind/commit/0715b278ba806cf13cf63e41d62438481e7b73b8>`__).
* Made the conversion :cpp:func:`handle::operator bool() <handle::operator
  bool>` explicit. (PR `#173 <https://github.com/wjakob/nanobind/pull/173>`__).
* Support :cpp:class:`nb::typed\<..\> <typed>` in return values. (PR `#174
  <https://github.com/wjakob/nanobind/pull/174>`__).
* Tweaks to definitions in ``nb_types.h`` to improve compatibility with further
  C++ compilers (that said, there is no change about the official set of
  supported compilers). (commit `b8bd10
  <https://github.com/wjakob/nanobind/commit/b8bd1086e9b20da8a81a954f03e7947bee5422fd>`__)

Version 1.1.0 (April 5, 2023)
-----------------------------

* Added :cpp:func:`size <ndarray::size>`, :cpp:func:`shape_ptr
  <ndarray::shape_ptr>`, :cpp:func:`stride_ptr <ndarray::stride_ptr>` members
  to to the :cpp:class:`nb::ndarray\<..\> <ndarray>` class. (PR `#161
  <https://github.com/wjakob/nanobind/pull/161>`__).
* Allow macros in :c:macro:`NB_MODULE(..) <NB_MODULE>` name parameter. (PR
  `#168 <https://github.com/wjakob/nanobind/pull/168>`__).
* The :cpp:class:`nb::ndarray\<..\> <ndarray>` interface is more tolerant when
  converting Python (PyTorch/NumPy/..) arrays with a size-0 dimension that have
  mismatched strides. (PR `#162
  <https://github.com/wjakob/nanobind/pull/162>`__).
* Removed the ``<anonymous>`` label from docstrings of anonymous functions,
  which caused issues in MyPy. (PR `#172
  <https://github.com/wjakob/nanobind/pull/172>`__).
* Fixed an issue in the propagation of return value policies that broke
  user-provided/custom policies in properties (PR `#170
  <https://github.com/wjakob/nanobind/pull/170>`__).
* The Eigen interface now converts 1x1 matrices to 1x1 NumPy arrays instead of
  scalars. (commit `445781
  <https://github.com/wjakob/nanobind/commit/445781fc2cf2fa326cc22e8fd483e8e4a7bf6cf5>`__).
* The ``nanobind`` package now has a simple command line interface. (commit
  `d5ccc8
  <https://github.com/wjakob/nanobind/commit/d5ccc8844b29ca6cd5188ffd8d16e034bcee9f73>`__).

Version 1.0.0 (March 28, 2023)
------------------------------

* Nanobind now has a logo. (commit `b65d31
  <https://github.com/wjakob/nanobind/commit/b65d3b134d8b9f8d153b51d87751d09a12e4235b>`__).
* Fixed a subtle issue involving function/method properties and the IPython
  command line interface. (PR `#151
  <https://github.com/wjakob/nanobind/pull/151>`__).
* Added a boolean type to the :cpp:class:`nb::ndarray\<..\> <ndarray>`
  interface. (PR `#150 <https://github.com/wjakob/nanobind/pull/150>`__).
* Minor fixes and improvements.


Version 0.3.1 (March 8, 2023)
-----------------------------

* Added a type caster for ``std::filesystem::path``. (PR `#138
  <https://github.com/wjakob/nanobind/pull/138>`__ and commit `0b05cd
  <https://github.com/wjakob/nanobind/commit/0b05cde8bd8685ab42328660da03cc4ee66e3ba2>`__).
* Fixed technical issues involving implicit conversions (commits `022935
  <https://github.com/wjakob/nanobind/commit/022935cbb92dfb1d02f90546bf6b34013f90e9e5>`__
  and `5aefe3
  <https://github.com/wjakob/nanobind/commit/5aefe36e3e07b5b98a6be7c0f3ce28a236fe2330>`__)
  and construction of type hierarchies with custom garbage collection hooks
  (commit `022935
  <https://github.com/wjakob/nanobind/commit/7b3e893e1c14d95f7b3fc838657e6f9ce520d609>`__).
* Re-enabled the 'chained fixups' linker optimization for recent macOS
  deployment targets. (commit `2f29ec
  <https://github.com/wjakob/nanobind/commit/2f29ec7d5fbebd5f55fb52da297c8d197279f659>`__).

Version 0.3.0 (March 8, 2023)
-----------------------------

* Botched release, replaced by 0.3.1 on the same day.

Version 0.2.0 (March 3, 2023)
-----------------------------
* Nanobind now features documentation on `readthedocs
  <https://nanobind.readthedocs.io>`__.
* The documentation process revealed a number of inconsistencies in the
  :cpp:func:`class_\<T\>::def* <class_::def>` naming scheme. nanobind will from
  now on use the following shortened and more logical interface:

  .. list-table::
    :widths: 40 60
    :header-rows: 1

    * - Type
      - method
    * - Methods & constructors
      - :cpp:func:`.def() <class_::def>`
    * - Fields
      - :cpp:func:`.def_ro() <class_::def_ro>`,
        :cpp:func:`.def_rw() <class_::def_rw>`
    * - Properties
      - :cpp:func:`.def_prop_ro() <class_::def_prop_ro>`,
        :cpp:func:`.def_prop_rw() <class_::def_prop_rw>`
    * - Static methods
      - :cpp:func:`.def_static() <class_::def_static>`
    * - Static fields
      - :cpp:func:`.def_ro_static() <class_::def_ro_static>`,
        :cpp:func:`.def_rw_static() <class_::def_rw_static>`
    * - Static properties
      - :cpp:func:`.def_prop_ro_static() <class_::def_prop_ro_static>`,
        :cpp:func:`.def_prop_rw_static() <class_::def_prop_rw_static>`

  Compatibility wrappers with deprecation warnings were also added to help port
  existing code. They will be removed when nanobind reaches version 1.0.
  (commits `cb0dc3
  <https://github.com/wjakob/nanobind/commit/cb0dc392b656fd9d0c85c56dc51a9be1de06e176>`__
  and `b5ed96
  <https://github.com/wjakob/nanobind/commit/b5ed696a7a68c9c9adc4d3aa3c6f4adb5b7defeb>`__)
* The ``nb::tensor<..>`` class has been renamed to :cpp:class:`nb::ndarray\<..\> <ndarray>`,
  and it is now located in a different header file (``nanobind/ndarray.h``). A
  compatibility wrappers with a deprecation warning was retained in the
  original header file. It will be removed when nanobind reaches version 1.0.
  (commit `a6ab8b
  <https://github.com/wjakob/nanobind/commit/a6ab8b06dd3316ac53fbed143c346c2b73c31b75>`__).
* Dropped the first two arguments of the :c:macro:`NB_OVERRIDE_*()
  <NB_OVERRIDE>` macros that turned out to be unnecessary in nanobind. (commit
  `22bc21
  <https://github.com/wjakob/nanobind/commit/22bc21b97cd2bbe060d7fb42d374bde72d973ada>`__).
* Added casters for dense matrix/array types from the `Eigen library
  <https://eigen.tuxfamily.org/index.php?title=Main_Page>`__. (PR `#120
  <https://github.com/wjakob/nanobind/pull/120>`__).
* Added casters for sparse matrix/array types from the `Eigen library
  <https://eigen.tuxfamily.org/index.php?title=Main_Page>`__. (PR `#126
  <https://github.com/wjakob/nanobind/pull/126>`_).
* Implemented `nb::bind_vector\<T\>() <bind_vector>` analogous to similar
  functionality in pybind11. (commit `f2df8a
  <https://github.com/wjakob/nanobind/commit/f2df8a90fbfb06ee03a79b0dd85fa0e266efeaa9>`__).
* Implemented :cpp:func:`nb::bind_map\<T\>() <bind_map>` analogous to
  similar functionality in pybind11. (PR `#114
  <https://github.com/wjakob/nanobind/pull/114>`__).
* nanobind now :ref:`automatically downcasts <automatic_downcasting>`
  polymorphic objects in return values analogous to pybind11. (commit `cab96a
  <https://github.com/wjakob/nanobind/commit/cab96a9160e0e1a626bc3e4f9fcddcad31e0f727>`__).
* nanobind now supports :ref:`tag-based polymorphism <tag_based_polymorphism>`.
  (commit `6ade94
  <https://github.com/wjakob/nanobind/commit/6ade94b8e5a2388d66fc9df6f81603c65108cbcc>`__).
* Updated tuple/list iterator to satisfy the ``std::forward_iterator`` concept.
  (PR `#117 <https://github.com/wjakob/nanobind/pull/117>`__).
* Fixed issues with non-writeable tensors in NumPy. (commit `25cc3c
  <https://github.com/wjakob/nanobind/commit/25cc3ccbd1174e7cfc4eef1d1e7206cc38e854ca>`__).
* Removed use of some C++20 features from the codebase. This now makes it
  possible to use nanobind on  Visual Studio 2017 and GCC 7.3.1 (used on RHEL 7).
  (PR `#115 <https://github.com/wjakob/nanobind/pull/115>`__).
* Added the :cpp:class:`nb::typed\<...\> <typed>` wrapper to override the type signature of an
  argument in a bound function in the generated docstring. (commit `b3404c4
  <https://github.com/wjakob/nanobind/commit/b3404c4f347981bce7f4c7a9bac762656bed8385>`__).
* Added an :cpp:func:`nb::implicit_convertible\<A, B\>() <implicitly_convertible>` function analogous to the one in
  pybind11. (commit `aba4af
  <https://github.com/wjakob/nanobind/commit/aba4af06992f14e21e5b7b379e7986e939316da4>`__).
* Updated :cpp:func:`nb::make_*_iterator\<..\>() <make_iterator>` so that it returns references of elements, not
  copies. (commit `8916f5
  <https://github.com/wjakob/nanobind/commit/8916f51ad1a25318b5c9fcb07c153f6b72a43bd2>`__).
* Changed the CMake build system so that the library component
  (``libnanobind``) is now compiled statically by default. (commit `8418a4
  <https://github.com/wjakob/nanobind/commit/8418a4aa93d19d7b9714b8d9473539b46cbed508>`__).
* Switched shared library linking on macOS back to a two-level namespace.
  (commit `fe4965
  <https://github.com/wjakob/nanobind/commit/fe4965369435bf7c0925bddf610553d0bb516e27>`__).
* Various minor fixes and improvements.
* ABI version 7.

Version 0.1.0 (January 3, 2023)
-------------------------------

* Allow nanobind methods on non-nanobind) classes. (PR `#104
  <https://github.com/wjakob/nanobind/pull/104>`__).
* Fix dangling `tp_members` pointer in type initialization. (PR `#99
  <https://github.com/wjakob/nanobind/pull/99>`__).
* Added a runtime setting to suppress leak warnings. (PR `#109
  <https://github.com/wjakob/nanobind/pull/109>`__).
* Added the ability to hash ``nb::enum_<..>`` instances (PR `#106
  <https://github.com/wjakob/nanobind/pull/106>`__).
* Fixed the signature of ``nb::enum_<..>::export_values()``. (commit `714d17
  <https://github.com/wjakob/nanobind/commit/714d17e71aa405c7633e0bd798a8bdb7b8916fa1>`__).
* Double-check GIL status when performing reference counting operations in
  debug mode. (commit `a1b245
  <https://github.com/wjakob/nanobind/commit/a1b245fcf210fbfb10d7eb19dc2dc31255d3f561>`__).
* Fixed a reference leak that occurred when module initialization fails.
  (commit `adfa9e
  <https://github.com/wjakob/nanobind/commit/adfa9e547be5575f025d92abeae2e649a690760a>`__).
* Improved robustness of ``nb::tensor<..>`` caster. (commit `633672
  <https://github.com/wjakob/nanobind/commit/633672cd154c0ef13f96fee84c2291562f4ce3d3>`__).
* Upgraded the internally used ``tsl::robin_map<>`` hash table to address a
  rare `overflow issue <https://github.com/Tessil/robin-map/issues/52>`__
  discovered in this codebase. (commit `3b81b1
  <https://github.com/wjakob/nanobind/commit/3b81b18577e243118a659b524d4de9500a320312>`__).
* Various minor fixes and improvements.
* ABI version 6.

Version 0.0.9 (Nov 23, 2022)
----------------------------

* PyPy 7.3.10 or newer is now supported subject to `certain limitations
  <https://github.com/wjakob/nanobind/blob/master/docs/pypy.rst>`__. (commits
  `f935f93
  <https://github.com/wjakob/nanobind/commit/f935f93b9d532a5ef1f385445f328d61eb2af97f>`__
  and `b343bbd
  <https://github.com/wjakob/nanobind/commit/b343bbd11c12b55bbc00492445c743cae18b298f>`__).
* Three changes that reduce the binary size and improve runtime performance of
  binding libraries. (commits `07b4e1fc
  <https://github.com/wjakob/nanobind/commit/07b4e1fc9e94eeaf5e9c2f4a63bdb275a25c82c6>`__,
  `9a803796
  <https://github.com/wjakob/nanobind/commit/9a803796cb05824f9df7593edb984130d20d3755>`__,
  and `cba4d285
  <https://github.com/wjakob/nanobind/commit/cba4d285f4e23b888dfcccc656c221414138a2b7>`__).
* Fixed a reference leak in ``python_error::what()`` (commit `61393ad
  <https://github.com/wjakob/nanobind/commit/61393ad3ce3bc68d195a1496422df43d5fb45ec0>`__).
* Adopted a new policy for function type annotations. (commit `c855c90 <https://github.com/wjakob/nanobind/commit/c855c90fc91d180f7c904c612766af6a84c017e3>`__).
* Improved the effectiveness of link-time-optimization when building extension modules
  with the ``NB_STATIC`` flag. This leads to smaller binaries. (commit `f64d2b9
  <https://github.com/wjakob/nanobind/commit/f64d2b9bb558afe28cf6909e4fa47ebf720f62b3>`__).
* Nanobind now relies on standard mechanisms to inherit the ``tp_traverse`` and
  ``tp_clear`` type slots instead of trying to reimplement the underlying
  CPython logic (commit `efa09a6b
  <https://github.com/wjakob/nanobind/commit/efa09a6bf6ac27f790b2c96389c2da42d4bc176b>`__).
* Moved nanobind internal data structures from ``builtins`` to Python
  interpreter state dictionary. (issue `#96
  <https://github.com/wjakob/nanobind/issues/96>`__, commit `ca23da7
  <https://github.com/wjakob/nanobind/commit/ca23da72ce71a45318f1e59474c9c2906fce5154>`__).
* Various minor fixes and improvements.


Version 0.0.8 (Oct 27, 2022)
----------------------------

* Caster for ``std::array<..>``. (commit `be34b16
  <https://github.com/wjakob/nanobind/commit/be34b165c6a0bed08e477755644f96759b9ed69a>`__).
* Caster for ``std::set<..>`` and ``std::unordered_set`` (PR `#87
  <https://github.com/wjakob/nanobind/pull/87>`__).
* Ported ``nb::make[_key_,_value]_iterator()`` from pybind11. (commit `34d0be1
  <https://github.com/wjakob/nanobind/commit/34d0be1bbeb54b8265456fd3a4a50e98f93fe6d4>`__).
* Caster for untyped ``void *`` pointers. (commit `6455fff
  <https://github.com/wjakob/nanobind/commit/6455fff7be5be2867063ea8138cf10e1d9f3065f>`__).
* Exploit move constructors in ``nb::class_<T>::def_readwrite()`` and
  ``nb::class_<T>::def_readwrite_static()`` (PR `#94
  <https://github.com/wjakob/nanobind/pull/94>`__).
* Redesign of the ``std::function<>`` caster to enable cyclic garbage collector
  traversal through inter-language callbacks (PR `#95
  <https://github.com/wjakob/nanobind/pull/95>`__).
* New interface for specifying custom type slots during Python type
  construction. (commit `38ba18a
  <https://github.com/wjakob/nanobind/commit/38ba18a835cfcd561efb4b4c640ee5c6d525decb>`__).
* Fixed potential undefined behavior related to ``nb_func`` garbage collection by
  Python's cyclic garbage collector. (commit `662e1b9
  <https://github.com/wjakob/nanobind/commit/662e1b9311e693f84c58799a67064d4a44bb706a>`__).
* Added a workaround for spurious reference leak warnings caused by other
  extension modules in conjunction with ``typing.py`` (commit `5e11e80
  <https://github.com/wjakob/nanobind/commit/5e11e8032f777c0a34abd437dc6e84a909907c91>`__).
* Various minor fixes and improvements.
* ABI version 5.

Version 0.0.7 (Oct 14, 2022)
----------------------------

* Fixed a regression involving function docstrings in ``pydoc``. (commit
  `384f4a
  <https://github.com/wjakob/nanobind/commit/384f4ada1f3f08486fb03427227878ddbbcaad43>`__).

Version 0.0.6 (Oct 14, 2022)
----------------------------

* Fixed undefined behavior that could lead to crashes when nanobind types were
  freed. (commit `39266e
  <https://github.com/wjakob/nanobind/commit/39266ef0b0ccd7fa3e9237243a6c97ba8db2cd2a>`__).
* Refactored nanobind so that it works with ``Py_LIMITED_API`` (PR `#37 <https://github.com/wjakob/nanobind/pull/37>`__).
* Dynamic instance attributes (PR `#38 <https://github.com/wjakob/nanobind/pull/38>`__).
* Intrusive pointer support (PR `#43 <https://github.com/wjakob/nanobind/pull/43>`__).
* Byte string support (PR `#62 <https://github.com/wjakob/nanobind/pull/62>`__).
* Casters for ``std::variant<..>`` and ``std::optional<..>`` (PR `#67 <https://github.com/wjakob/nanobind/pull/67>`__).
* Casters for ``std::map<..>`` and ``std::unordered_map<..>`` (PR `#73 <https://github.com/wjakob/nanobind/pull/73>`__).
* Caster for ``std::string_view<..>`` (PR `#68 <https://github.com/wjakob/nanobind/pull/68>`__).
* Custom exception support (commit `41b7da <https://github.com/wjakob/nanobind/commit/41b7da33f1bc5c583bb98df66bdac2a058ec5c15>`__).
* Register nanobind functions with Python's cyclic garbage collector (PR `#86 <https://github.com/wjakob/nanobind/pull/86>`__).
* Various minor fixes and improvements.
* ABI version 3.

Version 0.0.5 (May 13, 2022)
----------------------------

* Enumeration export.
* Implicit number conversion for NumPy scalars.
* Various minor fixes and improvements.

Version 0.0.4 (May 13, 2022)
----------------------------

* Botched release, replaced by 0.0.5 on the same day.

Version 0.0.3 (Apr 14, 2022)
----------------------------

* DLPack support.
* Iterators for various Python type wrappers.
* Low-level interface to instance creation.
* Docstring generation improvements.
* Various minor fixes and improvements.

Version 0.0.2 (Mar 10, 2022)
----------------------------

* Initial release of the nanobind codebase.
* ABI version 1.

Version 0.0.1 (Feb 21, 2022)
----------------------------

* Placeholder package on PyPI.
