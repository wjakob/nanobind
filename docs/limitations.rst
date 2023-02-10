Limitations
===========

nanobind strives to be a general solution to binding generation, but it also
has certain limitations:

- nanobind casts away ``const``-ness in function arguments and return values.
  This is in line with the Python language, which has no concept of const
  values. This means that some additional care is needed to avoid bugs that
  would be caught by the type checker in a traditional C++ program.


- :ref:`Type casters <type_casters>` do not support mutable references. They
  also do not support pointers on the C++ side (e.g. ``std::vector<T>*``),
  which is a new limitation compared to pybind11.
