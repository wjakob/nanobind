import sys
import os

if sys.version_info < (3, 8):
    raise ImportError("nanobind does not support Python < 3.8.")

def source_dir() -> str:
    "Return the path to the nanobind source directory."
    return os.path.join(os.path.abspath(os.path.dirname(__file__)), "src")

def include_dir() -> str:
    "Return the path to the nanobind include directory"
    return os.path.join(os.path.abspath(os.path.dirname(__file__)), "include")

def cmake_dir() -> str:
    "Return the path to the nanobind CMake module directory."
    return os.path.join(os.path.abspath(os.path.dirname(__file__)), "cmake")

__version__ = "2.7.1-dev1"

__all__ = (
    "__version__",
    "source_dir",
    "include_dir",
    "cmake_dir",
)
