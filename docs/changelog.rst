.. _changelog:

Changelog
#########

*nanobind* uses a `semantic versioning <http://semver.org>`_ policy. Since the
current version is still in the prototype range (*0.x.y*), there are no (formal)
guarantees of API or ABI stability. That said, I will do my best to minimize
inconvenience whenever possible.

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

* Initial release of _nanobind_ codebase.

Version 0.0.1 (Feb 21, 2022)
----------------------------

* Placeholder package on PyPI.
