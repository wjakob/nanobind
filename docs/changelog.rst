.. _changelog:

Changelog
#########

*nanobind* uses a `semantic versioning <http://semver.org>`_ policy. Since the
current version is still in the prototype range (*0.x.y*), there are no (formal)
guarantees of API or ABI stability. That said, I will do my best to minimize
inconvenience whenever possible.

Version 0.0.9 (TBA)
----------------------------

- Move nanobind internal data structures from ``builtins`` to Python
  interpreter state dictionary (commit `ca23da7
  <https://github.com/wjakob/nanobind/commit/ca23da72ce71a45318f1e59474c9c2906fce5154>`_,
  issue `#96 <https://github.com/wjakob/nanobind/issues/96>`_).
- Fixed a reference leak in ``python_error::what()`` (commit `61393ad
  <https://github.com/wjakob/nanobind/commit/61393ad3ce3bc68d195a1496422df43d5fb45ec0>`_).
- Improved the effectiveness of link-time-optimization when building extension modules
  with the ``NB_STATIC`` flag. This leads to smaller binaries. (commit `f64d2b9
  <https://github.com/wjakob/nanobind/commit/f64d2b9bb558afe28cf6909e4fa47ebf720f62b3>`_).

- ... TBA ...


Version 0.0.8 (Oct 27, 2022)
----------------------------

* Caster for ``std::array<..>``. (commit `be34b16
  <https://github.com/wjakob/nanobind/commit/be34b165c6a0bed08e477755644f96759b9ed69a>`_).
* Caster for ``std::set<..>`` and ``std::unordered_set`` (PR `#87
  <https://github.com/wjakob/nanobind/pull/87>`_).
* Ported ``nb::make[_key_,_value]_iterator()`` from pybind11. (commit `34d0be1
  <https://github.com/wjakob/nanobind/commit/34d0be1bbeb54b8265456fd3a4a50e98f93fe6d4>`_).
* Caster for untyped ``void *`` pointers. (commit `6455fff
  <https://github.com/wjakob/nanobind/commit/6455fff7be5be2867063ea8138cf10e1d9f3065f>`_).
* Exploit move constructors in ``nb::class_<T>::def_readwrite()`` and
  ``nb::class_<T>::def_readwrite_static()`` (PR `#94
  <https://github.com/wjakob/nanobind/pull/94>`_).
* Redesign of the ``std::function<>`` caster to enable cyclic garbage collector
  traversal through inter-language callbacks (PR `#95
  <https://github.com/wjakob/nanobind/pull/95>`_).
* New interface for specifying custom type slots during Python type
  construction. (commit `38ba18a
  <https://github.com/wjakob/nanobind/commit/38ba18a835cfcd561efb4b4c640ee5c6d525decb>`_).
* Fixed potential undefined behavior related to ``nb_func`` garbage collection by
  Python's cyclic garbage collector. (commit `662e1b9
  <https://github.com/wjakob/nanobind/commit/662e1b9311e693f84c58799a67064d4a44bb706a>`_).
* Added a workaround for spurious reference leak warnings caused by other
  extension modules in conjunction with ``typing.py`` (commit `5e11e80
  <https://github.com/wjakob/nanobind/commit/5e11e8032f777c0a34abd437dc6e84a909907c91>`_).
* Various minor fixes and improvements.
* Internals ABI version bump.

Version 0.0.7 (Oct 14, 2022)
----------------------------

* Fixed a regression involving function docstrings in ``pydoc``. (commit
  `384f4a
  <https://github.com/wjakob/nanobind/commit/384f4ada1f3f08486fb03427227878ddbbcaad43>`_).

Version 0.0.6 (Oct 14, 2022)
----------------------------

* Fixed undefined behavior that could lead to crashes when nanobind types were
  freed. (commit `39266e
  <https://github.com/wjakob/nanobind/commit/39266ef0b0ccd7fa3e9237243a6c97ba8db2cd2a>`_).
* Refactored nanobind so that it works with ``Py_LIMITED_API`` (PR `#37 <https://github.com/wjakob/nanobind/pull/37>`_).
* Dynamic instance attributes (PR `#38 <https://github.com/wjakob/nanobind/pull/38>`_).
* Intrusive pointer support (PR `#43 <https://github.com/wjakob/nanobind/pull/43>`_).
* Byte string support (PR `#62 <https://github.com/wjakob/nanobind/pull/62>`_).
* Casters for ``std::variant<..>`` and ``std::optional<..>`` (PR `#67 <https://github.com/wjakob/nanobind/pull/67>`_).
* Casters for ``std::map<..>`` and ``std::unordered_map<..>`` (PR `#73 <https://github.com/wjakob/nanobind/pull/73>`_).
* Caster for ``std::string_view<..>`` (PR `#68 <https://github.com/wjakob/nanobind/pull/68>`_).
* Custom exception support (commit `41b7da <https://github.com/wjakob/nanobind/commit/41b7da33f1bc5c583bb98df66bdac2a058ec5c15>`_).
* Register nanobind functions in cyclic GC (PR `#86 <https://github.com/wjakob/nanobind/pull/86>`_).
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
