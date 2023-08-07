How to make a new release?
--------------------------

1. Update version in ``src/__init__.py`` and ``include/nanobind/nanobind.h``

2. Add release date to ``docs/changelog.rst``.

3. Update ``setup.py`` if new directories were added (see ``package_data``).
   Update ``cmake/nanobind-config.cmake`` if new C++ source or header files
   were added.

4. Commit: ``git commit -am "vX.Y.Z release"``

5. Tag: ``git tag -a vX.Y.Z -m "vX.Y.Z release"``

6. Push: ``git push`` and ``git push --tags``

7. Run ``python setup.py bdist_wheel``

8. Upload: ``twine upload --repository nanobind <filename>``
