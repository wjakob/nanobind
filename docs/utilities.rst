.. cpp:namespace:: nanobind

.. _utilities:

Utilities
==========

.. _utilities_eval:

Evaluating Python expressions from strings
------------------------------------------

nanobind provides the :cpp:func:`eval` and :cpp:func:`exec` functions to
evaluate Python expressions and statements. The following example illustrates
how they can be used.

.. code-block:: cpp

    // At beginning of file
    #include <nanobind/eval.h>

    ...

    // Evaluate in scope of main module
    nb::object scope = nb::module_::import_("__main__").attr("__dict__");

    // Evaluate an isolated expression
    int result = nb::eval("my_variable + 10", scope).cast<int>();

    // Evaluate a sequence of statements
    nb::exec(
        "print('Hello')\n"
        "print('world!');",
        scope);

C++11 raw string literals are also supported and quite handy for this purpose.
The only requirement is that the first statement must be on a new line
following the raw string delimiter ``R"(``, ensuring all lines have common
leading indent:

.. code-block:: cpp

    nb::exec(R"(
        x = get_answer()
        if x == 42:
            print('Hello World!')
        else:
            print('Bye!')
        )", scope
    );

.. note::

    :cpp:func:`eval` accepts a template parameter that describes how the
    string/file should be interpreted. Possible choices include ``eval_expr``
    (isolated expression), ``eval_single_statement`` (a single statement,
    return value is always ``none``), and ``eval_statements`` (sequence of
    statements, return value is always ``none``). `eval` defaults to
    ``eval_expr`` and `exec` is just a shortcut for ``eval<eval_statements>``.

.. _utilities_named_tuple:

Binding C++ structs as Python NamedTuples
-----------------------------------------

Plain C++ data-carrier structs (configuration records, small geometric types,
function return aggregates) often map most naturally onto Python's
:py:class:`typing.NamedTuple`: an immutable, positionally-indexable record with
named fields, defaults, ``_asdict``, ``_replace``, and structural pattern
matching. nanobind ships an opt-in helper for this pattern in
``<nanobind/nb_named_tuple.h>``.

The header is **not** included by ``<nanobind/nanobind.h>``; add it explicitly
in any translation unit that registers or passes a NamedTuple-bound type.

Macro API
^^^^^^^^^

For simple aggregate structs the :c:macro:`NB_NAMED_TUPLE` macro is a one-liner
that registers the class. It is paired with the file-scope
:c:macro:`NB_NAMED_TUPLE_CASTER` macro that opts the type into the named-tuple
type caster -- the same two-macro pattern used by
:c:macro:`NB_MAKE_OPAQUE`:

.. code-block:: cpp

    // At beginning of file (top-level, outside any namespace)
    #include <nanobind/nb_named_tuple.h>

    struct Point {
        double x;
        double y;
    };

    NB_NAMED_TUPLE_CASTER(Point)

    NB_MODULE(my_ext, m) {
        NB_NAMED_TUPLE(m, Point, x, y);

        m.def("midpoint", [](Point a, Point b) {
            return Point{(a.x + b.x) / 2, (a.y + b.y) / 2};
        });
    }

On the Python side:

.. code-block:: pycon

    >>> from my_ext import Point, midpoint
    >>> p = Point(1.0, 2.0)
    >>> isinstance(p, tuple)
    True
    >>> p._fields
    ('x', 'y')
    >>> midpoint(Point(0, 0), (4, 6))     # plain tuple also accepted
    Point(x=2.0, y=3.0)
    >>> midpoint(Point(0, 0), Point(4, 6))._asdict()
    {'x': 2.0, 'y': 3.0}

The macro accepts up to 16 fields. For more fields, or any case that needs
escape-hatch behaviour (custom field names, defaults, computed fields), use
the helper class directly -- see :ref:`utilities_named_tuple_helper` below.

Two-macro pattern
^^^^^^^^^^^^^^^^^

:c:macro:`NB_NAMED_TUPLE_CASTER` lives at file scope and registers the type
caster specialization. :c:macro:`NB_NAMED_TUPLE` lives inside ``NB_MODULE``
(or any function that has a :cpp:class:`module_` / :cpp:class:`handle` in
scope) and actually creates the Python class. The split mirrors
:c:macro:`NB_MAKE_OPAQUE` and lets the same C++ type be referenced from
multiple translation units while only being *registered* once.

.. _utilities_named_tuple_helper:

Helper API
^^^^^^^^^^

The macro expands to a use of the underlying ``nanobind::named_tuple<T>``
helper class. Instantiating it directly gives full control over field names
and order, and is also the way to express features the macro does not cover
(per-field defaults, more than 16 fields):

.. code-block:: cpp

    nb::named_tuple<Point>(m, "Point")
        .def_rw("x", &Point::x)
        .def_rw("y", &Point::y);

You still need :c:macro:`NB_NAMED_TUPLE_CASTER(Point) <NB_NAMED_TUPLE_CASTER>`
at file scope for the type to be usable as a function argument or return type.

Per-field defaults
^^^^^^^^^^^^^^^^^^

``def_rw`` accepts an optional default value that becomes the Python field
default. As with :py:func:`collections.namedtuple`, defaults are right-aligned:
any field with a default must be preceded only by fields that also have
defaults. The default is applied automatically when calling the class from
Python and is surfaced in the generated stub.

.. code-block:: cpp

    struct LogConfig {
        std::string path;
        int level;
        bool color;
    };
    NB_NAMED_TUPLE_CASTER(LogConfig)

    // In NB_MODULE:
    nb::named_tuple<LogConfig>(m, "LogConfig")
        .def_rw("path", &LogConfig::path)
        .def_rw("level", &LogConfig::level, 20)        // default: INFO
        .def_rw("color", &LogConfig::color, true);

    // Python:
    //   LogConfig("/tmp/app.log")
    //   LogConfig("/tmp/app.log", level=30)
    //   LogConfig("/tmp/app.log", 30, False)

Optional fields and nesting
^^^^^^^^^^^^^^^^^^^^^^^^^^^

Fields of type ``std::optional<U>`` are converted to ``typing.Optional[U]`` in
the generated stub; ``std::nullopt`` round-trips as ``None``. A NamedTuple
type can be used as a field of another NamedTuple, including itself, as long
as the referenced class has been registered by the time the enclosing
``named_tuple<T>`` goes out of scope. Because the C++ struct definition needs
to be complete at the point a field of the same type is declared, self-
references must go through indirection -- ``std::vector<T>``, ``T *``, or
``std::shared_ptr<T>`` all work. ``std::optional<T>`` does *not*, since
``std::optional`` requires its element type to be complete:

.. code-block:: cpp

    struct Tree {
        int value;
        std::vector<Tree> children;   // self-reference via std::vector
    };
    NB_NAMED_TUPLE_CASTER(Tree)

    NB_NAMED_TUPLE(m, Tree, value, children);

Docstrings
^^^^^^^^^^

The helper API accepts an optional class docstring as the third constructor
argument, and per-field docstrings via :cpp:struct:`nb::doc` annotations on
``def_rw``. Both flow through to the generated ``.pyi`` and Python's standard
``__doc__`` introspection:

.. code-block:: cpp

    nb::named_tuple<Point>(m, "Point", "A 2D point in screen coordinates.")
        .def_rw("x", &Point::x, nb::doc("horizontal pixel offset"))
        .def_rw("y", &Point::y, 0, nb::doc("vertical pixel offset, default 0"));

The macro form (``NB_NAMED_TUPLE`` / ``NB_NAMED_TUPLE_NAMED``) is positional
and intentionally does not carry docstrings; use the helper API when
documentation is needed.

Qualified and templated C++ type names
^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^

:c:macro:`NB_NAMED_TUPLE` stringifies its ``Type`` argument and uses the
result as the Python class name. This only works when ``Type`` is itself a
valid Python identifier; for qualified names (``geom::Point``,
``ns::Inner``) the stringified form (``"geom::Point"``) is rejected by
:py:func:`collections.namedtuple`. Use :c:macro:`NB_NAMED_TUPLE_NAMED` to
provide an explicit Python identifier in that case, or instantiate
``nanobind::named_tuple<T>`` directly:

.. code-block:: cpp

    NB_NAMED_TUPLE_NAMED(m, geom::Point, "Point", x, y);

Templated types such as ``Foo<int, float>`` cannot be passed directly to the
macros: the comma between template arguments terminates the variadic
preprocessor argument list. Introduce a typedef first, or call the helper
API which does not go through the preprocessor at all:

.. code-block:: cpp

    using FooIF = Foo<int, float>;
    NB_NAMED_TUPLE(m, FooIF, x, y);

    // or, equivalently, directly through the helper API:
    nb::named_tuple<Foo<int, float>>(m, "FooIF")
        .def_rw("x", &Foo<int, float>::x)
        .def_rw("y", &Foo<int, float>::y);

Limitations
^^^^^^^^^^^

The runtime state used by NamedTuple bindings (per-T registries and the
typeid-keyed lookup map) is stored in function-local statics that are shared
process-wide. This matches nanobind's overall stance on sub-interpreters:
extension modules set ``Py_MOD_MULTIPLE_INTERPRETERS_NOT_SUPPORTED`` and
sub-interpreter isolation is not currently a supported configuration. If a
future nanobind release relaxes that restriction, NamedTuple state will need
to be relocated into ``nb_internals`` alongside other per-interpreter
registrations.

Accepted inputs and stub generation
^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^

When a function takes a NamedTuple-bound type, nanobind accepts either an
instance of the registered Python class *or* any plain tuple of the right
length and field types, mirroring the permissiveness of the built-in
``std::tuple`` caster. C++ functions returning the type always produce an
instance of the registered class.

The runtime class is constructed with :py:func:`collections.namedtuple`; the
generated ``.pyi`` stub, on the other hand, emits the canonical
``class Name(typing.NamedTuple): ...`` form with annotated fields and
defaults. This is the standard pattern for typing C-extension NamedTuples:
the runtime factory and the stub class share a binary-compatible shape, but
the stub gives static type checkers the field-level type information they
need.
