/*
    tests/test_abi_layout.cpp: compile-time freeze checks for nanobind's
    frozen ABI surface (see nb_backend.h). The records checked here are baked
    into compiled extensions, so a failing assertion means the ABI changed
    and NB_BACKEND_ABI_MAJOR or NB_BACKEND_ABI_MINOR must move (see the rules at the top of
    nb_backend.h). Only 64-bit layouts are pinned; 32-bit layouts follow from
    the field order.

    This file contains no test code. Compiling it as part of the test suite
    is the check.
*/

#include <nanobind/nanobind.h>
#include <nanobind/ndarray.h>
#include <nanobind/trampoline.h>

using namespace nanobind;
using namespace nanobind::detail;

#define NB_FROZEN_OFF(S, F, V)                                                 \
    static_assert(sizeof(void *) != 8 || offsetof(S, F) == (V),                \
                  "frozen ABI layout of " #S "::" #F " changed")

static_assert(NB_BACKEND_ABI_MINOR < 256,
              "the ABI tag reserves 8 bits for the minor version");

static_assert(cleanup_list::Small == 5 &&
              (sizeof(void *) != 8 || sizeof(cleanup_list) == 64),
              "frozen ABI layout of cleanup_list changed");

static_assert(sizeof(void *) != 8 || sizeof(arg_data_init) == 32,
              "frozen ABI layout of arg_data_init changed");
NB_FROZEN_OFF(arg_data_init, name, 0);
NB_FROZEN_OFF(arg_data_init, signature, 8);
NB_FROZEN_OFF(arg_data_init, value, 16);
NB_FROZEN_OFF(arg_data_init, flag, 24);
NB_FROZEN_OFF(arg_data_init, unused, 28);

static_assert(sizeof(void *) != 8 || sizeof(call_arg) == 24,
              "frozen ABI layout of call_arg changed");
NB_FROZEN_OFF(call_arg, value, 0);
NB_FROZEN_OFF(call_arg, name, 8);
NB_FROZEN_OFF(call_arg, kind, 16);
NB_FROZEN_OFF(call_arg, unused, 20);

static_assert(sizeof(void *) != 8 || sizeof(func_data_init_base) == 88,
              "frozen ABI layout of func_data_init_base changed");
NB_FROZEN_OFF(func_data_init_base, capture, 0);
NB_FROZEN_OFF(func_data_init_base, free_capture, 24);
NB_FROZEN_OFF(func_data_init_base, impl, 32);
NB_FROZEN_OFF(func_data_init_base, descr, 40);
NB_FROZEN_OFF(func_data_init_base, descr_types, 48);
NB_FROZEN_OFF(func_data_init_base, flags, 56);
NB_FROZEN_OFF(func_data_init_base, nargs, 60);
NB_FROZEN_OFF(func_data_init_base, nargs_pos, 62);
NB_FROZEN_OFF(func_data_init_base, name, 64);
NB_FROZEN_OFF(func_data_init_base, doc, 72);
NB_FROZEN_OFF(func_data_init_base, scope, 80);

static_assert(sizeof(void *) != 8 || sizeof(type_data_init) == 112,
              "frozen ABI layout of type_data_init changed");
NB_FROZEN_OFF(type_data_init, size_align, 0);
NB_FROZEN_OFF(type_data_init, flags, 4);
NB_FROZEN_OFF(type_data_init, name, 8);
NB_FROZEN_OFF(type_data_init, type, 16);
NB_FROZEN_OFF(type_data_init, destruct, 24);
NB_FROZEN_OFF(type_data_init, copy, 32);
NB_FROZEN_OFF(type_data_init, move, 40);
NB_FROZEN_OFF(type_data_init, set_self_py, 48);
NB_FROZEN_OFF(type_data_init, keep_shared_from_this_alive, 56);
NB_FROZEN_OFF(type_data_init, scope, 64);
NB_FROZEN_OFF(type_data_init, base, 72);
NB_FROZEN_OFF(type_data_init, base_py, 80);
NB_FROZEN_OFF(type_data_init, doc, 88);
NB_FROZEN_OFF(type_data_init, type_slots, 96);
NB_FROZEN_OFF(type_data_init, pool_capacity, 104);
NB_FROZEN_OFF(type_data_init, supplement_size, 108);

static_assert(sizeof(void *) != 8 || sizeof(enum_data_init) == 40,
              "frozen ABI layout of enum_data_init changed");
NB_FROZEN_OFF(enum_data_init, type, 0);
NB_FROZEN_OFF(enum_data_init, scope, 8);
NB_FROZEN_OFF(enum_data_init, name, 16);
NB_FROZEN_OFF(enum_data_init, docstr, 24);
NB_FROZEN_OFF(enum_data_init, flags, 32);
NB_FROZEN_OFF(enum_data_init, unused, 36);

static_assert(sizeof(void *) != 8 || sizeof(error_payload) == 24,
              "frozen ABI layout of error_payload changed");
NB_FROZEN_OFF(error_payload, value, 0);
NB_FROZEN_OFF(error_payload, internal, 8);

static_assert(sizeof(python_error) ==
                  sizeof(std::exception) + sizeof(error_payload),
              "frozen layout of python_error changed");

static_assert(sizeof(void *) != 8 || sizeof(ndarray_config) == 32,
              "frozen ABI layout of ndarray_config changed");
NB_FROZEN_OFF(ndarray_config, flags, 0);
NB_FROZEN_OFF(ndarray_config, device_type, 4);
NB_FROZEN_OFF(ndarray_config, ndim, 8);
NB_FROZEN_OFF(ndarray_config, dtype, 12);
NB_FROZEN_OFF(ndarray_config, order, 16);
NB_FROZEN_OFF(ndarray_config, ro, 17);
NB_FROZEN_OFF(ndarray_config, unused_0, 18);
NB_FROZEN_OFF(ndarray_config, unused_1, 20);
NB_FROZEN_OFF(ndarray_config, shape, 24);

static_assert(sizeof(void *) != 8 || sizeof(ndarray_create_args) == 64,
              "frozen ABI layout of ndarray_create_args changed");
NB_FROZEN_OFF(ndarray_create_args, data, 0);
NB_FROZEN_OFF(ndarray_create_args, shape, 8);
NB_FROZEN_OFF(ndarray_create_args, strides, 16);
NB_FROZEN_OFF(ndarray_create_args, owner, 24);
NB_FROZEN_OFF(ndarray_create_args, byte_offset, 32);
NB_FROZEN_OFF(ndarray_create_args, ndim, 40);
NB_FROZEN_OFF(ndarray_create_args, flags, 44);
NB_FROZEN_OFF(ndarray_create_args, device_type, 48);
NB_FROZEN_OFF(ndarray_create_args, device_id, 52);
NB_FROZEN_OFF(ndarray_create_args, dtype, 56);
NB_FROZEN_OFF(ndarray_create_args, order, 60);
NB_FROZEN_OFF(ndarray_create_args, ro, 61);
NB_FROZEN_OFF(ndarray_create_args, unused, 62);

static_assert(sizeof(dlpack::dtype) == 4,
              "frozen ABI layout of dlpack::dtype changed");
NB_FROZEN_OFF(dlpack::dtype, code, 0);
NB_FROZEN_OFF(dlpack::dtype, bits, 1);
NB_FROZEN_OFF(dlpack::dtype, lanes, 2);

static_assert(sizeof(void *) != 8 || sizeof(dlpack::dltensor) == 48,
              "frozen ABI layout of dlpack::dltensor changed");
NB_FROZEN_OFF(dlpack::dltensor, data, 0);
NB_FROZEN_OFF(dlpack::dltensor, device, 8);
NB_FROZEN_OFF(dlpack::dltensor, ndim, 16);
NB_FROZEN_OFF(dlpack::dltensor, dtype, 20);
NB_FROZEN_OFF(dlpack::dltensor, shape, 24);
NB_FROZEN_OFF(dlpack::dltensor, strides, 32);
NB_FROZEN_OFF(dlpack::dltensor, byte_offset, 40);

static_assert(sizeof(void *) != 8 || sizeof(trampoline) == 8,
              "frozen ABI layout of trampoline changed");
NB_FROZEN_OFF(trampoline, self, 0);

static_assert(sizeof(void *) != 8 || sizeof(ticket) == 32,
              "frozen ABI layout of ticket changed");
NB_FROZEN_OFF(ticket, self, 0);
NB_FROZEN_OFF(ticket, key, 8);
NB_FROZEN_OFF(ticket, prev, 16);
NB_FROZEN_OFF(ticket, state, 24);

static_assert(sizeof(void *) != 8 || sizeof(import_cache) == 24,
              "frozen ABI layout of import_cache changed");
NB_FROZEN_OFF(import_cache, module, 0);
NB_FROZEN_OFF(import_cache, attr, 8);
