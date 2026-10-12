# Fetch and prepare HdrHistogram via FetchContent
include(FetchContent)

# Prefer a static library build for this project and disable the examples/tests
# that are not needed by the benchmark executable. The upstream project exposes
# these options for this purpose.
set(HDR_HISTOGRAM_BUILD_PROGRAMS OFF CACHE BOOL "" FORCE)
set(HDR_HISTOGRAM_INSTALL_SHARED OFF CACHE BOOL "" FORCE)
set(HDR_HISTOGRAM_INSTALL_STATIC ON CACHE BOOL "" FORCE)
set(HDR_LOG_REQUIRED OFF CACHE BOOL "" FORCE)
set(BUILD_SHARED_LIBS OFF CACHE BOOL "" FORCE)

FetchContent_Declare(
  hdr_histogram
  GIT_REPOSITORY https://github.com/HdrHistogram/HdrHistogram_c.git
  GIT_TAG        0.11.10
  GIT_SHALLOW    TRUE
)

FetchContent_MakeAvailable(hdr_histogram)
