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

Internally, stub generation involves two potentially complex steps: converting
nanobind overload chains into '@overload' declarations that can be understood
by the 'typing' module, and turning default values back into Python
expressions. To make this process more well-defined, the implementation relies
on an internal ``__nb_signature__`` property that nanobind functions expose
specifically to simplify stub generation.

(Note that for now, the StubGen API is considered experimental and not subject
 to the semantic versioning policy used by the nanobind project.)
"""

import inspect
import textwrap
import importlib
import types
import typing
import re
import sys

# Standard operations supported by arithmetic enumerations
# fmt: off
ENUM_OPS = [
    "add", "sub", "mul", "floordiv", "eq", "ne", "gt", "ge", "lt", "le",
    "index", "repr", "hash", "int", "rshift", "lshift", "and", "or", "xor",
    "neg", "abs", "invert",
]  # fmt: on


class StubGen:
    def __init__(
        self,
        include_docstrings=True,
        include_private=False,
        module=None,
        max_expr_length=50,
    ):
        # Store the module (if available) so that we can check for naming
        # conflicts when introducing temporary variables needed within the stub
        self.module = module

        # Should docstrings be included in the generated stub?
        self.include_docstrings = include_docstrings

        # Should private members (that start or end with
        # a single underscore) be included?
        self.include_private = include_private

        # Maximal length (in characters) before an expression gets abbreviated as '...'
        self.max_expr_length = max_expr_length

        # ---------- Internal fields ----------

        # Current depth / indentation level
        self.depth = 0

        # Output will be appended to this string
        self.output = ""

        # A stack to avoid infinite recursion
        self.stack = []

        # Dictionary to keep track of import directives added by the stub generator
        self.imports = {}

        # Negative lookbehind matching word boundaries except '.'
        sep_before = r"(?<![\\B\.])"

        # Negative lookforward matching word boundaries except '.'
        sep_after = r"(?![\\B\.])"

        # Positive lookforward matching an opening bracket
        bracket = r"(?=\[)"

        # Regexp matching a Python identifier
        identifier = r"[^\d\W]\w*"

        # Regexp matching a sequence of identifiers separated by periods
        identifier_seq = (
            sep_before
            + "((?:"
            + identifier
            + r"\.)+)("
            + identifier
            + r")\b"
            + sep_after
        )

        # Precompile a regular expression used to extract types from 'typing.*'
        self.typing_re = re.compile(
            sep_before + r"(Union|Optional|Tuple|Dict|List|Annotated)" + bracket
        )

        # ditto, for 'typing.*' or 'collections.abc.*' depending on the Python version
        self.abc_re = re.compile(
            sep_before + r"(Callable|Tuple|Sequence|Mapping|Set|Iterator|Iterable)\b"
        )

        # Precompile a regular expression used to extract types from 'types.*'
        self.types_re = re.compile(
            sep_before + r"(ModuleType|CapsuleType|EllipsisType)\b"
        )

        # Precompile a regular expression used to extract nanobind nd-arrays
        self.ndarray_re = re.compile(
            sep_before + r"(numpy\.ndarray|ndarray|torch\.Tensor)\[([^\]]*)\]"
        )

        # Regular expression matching `builtins.*` types
        self.none_re = re.compile(sep_before + r"builtins\.(None)Type\b")
        self.builtins_re = re.compile(sep_before + r"builtins\.(" + identifier + ")")

        # Precompile a regular expression used to extract a few other types
        self.identifier_seq_re = re.compile(identifier_seq)

        # Regular expression to strip away the module for locals
        if module:
            module_name_re = self.module.__name__.replace(".", r"\.")
            self.module_member_re = re.compile(
                sep_before + module_name_re + r"\.(" + identifier + ")" + sep_after
            )
        else:
            self.module_member_re = None

        # Should we insert a dummy base class to handle enumerations?
        self.abstract_enum = False
        self.abstract_enum_arith = False

    def write(self, s):
        """Append raw characters to the output"""
        self.output += s

    def write_ln(self, line):
        """Append an indented line"""
        self.output += "    " * self.depth + line + "\n"

    def write_par(self, line):
        """Append an indented paragraph"""
        self.output += textwrap.indent(line, "    " * self.depth)

    def put_docstr(self, docstr):
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

    def put_nb_overload(self, fn, sig, name=None):
        """Append an 'nb_func' function overload"""
        sig_str, docstr, start = sig[0], sig[1], 0
        if sig_str.startswith("def (") and name is not None:
            sig_str = 'def ' + name + sig_str[4:]

        sig_str = self.replace_standard_types(sig_str)

        # Render function default arguments
        for index, arg in enumerate(sig[2:]):
            pos = sig_str.find(f"\\{index}", start)
            arg_str = self.expr_str(arg)
            if arg_str is None:
                arg_str = "..."
            assert (
                "\n" not in arg_str
            ), "Default argument string may not contain newlines."
            sig_str = sig_str[:pos] + arg_str + sig_str[pos + 2 :]
            start = pos + len(arg_str)

        if type(fn).__name__ == "nb_func" and self.depth > 0:
            self.write_ln("@staticmethod")

        if not docstr or not self.include_docstrings:
            for s in sig_str.split('\n'):
                self.write_ln(s)
            self.output = self.output[:-1] + ': ...\n'
        else:
            docstr = textwrap.dedent(docstr)
            for s in sig_str.split('\n'):
                self.write_ln(s)
            self.output = self.output[:-1] + ':\n'
            self.depth += 1
            self.put_docstr(docstr)
            self.depth -= 1
        self.write("\n")

    def put_nb_func(self, fn, name=None):
        """Append an 'nb_func' function object"""
        sigs = fn.__nb_signature__
        count = len(sigs)
        assert count > 0
        if count == 1:
            self.put_nb_overload(fn, sigs[0], name)
        else:
            overload = self.import_object("typing", "overload")
            for s in sigs:
                self.write_ln(f"@{overload}")
                self.put_nb_overload(fn, s, name)

    def put_function(self, fn, name=None, parent = None):
        """Append a function of an arbitrary type"""
        # Don't generate a constructor for nanobind classes that aren't constructible
        if name == "__init__" and type(parent).__name__.startswith("nb_type"):
            return

        if self.module:
            fn_module = getattr(fn, "__module__", None)
            fn_name = getattr(fn, "__name__", None)

            # Check if this function is an alias from another module
            if fn_module and fn_module != self.module.__name__:
                self.put_value(fn, name, None)
                return

            # Check if this function is an alias from the same module
            if name and fn_name and name != fn_name:
                self.write_ln(f"{name} = {fn_name}\n")
                return

        # Special handling for nanobind functions with overloads
        if type(fn).__module__ == 'nanobind':
            self.put_nb_func(fn, name)
            return

        if name is None:
            name = fn.__name__

        sig = inspect.signature(fn)
        sig_str = f"{name}{str(sig):}"
        docstr = fn.__doc__

        sig_str = self.replace_standard_types(sig_str)
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
        self.write_ln("@property")
        self.put(prop.fget, name=name)
        if prop.fset:
            self.write_ln(f"@{name}.setter")
            docstrings_backup = self.include_docstrings
            self.include_docstrings = (
                self.include_docstrings and prop.fget.__doc__ == prop.fset.__doc__
            )
            self.put(prop.fset, name=name)
            self.include_docstrings = docstrings_backup

    def put_nb_static_property(self, name, prop):
        """Append an 'nb_static_property' object"""
        getter_sig = prop.fget.__nb_signature__[0][0]
        getter_sig = getter_sig[getter_sig.find("/) -> ") + 6 :]
        self.write_ln(f"{name}: {getter_sig} = ...")
        if prop.__doc__ and self.include_docstrings:
            self.put_docstr(prop.__doc__)
        self.write("\n")

    def put_type(self, tp, module, name):
        """Append a 'nb_type' type object"""
        if name and (name != tp.__name__ or module.__name__ != tp.__module__):
            if module.__name__ == tp.__module__:
                # This is an alias of a type in the same module
                self.write_ln(f"{name} = {tp.__name__}\n")
            else:
                # Import from a different module
                self.put_value(tp, name, None)
        else:
            is_enum = self.is_enum(tp)
            docstr = tp.__doc__
            tp_dict = dict(tp.__dict__)
            tp_bases = None

            if is_enum:
                # Rewrite enumerations so that they derive from a helper
                # type to avoid bloat from a large number of repeated
                # function declaarations

                docstr = docstr.__doc__
                is_arith = "__add__" in tp_dict
                self.abstract_enum = True
                self.abstract_enum_arith |= is_arith

                tp_bases = ["_Enum" + ("Arith" if is_arith else "")]
                for op in ENUM_OPS:
                    name, rname = f"__{op}__", f"__r{op}__"
                    if name in tp_dict:
                        del tp_dict[name]
                    if rname in tp_dict:
                        del tp_dict[rname]

            if hasattr(tp, '__nb_signature__'):
                # Types with a custom signature override
                for s in tp.__nb_signature__.split('\n'):
                    self.write_ln(self.replace_standard_types(s))
                self.output = self.output[:-1] + ':\n'
            else:
                self.write_ln(f"class {tp.__name__}:")
                if tp_bases is None:
                    tp_bases = [self.type_str(base) for base in tp.__bases__]

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
                self.put(v, module, k, tp)
            if output_len == len(self.output):
                self.write_ln("pass\n")
            self.depth -= 1

    def is_enum(self, tp):
        """Check if the given type is an enumeration"""
        return hasattr(tp, "@entries")

    def is_function(self, tp):
        return (
            issubclass(tp, types.FunctionType)
            or issubclass(tp, types.BuiltinFunctionType)
            or issubclass(tp, types.BuiltinMethodType)
            or issubclass(tp, types.WrapperDescriptorType)
            or (tp.__module__ == "nanobind" and tp.__name__ == "nb_func")
        )

    def put_value(self, value, name, parent, abbrev=True):
        tp = type(value)
        is_function = self.is_function(tp)
        is_enum_entry = (
            isinstance(parent, type) and issubclass(tp, parent) and self.is_enum(parent)
        )

        if is_enum_entry:
            self.write_ln(f"{name}: {self.type_str(tp)}")
            if value.__doc__ and self.include_docstrings:
                self.put_docstr(value.__doc__)
            self.write("\n")
        elif is_function or isinstance(value, type):
            self.import_object(value.__module__, value.__name__, name)
        else:
            value_str = self.expr_str(value, abbrev)
            if value_str is None:
                value_str = "..."

            if issubclass(tp, typing.TypeVar):
                self.write_ln(f"{name} = {value_str}\n")
            else:
                self.write_ln(f"{name}: {self.type_str(tp)} = {value_str}\n")

    def replace_standard_types(self, s):
        """Detect standard types (e.g. typing.Optional) within a type signature"""

        # Strip module from types declared in the same module
        is_local = False
        if self.module_member_re:
            s_old = s
            s = self.module_member_re.sub(lambda m: m.group(1), s)
            is_local = s != s_old

        # Remove 'builtins.*'
        s = self.none_re.sub(lambda m: m.group(1), s)
        s = self.builtins_re.sub(lambda m: m.group(1), s)

        # tuple[] is not a valid type annotation
        s = s.replace("tuple[]", "tuple[()]").replace("Tuple[]", "Tuple[()]")

        # Rewrite typings/collection.abc types
        s = self.typing_re.sub(lambda m: self.import_object("typing", m.group(1)), s)
        source_pkg = "typing" if sys.version_info < (3, 9, 0) else "collections.abc"
        s = self.abc_re.sub(lambda m: self.import_object(source_pkg, m.group(1)), s)

        # Import a few types from 'types.*' as needed
        s = self.types_re.sub(lambda m: self.import_object("types", m.group(1)), s)

        # Process nd-array type annotations so that MyPy accepts them
        def replace_ndarray(m):
            s = m.group(2)

            annotated = self.import_object("typing", "Annotated")
            ndarray = self.import_object("numpy.typing", "ArrayLike")
            s = re.sub(r"dtype=([\w]*)\b", r"dtype='\g<1>'", s)
            s = s.replace("*", "None")

            if s:
                return f"{annotated}[{ndarray}, dict({s})]"
            else:
                return ndarray

        s = self.ndarray_re.sub(replace_ndarray, s)

        # For types from other modules, add suitable import statements
        def ensure_module_imported(m):
            self.import_object(m.group(1)[:-1], None)
            return m.group(0)

        if not is_local:
            s = self.identifier_seq_re.sub(ensure_module_imported, s)

        return s

    def put(self, value, module=None, name=None, parent=None):
        # Avoid infinite recursion due to cycles
        if value in self.stack:
            return
        try:
            self.stack.append(value)

            # Don't explicitly include various standard elements found
            # in modules, classes, etc.
            if name in (
                "__doc__",
                "__module__",
                "__name__",
                "__new__",
                "__builtins__",
                "__cached__",
                "__path__",
                "__version__",
                "__spec__",
                "__loader__",
                "__package__",
                "__getattribute__",
                "__setattribute__",
                "__nb_signature__",
                "__class_getitem__",
                "__file__",
                "__dict__",
                "__weakref__",
                "@entries",
            ):
                return

            # Potentially exclude private members
            if (
                not self.include_private
                and name
                and len(name) > 2
                and (
                    (name[0] == "_" and name[1] != "_")
                    or (name[-1] == "_" and name[-2] != "_")
                )
            ):
                return

            tp = type(value)
            tp_mod, tp_name = tp.__module__, tp.__name__

            if inspect.ismodule(value):
                if len(self.stack) != 1:
                    # Do not recurse into submodules, but include a directive to import them
                    self.import_object(".", name)
                    return
                for name, child in inspect.getmembers(value):
                    self.put(child, module=value, name=name, parent=value)
            elif self.is_function(tp):
                self.put_function(value, name, parent)
            elif issubclass(tp, type):
                self.put_type(value, module, name)
            elif tp_mod == "nanobind":
                if tp_name == "nb_method":
                    self.put_nb_func(value, name)
                elif tp_name == "nb_static_property":
                    self.put_nb_static_property(name, value)
            elif tp_mod == "builtins":
                if tp is property:
                    self.put_property(value, name)
                else:
                    abbrev = name != "__all__"
                    self.put_value(value, name, parent, abbrev=abbrev)
            else:
                self.put_value(value, name, parent)
        finally:
            self.stack.pop()

    def import_object(self, module, name, as_name=None):
        """
        Import a type (e.g. typing.Optional) used within the stub,
        ensuring that this does not cause conflicts
        """
        if module == "builtins" and (as_name is None or name == as_name):
            return name
        if self.module and module.startswith(self.module.__name__):
            module = module[len(self.module.__name__) :]

        if module not in self.imports:
            self.imports[module] = {}
        if name not in self.imports[module]:
            if as_name is None and name and self.module:
                # Perform conflict resolution if possible
                as_name = name
                while True:
                    if not hasattr(self.module, as_name):
                        break
                    try:
                        value = getattr(
                            importlib.import_module(self.module.__name__), as_name
                        )
                    except ImportError as e:
                        value = None
                    if getattr(self.module, as_name) is value:
                        break
                    as_name = "_" + as_name
                if as_name == name:
                    as_name = None
            self.imports[module][name] = as_name

        as_name = self.imports[module][name]
        return name if as_name is None else as_name

    def expr_str(self, e, abbrev=True):
        """Attempt to convert a value into a Python expression to generate that value"""
        tp = type(e)
        for t in [bool, int, type(None), type(...)]:
            if issubclass(tp, t):
                return repr(e)
        if issubclass(tp, float):
            s = repr(e)
            if 'inf' in s or 'nan' in s:
                return f"float('{s}')"
            else:
                return s
        elif self.is_enum(tp):
            return self.type_str(type(e)) + '.' + e.__name__
        elif issubclass(tp, type):
            return self.type_str(e)
        elif issubclass(tp, typing.TypeVar):
            s = f"typing.TypeVar(\"{e.__name__}\""
            for v in getattr(e, '__constraints__', ()):
                s += ", " + self.expr_str(v)
            for k in ['contravariant', 'covariant', 'bound', 'infer_variance']:
                v = getattr(e, f'__{k}__', None)
                if v:
                    s += f", {k}=" + self.expr_str(v)
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
                if k == None or v == None:
                    return None
                s += k + " : " + v
                if i + 1 < len(e):
                    s += ", "
            s += "}"
            if len(s) < self.max_expr_length or not abbrev:
                return s
            pass
        return None

    def type_str(self, tp):
        """Attempt to convert a type into a Python expression which reproduces it"""
        return self.replace_standard_types(tp.__module__ + "." + tp.__qualname__)

    def get(self):
        """Generate the final stub output"""
        s = ""

        for module in sorted(self.imports):
            si = ""
            imports = self.imports[module]
            for i, (k, v) in enumerate(imports.items()):
                if k == None:
                    continue
                si += k if v is None else f"{k} as {v}"
                if i + 1 < len(imports):
                    si += ","
                    if len(si) > 50:
                        si += "\n    "
                    else:
                        si += " "
            if None in imports:
                s += f"import {module}\n"
            if si:
                if "\n" in si:
                    s += f"from {module} import (\n    {si}\n)\n"
                else:
                    s += f"from {module} import {si}\n"
        if s:
            s += "\n"
        s += self.put_abstract_enum_class()
        s += self.output
        return s.rstrip() + "\n"

    def put_abstract_enum_class(self):
        s = ""
        if not self.abstract_enum:
            return s

        s += f"class _Enum:\n"
        s += f"    def __init__(self, arg: object, /) -> None: ...\n"
        s += f"    def __repr__(self, /) -> str: ...\n"
        s += f"    def __hash__(self, /) -> int: ...\n"
        s += f"    def __int__(self, /) -> int: ...\n"
        s += f"    def __index__(self, /) -> int: ...\n"

        for op in ["eq", "ne"]:
            s += f"    def __{op}__(self, arg: object, /) -> bool: ...\n"

        for op in ["gt", "ge", "lt", "le"]:
            s += f"    def __{op}__(self, arg: object, /) -> bool: ...\n"
        s += '\n'

        if not self.abstract_enum_arith:
            return s

        self_t = '_EnumArithT'
        s += f"class _EnumArith(_Enum):\n"
        for op in ["abs", "neg", "invert"]:
            s += f"    def __{op}__(self) -> int: ...\n"
        for op in ["add", "sub", "mul", "floordiv", "lshift", "rshift", "and", "or", "xor"]:
            s += f"    def __{op}__(self, arg: object, /) -> int: ...\n"
            s += f"    def __r{op}__(self, arg: object, /) -> int: ...\n"

        s += "\n"
        return s


def parse_options(args):
    import argparse

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
        "--marker",
        metavar="FILE",
        dest="marker",
        default=None,
        help="generate a marker file (usually named 'py.typed')",
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


def main(args=None):
    from pathlib import Path
    import sys
    import os

    # Ensure that the current directory is on the path
    if "" not in sys.path and "." not in sys.path:
        sys.path.insert(0, "")

    opt = parse_options(sys.argv[1:] if args is None else args)

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
            include_docstrings=opt.include_docstrings,
            include_private=opt.include_private,
            module=mod_imported,
        )

        if not opt.quiet:
            print("  - analyzing ..")

        sg.put(mod_imported)

        if opt.output_file:
            file = Path(opt.output_file)
        else:
            file = getattr(mod_imported, "__file__", None)
            if file is None:
                raise Exception(
                    'the module lacks a "__file__" attribute, hence '
                    "stubgen cannot infer where to place the generated "
                    "stub. You must specify the -o parameter to provide "
                    "the name of an output file."
                )
            file = Path(file)

            ext_loader = importlib.machinery.ExtensionFileLoader
            if isinstance(mod_imported.__loader__, ext_loader):
                file = file.with_name(mod_imported.__name__)
            file = file.with_suffix(".pyi")

            if opt.output_dir:
                file = Path(opt.output_dir, file.name)

        if not opt.quiet:
            print('  - writing stub "%s" ..' % str(file))

        with open(file, "w") as f:
            f.write(sg.get())

    if opt.marker:
        if not opt.quiet:
            print('  - writing marker file "%s" ..' % opt.marker)
        Path(opt.marker).touch()


if __name__ == "__main__":
    main()
