# Fetch and prepare yaml-cpp via FetchContent
include(FetchContent)

set(YAML_CPP_BUILD_TESTS OFF CACHE BOOL "" FORCE)
set(YAML_CPP_BUILD_TOOLS OFF CACHE BOOL "" FORCE)
set(YAML_CPP_INSTALL OFF CACHE BOOL "" FORCE)

FetchContent_Declare(
  yaml-cpp
  GIT_REPOSITORY https://github.com/jbeder/yaml-cpp.git
  GIT_TAG        yaml-cpp-0.9.0
  GIT_SHALLOW    TRUE
)

# Always include `<cstdint>` for C++ sources
if(CMAKE_CXX_COMPILER_ID STREQUAL "Clang"
   OR (CMAKE_CXX_COMPILER_ID STREQUAL "GNU" AND CMAKE_CXX_COMPILER_VERSION VERSION_GREATER_EQUAL "13.0"))
  add_compile_options("$<$<COMPILE_LANGUAGE:CXX>:SHELL:-include cstdint>")
endif()

FetchContent_MakeAvailable(yaml-cpp)
