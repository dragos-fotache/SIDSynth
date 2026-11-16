# SPDX-License-Identifier: AGPL-3.0-only
include(FetchContent)
set(SIDSYNTH_DEPENDENCY_ARCHIVE_DIR "" CACHE PATH "Optional directory containing the pinned dependency archives")
set(juce_url https://codeload.github.com/juce-framework/JUCE/tar.gz/d60b7a78030661574a615b586d9325ba7a7fa7dd)
set(residfp_url https://codeload.github.com/libsidplayfp/libresidfp/tar.gz/5393471a02a505ff43aa710148b2d35289b72c31)
if(SIDSYNTH_DEPENDENCY_ARCHIVE_DIR)
    set(juce_url "${SIDSYNTH_DEPENDENCY_ARCHIVE_DIR}/juce-9.0.5.tar.gz")
    set(residfp_url "${SIDSYNTH_DEPENDENCY_ARCHIVE_DIR}/libresidfp-1.3.0.tar.gz")
endif()
FetchContent_Declare(juce
    URL "${juce_url}"
    URL_HASH SHA256=8237b2976871982eb9107fd67b576d95bf02355d99540a0127c4e7c001ecd9f5
    DOWNLOAD_EXTRACT_TIMESTAMP TRUE)
FetchContent_MakeAvailable(juce)

FetchContent_Declare(libresidfp
    URL "${residfp_url}"
    URL_HASH SHA256=0c38824bb51a8c5b62bb5400c290c60cc4f8e7c2d145c559e6449f7d42cf5358
    DOWNLOAD_EXTRACT_TIMESTAMP TRUE)
FetchContent_MakeAvailable(libresidfp)

# Upstream 1.3.0 uses autotools. Mirror its Makefile.am source list and
# configure.ac release configuration. Keep the downloaded source immutable.
# One explicit friend declaration in a build-tree copy lets our adapter cache
# model tables/resamplers outside the callback (upstream setters allocate/lock).
set(residfp_rt_src "${libresidfp_BINARY_DIR}/realtime-src")
file(COPY "${libresidfp_SOURCE_DIR}/src/" DESTINATION "${residfp_rt_src}" PATTERN "SID.h" EXCLUDE)
file(READ "${libresidfp_SOURCE_DIR}/src/SID.h" sid_header)
set(sid_anchor "    friend class State;")
string(FIND "${sid_header}" "${sid_anchor}" sid_anchor_position)
if(sid_anchor_position EQUAL -1)
    message(FATAL_ERROR "Pinned SID.h adapter anchor changed")
endif()
string(REPLACE "#define SIDFP_H" "#define SIDFP_H\n\nnamespace sidsynth { class SidBackend; }" sid_header "${sid_header}")
string(REPLACE "${sid_anchor}" "${sid_anchor}\n    friend class sidsynth::SidBackend;" sid_header "${sid_header}")
file(WRITE "${libresidfp_BINARY_DIR}/SID-realtime.h.in" "${sid_header}")
configure_file("${libresidfp_BINARY_DIR}/SID-realtime.h.in" "${residfp_rt_src}/SID.h" COPYONLY)
set(RESIDFP_BRANCH_HINTS 1)
set(HAVE_BUILTIN_EXPECT 1)
set(RESIDFP_INLINING 1)
set(RESIDFP_INLINE inline)
set(PACKAGE_VERSION 1.3.0)
set(LIB_MAJOR 1)
set(LIB_MINOR 3)
set(LIB_LEVEL 0)
file(MAKE_DIRECTORY "${libresidfp_BINARY_DIR}/residfp")
configure_file("${libresidfp_SOURCE_DIR}/src/siddefs-fp.h.in"
    "${libresidfp_BINARY_DIR}/siddefs-fp.h" @ONLY)
configure_file("${libresidfp_SOURCE_DIR}/src/residfp/sidversion.h.in"
    "${libresidfp_BINARY_DIR}/residfp/sidversion.h" @ONLY)
set(residfp_sources Dac EnvelopeGenerator ExternalFilter Filter Filter6581 Filter8580
    FilterModelConfig FilterModelConfig6581 FilterModelConfig8580 Integrator6581
    Integrator8580 OpAmp SID Spline State WaveformCalculator WaveformGenerator)
list(TRANSFORM residfp_sources PREPEND "${residfp_rt_src}/")
list(TRANSFORM residfp_sources APPEND ".cpp")
add_library(libresidfp STATIC ${residfp_sources}
    "${residfp_rt_src}/resample/SincResampler.cpp"
    "${residfp_rt_src}/residfp/residfp.cpp"
    "${residfp_rt_src}/version.cc")
target_include_directories(libresidfp SYSTEM PUBLIC
    "${residfp_rt_src}" "${libresidfp_BINARY_DIR}")
find_package(Threads REQUIRED)
target_link_libraries(libresidfp PRIVATE Threads::Threads)
target_compile_options(libresidfp PRIVATE -fvisibility=hidden -fvisibility-inlines-hidden)
