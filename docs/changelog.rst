.. _changelog:

.. cpp:namespace:: nanobind

Changelog
#########

nanobind uses a `semantic versioning <http://semver.org>`__ policy. Since the
current version is still in the prototype range (*0.x.y*), there are no (formal)
guarantees of API or ABI stability. That said, I will do my best to minimize
inconvenience whenever possible.

Version 0.3.0 (March 8, 2023)
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
* Updated :cpp:func:`nb::make*_iterator\<..\>() <make_iterator>` so that it returns references of elements, not
  copies. (commit `8916f5
  <https://github.com/wjakob/nanobind/commit/8916f51ad1a25318b5c9fcb07c153f6b72a43bd2>`__).
* Changed the CMake build system so that the library component
  (``libnanobind``) is now compiled statically by default. (commit `8418a4
  <https://github.com/wjakob/nanobind/commit/8418a4aa93d19d7b9714b8d9473539b46cbed508>`__).
* Switched shared library linking on macOS back to a two-level namespace.
  (commit `fe4965
  <https://github.com/wjakob/nanobind/commit/fe4965369435bf7c0925bddf610553d0bb516e27>`__).
* Various minor fixes and improvements.
* Internals ABI version bump.

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
* Internals ABI version bump.

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
* Internals ABI version bump.

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

Version 0.0.5 (May 13, 2022)
----------------------------

* Enumeration export.
* Implicit number conversion for numpy scalars.
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

Version 0.0.1 (Feb 21, 2022)
----------------------------

* Placeholder package on PyPI.
