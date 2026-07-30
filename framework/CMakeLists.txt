cmake_minimum_required(VERSION 3.5)
project(nereid-bench LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

if(NOT DEFINED CMAKE_CXX_COMPILER OR CMAKE_CXX_COMPILER STREQUAL "")
  set(CMAKE_CXX_COMPILER g++ CACHE STRING "C++ compiler" FORCE)
endif()

list(APPEND CMAKE_MODULE_PATH "${CMAKE_CURRENT_SOURCE_DIR}/cmake")
include(FetchContent)

# --- gRPC / protobuf ---
include(FetchGrpc)

# --- yaml-cpp ---
set(YAML_CPP_BUILD_TESTS OFF CACHE BOOL "" FORCE)
set(YAML_CPP_BUILD_TOOLS OFF CACHE BOOL "" FORCE)
set(YAML_CPP_INSTALL OFF CACHE BOOL "" FORCE)
FetchContent_Declare(
  yaml-cpp
  GIT_REPOSITORY https://github.com/jbeder/yaml-cpp.git
  GIT_TAG        0.8.0
  GIT_SHALLOW    TRUE
)
FetchContent_MakeAvailable(yaml-cpp)

# --- nlohmann/json ---
FetchContent_Declare(
  nlohmann_json
  GIT_REPOSITORY https://github.com/nlohmann/json.git
  GIT_TAG        v3.11.3
  GIT_SHALLOW    TRUE
)
FetchContent_MakeAvailable(nlohmann_json)

# --- proto codegen ---
set(PROTO_PATH "${CMAKE_CURRENT_SOURCE_DIR}/proto")
set(PROTO_FILE "${PROTO_PATH}/grpc_service.proto")
get_filename_component(PROTO_NAME "${PROTO_FILE}" NAME_WE)

set(GENERATED_PROTO_DIR "${CMAKE_CURRENT_BINARY_DIR}/generated")
file(MAKE_DIRECTORY "${GENERATED_PROTO_DIR}")

set(PROTO_SRCS "${GENERATED_PROTO_DIR}/${PROTO_NAME}.pb.cc")
set(PROTO_HDRS "${GENERATED_PROTO_DIR}/${PROTO_NAME}.pb.h")
set(GRPC_SRCS  "${GENERATED_PROTO_DIR}/${PROTO_NAME}.grpc.pb.cc")
set(GRPC_HDRS  "${GENERATED_PROTO_DIR}/${PROTO_NAME}.grpc.pb.h")

add_custom_command(
  OUTPUT  "${PROTO_SRCS}" "${PROTO_HDRS}" "${GRPC_SRCS}" "${GRPC_HDRS}"
  COMMAND $<TARGET_FILE:protobuf::protoc>
  ARGS    --grpc_out="${GENERATED_PROTO_DIR}"
          --cpp_out="${GENERATED_PROTO_DIR}"
          -I "${PROTO_PATH}"
          --plugin=protoc-gen-grpc=$<TARGET_FILE:grpc_cpp_plugin>
          "${PROTO_FILE}"
  DEPENDS "${PROTO_FILE}" grpc_cpp_plugin protobuf::protoc
  COMMENT "Generating gRPC stubs from grpc_service.proto"
)

add_library(grpc_service_proto STATIC
  "${PROTO_SRCS}" "${GRPC_SRCS}"
)
target_link_libraries(grpc_service_proto PUBLIC grpc++ protobuf::libprotobuf)
target_include_directories(grpc_service_proto PUBLIC "${GENERATED_PROTO_DIR}")

# --- nereid-bench executable ---
set(NEREID_BENCH_SOURCES
  src/main.cpp
)

add_executable(nereid-bench ${NEREID_BENCH_SOURCES})
target_include_directories(nereid-bench PRIVATE
  "${CMAKE_CURRENT_SOURCE_DIR}/src"
  "${GENERATED_PROTO_DIR}"
)
target_link_libraries(nereid-bench PRIVATE
  grpc_service_proto
  yaml-cpp
  nlohmann_json::nlohmann_json
)
