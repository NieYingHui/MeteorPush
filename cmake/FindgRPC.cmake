# FindgRPC.cmake
# 通过 pkg-config 查找系统安装的 gRPC (apt 安装的版本不提供 CMake Config 文件)
# 创建 gRPC::grpc++ 等 imported target 供项目 link 使用

find_package(PkgConfig QUIET)
if(PkgConfig_FOUND)
  pkg_check_modules(PC_GRPC grpc)
  pkg_check_modules(PC_GRPCPP grpc++)
  pkg_check_modules(PC_GRPCUNSECURE grpc_unsecure)
  pkg_check_modules(PC_GPR gpr)
endif()

find_path(GRPC_INCLUDE_DIR
  NAMES grpc/grpc.h
  HINTS ${PC_GRPC_INCLUDEDIR} ${PC_GRPC_INCLUDE_DIRS}
)

find_library(GRPC_LIBRARY
  NAMES grpc
  HINTS ${PC_GRPC_LIBDIR} ${PC_GRPC_LIBRARY_DIRS}
)

find_library(GRPCXX_LIBRARY
  NAMES grpc++
  HINTS ${PC_GRPCPP_LIBDIR} ${PC_GRPCPP_LIBRARY_DIRS}
)

find_library(GRPCXX_UNSECURE_LIBRARY
  NAMES grpc++_unsecure
  HINTS ${PC_GRPC_LIBDIR} ${PC_GRPC_LIBRARY_DIRS}
)

find_library(GPR_LIBRARY
  NAMES gpr
  HINTS ${PC_GPR_LIBDIR} ${PC_GPR_LIBRARY_DIRS}
)

# Protobuf 由项目单独 find_package(Protobuf REQUIRED)，这里直接复用
if(NOT TARGET protobuf::libprotobuf)
  find_package(Protobuf REQUIRED)
endif()

include(FindPackageHandleStandardArgs)
find_package_handle_standard_args(gRPC
  REQUIRED_VARS GRPC_LIBRARY GRPCXX_LIBRARY GRPC_INCLUDE_DIR
  VERSION_VAR PC_GRPC_VERSION
)

if(gRPC_FOUND AND NOT TARGET gRPC::grpc++)
  # 提取 include 目录的父目录（因为 include 路径以 grpc/ 结尾）
  get_filename_component(_GRPC_ROOT_INCLUDE_DIR ${GRPC_INCLUDE_DIR} DIRECTORY)

  add_library(gRPC::grpc UNKNOWN IMPORTED)
  set_target_properties(gRPC::grpc PROPERTIES
    IMPORTED_LOCATION "${GRPC_LIBRARY}"
    INTERFACE_INCLUDE_DIRECTORIES "${_GRPC_ROOT_INCLUDE_DIR}"
    INTERFACE_LINK_LIBRARIES "${GPR_LIBRARY};${CMAKE_DL_LIBS};protobuf::libprotobuf"
  )

  add_library(gRPC::grpc++ UNKNOWN IMPORTED)
  set_target_properties(gRPC::grpc++ PROPERTIES
    IMPORTED_LOCATION "${GRPCXX_LIBRARY}"
    INTERFACE_INCLUDE_DIRECTORIES "${_GRPC_ROOT_INCLUDE_DIR}"
    INTERFACE_LINK_LIBRARIES "gRPC::grpc;protobuf::libprotobuf"
  )

  if(GRPCXX_UNSECURE_LIBRARY)
    add_library(gRPC::grpc++_unsecure UNKNOWN IMPORTED)
    set_target_properties(gRPC::grpc++_unsecure PROPERTIES
      IMPORTED_LOCATION "${GRPCXX_UNSECURE_LIBRARY}"
      INTERFACE_INCLUDE_DIRECTORIES "${_GRPC_ROOT_INCLUDE_DIR}"
    )
  endif()

  add_library(gRPC::gpr UNKNOWN IMPORTED)
  set_target_properties(gRPC::gpr PROPERTIES
    IMPORTED_LOCATION "${GPR_LIBRARY}"
    INTERFACE_INCLUDE_DIRECTORIES "${_GRPC_ROOT_INCLUDE_DIR}"
  )

  set(GRPC_LIBRARIES ${GRPCXX_LIBRARY} ${GRPC_LIBRARY} ${GPR_LIBRARY})
  set(GRPC_INCLUDE_DIRS ${_GRPC_ROOT_INCLUDE_DIR})
endif()
