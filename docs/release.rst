How to make a new release?
--------------------------

1. Ensure that the full version of nanobind is checked out (including the
   ``robin_map`` submodule)

2. Run ``python src/version.py -w X.Y.Z``

3. Add release date to ``docs/changelog.rst``.

4. Update ``setup.py`` if new directories were added (see ``package_data``).
   Update ``cmake/nanobind-config.cmake`` if new C++ source or header files
   were added.

5. Commit: ``git commit -am "vX.Y.Z release"``

6. Tag: ``git tag -a vX.Y.Z -m "vX.Y.Z release"``

7. Push: ``git push`` and ``git push --tags``

8. Run ``pipx run build``

9. Upload: ``twine upload --repository nanobind <filename>``

10. Run ``python src/version.py -w X.Y.Zdev1``
