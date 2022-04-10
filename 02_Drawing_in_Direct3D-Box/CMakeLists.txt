cmake_minimum_required(VERSION 3.12)


# ------------------------------------------------------------------------------
# 用户配置
# ------------------------------------------------------------------------------
# 设置项目名(不能有空格) 
set(TARGET_NAME "02_Drawing_in_Direct3D-Box")
# 设置HLSL文件的存放位置
set(HLSL_DIR "Shaders")
# ------------------------------------------------------------------------------
# 项目配置
# ------------------------------------------------------------------------------
set(CMAKE_CXX_STANDARD 17)
add_compile_definitions(UNICODE _UNICODE)
# add_compile_definitions(_WIN32_WINNT=0x601)  # Win7系统需要取消注释
add_compile_options("$<$<CXX_COMPILER_ID:MSVC>:/utf-8>")

file(COPY ${HLSL_DIR} DESTINATION ${CMAKE_CURRENT_BINARY_DIR})
file(GLOB HLSL_FILES ${HLSL_DIR}/*.hlsl)

file(GLOB HEADER_FILES ./*.h)

set(COMMON_SRC "../Common")

aux_source_directory(. DIR_SRCS)
aux_source_directory(${COMMON_SRC} COMMON_SOURCES)

LIST(APPEND ALL_SRC
        ${DIR_SRCS}
        ${COMMON_SOURCES}
        )

add_executable(${TARGET_NAME} WIN32 ${ALL_SRC} ${HEADER_FILES})

target_link_libraries(${TARGET_NAME} d3d12.lib d3d11.lib dxgi.lib dxguid.lib d3dcompiler.lib winmm.lib)
target_include_directories(${TARGET_NAME} PRIVATE ${COMMON_SRC})

#message(STATUS "all src: ${ALL_SRC}")
#message(STATUS "common include dir: ${COMMON_SRC}")

# target_link_libraries(${TARGET_NAME} d2d1.lib dwrite.lib)  # 使用Direct2D需要取消注释

source_group("HLSL Files" FILES ${HLSL_FILES})

# 复制 Shaders 文件夹
file(COPY ${HLSL_DIR} DESTINATION ${CMAKE_CURRENT_BINARY_DIR})
#foreach (f ${HLSL_DIR})
#    add_custom_command(TARGET ${TARGET_NAME} PRE_BUILD
#            COMMAND ${CMAKE_COMMAND} -E
#            copy ${f} ${CMAKE_CURRENT_BINARY_DIR})
#endforeach ()

# 输出文件名
set_target_properties(${TARGET_NAME} PROPERTIES OUTPUT_NAME ${TARGET_NAME})
set_target_properties(${TARGET_NAME} PROPERTIES RUNTIME_OUTPUT_DIRECTORY_DEBUG ${CMAKE_CURRENT_BINARY_DIR})
set_target_properties(${TARGET_NAME} PROPERTIES RUNTIME_OUTPUT_DIRECTORY_RELEASE ${CMAKE_CURRENT_BINARY_DIR})
# set_target_properties(${TARGET_NAME} PROPERTIES FOLDER "Project 01-09")


# ------------------------------------------------------------------------------
# 着色器配置
# ------------------------------------------------------------------------------
#
# 如果需要编译期编译着色器，取消注释如下代码
# 要求着色器命名格式为：ShaderName_ShaderType.hlsl
# ShaderType 为右边任意一种：VS, vs, HS, hs, DS, ds, GS, gs, PS, ps, CS, cs
#

# target_sources(${TARGET_NAME} ${HLSL_FILES})
# file(GLOB HLSL_FILES ${HLSL_DIR}/*.hlsl)
# foreach(HLSL_FILE ${HLSL_FILES})
#     get_filename_component(HLSL_FDIR ${HLSL_FILE} DIRECTORY)
#     get_filename_component(HLSL_FBASENAME_WE ${HLSL_FILE} NAME_WE)
#     string(CONCAT HLSL_FNAME_WE ${HLSL_FDIR} / ${HLSL_FBASENAME_WE})
#     string(LENGTH ${HLSL_FBASENAME_WE} LEN_FNAME_WE)
#     math(EXPR LEN_FNAME_WE "${LEN_FNAME_WE}-2")
#     string(SUBSTRING ${HLSL_FBASENAME_WE} ${LEN_FNAME_WE} 2 ENTRY_POINT)
#     string(TOLOWER ${ENTRY_POINT} SHADER_TYPE)
#     if ("${SHADER_TYPE}" STREQUAL "vs") 
#         set(SHADER_TYPE "Vertex")
#     elseif("${SHADER_TYPE}" STREQUAL "hs") 
#         set(SHADER_TYPE "Hull")
#       elseif("${SHADER_TYPE}" STREQUAL "ds") 
#         set(SHADER_TYPE "Domain")
#     elseif("${SHADER_TYPE}" STREQUAL "gs") 
#         set(SHADER_TYPE "Geometry")
#     elseif("${SHADER_TYPE}" STREQUAL "ps")
#         set(SHADER_TYPE "Pixel")
#     elseif("${SHADER_TYPE}" STREQUAL "cs")
#         set(SHADER_TYPE "Compute")
#     endif()
#     set_source_files_properties(${HLSL_FILE} PROPERTIES
#                                 VS_SHADER_OBJECT_FILE_NAME HLSL/${HLSL_FBASENAME_WE}.cso
#                                 VS_SHADER_TYPE ${SHADER_TYPE}
#                                 VS_SHADER_MODEL 5.0
#                                 VS_SHADER_ENTRYPOINT ${ENTRY_POINT}
#                                 VS_SHADER_DISABLE_OPTIMIZATIONS $<$<CONFIG:Debug>:true>
#                                 VS_SHADER_ENABLE_DEBUG $<$<CONFIG:Debug>:true>)
# endforeach()

# ------------------------------------------------------------------------------
# 第三方库配置
# ------------------------------------------------------------------------------
#
# 使用ImGui 需要将项目中的ImGui文件夹复制进来，保留*.h/*.cpp/CMakeLists.txt
#

# add_subdirectory(ImGui)
# target_compile_definitions(${TARGET_NAME} PRIVATE USE_IMGUI)
# target_link_libraries(${TARGET_NAME} ImGui)
# file(COPY imgui.ini DESTINATION ${CMAKE_CURRENT_BINARY_DIR})

#
# 使用Assimp 需要先在cmake中完成Install
#

# set(Assimp_INSTALLED_DIR "C:/Program Files (x86)/Assimp")
# target_link_libraries(${TARGET_NAME} "${Assimp_INSTALLED_DIR}/lib/assimp-vc${MSVC_TOOLSET_VERSION}-# mt$<$<CONFIG:Debug>:d>.lib")
# include_directories(${Assimp_INSTALLED_DIR}/include)
# set_target_properties(${TARGET_NAME} PROPERTIES VS_DEBUGGER_ENVIRONMENT "PATH=${Assimp_INSTALLED_DIR}/bin")


