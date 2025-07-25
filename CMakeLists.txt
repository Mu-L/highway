# Copyright 2019 Google LLC
# Copyright 2024 Arm Limited and/or its affiliates <open-source-office@arm.com>
# SPDX-License-Identifier: Apache-2.0
# SPDX-License-Identifier: BSD-3-Clause
#
# Licensed under the Apache License, Version 2.0 (the "License");
# you may not use this file except in compliance with the License.
# You may obtain a copy of the License at
#
#      http://www.apache.org/licenses/LICENSE-2.0
#
# Unless required by applicable law or agreed to in writing, software
# distributed under the License is distributed on an "AS IS" BASIS,
# WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
# See the License for the specific language governing permissions and
# limitations under the License.

cmake_minimum_required(VERSION 3.10)

# Set PIE flags for POSITION_INDEPENDENT_CODE targets, added in 3.14.
if(POLICY CMP0083)
  cmake_policy(SET CMP0083 NEW)
endif()

# Workaround for 3.19 raising error 'IMPORTED_LOCATION not set for imported
# target "GTest::gtest_main"'.
if(POLICY CMP0111)
  cmake_policy(SET CMP0111 OLD)
endif()

# Starting with GCC-13, we want to make sure to remove gnu extension (ie.
# explicit -std=c++17 instead of implicit `gnu++17`)
# Without this cmake property, CMAKE_CXX_EXTENSIONS=OFF was not properly
# considered
if(POLICY CMP0128)
  cmake_policy(SET CMP0128 NEW)
endif()

project(hwy VERSION 1.3.0)  # Keep in sync with base.h version
# `hwy` is lowercase to handle find_package() in Config mode:
set(namespace "${PROJECT_NAME}::")

# Directly define the ABI version from the cmake project() version values:
set(LIBRARY_VERSION "${hwy_VERSION}")
set(LIBRARY_SOVERSION ${hwy_VERSION_MAJOR})

set(CMAKE_CXX_EXTENSIONS OFF)

list(APPEND CMAKE_MODULE_PATH "${CMAKE_CURRENT_SOURCE_DIR}/cmake")
# Search for Atomics implementation:
find_package(Atomics REQUIRED)

# Enabled PIE binaries by default if supported.
include(CheckPIESupported OPTIONAL RESULT_VARIABLE CHECK_PIE_SUPPORTED)
if(CHECK_PIE_SUPPORTED)
  check_pie_supported(LANGUAGES CXX)
  if(CMAKE_CXX_LINK_PIE_SUPPORTED)
    set(CMAKE_POSITION_INDEPENDENT_CODE TRUE)
  endif()
endif()

include(GNUInstallDirs)

if (NOT CMAKE_BUILD_TYPE)
  set(CMAKE_BUILD_TYPE RelWithDebInfo)
endif()

# The following is only required with GCC < 6.1.0 or CLANG < 16.0
set(HWY_CMAKE_ARM7 OFF CACHE BOOL "Set copts for Armv7 with NEON (requires vfpv4)?")

# Upstream compilers(GCC 14 or CLANG 18) start enabling LSX by default, but
# enabling LASX still require -mlasx flag to be passed explicitly, in order
# to enable all targets, we can check them directly, adding them if they are
# supported.  In this way, Our local compilers(GCC 8.3.0 or CLANG 8.0.1) also
# could enable LSX & LASX targets.  Any better ideas or suggestions are welcome.
set(HWY_CMAKE_LSX ON CACHE BOOL "Add -mlsx flag?")
set(HWY_CMAKE_LASX ON CACHE BOOL "Add -mlasx flag?")

# This must be set on 32-bit x86 with GCC < 13.1, otherwise math_test will be
# skipped. For GCC 13.1+, you can also build with -fexcess-precision=standard.
set(HWY_CMAKE_SSE2 OFF CACHE BOOL "Set SSE2 as baseline for 32-bit x86?")

# Currently this will compile the entire codebase with `-march=rv<XLEN>gcv1p0`:
set(HWY_CMAKE_RVV ON CACHE BOOL "Set copts for RISCV with RVV?")

# Unconditionally adding -Werror risks breaking the build when new warnings
# arise due to compiler/platform changes. Enable this in CI/tests.
set(HWY_WARNINGS_ARE_ERRORS OFF CACHE BOOL "Add -Werror flag?")

# Experimental support for header-only builds
set(HWY_CMAKE_HEADER_ONLY OFF CACHE BOOL "Change to header-only?")

set(HWY_ENABLE_CONTRIB ON CACHE BOOL "Include contrib/")
set(HWY_ENABLE_EXAMPLES ON CACHE BOOL "Build examples")
set(HWY_ENABLE_INSTALL ON CACHE BOOL "Install library")
set(HWY_ENABLE_TESTS ON CACHE BOOL "Enable HWY tests")

if (MSVC)
set(HWY_TEST_STANDALONE ON CACHE BOOL "Disable use of googletest")
else()
set(HWY_TEST_STANDALONE OFF CACHE BOOL "Disable use of googletest")
endif()

if (NOT DEFINED CMAKE_CXX_STANDARD)
  if ("cxx_std_17" IN_LIST CMAKE_CXX_COMPILE_FEATURES)
    set(HWY_CXX_STD_TGT_COMPILE_FEATURE cxx_std_17)
  else()
    if ("cxx_std_14" IN_LIST CMAKE_CXX_COMPILE_FEATURES)
      set(HWY_CXX_STD_TGT_COMPILE_FEATURE cxx_std_14)
    else()
      set(HWY_CXX_STD_TGT_COMPILE_FEATURE cxx_std_11)
    endif()
  endif()
else()
  if (CMAKE_CXX_STANDARD GREATER_EQUAL 14 AND CMAKE_CXX_STANDARD LESS 98)
    if (CMAKE_CXX_STANDARD GREATER_EQUAL 17)
      set(HWY_CXX_STD_TGT_COMPILE_FEATURE cxx_std_17)
    else()
      set(HWY_CXX_STD_TGT_COMPILE_FEATURE cxx_std_14)
    endif()
  else()
    set(HWY_CXX_STD_TGT_COMPILE_FEATURE cxx_std_11)
  endif()
endif()

include(CheckCXXSourceCompiles)
check_cxx_source_compiles(
   "int main() {
      #if !defined(__EMSCRIPTEN__)
      static_assert(false, \"__EMSCRIPTEN__ is not defined\");
      #endif
      return 0;
    }"
  HWY_EMSCRIPTEN
)

check_cxx_source_compiles(
   "int main() {
      #if !defined(__riscv)
      static_assert(false, \"__riscv is not defined\");
      #endif
      return 0;
    }"
  HWY_RISCV
)

if (CMAKE_SYSTEM_PROCESSOR MATCHES "loongarch32|loongarch64")
  include(CheckCXXCompilerFlag)
  if (HWY_CMAKE_LSX)
    set (CMAKE_REQUIRED_FLAGS -mlsx)
    check_cxx_source_compiles(
       "int main() {
          #if !defined(__loongarch_sx)
          static_assert(false, \"__loongarch_sx is not defined\");
          #endif
          return 0;
        }"
      COMPILER_SUPPORT_LSX_FLAG
    )
  endif()
  if (HWY_CMAKE_LASX)
    set (CMAKE_REQUIRED_FLAGS -mlasx)
    check_cxx_source_compiles(
       "int main() {
          #if !defined(__loongarch_asx)
          static_assert(false, \"__loongarch_asx is not defined\");
          #endif
          return 0;
       }"
      COMPILER_SUPPORT_LASX_FLAG
    )
  endif()
endif()

if (WIN32)
  set (ORIG_CMAKE_REQUIRED_LIBRARIES ${CMAKE_REQUIRED_LIBRARIES})
  set (CMAKE_REQUIRED_LIBRARIES synchronization)
  check_cxx_source_compiles(
    "#ifndef NOMINMAX
     #define NOMINMAX
     #endif

     #include <windows.h>

     int main() {
       unsigned val1 = 0u;
       unsigned val2 = 1u;
       WaitOnAddress(&val1, &val2, sizeof(unsigned), 1);
       WakeByAddressAll(&val1);
       WakeByAddressSingle(&val1);
       return 0;
     }"
  HWY_HAVE_WIN32_SYNCHRONIZATION_LIB)
  set (CMAKE_REQUIRED_LIBRARIES ${ORIG_CMAKE_REQUIRED_LIBRARIES})
else()
  set (HWY_HAVE_WIN32_SYNCHRONIZATION_LIB OFF)
endif ()

if (HWY_HAVE_WIN32_SYNCHRONIZATION_LIB OR NOT WIN32)
  set (HWY_DISABLE_FUTEX OFF CACHE BOOL "Disable futex for thread_pool")
else()
  # Force HWY_DISABLE_FUTEX to ON if compiling for Win32 and
  # libsynchronization.a or synchronization.lib is not available
  set (HWY_DISABLE_FUTEX ON CACHE BOOL "Disable futex for thread_pool" FORCE)
endif()

find_package(Threads)

if (NOT Threads_FOUND AND HWY_ENABLE_CONTRIB)
  message(FATAL_ERROR "Threads must be available if HWY_ENABLE_CONTRIB is ON")
endif()

if(Threads_FOUND)
  if(THREADS_HAVE_PTHREAD_ARG)
    set(HWY_THREAD_FLAGS "-pthread")
  else()
    set(HWY_THREAD_FLAGS "")
  endif()

  if(CMAKE_THREAD_LIBS_INIT)
    set(HWY_THREAD_LIBS ${CMAKE_THREAD_LIBS_INIT})
  else()
    set(HWY_THREAD_LIBS "")
  endif()
else()
  set(HWY_THREAD_FLAGS "")
  set(HWY_THREAD_LIBS "")
endif()

if (HWY_RISCV OR CMAKE_CXX_COMPILER_ARCHITECTURE_ID MATCHES "RISCV32|RISCV64|RISCV128" OR CMAKE_SYSTEM_PROCESSOR MATCHES "riscv32|riscv64|riscv128")
  include(CheckCSourceCompiles)
  check_c_source_compiles("
  #if __riscv_xlen == 64
  int main() { return 0; }
  #else
  #error Not RISCV-64
  #endif
  " IS_RISCV_XLEN_64)

  check_c_source_compiles("
  #if __riscv_xlen == 32
  int main() { return 0; }
  #else
  #error Not RISCV-32
  #endif
  " IS_RISCV_XLEN_32)

  if(IS_RISCV_XLEN_32)
    set(RISCV_XLEN 32)
  elseif(IS_RISCV_XLEN_64)
    set(RISCV_XLEN 64)
  else()
    message(WARNING "Unable to determine RISC-V XLEN")
  endif()
endif()

if (HWY_ENABLE_CONTRIB)
# Glob all the traits so we don't need to modify this file when adding
# additional special cases.
file(GLOB HWY_CONTRIB_SOURCES "hwy/contrib/sort/vqsort_*.cc")
list(APPEND HWY_CONTRIB_SOURCES
    hwy/contrib/bit_pack/bit_pack-inl.h
    hwy/contrib/dot/dot-inl.h
    hwy/contrib/image/image.cc
    hwy/contrib/image/image.h
    hwy/contrib/math/math-inl.h
    hwy/contrib/matvec/matvec-inl.h
    hwy/contrib/random/random-inl.h
    hwy/contrib/sort/order.h
    hwy/contrib/sort/shared-inl.h
    hwy/contrib/sort/sorting_networks-inl.h
    hwy/contrib/sort/traits-inl.h
    hwy/contrib/sort/traits128-inl.h
    hwy/contrib/sort/vqsort-inl.h
    hwy/contrib/sort/vqsort.cc
    hwy/contrib/sort/vqsort.h
    hwy/contrib/thread_pool/futex.h
    hwy/contrib/thread_pool/spin.h
    hwy/contrib/thread_pool/thread_pool.h
    hwy/contrib/thread_pool/topology.cc
    hwy/contrib/thread_pool/topology.h
    hwy/contrib/algo/copy-inl.h
    hwy/contrib/algo/find-inl.h
    hwy/contrib/algo/transform-inl.h
    hwy/contrib/unroller/unroller-inl.h
)
endif()  # HWY_ENABLE_CONTRIB

set(HWY_SOURCES
    hwy/abort.h
    hwy/aligned_allocator.h
    hwy/auto_tune.h
    hwy/base.h
    hwy/cache_control.h
    hwy/detect_compiler_arch.h  # private
    hwy/detect_targets.h  # private
    hwy/foreach_target.h
    hwy/highway_export.h
    hwy/highway.h
    hwy/nanobenchmark.h
    hwy/ops/arm_neon-inl.h
    hwy/ops/arm_sve-inl.h
    hwy/ops/emu128-inl.h
    hwy/ops/generic_ops-inl.h
    hwy/ops/inside-inl.h
    hwy/ops/loongarch_lsx-inl.h
    hwy/ops/loongarch_lasx-inl.h
    hwy/ops/ppc_vsx-inl.h
    hwy/ops/rvv-inl.h
    hwy/ops/scalar-inl.h
    hwy/ops/set_macros-inl.h
    hwy/ops/shared-inl.h
    hwy/ops/wasm_128-inl.h
    hwy/ops/x86_128-inl.h
    hwy/ops/x86_256-inl.h
    hwy/ops/x86_512-inl.h
    hwy/ops/x86_avx3-inl.h
    hwy/per_target.h
    hwy/print-inl.h
    hwy/print.h
    hwy/profiler.h
    hwy/robust_statistics.h
    hwy/targets.h
    hwy/timer-inl.h
    hwy/timer.h
    hwy/x86_cpuid.h
)

if (NOT HWY_CMAKE_HEADER_ONLY)
  list(APPEND HWY_SOURCES
    hwy/abort.cc
    hwy/aligned_allocator.cc
    hwy/nanobenchmark.cc
    hwy/per_target.cc
    hwy/perf_counters.cc
    hwy/print.cc
    hwy/profiler.cc
    hwy/targets.cc
    hwy/timer.cc
  )
endif()

set(HWY_TEST_SOURCES
    hwy/tests/hwy_gtest.h
    hwy/tests/test_util-inl.h
    hwy/tests/test_util.cc
    hwy/tests/test_util.h
)

if (MSVC)
  set(HWY_FLAGS
    # fix build error C1128 in blockwise*_test & arithmetic_test
    /bigobj

    # Warnings
    /W4
    # Disable some W4 warnings.  Enable them individually after they are cleaned up.
    /wd4100
    /wd4127
    /wd4324
    /wd4456
    /wd4701
    /wd4702
    /wd4723

    # CMake automatically adds exception handling flags.  Remove them.
    /GR-
    /EHs-c-
    # Disable exceptions in STL code.
    -D_HAS_EXCEPTIONS=0
  )

  # This adds extra warnings for the clang-cl compiler on Windows.
  # This is the same as the sections in the else part.
  # These could be refactored.
  if (${CMAKE_CXX_COMPILER_ID} MATCHES "Clang")
    list(APPEND HWY_FLAGS
      # These are not included in Wall nor Wextra:
      -Wconversion
      -Wsign-conversion
      -Wvla
      -Wnon-virtual-dtor

      -Wfloat-overflow-conversion
      -Wfloat-zero-conversion
      -Wfor-loop-analysis
      -Wgnu-redeclared-enum
      -Winfinite-recursion
      -Wself-assign
      -Wstring-conversion
      -Wtautological-overlap-compare
      -Wthread-safety-analysis
      -Wundefined-func-template
    )
  endif()

  if (HWY_WARNINGS_ARE_ERRORS)
    list(APPEND HWY_FLAGS /WX)
  endif()
else()
  set(HWY_FLAGS
    # Avoid changing binaries based on the current time and date.
    -Wno-builtin-macro-redefined
    -D__DATE__="redacted"
    -D__TIMESTAMP__="redacted"
    -D__TIME__="redacted"

    # Optimizations
    -fmerge-all-constants

    # Warnings
    -Wall
    -Wextra
    # These are not included in Wall nor Wextra:
    -Wconversion
    -Wsign-conversion
    -Wvla
    -Wnon-virtual-dtor
    -Wcast-align # see -Wcast-align=strict on x86
  )

  # In case LSX/LASX are available but not enabled by defautl.
  if(COMPILER_SUPPORT_LSX_FLAG)
    list(APPEND HWY_FLAGS -mlsx)
  endif()
  if(COMPILER_SUPPORT_LASX_FLAG)
    list(APPEND HWY_FLAGS -mlasx)
  endif()

  if(${CMAKE_CXX_COMPILER_ID} MATCHES "Clang")
    list(APPEND HWY_FLAGS
      -Wfloat-overflow-conversion
      -Wfloat-zero-conversion
      -Wfor-loop-analysis
      -Wgnu-redeclared-enum
      -Winfinite-recursion
      -Wself-assign
      -Wstring-conversion
      -Wtautological-overlap-compare
      -Wthread-safety-analysis
      -Wundefined-func-template

      -fno-cxx-exceptions
      -fno-slp-vectorize
      -fno-vectorize

      # Use color in messages
      -fdiagnostics-show-option -fcolor-diagnostics
    )
    if (CMAKE_CXX_COMPILER_VERSION VERSION_GREATER_EQUAL 6.0)
      list(APPEND HWY_FLAGS -Wc++2a-extensions)
    endif()
  endif()

  if (WIN32)
    if(${CMAKE_CXX_COMPILER_ID} MATCHES "Clang")
      list(APPEND HWY_FLAGS
        -Wno-global-constructors
        -Wno-language-extension-token
        -Wno-used-but-marked-unused
        -Wno-shadow-field-in-constructor
        -Wno-unused-member-function
        -Wno-unused-template
        -Wno-c++98-compat-pedantic
        -Wno-used-but-marked-unused
        -Wno-zero-as-null-pointer-constant
      )
    endif()

    list(APPEND HWY_FLAGS
      -Wno-cast-align
      -Wno-double-promotion
      -Wno-float-equal
      -Wno-format-nonliteral
      -Wno-shadow
      -Wno-sign-conversion
    )
  else()
    list(APPEND HWY_FLAGS
      -fmath-errno
      -fno-exceptions
    )
  endif()  # WIN32

  # Workaround for excess precision, see #1488.
  if (HWY_CMAKE_SSE2)
    list(APPEND HWY_FLAGS -msse2 -mfpmath=sse)
  endif()

  # Suppress STL iterator warnings. Supported by GCC 4.4.7 and newer, which
  # predates the C++11 we require.
  if (${CMAKE_CXX_COMPILER_ID} MATCHES "GNU")
    list(APPEND HWY_FLAGS -Wno-psabi)
  endif()
  # Clang supports this flag from 11.0.
  if (${CMAKE_CXX_COMPILER_ID} MATCHES "Clang")
    if (CMAKE_CXX_COMPILER_VERSION VERSION_GREATER_EQUAL 11.0)
      list(APPEND HWY_FLAGS -Wno-psabi)
    endif()
  endif()

  if (HWY_CMAKE_ARM7)
    list(APPEND HWY_FLAGS
      -march=armv7-a
      -mfpu=neon-vfpv4
      -mfloat-abi=hard  # must match the toolchain specified as CXX=
      -DHWY_HAVE_SCALAR_F16_TYPE=0  # See #2625
      -DHWY_NEON_HAVE_F16C=0
    )
    if(${CMAKE_CXX_COMPILER_ID} MATCHES "GNU")
      # using GCC
      list(APPEND HWY_FLAGS
        -mfp16-format=ieee  # required for vcvt_f32_f16
      )
    endif()
  endif()  # HWY_CMAKE_ARM7

  if(HWY_RISCV)
    # gcc(13) and recent clang both support V, but not yet runtime dispatch, so
    # we add the gcv compiler flag, which then requires the CPU (now when using
    # either compiler) to support V.
    if(HWY_CMAKE_RVV)
      if(RISCV_XLEN EQUAL 64)
        list(APPEND HWY_FLAGS -march=rv64gcv1p0)
        add_link_options(-march=rv64gcv1p0)
      elseif(RISCV_XLEN EQUAL 32)
        list(APPEND HWY_FLAGS -march=rv32gcv1p0)
        add_link_options(-march=rv32gcv1p0)
      endif()
      if(${CMAKE_CXX_COMPILER_ID} MATCHES "Clang")
        list(APPEND HWY_FLAGS -menable-experimental-extensions)
      endif()
    endif()
  endif()

  if (HWY_WARNINGS_ARE_ERRORS)
    list(APPEND HWY_FLAGS -Werror)
  endif()

  # Prevent "wasm-ld: error: --shared-memory is disallowed by targets.cc.o
  # because it was not compiled with 'atomics' or 'bulk-memory' features."
  if (HWY_EMSCRIPTEN)
    list(APPEND HWY_FLAGS -matomics)
  endif()

endif()  # !MSVC

if (HWY_DISABLE_FUTEX)
  list(APPEND HWY_FLAGS -DHWY_DISABLE_FUTEX)
endif()

if (HWY_CMAKE_HEADER_ONLY)
  list(APPEND HWY_FLAGS -DHWY_HEADER_ONLY)
endif()

include(CheckIncludeFile)
check_include_file(sys/auxv.h  HAVE_SYS_AUXV_H)
check_include_file(asm/hwcap.h HAVE_ASM_HWCAP_H)

# By default prefer STATIC build (legacy behavior)
option(BUILD_SHARED_LIBS "Build shared libraries" OFF)
option(HWY_FORCE_STATIC_LIBS "Ignore BUILD_SHARED_LIBS" OFF)
# only expose shared/static options to advanced users:
mark_as_advanced(BUILD_SHARED_LIBS)
mark_as_advanced(HWY_FORCE_STATIC_LIBS)
# Define visibility settings globally:
set(CMAKE_CXX_VISIBILITY_PRESET hidden)
set(CMAKE_VISIBILITY_INLINES_HIDDEN 1)

# Copy-cat "add_library" logic + add override.
set(HWY_LIBRARY_TYPE "SHARED")
if (NOT BUILD_SHARED_LIBS OR HWY_FORCE_STATIC_LIBS)
  set(HWY_LIBRARY_TYPE "STATIC")
endif()

# This preprocessor define will drive the build, also used in the *.pc files:
if("${HWY_LIBRARY_TYPE}" STREQUAL "SHARED")
  set(DLLEXPORT_TO_DEFINE "HWY_SHARED_DEFINE")
else()
  set(DLLEXPORT_TO_DEFINE "HWY_STATIC_DEFINE")
endif()

add_library(hwy ${HWY_LIBRARY_TYPE} ${HWY_SOURCES})
if(NOT HAVE_SYS_AUXV_H)
  target_compile_definitions(hwy PUBLIC TOOLCHAIN_MISS_SYS_AUXV_H)
endif()
if(NOT HAVE_ASM_HWCAP_H)
  target_compile_definitions(hwy PUBLIC TOOLCHAIN_MISS_ASM_HWCAP_H)
endif()
target_compile_definitions(hwy PUBLIC "${DLLEXPORT_TO_DEFINE}")
target_compile_options(hwy PRIVATE ${HWY_FLAGS})
set_property(TARGET hwy PROPERTY POSITION_INDEPENDENT_CODE ON)
set_target_properties(hwy PROPERTIES VERSION ${LIBRARY_VERSION} SOVERSION ${LIBRARY_SOVERSION})
target_include_directories(hwy PUBLIC
    $<BUILD_INTERFACE:${CMAKE_CURRENT_LIST_DIR}>
    $<INSTALL_INTERFACE:${CMAKE_INSTALL_INCLUDEDIR}>)
target_compile_features(hwy PUBLIC cxx_std_11)
if (NOT HWY_CXX_STD_TGT_COMPILE_FEATURE STREQUAL "cxx_std_11")
  target_compile_features(hwy PRIVATE ${HWY_CXX_STD_TGT_COMPILE_FEATURE})
endif()
set_target_properties(hwy PROPERTIES
  LINK_DEPENDS ${CMAKE_CURRENT_SOURCE_DIR}/hwy/hwy.version)
# For GCC __atomic_store_8, see #887
target_link_libraries(hwy PRIVATE ${ATOMICS_LIBRARIES})
# not supported by MSVC/Clang, safe to skip (we use DLLEXPORT annotations)
if(UNIX AND NOT APPLE)
  set_property(TARGET hwy APPEND_STRING PROPERTY
    LINK_FLAGS " -Wl,--version-script=${CMAKE_CURRENT_SOURCE_DIR}/hwy/hwy.version")
endif()

if (HWY_ENABLE_CONTRIB)
add_library(hwy_contrib ${HWY_LIBRARY_TYPE} ${HWY_CONTRIB_SOURCES})
target_link_libraries(hwy_contrib PUBLIC hwy)
target_compile_options(hwy_contrib PRIVATE ${HWY_FLAGS} ${HWY_THREAD_FLAGS})
set_property(TARGET hwy_contrib PROPERTY POSITION_INDEPENDENT_CODE ON)
set_target_properties(hwy_contrib PROPERTIES VERSION ${LIBRARY_VERSION} SOVERSION ${LIBRARY_SOVERSION})
target_include_directories(hwy_contrib PUBLIC
    $<BUILD_INTERFACE:${CMAKE_CURRENT_LIST_DIR}>
    $<INSTALL_INTERFACE:${CMAKE_INSTALL_INCLUDEDIR}>)
target_compile_features(hwy_contrib PUBLIC cxx_std_11)
if (NOT HWY_CXX_STD_TGT_COMPILE_FEATURE STREQUAL "cxx_std_11")
  target_compile_features(hwy_contrib PRIVATE ${HWY_CXX_STD_TGT_COMPILE_FEATURE})
endif()
set_target_properties(hwy_contrib PROPERTIES
  LINK_DEPENDS ${CMAKE_CURRENT_SOURCE_DIR}/hwy/hwy.version)
# For GCC __atomic_store_8, see #887
target_link_libraries(hwy_contrib PRIVATE ${ATOMICS_LIBRARIES})

# Avoid linker errors if libpthread needs to be linked
target_link_libraries(hwy_contrib PRIVATE ${HWY_THREAD_LIBS})

if (WIN32 AND NOT MSVC AND NOT HWY_DISABLE_FUTEX)
target_link_libraries(hwy_contrib PUBLIC synchronization)
endif()

# not supported by MSVC/Clang, safe to skip (we use DLLEXPORT annotations)
if(UNIX AND NOT APPLE)
  set_property(TARGET hwy_contrib APPEND_STRING PROPERTY
    LINK_FLAGS " -Wl,--version-script=${CMAKE_CURRENT_SOURCE_DIR}/hwy/hwy.version")
endif()
endif()  # HWY_ENABLE_CONTRIB

add_library(hwy_test ${HWY_LIBRARY_TYPE} ${HWY_TEST_SOURCES})
target_link_libraries(hwy_test PUBLIC hwy)
target_compile_options(hwy_test PRIVATE ${HWY_FLAGS})
set_property(TARGET hwy_test PROPERTY POSITION_INDEPENDENT_CODE ON)
set_target_properties(hwy_test PROPERTIES VERSION ${LIBRARY_VERSION} SOVERSION ${LIBRARY_SOVERSION})
target_include_directories(hwy_test PUBLIC
    $<BUILD_INTERFACE:${CMAKE_CURRENT_LIST_DIR}>
    $<INSTALL_INTERFACE:${CMAKE_INSTALL_INCLUDEDIR}>)
target_compile_features(hwy_test PUBLIC cxx_std_11)
if (NOT HWY_CXX_STD_TGT_COMPILE_FEATURE STREQUAL "cxx_std_11")
  target_compile_features(hwy_test PRIVATE ${HWY_CXX_STD_TGT_COMPILE_FEATURE})
endif()
set_target_properties(hwy_test PROPERTIES
  LINK_DEPENDS ${CMAKE_CURRENT_SOURCE_DIR}/hwy/hwy.version)
# not supported by MSVC/Clang, safe to skip (we use DLLEXPORT annotations)
if(UNIX AND NOT APPLE)
  set_property(TARGET hwy_test APPEND_STRING PROPERTY
    LINK_FLAGS " -Wl,--version-script=${CMAKE_CURRENT_SOURCE_DIR}/hwy/hwy.version")
endif()

# -------------------------------------------------------- hwy_list_targets
# Generate a tool to print the compiled-in targets as defined by the current
# flags. This tool will print to stderr at build time, after building hwy.
add_executable(hwy_list_targets hwy/tests/list_targets.cc)
target_compile_options(hwy_list_targets PRIVATE ${HWY_FLAGS})
target_compile_features(hwy_list_targets PRIVATE ${HWY_CXX_STD_TGT_COMPILE_FEATURE})
target_link_libraries(hwy_list_targets PRIVATE hwy)
target_include_directories(hwy_list_targets PRIVATE
  $<TARGET_PROPERTY:hwy,INCLUDE_DIRECTORIES>)
# TARGET_FILE always returns the path to executable
# Naked target also not always could be run (due to the lack of '.\' prefix)
# Thus effective command to run should contain the full path
# and emulator prefix (if any).
if (NOT CMAKE_CROSSCOMPILING OR CMAKE_CROSSCOMPILING_EMULATOR)
add_custom_command(TARGET hwy_list_targets POST_BUILD
    COMMAND ${CMAKE_CROSSCOMPILING_EMULATOR} $<TARGET_FILE:hwy_list_targets> || (exit 0))
endif()

# --------------------------------------------------------
# Allow skipping the following sections for projects that do not need them:
# tests, examples, benchmarks and installation.

# -------------------------------------------------------- install library
if (HWY_ENABLE_INSTALL)

install(TARGETS hwy EXPORT hwy_targets
  LIBRARY DESTINATION "${CMAKE_INSTALL_LIBDIR}"
  ARCHIVE DESTINATION "${CMAKE_INSTALL_LIBDIR}"
  RUNTIME DESTINATION "${CMAKE_INSTALL_BINDIR}")
# Install all the headers keeping the relative path to the current directory
# when installing them.
foreach (source ${HWY_SOURCES})
  if ("${source}" MATCHES "\.h$")
    get_filename_component(dirname "${source}" DIRECTORY)
    install(FILES "${source}"
        DESTINATION "${CMAKE_INSTALL_INCLUDEDIR}/${dirname}")
  endif()
endforeach()

if (HWY_ENABLE_CONTRIB)
install(TARGETS hwy_contrib EXPORT hwy_targets
  LIBRARY DESTINATION "${CMAKE_INSTALL_LIBDIR}"
  ARCHIVE DESTINATION "${CMAKE_INSTALL_LIBDIR}"
  RUNTIME DESTINATION "${CMAKE_INSTALL_BINDIR}")
# Install all the headers keeping the relative path to the current directory
# when installing them.
foreach (source ${HWY_CONTRIB_SOURCES})
  if ("${source}" MATCHES "\.h$")
    get_filename_component(dirname "${source}" DIRECTORY)
    install(FILES "${source}"
        DESTINATION "${CMAKE_INSTALL_INCLUDEDIR}/${dirname}")
  endif()
endforeach()
endif()  # HWY_ENABLE_CONTRIB

if (HWY_ENABLE_TESTS)
install(TARGETS hwy_test EXPORT hwy_targets
  LIBRARY DESTINATION "${CMAKE_INSTALL_LIBDIR}"
  ARCHIVE DESTINATION "${CMAKE_INSTALL_LIBDIR}"
  RUNTIME DESTINATION "${CMAKE_INSTALL_BINDIR}")
# Install all the headers keeping the relative path to the current directory
# when installing them.
foreach (source ${HWY_TEST_SOURCES})
  if ("${source}" MATCHES "\.h$")
    get_filename_component(dirname "${source}" DIRECTORY)
    install(FILES "${source}"
        DESTINATION "${CMAKE_INSTALL_INCLUDEDIR}/${dirname}")
  endif()
endforeach()
endif()  # HWY_ENABLE_TESTS

# Add a pkg-config file for libhwy and the contrib/test libraries.
set(HWY_LIBRARY_VERSION "${CMAKE_PROJECT_VERSION}")
set(HWY_PC_FILES libhwy.pc)

if (HWY_DISABLE_FUTEX)
  set(HWY_PC_DISABLE_FUTEX_CFLAGS "-DHWY_DISABLE_FUTEX")
else()
  set(HWY_PC_DISABLE_FUTEX_CFLAGS "")
endif()

if (WIN32 AND NOT MSVC AND NOT HWY_DISABLE_FUTEX)
  set(HWY_PC_WIN32_SYNCHRONIZATION_LIBS "-lsynchronization")
else()
  set(HWY_PC_WIN32_SYNCHRONIZATION_LIBS "")
endif()

if (HWY_ENABLE_CONTRIB)
list(APPEND HWY_PC_FILES libhwy-contrib.pc)
endif()  # HWY_ENABLE_CONTRIB
if (HWY_ENABLE_TESTS)

if (HWY_TEST_STANDALONE)
  set(HWY_PC_HWY_TEST_REQUIRES "")
  set(HWY_PC_HWY_TEST_CFLAGS "-DHWY_TEST_STANDALONE=1")
else()
  set(HWY_PC_HWY_TEST_REQUIRES "gtest")
  set(HWY_PC_HWY_TEST_CFLAGS "")
endif()

list(APPEND HWY_PC_FILES libhwy-test.pc)
endif()  # HWY_ENABLE_TESTS
foreach (pc ${HWY_PC_FILES})
  configure_file("${CMAKE_CURRENT_SOURCE_DIR}/${pc}.in" "${pc}" @ONLY)
  install(FILES "${CMAKE_CURRENT_BINARY_DIR}/${pc}"
      DESTINATION "${CMAKE_INSTALL_LIBDIR}/pkgconfig")
endforeach()

endif()  # HWY_ENABLE_INSTALL
# -------------------------------------------------------- Examples
if (HWY_ENABLE_EXAMPLES)

# Avoids mismatch between GTest's static CRT and our dynamic.
set(gtest_force_shared_crt ON CACHE BOOL "" FORCE)

# Programming exercise with integrated benchmark
add_executable(hwy_benchmark hwy/examples/benchmark.cc)
target_sources(hwy_benchmark PRIVATE
    hwy/nanobenchmark.h)
# Try adding one of -DHWY_COMPILE_ONLY_SCALAR, -DHWY_COMPILE_ONLY_EMU128 or
# -DHWY_COMPILE_ONLY_STATIC to observe the difference in targets printed.
target_compile_options(hwy_benchmark PRIVATE ${HWY_FLAGS})
target_compile_features(hwy_benchmark PRIVATE ${HWY_CXX_STD_TGT_COMPILE_FEATURE})
target_link_libraries(hwy_benchmark PRIVATE hwy)
target_link_libraries(hwy_benchmark PRIVATE ${ATOMICS_LIBRARIES})
set_target_properties(hwy_benchmark
    PROPERTIES RUNTIME_OUTPUT_DIRECTORY "examples/")

# Profiler demo
if (HWY_ENABLE_CONTRIB)
add_executable(hwy_profiler_example hwy/examples/profiler_example.cc)
target_sources(hwy_profiler_example PRIVATE
    hwy/profiler.h)
target_compile_options(hwy_profiler_example PRIVATE ${HWY_FLAGS} ${HWY_THREAD_FLAGS})
target_compile_features(hwy_profiler_example PRIVATE ${HWY_CXX_STD_TGT_COMPILE_FEATURE})
target_link_libraries(hwy_profiler_example PRIVATE hwy hwy_contrib)
target_link_libraries(hwy_profiler_example PRIVATE ${ATOMICS_LIBRARIES})
target_link_libraries(hwy_profiler_example PRIVATE ${HWY_THREAD_LIBS})
set_target_properties(hwy_profiler_example
    PROPERTIES RUNTIME_OUTPUT_DIRECTORY "examples/")
endif()  # HWY_ENABLE_CONTRIB

endif()  # HWY_ENABLE_EXAMPLES
# -------------------------------------------------------- Tests

include(CTest)

if(BUILD_TESTING AND HWY_ENABLE_TESTS)
enable_testing()
include(GoogleTest)

set(HWY_SYSTEM_GTEST OFF CACHE BOOL "Use pre-installed googletest?")

if(NOT HWY_TEST_STANDALONE)
if(HWY_SYSTEM_GTEST)
find_package(GTest REQUIRED)
else()
# Download and unpack googletest at configure time
configure_file(CMakeLists.txt.in googletest-download/CMakeLists.txt)
execute_process(COMMAND ${CMAKE_COMMAND} -G "${CMAKE_GENERATOR}" .
  RESULT_VARIABLE result
  WORKING_DIRECTORY ${CMAKE_CURRENT_BINARY_DIR}/googletest-download )
if(result)
  message(FATAL_ERROR "CMake step for googletest failed: ${result}")
endif()
execute_process(COMMAND ${CMAKE_COMMAND} --build .
  RESULT_VARIABLE result
  WORKING_DIRECTORY ${CMAKE_CURRENT_BINARY_DIR}/googletest-download )
if(result)
  message(FATAL_ERROR "Build step for googletest failed: ${result}")
endif()

# Prevent overriding the parent project's compiler/linker
# settings on Windows
set(gtest_force_shared_crt ON CACHE BOOL "" FORCE)

# Add googletest directly to our build. This defines
# the gtest and gtest_main targets.
add_subdirectory(${CMAKE_CURRENT_BINARY_DIR}/googletest-src
                 ${CMAKE_CURRENT_BINARY_DIR}/googletest-build
                 EXCLUDE_FROM_ALL)
endif()  # HWY_SYSTEM_GTEST
endif()  # HWY_TEST_STANDALONE

set(HWY_TEST_FILES
  hwy/abort_test.cc
  hwy/aligned_allocator_test.cc
  hwy/base_test.cc
  hwy/bit_set_test.cc
  hwy/highway_test.cc
  hwy/nanobenchmark_test.cc
  hwy/perf_counters_test.cc
  hwy/targets_test.cc
  hwy/examples/skeleton_test.cc
  hwy/tests/arithmetic_test.cc
  hwy/tests/bit_permute_test.cc
  hwy/tests/blockwise_combine_test.cc
  hwy/tests/blockwise_shift_test.cc
  hwy/tests/blockwise_test.cc
  hwy/tests/cast_test.cc
  hwy/tests/combine_test.cc
  hwy/tests/compare_test.cc
  hwy/tests/complex_arithmetic_test.cc
  hwy/tests/compress_test.cc
  hwy/tests/concat_test.cc
  hwy/tests/convert_test.cc
  hwy/tests/count_test.cc
  hwy/tests/crypto_test.cc
  hwy/tests/demote_test.cc
  hwy/tests/div_test.cc
  hwy/tests/dup128_vec_test.cc
  hwy/tests/expand_test.cc
  hwy/tests/float_test.cc
  hwy/tests/fma_test.cc
  hwy/tests/foreach_vec_test.cc
  hwy/tests/if_test.cc
  hwy/tests/in_range_float_to_int_conv_test.cc
  hwy/tests/interleaved_test.cc
  hwy/tests/logical_test.cc
  hwy/tests/mask_combine_test.cc
  hwy/tests/mask_convert_test.cc
  hwy/tests/mask_mem_test.cc
  hwy/tests/mask_set_test.cc
  hwy/tests/mask_slide_test.cc
  hwy/tests/mask_test.cc
  hwy/tests/masked_arithmetic_test.cc
  hwy/tests/masked_minmax_test.cc
  hwy/tests/memory_test.cc
  hwy/tests/minmax_magnitude_test.cc
  hwy/tests/minmax_number_test.cc
  hwy/tests/minmax_test.cc
  hwy/tests/minmax128_test.cc
  hwy/tests/mul_by_pow2_test.cc
  hwy/tests/mul_pairwise_test.cc
  hwy/tests/mul_test.cc
  hwy/tests/reduction_test.cc
  hwy/tests/resize_test.cc
  hwy/tests/reverse_test.cc
  hwy/tests/rotate_test.cc
  hwy/tests/saturated_test.cc
  hwy/tests/shift_test.cc
  hwy/tests/shuffle4_test.cc
  hwy/tests/sign_test.cc
  hwy/tests/slide_up_down_test.cc
  hwy/tests/sums_abs_diff_test.cc
  hwy/tests/swizzle_block_test.cc
  hwy/tests/swizzle_test.cc
  hwy/tests/table_test.cc
  hwy/tests/test_util_test.cc
  hwy/tests/truncate_test.cc
  hwy/tests/tuple_test.cc
  hwy/tests/widen_mul_test.cc
)

set(HWY_TEST_LIBS hwy hwy_test)

if (HWY_ENABLE_CONTRIB)
list(APPEND HWY_TEST_LIBS hwy_contrib)

list(APPEND HWY_TEST_FILES
  hwy/auto_tune_test.cc
  hwy/contrib/algo/copy_test.cc
  hwy/contrib/algo/find_test.cc
  hwy/contrib/algo/transform_test.cc
  hwy/contrib/bit_pack/bit_pack_test.cc
  hwy/contrib/dot/dot_test.cc
  hwy/contrib/matvec/matvec_test.cc
  hwy/contrib/image/image_test.cc
  # Disabled due to SIGILL in clang7 debug build during gtest discovery phase,
  # not reproducible locally. Still tested via bazel build.
  hwy/contrib/math/math_test.cc
  hwy/contrib/random/random_test.cc
  hwy/contrib/sort/bench_sort.cc
  hwy/contrib/sort/sort_test.cc
  hwy/contrib/sort/sort_unit_test.cc
  hwy/contrib/thread_pool/spin_test.cc
  hwy/contrib/thread_pool/thread_pool_test.cc
  hwy/contrib/thread_pool/topology_test.cc
  hwy/contrib/unroller/unroller_test.cc
)
endif()  # HWY_ENABLE_CONTRIB

if(HWY_TEST_STANDALONE)
  set(HWY_GTEST_LIBS "")
else()
  if(HWY_SYSTEM_GTEST)
    if (CMAKE_VERSION VERSION_LESS 3.20)
      set(HWY_GTEST_LIBS GTest::GTest GTest::Main)
    else()
      set(HWY_GTEST_LIBS GTest::gtest GTest::gtest_main)
    endif()
  else()
    set(HWY_GTEST_LIBS gtest gtest_main)
  endif()
endif()  # HWY_TEST_STANDALONE

file(MAKE_DIRECTORY ${CMAKE_CURRENT_BINARY_DIR}/tests)
foreach (TESTFILE IN LISTS HWY_TEST_FILES)
  # The TESTNAME is the name without the extension or directory.
  get_filename_component(TESTNAME ${TESTFILE} NAME_WE)
  add_executable(${TESTNAME} ${TESTFILE})
  target_compile_options(${TESTNAME} PRIVATE ${HWY_FLAGS} ${HWY_THREAD_FLAGS})
  # Test all targets, not just the best/baseline. This changes the default
  # policy to all-attainable; note that setting -DHWY_COMPILE_* directly can
  # cause compile errors because only one may be set, and other CMakeLists.txt
  # that include us may set them.
  target_compile_options(${TESTNAME} PRIVATE -DHWY_IS_TEST=1)
  if(HWY_TEST_STANDALONE)
    target_compile_options(${TESTNAME} PRIVATE -DHWY_TEST_STANDALONE=1)
  endif()
  target_compile_features(${TESTNAME} PRIVATE ${HWY_CXX_STD_TGT_COMPILE_FEATURE})

  target_link_libraries(${TESTNAME} PRIVATE ${HWY_TEST_LIBS} ${HWY_GTEST_LIBS})
  # For GCC __atomic_store_8, see #887
  target_link_libraries(${TESTNAME} PRIVATE ${ATOMICS_LIBRARIES})

  # Avoid linker errors if libpthread needs to be linked
  target_link_libraries(${TESTNAME} PRIVATE ${HWY_THREAD_LIBS})

  # Output test targets in the test directory.
  set_target_properties(${TESTNAME} PROPERTIES RUNTIME_OUTPUT_DIRECTORY "tests")

  if (HWY_EMSCRIPTEN)
    set_target_properties(${TESTNAME} PROPERTIES LINK_FLAGS "-s SINGLE_FILE=1")
  endif()

  if(${CMAKE_VERSION} VERSION_LESS "3.10.3")
    gtest_discover_tests(${TESTNAME} TIMEOUT 60)
  else ()
    gtest_discover_tests(${TESTNAME} DISCOVERY_TIMEOUT 60)
  endif ()
endforeach ()

# The skeleton test uses the skeleton library code.
target_sources(skeleton_test PRIVATE hwy/examples/skeleton.cc)

endif()  # BUILD_TESTING

if (HWY_ENABLE_INSTALL)
  # write hwy-config file to handle `Config` mode
  include(CMakePackageConfigHelpers)
  write_basic_package_version_file("${CMAKE_CURRENT_BINARY_DIR}/hwy-config-version.cmake" COMPATIBILITY SameMajorVersion)
  install(FILES "${CMAKE_CURRENT_BINARY_DIR}/hwy-config-version.cmake" DESTINATION "${CMAKE_INSTALL_LIBDIR}/cmake/hwy")
  install(EXPORT hwy_targets NAMESPACE "${namespace}" FILE hwy-config.cmake DESTINATION "${CMAKE_INSTALL_LIBDIR}/cmake/hwy")
endif()
