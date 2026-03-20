cmake_minimum_required(VERSION 3.13)
project(VarRenamer)

find_package(LLVM REQUIRED CONFIG)

find_package(Clang REQUIRED CONFIG)

message(STATUS "Found LLVM ${LLVM_PACKAGE_VERSION}")
message(STATUS "LLVM include dirs: ${LLVM_INCLUDE_DIRS}")
message(STATUS "Clang include dirs: ${CLANG_INCLUDE_DIRS}")

include_directories(${LLVM_INCLUDE_DIRS} ${CLANG_INCLUDE_DIRS})
add_definitions(${LLVM_DEFINITIONS})

add_executable(VarRenamer VarRenamer.cpp)

target_link_libraries(VarRenamer PRIVATE
  clangTooling
  clangAST
  clangBasic
  clangRewrite
  clangFrontend
  ${LLVM_COMMON_LIBS}
)

set_property(TARGET VarRenamer PROPERTY CXX_STANDARD 17)
