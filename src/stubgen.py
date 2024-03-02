#!/usr/bin/env python3
"""
stubgen.py: nanobind stub generation tool

This file provides both an API (``nanobind.stubgen.StubGen``) and a command
line interface to generate stubs for nanobind extensions.

To generate stubs on the command line, invoke the stub generator with a module
name, which will place the newly generated ``.pyi`` file directly into the
module folder.

```
python -m nanobind.stubgen <module name>
```

Specify ``-o <filename>`` or ``-O <path>`` to redirect the output somewhere
else in case this is not desired.

To programmatically generate stubs, construct an instance of the ``StubGen``
class and repeatedly call ``.put()`` to register modules or contents within the
modules (specific methods, classes, etc.). Afterwards, the ``.get()`` method
returns a string containing the stub declarations.

```
from nanobind.stubgen import StubGen
import my_module

sg = StubGen()
sg.put(my_module)
print(sg.get())
```

Internals:
----------

The implementation of stubgen performs a DFS traversal of the loaded module and
directly generates the stub in lockstep. There are no ASTs or other
intermediate data structures to keep things as simple as possible. Every kind
of object that could be encountered (functions, properties, values, types) has
a corresponding ``put_..`` method, along with the main ``put()`` entry point
that dispatches to the others as appropriate.

Internally, stub generation involves two potentially complex steps: converting
nanobind overload chains into '@overload' declarations that can be understood
by the 'typing' module, and turning default values back into Python
expressions. To make both steps well-defined, the implementation relies on an
internal ``__nb_signature__`` property that nanobind functions expose
specifically to simplify stub generation.

(Note that for now, the StubGen API is considered experimental and not subject
 to the semantic versioning policy used by the nanobind project.)
"""

import argparse
from inspect import Signature, Parameter, signature, ismodule, getmembers
import textwrap
import importlib
import importlib.machinery
import types
import typing
from dataclasses import dataclass
from typing import Dict, Sequence, List, Optional, Callable, Tuple, cast
import re
import sys

# Standard operations supported by arithmetic enumerations
# fmt: off
ENUM_OPS = [
    "add", "sub", "mul", "floordiv", "eq", "ne", "gt", "ge", "lt", "le",
    "index", "repr", "hash", "int", "rshift", "lshift", "and", "or", "xor",
    "neg", "abs", "invert",
] 


# Exclude various standard elements found in modules, classes, etc.
SKIP_LIST = [
    "__doc__", "__module__", "__name__", "__new__", "__builtins__",
    "__cached__", "__path__", "__version__", "__spec__", "__loader__",
    "__package__", "__nb_signature__", "__class_getitem__", "__orig_bases__",
    "__file__", "__dict__", "__weakref__", "@entries"
]
# fmt: on


@dataclass
class Pattern:
    # A replacement patterns as produced by ``load_pattern_file()`` below
    query: re.Pattern
    lines: List[str]
    matches: int


class StubGen:
    def __init__(
        self,
        module: types.ModuleType,
        include_docstrings: bool = True,
        include_private: bool = False,
        max_expr_length: int = 50,
        patterns: List[Pattern] = [],
    ) -> None:
        # Module to check for name conflicts when adding helper imports
        self.module = module

        # Include docstrings in the generated stub?
        self.include_docstrings = include_docstrings

        # Include private members that start or end with a single underscore?
        self.include_private = include_private

        # Maximal length (in characters) before an expression gets abbreviated as '...'
        self.max_expr_length = max_expr_length

        # Replacement patterns as produced by ``load_pattern_file()`` below
        self.patterns = patterns

        # ---------- Internal fields ----------

        # Current depth / indentation level
        self.depth = 0

        # Output will be appended to this string
        self.output = ""

        # A stack to avoid infinite recursion
        self.stack: List[object] = []

        # An identifier associated with the top element of the stack
        self.prefix = module.__name__

        # Dictionary to keep track of import directives added by the stub generator
        # Maps package_name -> ((name, desired_as_name) -> actual_as_name)
        self.imports: Dict[
            str, Dict[Tuple[Optional[str], Optional[str]], Optional[str]]
        ] = {}

        # ---------- Regular expressions ----------

        # Negative lookbehind matching word boundaries except '.'
        sep_before = r"(?<![\\B\.])"

        # Negative lookforward matching word boundaries except '.'
        sep_after = r"(?![\\B\.])"

        # Regexp matching a Python identifier
        identifier = r"[^\d\W]\w*"

        # Precompile RE a sequence of identifiers separated by periods
        self.id_seq = re.compile(
            sep_before
            + "((?:"
            + identifier
            + r"\.)+)("
            + identifier
            + r")\b"
            + sep_after
        )

        # Precompile RE to extract nanobind nd-arrays
        self.ndarray_re = re.compile(
            sep_before + r"(numpy\.ndarray|ndarray|torch\.Tensor)\[([^\]]*)\]"
        )

        # Should we insert a dummy base class to handle enumerations?
        self.abstract_enum = False
        self.abstract_enum_arith = False

    def write(self, s: str) -> None:
        """Append raw characters to the output"""
        self.output += s

    def write_ln(self, line: str) -> None:
        """Append an indented line"""
        if len(line) != 0 and not line.isspace():
            self.output += "    " * self.depth + line
        self.output += "\n"

    def write_par(self, line: str) -> None:
        """Append an indented paragraph"""
        self.output += textwrap.indent(line, "    " * self.depth)

    def put_docstr(self, docstr: str) -> None:
        """Append an indented single or multi-line docstring"""
        docstr = textwrap.dedent(docstr).strip()
        raw_str = ""
        if "''" in docstr or "\\" in docstr:
            # Escape all double quotes so that no unquoted triple quote can exist
            docstr = docstr.replace("''", "\\'\\'")
            raw_str = "r"
        if len(docstr) > 70 or "\n" in docstr:
            docstr = "\n" + docstr + "\n"
        docstr = f'{raw_str}"""{docstr}"""\n'
        self.write_par(docstr)

    def put_nb_overload(self, fn, sig: List, name: Optional[str] = None) -> None:
        """
        The ``put_nb_func()`` repeatedly calls this method to render the
        individual method overloads.
        """
        sig_str, docstr, start = cast(str, sig[0]), cast(str, sig[1]), 0

        # Label anonymous functoins
        if sig_str.startswith("def (") and name is not None:
            sig_str = "def " + name + sig_str[4:]

        # Simplify type names present in the signature
        paren = sig_str.find("(")
        sig_str = sig_str[:paren] + self.simplify_types(sig_str[paren:])

        # Substitute in string versions of the default arguments
        for index, arg in enumerate(sig[2:]):
            pos = -1
            custom_signature = False
            pattern = None

            # First, handle the case where the user overrode the default value signature
            if isinstance(arg, str):
                pattern = f"\\={index}"
                pos = sig_str.find(pattern, start)
                if pos >= 0:
                    custom_signature = True

            # General case
            if pos < 0:
                pattern = f"\\{index}"
                pos = sig_str.find(pattern, start)

            if pos < 0:
                raise Exception(
                    "Could not locate default argument in function signature"
                )

            if custom_signature:
                arg_str = arg
            else:
                # Call expr_str to convert the default value to a string.
                # Abbreviate with '...' if it is too long.
                arg_str = self.expr_str(arg, abbrev=True)
                if arg_str is None:
                    arg_str = "..."

            assert (
                "\n" not in arg_str
            ), "Default argument string may not contain newlines."

            assert pattern is not None
            sig_str = sig_str[:pos] + arg_str + sig_str[pos + len(pattern) :]
            start = pos + len(arg_str)

        if type(fn).__name__ == "nb_func" and self.depth > 0:
            self.write_ln("@staticmethod")

        if not docstr or not self.include_docstrings:
            for s in sig_str.split("\n"):
                self.write_ln(s)
            self.output = self.output[:-1] + ": ...\n"
        else:
            docstr = textwrap.dedent(docstr)
            for s in sig_str.split("\n"):
                self.write_ln(s)
            self.output = self.output[:-1] + ":\n"
            self.depth += 1
            self.put_docstr(docstr)
            self.depth -= 1
        self.write("\n")

    def put_nb_func(self, fn, name: Optional[str] = None) -> None:
        """Append a nanobind function binding to the stub"""
        sigs = fn.__nb_signature__
        count = len(sigs)
        assert count > 0
        if count == 1:
            # No overloads write directly
            self.put_nb_overload(fn, sigs[0], name)
        else:
            # Render an @overload-decorated chain
            overload = self.import_object("typing", "overload")
            for s in sigs:
                self.write_ln(f"@{overload}")
                self.put_nb_overload(fn, s, name)

    def put_function(self, fn, name: Optional[str] = None, parent=None):
        """Append a function of an arbitrary type to the stub"""
        # Don't generate a constructor for nanobind classes that aren't constructible
        if name == "__init__" and type(parent).__name__.startswith("nb_type"):
            return

        fn_module = getattr(fn, "__module__", None)
        fn_name = getattr(fn, "__name__", None)

        # Check if this function is an alias from *another* module
        if name and fn_module and fn_module != self.module.__name__:
            self.put_value(fn, name)
            return

        # Check if this function is an alias from the *same* module
        if name and fn_name and name != fn_name:
            self.write_ln(f"{name} = {fn_name}\n")
            return

        if isinstance(fn, staticmethod):
            self.write_ln("@staticmethod")
            fn = fn.__func__
        elif isinstance(fn, classmethod):
            self.write_ln("@staticmethod")
            fn = fn.__func__

        # Special handling for nanobind functions with overloads
        if type(fn).__module__ == "nanobind":
            self.put_nb_func(fn, name)
            return

        if name is None:
            name = fn.__name__

        overloads: Sequence[Callable] = []
        if hasattr(fn, "__module__"):
            if sys.version_info >= (3, 11, 0):
                overloads = typing.get_overloads(fn)
            else:
                try:
                    import typing_extensions

                    overloads = typing_extensions.get_overloads(fn)
                except ModuleNotFoundError:
                    raise RuntimeError(
                        "stubgen.py requires the 'typing_extension' package on Python <3.11"
                    )
        if not overloads:
            overloads = [fn]

        for i, fno in enumerate(overloads):
            if len(overloads) > 1:
                overload = self.import_object("typing", "overload")
                self.write_ln(f"@{overload}")

            sig_str = f"{name}{self.signature_str(signature(fno))}"

            # Potentially copy docstring from the implementation function
            docstr = fno.__doc__
            if i == 0 and not docstr and fn.__doc__:
                docstr = fn.__doc__

            if not docstr or not self.include_docstrings:
                self.write_ln("def " + sig_str + ": ...")
            else:
                self.write_ln("def " + sig_str + ":")
                self.depth += 1
                self.put_docstr(docstr)
                self.depth -= 1
            self.write("\n")

    def put_property(self, prop, name):
        """Append a Python 'property' object"""
        fget, fset = prop.fget, prop.fset
        self.write_ln("@property")
        self.put(fget, name=name)
        if fset:
            self.write_ln(f"@{name}.setter")
            docstrings_backup = self.include_docstrings
            if (
                type(fget).__module__ == "nanobind"
                and type(fset).__module__ == "nanobind"
            ):
                doc1 = fget.__nb_signature__[0][1]
                doc2 = fset.__nb_signature__[0][1]
                if doc1 and doc2 and doc1 == doc2:
                    self.include_docstrings = False
            self.put(prop.fset, name=name)
            self.include_docstrings = docstrings_backup

    def put_nb_static_property(self, name, prop):
        """Append a 'nb_static_property' object"""
        getter_sig = prop.fget.__nb_signature__[0][0]
        getter_sig = getter_sig[getter_sig.find("/) -> ") + 6 :]
        self.write_ln(f"{name}: {getter_sig} = ...")
        if prop.__doc__ and self.include_docstrings:
            self.put_docstr(prop.__doc__)
        self.write("\n")

    def put_type(self, tp, name):
        """Append a 'nb_type' type object"""
        if name and (name != tp.__name__ or self.module.__name__ != tp.__module__):
            if self.module.__name__ == tp.__module__:
                # This is an alias of a type in the same module
                alias_tp = self.import_object("typing", "TypeAlias")
                self.write_ln(f"{name}: {alias_tp} = {tp.__name__}\n")
            else:
                # Import from a different module
                self.put_value(tp, name)
        else:
            is_enum = self.is_enum(tp)
            docstr = tp.__doc__
            tp_dict = dict(tp.__dict__)
            tp_bases = None

            if is_enum:
                # Rewrite enumerations so that they derive from a helper type
                # to avoid bloat from a large number of repeated function
                # declarations

                docstr = docstr.__doc__
                is_arith = "__add__" in tp_dict
                self.abstract_enum = True
                self.abstract_enum_arith |= is_arith

                tp_bases = ["_Enum" + ("Arith" if is_arith else "")]
                del tp_dict["name"]
                del tp_dict["value"]
                for op in ENUM_OPS:
                    name, rname = f"__{op}__", f"__r{op}__"
                    if name in tp_dict:
                        del tp_dict[name]
                    if rname in tp_dict:
                        del tp_dict[rname]

            if "__nb_signature__" in tp.__dict__:
                # Types with a custom signature override
                for s in tp.__nb_signature__.split("\n"):
                    self.write_ln(self.simplify_types(s))
                self.output = self.output[:-1] + ":\n"
            else:
                self.write_ln(f"class {tp.__name__}:")
                if tp_bases is None:
                    tp_bases = getattr(tp, "__orig_bases__", None)
                    if tp_bases is None:
                        tp_bases = tp.__bases__
                    tp_bases = [self.type_str(base) for base in tp_bases]

                if tp_bases != ["object"]:
                    self.output = self.output[:-2] + "("
                    for i, base in enumerate(tp_bases):
                        if i:
                            self.write(", ")
                        self.write(base)
                    self.write("):\n")

            self.depth += 1
            output_len = len(self.output)
            if docstr and self.include_docstrings:
                self.put_docstr(docstr)
                if len(tp_dict):
                    self.write("\n")
            for k, v in tp_dict.items():
                self.put(v, k, tp)
            if output_len == len(self.output):
                self.write_ln("pass\n")
            self.depth -= 1

    def is_enum(self, tp: type) -> bool:
        """Check if the given type is an enumeration"""
        return hasattr(tp, "@entries")

    def is_function(self, tp) -> bool:
        """
        Test if this is one of the many types of built-in functions supported
        by Python, or if it is a nanobind ``nb_func``.
        """
        return (
            issubclass(tp, types.FunctionType)
            or issubclass(tp, types.BuiltinFunctionType)
            or issubclass(tp, types.BuiltinMethodType)
            or issubclass(tp, types.WrapperDescriptorType)
            or issubclass(tp, staticmethod)
            or issubclass(tp, classmethod)
            or (tp.__module__ == "nanobind" and tp.__name__ == "nb_func")
        )

    def put_value(self, value: object, name: str, parent=None, abbrev=True) -> None:
        """
        Render a ``name: type = value`` assignment at the module, class, or
        enum scope.

        The parameter ``abbrev`` indicates if it is acceptable to reduce very
        long expressions to ``...``.
        """
        tp = type(value)

        if isinstance(parent, type) and issubclass(tp, parent) and self.is_enum(parent):
            # This is an entry of an enumeration
            self.write_ln(f"{name}: {self.type_str(tp)}")
            if value.__doc__ and self.include_docstrings:
                self.put_docstr(value.__doc__)
            self.write("\n")
        elif self.is_function(tp) or isinstance(value, type):
            # This is a function or a type, import it from its actual source
            self.import_object(value.__module__, value.__name__, name)  # type: ignore
        else:
            value_str = self.expr_str(value, abbrev)

            if value_str is None:
                value_str = "..."

            # Catch a few different typing.* constructs
            if issubclass(tp, typing.TypeVar) or (
                hasattr(typing, "TypeVarTuple") and issubclass(tp, typing.TypeVarTuple)
            ):
                types = ""
            elif typing.get_origin(value):
                types = ": " + self.import_object("typing", "TypeAlias")
            else:
                types = f": {self.type_str(tp)}"

            self.write_ln(f"{name}{types} = {value_str}\n")

    def simplify_types(self, s: str) -> str:
        """
        Process types that occur within a signature string ``s`` and simplify
        them. This function applies the following rules:

        - "local_module.X" -> "X"

        - "other_module.X" -> "other_module.XX"
          (with "import other_module" added at top)

        - "builtins.X" -> "X"

        - "NoneType" -> "None"

        - "ndarray[...]" -> "Annotated[ArrayLike, dict(...)]"

        - "collections.abc.X" -> "X"
          (with "from collections.abc import X" added at top)

        - "typing.X" -> "X"
          (with "from typing import X" added at top)
        """

        # Process nd-array type annotations so that MyPy accepts them
        def process_ndarray(m: re.Match) -> str:
            s = m.group(2)

            ndarray = self.import_object("numpy.typing", "ArrayLike")
            s = re.sub(r"dtype=([\w]*)\b", r"dtype='\g<1>'", s)
            s = s.replace("*", "None")

            if s:
                annotated = self.import_object("typing", "Annotated")
                return f"{annotated}[{ndarray}, dict({s})]"
            else:
                return ndarray

        s = self.ndarray_re.sub(process_ndarray, s)

        # Process other type names and add suitable import statements
        def process_general(m: re.Match) -> str:
            full_name, mod_name, cls_name = m.group(0), m.group(1)[:-1], m.group(2)

            if mod_name == "builtins":
                # Simplify builtins
                return cls_name if cls_name != "NoneType" else "None"
            if full_name.startswith(self.module.__name__):
                # Strip away the module prefix for local classes
                return full_name[len(self.module.__name__) + 1 :]
            elif mod_name == "typing" or mod_name == "collections.abc":
                # Import frequently-occuring typing classes and ABCs directly
                return self.import_object(mod_name, cls_name)
            else:
                # Import the module and reference the contained class by name
                self.import_object(mod_name, None)
                return full_name

        s = self.id_seq.sub(process_general, s)

        return s

    def apply_pattern(self, value: object, pattern: Pattern, match: re.Match) -> None:
        """
        Called when ``value`` matched an entry of a pattern file
        """
        for line in pattern.lines:
            ls = line.strip()
            if ls == "\\doc":
                # Docstring reference
                tp = type(value)
                doc: Optional[str] = None
                if tp.__module__ == "nanobind" and tp.__name__ in (
                    "nb_func",
                    "nb_method",
                ):
                    for tp_i in value.__nb_signature__:  # type: ignore
                        doc = tp_i[1]
                        if doc:
                            break
                else:
                    doc = getattr(value, "__doc__", None)
                self.depth += 1
                if doc and self.include_docstrings:
                    self.put_docstr(doc)
                else:
                    self.write_ln("pass")
                self.depth -= 1
                continue
            elif ls.startswith("\\from "):
                items = ls[5:].split(" import ")
                if len(items) != 2:
                    raise RuntimeError(f"Could not parse import declaration {ls}")
                for item in items[1].strip("()").split(","):
                    item_list = item.split(" as ")
                    import_module, import_name = (
                        items[0].strip(),
                        item_list[0].strip(),
                    )
                    import_as = item_list[1].strip() if len(item_list) > 1 else None
                    self.import_object(import_module, import_name, import_as)
                continue

            groups = match.groups()
            for i in reversed(range(len(groups))):
                line = line.replace(f"\\{i+1}", groups[i])
            for k, v in match.groupdict().items():
                line = line.replace(f"\\{k}", v)
            self.write_ln(line)

    def put(self, value: object, name: Optional[str] = None, parent=None) -> None:
        old_prefix = self.prefix

        if value in self.stack:
            # Avoid infinite recursion due to cycles
            return

        try:
            self.stack.append(value)
            self.prefix = self.prefix + (("." + name) if name else "")

            # Check if an entry in a provided pattern file matches
            if self.prefix:
                for pattern in self.patterns:
                    match = pattern.query.search(self.prefix)
                    if match:
                        # If so, don't recurse
                        self.apply_pattern(value, pattern, match)
                        return

            # Exclude various standard elements found in modules, classes, etc.
            if name in SKIP_LIST:
                return

            is_type_alias = typing.get_origin(value) or (
                isinstance(value, type)
                and (value.__name__ != name or value.__module__ != self.module.__name__)
            )

            # Ignore private members unless the user requests their inclusion
            if (
                not self.include_private
                and name
                and not is_type_alias
                and len(name) > 2
                and (
                    (name[0] == "_" and name[1] != "_")
                    or (name[-1] == "_" and name[-2] != "_")
                )
            ):
                return

            tp = type(value)
            tp_mod, tp_name = tp.__module__, tp.__name__

            if ismodule(value):
                if len(self.stack) != 1:
                    # Do not recurse into submodules, but include a directive to import them
                    self.import_object(value.__name__, name=None, as_name=name)
                    return
                for name, child in getmembers(value):
                    self.put(child, name=name, parent=value)
            elif self.is_function(tp):
                self.put_function(value, name, parent)
            elif issubclass(tp, type):
                self.put_type(value, name)
            elif tp_mod == "nanobind":
                if tp_name == "nb_method":
                    self.put_nb_func(value, name)
                elif tp_name == "nb_static_property":
                    self.put_nb_static_property(name, value)
            elif tp_mod == "builtins":
                if tp is property:
                    self.put_property(value, name)
                else:
                    assert name is not None
                    abbrev = name != "__all__"
                    self.put_value(value, name, parent, abbrev=abbrev)
            else:
                assert name is not None
                self.put_value(value, name, parent)
        finally:
            self.stack.pop()
            self.prefix = old_prefix

    def import_object(
        self, module: str, name: Optional[str], as_name: Optional[str] = None
    ) -> str:
        """
        Import a type (e.g. typing.Optional) used within the stub, ensuring
        that this does not cause conflicts. Specify ``as_name`` to ensure that
        the import is bound to a specified name.

        When ``name`` is None, the entire module is imported.
        """
        if module == "builtins" and name and (not as_name or name == as_name):
            return name

        # Rewrite module name if this is relative import from a submodule
        if module.startswith(self.module.__name__):
            module_short = module[len(self.module.__name__) :]
            if not name and as_name and module_short[0] == ".":
                name = as_name = module_short[1:]
                module_short = "."
        else:
            module_short = module

        # Query a cache of previously imported objects
        imports_module = self.imports.get(module_short, None)
        if not imports_module:
            imports_module = {}
            self.imports[module_short] = imports_module

        key = (name, as_name)
        final_name = imports_module.get(key, None)
        if final_name:
            return final_name

        # Cache miss, import the object
        final_name = as_name if as_name else name

        # If no as_name constraint was set, potentially adjust the name to
        # avoid conflicts with an existing object of the same name
        if name and not as_name:
            test_name = name
            while True:
                # Accept the name if there are no conflicts
                if not hasattr(self.module, test_name):
                    break
                value = getattr(self.module, test_name)
                try:
                    if module == ".":
                        mod_o = self.module
                    else:
                        mod_o = importlib.import_module(module)

                    # If there is a conflict, accept it if it refers to the same object
                    if getattr(mod_o, name) is value:
                        break
                except ImportError:
                    pass

                # Prefix with an underscore
                test_name = "_" + test_name
            final_name = test_name

        imports_module[key] = final_name
        return final_name if final_name else ""

    def expr_str(self, e, abbrev=True):
        """
        Attempt to convert a value into valid Python syntax that regenerates
        that value. When ``abbrev`` is True, give up and replace with '...' if
        the expression is too complicated to be included in the stubs
        """
        tp = type(e)
        for t in [bool, int, type(None), type(...)]:
            if issubclass(tp, t):
                return repr(e)
        if issubclass(tp, float):
            s = repr(e)
            if "inf" in s or "nan" in s:
                return f"float('{s}')"
            else:
                return s
        elif self.is_enum(tp):
            return self.type_str(type(e)) + "." + e.__name__
        elif issubclass(tp, type) or typing.get_origin(e):
            return self.type_str(e)
        elif issubclass(tp, typing.ForwardRef):
            return f'"{e.__forward_arg__}"'
        elif hasattr(typing, "TypeVarTuple") and issubclass(tp, typing.TypeVarTuple):
            tv = self.import_object("typing", "TypeVarTuple")
            return f'{tv}("{e.__name__}")'
        elif issubclass(tp, typing.TypeVar):
            tv = self.import_object("typing", "TypeVar")
            s = f'{tv}("{e.__name__}"'
            for v in getattr(e, "__constraints__", ()):
                s += ", " + self.expr_str(v)
            for k in ["contravariant", "covariant", "bound", "infer_variance"]:
                v = getattr(e, f"__{k}__", None)
                if v:
                    v = self.expr_str(v)
                    if v is None:
                        return
                    s += f", {k}=" + v
            s += ")"
            return s
        elif issubclass(tp, str):
            s = repr(e)
            if len(s) < self.max_expr_length or not abbrev:
                return s
        elif issubclass(tp, list) or issubclass(tp, tuple):
            e = [self.expr_str(v, abbrev) for v in e]
            if None in e:
                return None
            if issubclass(tp, list):
                s = "[" + ", ".join(e) + "]"
            else:
                s = "(" + ", ".join(e) + ")"
            if len(s) < self.max_expr_length or not abbrev:
                return s
        elif issubclass(tp, dict):
            e = [
                (self.expr_str(k, abbrev), self.expr_str(v, abbrev))
                for k, v in e.items()
            ]
            s = "{"
            for i, (k, v) in enumerate(e):
                if k is None or v is None:
                    return None
                s += k + " : " + v
                if i + 1 < len(e):
                    s += ", "
            s += "}"
            if len(s) < self.max_expr_length or not abbrev:
                return s
            pass
        return None

    def signature_str(self, s: Signature):
        """Convert an inspect.Signature to into valid Python syntax"""
        posonly_sep, kwonly_sep = False, True
        params = []

        # Logic for placing '*' and '/' based on the
        # signature.Signature implementation
        for param in s.parameters.values():
            kind = param.kind

            if kind == Parameter.POSITIONAL_ONLY:
                posonly_sep = True
            elif posonly_sep:
                params.append("/")
                posonly_sep = False

            if kind == Parameter.VAR_POSITIONAL:
                kwonly_sep = False
            elif kind == Parameter.KEYWORD_ONLY and kwonly_sep:
                params.append("*")
                kwonly_sep = False
            params.append(self.param_str(param))

        if posonly_sep:
            params.append("/")

        result = f"({', '.join(params)})"
        if s.return_annotation != Signature.empty:
            result += " -> " + self.type_str(s.return_annotation)
        return result

    def param_str(self, p: Parameter):
        result = ""
        if p.kind == Parameter.VAR_POSITIONAL:
            result += "*"
        elif p.kind == Parameter.VAR_KEYWORD:
            result += "**"
        result += p.name
        has_type = p.annotation != Parameter.empty
        has_def = p.default != Parameter.empty

        if has_type:
            result += ": " + self.type_str(p.annotation)
        if has_def:
            result += " = " if has_type else "="
            result += self.expr_str(p.default)
        return result

    def type_str(self, tp):
        """Attempt to convert a type into a Python expression which reproduces it"""
        origin, args = typing.get_origin(tp), typing.get_args(tp)

        if isinstance(tp, typing.TypeVar):
            return tp.__name__
        elif isinstance(tp, typing.ForwardRef):
            return repr(tp.__forward_arg__)
        elif isinstance(tp, list):
            return "[" + ", ".join(self.type_str(a) for a in tp) + "]"
        elif isinstance(tp, tuple):
            return "(" + ", ".join(self.type_str(a) for a in tp) + ")"
        elif isinstance(tp, dict):
            return (
                "{"
                + ", ".join(repr(k) + ": " + self.type_str(v) for k, v in tp.items())
                + "}"
            )
        elif origin and args:
            result = (
                self.type_str(origin)
                + "["
                + ", ".join(self.type_str(a) for a in args)
                + "]"
            )
        elif tp is types.ModuleType:
            result = "types.ModuleType"
        elif isinstance(tp, type):
            result = tp.__module__ + "." + tp.__qualname__
        else:
            result = repr(tp)
        return self.simplify_types(result)

    def get(self) -> str:
        """Generate the final stub output"""
        s = ""

        for module in sorted(self.imports):
            imports = self.imports[module]
            items = []

            for (k, v1), v2 in imports.items():
                if k is None:
                    if v1 and v1 != module:
                        s += f"import {module} as {v1}\n"
                    else:
                        s += f"import {module}\n"
                else:
                    if k != v2 or v1:
                        items.append(f"{k} as {v2}")
                    else:
                        items.append(k)

            if items:
                items_v0 = ", ".join(items)
                items_v0 = f"from {module} import {items_v0}\n"
                items_v1 = "(\n    " + ",\n    ".join(items) + "\n)"
                items_v1 = f"from {module} import {items_v1}\n"
                s += items_v0 if len(items_v0) <= 70 else items_v1
        if s:
            s += "\n"
        s += self.put_abstract_enum_class()
        s += self.output
        return s.rstrip() + "\n"

    def put_abstract_enum_class(self) -> str:
        s = ""
        if not self.abstract_enum:
            return s

        s += "class _Enum:\n"
        s += "    def __init__(self, arg: object, /) -> None: ...\n"
        s += "    def __repr__(self, /) -> str: ...\n"
        s += "    def __hash__(self, /) -> int: ...\n"
        s += "    def __int__(self, /) -> int: ...\n"
        s += "    def __index__(self, /) -> int: ...\n"

        for op in ["eq", "ne"]:
            s += f"    def __{op}__(self, arg: object, /) -> bool: ...\n"

        for op in ["gt", "ge", "lt", "le"]:
            s += f"    def __{op}__(self, arg: object, /) -> bool: ...\n"
        s += "    def name(self, /) -> str: ...\n"
        s += "    def value(self, /) -> int: ...\n"
        s += "\n"

        if not self.abstract_enum_arith:
            return s

        s += "class _EnumArith(_Enum):\n"
        for op in ["abs", "neg", "invert"]:
            s += f"    def __{op}__(self) -> int: ...\n"
        for op in [
            "add",
            "sub",
            "mul",
            "floordiv",
            "lshift",
            "rshift",
            "and",
            "or",
            "xor",
        ]:
            s += f"    def __{op}__(self, arg: object, /) -> int: ...\n"
            s += f"    def __r{op}__(self, arg: object, /) -> int: ...\n"

        s += "\n"
        return s


def parse_options(args: List[str]) -> argparse.Namespace:
    parser = argparse.ArgumentParser(
        prog="python -m nanobind.stubgen",
        description="Generate stubs for nanobind-based extensions.",
    )

    parser.add_argument(
        "-o",
        "--output-file",
        metavar="FILE",
        dest="output_file",
        default=None,
        help="write generated stubs to the specified file",
    )

    parser.add_argument(
        "-O",
        "--output-dir",
        metavar="PATH",
        dest="output_dir",
        default=None,
        help="write generated stubs to the specified directory",
    )

    parser.add_argument(
        "-i",
        "--import",
        action="append",
        metavar="PATH",
        dest="imports",
        default=[],
        help="add the directory to the Python import path (can specify multiple times)",
    )

    parser.add_argument(
        "-m",
        "--module",
        action="append",
        metavar="MODULE",
        dest="modules",
        default=[],
        help="generate a stub for the specified module (can specify multiple times)",
    )

    parser.add_argument(
        "-M",
        "--marker-file",
        metavar="FILE",
        dest="marker_file",
        default=None,
        help="generate a marker file (usually named 'py.typed')",
    )

    parser.add_argument(
        "-p",
        "--pattern-file",
        metavar="FILE",
        dest="pattern_file",
        default=None,
        help="apply the given patterns to the generated stub (see the docs for syntax)",
    )

    parser.add_argument(
        "-P",
        "--include-private",
        dest="include_private",
        default=False,
        action="store_true",
        help="include private members (with single leading or trailing underscore)",
    )

    parser.add_argument(
        "-D",
        "--exclude-docstrings",
        dest="include_docstrings",
        default=True,
        action="store_false",
        help="exclude docstrings from the generated stub",
    )

    parser.add_argument(
        "-q",
        "--quiet",
        default=False,
        action="store_true",
        help="do not generate any output in the absence of failures",
    )

    opt = parser.parse_args(args)
    if len(opt.modules) == 0:
        parser.error("At least one module must be specified.")
    if len(opt.modules) > 1 and opt.output_file:
        parser.error(
            "The -o option can only be specified when a single module is being processed."
        )
    return opt


def load_pattern_file(fname: str) -> List[Pattern]:
    with open(fname, "r") as f:
        f_lines = f.readlines()

    patterns: List[Pattern] = []

    def add_pattern(query, lines):
        # Exactly 1 empty line at the end
        while lines and lines[-1].isspace():
            lines.pop()
        lines.append("")

        # Identify deletions (replacement by only whitespace)
        if all((p.isspace() or len(p) == 0 for p in lines)):
            lines = []
        patterns.append(Pattern(re.compile(query[:-1]), lines, 0))

    lines: List[str]
    lines, query, dedent = [], None, 0
    for i, line in enumerate(f_lines):
        line = line.rstrip()

        if line.startswith("#"):
            continue

        if len(line) == 0 or line[0].isspace():
            if not lines:
                stripped = line.lstrip()
                dedent = len(line) - len(stripped)
                lines.append(stripped)
            else:
                s1, s2 = line.lstrip(), line[dedent:]
                lines.append(s2 if len(s2) > len(s1) else s1)
        else:
            if not line.endswith(":"):
                raise Exception(f'Cannot parse line {i+1} of pattern file "{fname}"')

            if query:
                add_pattern(query, lines)
            query = line
            lines = []

    if query:
        add_pattern(query, lines)

    return patterns


def main(args: Optional[List[str]] = None) -> None:
    from pathlib import Path
    import sys
    import os

    # Ensure that the current directory is on the path
    if "" not in sys.path and "." not in sys.path:
        sys.path.insert(0, "")

    opt = parse_options(sys.argv[1:] if args is None else args)

    patterns: List[Pattern]
    if opt.pattern_file:
        if not opt.quiet:
            print('Using pattern file "%s" ..' % opt.pattern_file)
        patterns = load_pattern_file(opt.pattern_file)
        if not opt.quiet:
            print("  - loaded %i patterns.\n" % len(patterns))
    else:
        patterns = []

    for i in opt.imports:
        sys.path.insert(0, i)

    if opt.output_dir:
        os.makedirs(opt.output_dir, exist_ok=True)

    for i, mod in enumerate(opt.modules):
        if not opt.quiet:
            if i > 0:
                print("\n")
            print('Module "%s" ..' % mod)
            print("  - importing ..")
        mod_imported = importlib.import_module(mod)

        sg = StubGen(
            module=mod_imported,
            include_docstrings=opt.include_docstrings,
            include_private=opt.include_private,
            patterns=patterns,
        )

        if not opt.quiet:
            print("  - analyzing ..")

        sg.put(mod_imported)

        if opt.output_file:
            file = Path(opt.output_file)
        else:
            file_s = getattr(mod_imported, "__file__", None)
            if file_s is None:
                raise Exception(
                    'the module lacks a "__file__" attribute, hence '
                    "stubgen cannot infer where to place the generated "
                    "stub. You must specify the -o parameter to provide "
                    "the name of an output file."
                )
            file = Path(str(file_s))

            ext_loader = importlib.machinery.ExtensionFileLoader
            if isinstance(mod_imported.__loader__, ext_loader):
                file = file.with_name(mod_imported.__name__)
            file = file.with_suffix(".pyi")

            if opt.output_dir:
                file = Path(opt.output_dir, file.name)

        if patterns:
            total_matches = 0
            for p in patterns:
                if p.matches != 0:
                    total_matches += p.matches
                    continue
                rule_str = str(p.query)
                if "re.compile" in rule_str:
                    rule_str = rule_str.replace("re.compile(", "")[:-1]
                if not opt.quiet:
                    print(f"  - warning: rule {rule_str} did not match any elements.")
            if not opt.quiet:
                print(f"  - applied {total_matches} patterns.")

        if not opt.quiet:
            print(f'  - writing stub "{file}" ..')

        with open(file, "w") as f:
            f.write(sg.get())

    if opt.marker_file:
        if not opt.quiet:
            print(f'  - writing marker file "{opt.marker_file}" ..')
        Path(opt.marker_file).touch()


if __name__ == "__main__":
    main()
