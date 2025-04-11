/*
    nanobind/nb_enums.h: enumerations used in nanobind (just rv_policy atm.)

    Copyright (c) 2022 Wenzel Jakob

    All rights reserved. Use of this source code is governed by a
    BSD-style license that can be found in the LICENSE file.
*/

NAMESPACE_BEGIN(NB_NAMESPACE)

// Approach used to cast a previously unknown C++ instance into a Python object
enum class rv_policy {
    automatic,
    automatic_reference,
    take_ownership,
    copy,
    move,
    reference,
    reference_internal,
    none,
    /* Note to self: nb_func.h assumes that this value fits into 3 bits,
       hence no further policies can be added. */

    /* Special internal-use policy that can only be passed to `nb_type_put()`
       or `type_caster_base<T>::from_cpp_raw()`. (To keep rv_policy fitting
       in 3 bits, it aliases `automatic_reference`, which would always have
       been converted into `copy` or `move` or `reference` before reaching
       those functions.) It means we are creating a Python instance that holds
       shared ownership of the C++ object we're casting, so we shouldn't reuse
       a Python instance that only holds a reference to that object. The actual
       enforcement of the shared ownership must be done separately after the
       cast completes, via a keep_alive callback or similar, to ensure that
       the C++ object lives at least as long as the new Python instance does.

       This is an internal tool that is not part of nanobind's public API,
       and may be changed without notice. */
    _shared_ownership = automatic_reference
};

NAMESPACE_END(NB_NAMESPACE)
