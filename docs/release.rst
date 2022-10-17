How to make a new release?
--------------------------

1. Update version in ``src/nanobind/__init__.py`` and ``include/nanobind/nanobind.h``

2. Commit: ``git commit -am "vX.Y.Z release"``

3. Tag: ``git tag -a vX.Y.Z -m "vX.Y.Z release"``

3. Push: ``git push`` and ``git push --tags``

2. Run ``python setup.py bdist_wheel``

3. Upload: ``twine upload <filename>``
