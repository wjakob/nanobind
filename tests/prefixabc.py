# This and prefix.py demonstrate an potential issue with import paths
# where we can't naively check for prefixes in the module name,
# but instead need to check for the module name followed by a dot.


class T:
    pass
