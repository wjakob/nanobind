# nanobind-backend

This package contains the backend part of the
[nanobind](https://github.com/wjakob/nanobind) C++ binding library. It serves
extension modules built using nanobind's *split mode*, in which the extension
delegates to an externally hosted backend instead of shipping its own.

End users receive this package as a transitive dependency of such an extension
and do not interact with it directly. There is no user-facing API. Please see
[split mode
documentation](https://nanobind.readthedocs.io/en/latest/split_mode.html) for
more details. This package only provides binary wheels; its source code is
part of the nanobind parent project.
