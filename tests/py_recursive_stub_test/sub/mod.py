MOD = 1011

# An ancestor binding planted by the import machinery, which stub
# generation must suppress
import py_recursive_stub_test

# Re-export of a sibling module from another subtree
from py_recursive_stub_test import bar

# An aliased module attribute (private in the stub, the typing spec cannot
# express a renamed re-export)
import py_recursive_stub_test.alias as nickname
