cmake_minimum_required(VERSION 3.16)

project(dxil
    LANGUAGES CXX
    VERSION 1.0
)

set(TargetName dxil)


set(Headers
  inc/dxil/d3d12shader.h
  inc/dxil/dxcapi.h
  inc/dxil/dxcerrors.h
  inc/dxil/dxcisense.h
)

add_library(${TargetName} SHARED IMPORTED GLOBAL)

set_property(TARGET ${TargetName} PROPERTY
             IMPORTED_LOCATION 
             "${CMAKE_CURRENT_SOURCE_DIR}/bin/x64/dxcompiler.dll"
             "${CMAKE_CURRENT_SOURCE_DIR}/bin/x64/dxil.dll"
)

set_property(TARGET ${TargetName} PROPERTY
             IMPORTED_IMPLIB "${CMAKE_CURRENT_SOURCE_DIR}/lib/x64/dxcompiler.lib")

target_include_directories(${TargetName} INTERFACE ${CMAKE_CURRENT_SOURCE_DIR}/inc)


