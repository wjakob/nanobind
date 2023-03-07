from setuptools import setup
import re
import os
import shutil


VERSION_REGEX = re.compile(
    r"^\s*#\s*define\s+NB_VERSION_([A-Z]+)\s+(.*)$", re.MULTILINE)

this_directory = os.path.abspath(os.path.dirname(__file__))

with open(os.path.join(this_directory, "include/nanobind/nanobind.h")) as f:
    matches = dict(VERSION_REGEX.findall(f.read()))
    nanobind_version = "{MAJOR}.{MINOR}.{PATCH}".format(**matches)

long_description = '''\
_nanobind_ is a small binding library that exposes C++ types in Python and
vice versa. It is reminiscent of
_[Boost.Python](https://www.boost.org/doc/libs/1_64_0/libs/python/doc/html)_
and _[pybind11](http://github.com/pybind/pybind11)_ and uses near-identical
syntax. In contrast to these existing tools, _nanobind_ is more _efficient_:
bindings compile in a shorter amount of time, produce smaller binaries, and
have better runtime performance.'''

from tempfile import TemporaryDirectory

with TemporaryDirectory() as temp_dir:
    base_dir = os.path.abspath(os.path.dirname(__file__))


    for name in ['include', 'ext', 'cmake', 'src']:
        shutil.copytree(os.path.join(base_dir, name),
                        os.path.join(temp_dir, name),
                        dirs_exist_ok=True)

    shutil.move(os.path.join(temp_dir, 'src', '__init__.py'),
                os.path.join(temp_dir, '__init__.py'))

    setup(
        name="nanobind",
        version=nanobind_version,
        author="Wenzel Jakob",
        author_email="wenzel.jakob@epfl.ch",
        description='Seamless operability between C++17 and Python',
        url="https://github.com/wjakob/nanobind",
        license="BSD",
        long_description=long_description,
        long_description_content_type='text/markdown',
        packages=['nanobind'],
        zip_safe=False,
        package_dir={'nanobind': temp_dir},
        package_data={'nanobind': [
            'include/nanobind/*.h',
            'include/nanobind/stl/*.h',
            'include/nanobind/stl/detail/*.h',
            'include/nanobind/eigen/*.h',
            'ext/robin_map/include/tsl/*.h',
            'cmake/nanobind-config.cmake',
            'cmake/darwin-ld-cpython.sym',
            'cmake/darwin-ld-pypy.sym',
            'src/*.h',
            'src/*.cpp'
        ]}
    )
