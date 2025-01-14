/*
   src/nb_abi.h: this file computes tags that are used to isolate extensions
   from each other in the case of platform or nanobind-related ABI
   incompatibilities. The file is included by ``nb_internals.cpp`` and should
   not be used directly.

   The implementation of this file (specifically, the NB_PLATFORM_ABI_TAG) is
   designed to be compatible with @rwgk's
   https://github.com/pybind/pybind11/blob/master/include/pybind11/conduit/pybind11_platform_abi_id.h

   Use of this source code is governed by a BSD-style license that can be found
   in the LICENSE file.
*/

/// Tracks the version of nanobind's internal data structures
#ifndef NB_INTERNALS_VERSION
#  define NB_INTERNALS_VERSION 16
#endif

#if defined(__MINGW32__)
#  define NB_COMPILER_TYPE "mingw"
#elif defined(__CYGWIN__)
#  define NB_COMPILER_TYPE "gcc_cygwin"
#elif defined(_MSC_VER)
#  define NB_COMPILER_TYPE "msvc"
#elif defined(__clang__) || defined(__GNUC__)
#  define NB_COMPILER_TYPE "system" // Assumed compatible with system compiler.
#else
#  error "Unknown compiler type. Please revise this code."
#endif

// Catch other conditions that imply ABI incompatibility
// - MSVC builds with different CRT versions
// - An anticipated MSVC ABI break ("vNext")
// - Builds using libc++ with unstable ABIs
// - Builds using libstdc++ with the legacy (pre-C++11) ABI, etc.
#if defined(_MSC_VER)
#  if defined(_MT) && defined(_DLL) // Corresponding to CL command line options /MD or /MDd.
#    if (_MSC_VER) / 100 == 19
#      define NB_BUILD_ABI "_md_mscver19"
#    else
#      error "Unknown MSVC major version. Please revise this code."
#    endif
#  elif defined(_MT) // Corresponding to CL command line options /MT or /MTd.
#    define NB_BUILD_ABI "_mt_mscver" NB_TOSTRING(_MSC_VER)
#  else
#    if (_MSC_VER) / 100 == 19
#      define NB_BUILD_ABI "_none_mscver19"
#    else
#      error "Unknown MSVC major version. Please revise this code."
#    endif
#  endif
#elif defined(_LIBCPP_ABI_VERSION) // https://libcxx.llvm.org/DesignDocs/ABIVersioning.html
#    define NB_BUILD_ABI "_libcpp_abi" NB_TOSTRING(_LIBCPP_ABI_VERSION)
#elif defined(_GLIBCXX_USE_CXX11_ABI)
#  if defined(__NVCOMPILER) && !defined(__GXX_ABI_VERSION)
#    error  "Unknown platform or compiler (_GLIBCXX_USE_CXX11_ABI). Please revise this code."
#  endif
#  if defined(__GXX_ABI_VERSION) && __GXX_ABI_VERSION < 1002 || __GXX_ABI_VERSION >= 2000
#    error "Unknown platform or compiler (__GXX_ABI_VERSION). Please revise this code."
#  endif
#  define NB_BUILD_ABI "_libstdcpp_gxx_abi_1xxx_use_cxx11_abi_" NB_TOSTRING(_GLIBCXX_USE_CXX11_ABI)
#else
#  error "Unknown platform or compiler. Please revise this code."
#endif

// On MSVC, debug and release builds are not ABI-compatible!
#if defined(_MSC_VER) && defined(_DEBUG)
#  define NB_BUILD_TYPE "_debug"
#else
#  define NB_BUILD_TYPE ""
#endif

// Tag to determine if inter-library C++ function can be safely dispatched
#define NB_PLATFORM_ABI_TAG \
    NB_COMPILER_TYPE NB_BUILD_ABI NB_BUILD_TYPE

// Can have limited and non-limited-API extensions in the same process.
// Nanobind data structures will differ, so these can't talk to each other
#if defined(Py_LIMITED_API)
#  define NB_STABLE_ABI "_stable"
#else
#  define NB_STABLE_ABI ""
#endif

// As above, but for free-threaded extensions
#if defined(NB_FREE_THREADED)
#  define NB_FREE_THREADED_ABI "_ft"
#else
#  define NB_FREE_THREADED_ABI ""
#endif

#if NB_VERSION_DEV > 0
  #define NB_VERSION_DEV_STR "_dev" NB_TOSTRING(NB_VERSION_DEV)
#else
  #define NB_VERSION_DEV_STR ""
#endif

#define NB_ABI_TAG                                                             \
    "v" NB_TOSTRING(NB_INTERNALS_VERSION)                                      \
        NB_VERSION_DEV_STR "_" NB_PLATFORM_ABI_TAG NB_STABLE_ABI               \
            NB_FREE_THREADED_ABI
