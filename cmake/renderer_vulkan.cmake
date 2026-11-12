# Copyright (C) 2026 the OpenMOHAA Plus contributors
# SPDX-License-Identifier: AGPL-3.0-or-later
# Added in OpenMOHAA Plus: renderer_vulkan, the GL1 renderer on a Vulkan emulation of the GL 1.x
# calls it makes (code/renderervk, docs/dev/architecture/renderer-vulkan.md). Built when the
# Vulkan headers are found (Debian/Ubuntu: libvulkan-dev); the Vulkan loader is loaded at run time.

if(NOT BUILD_CLIENT OR NOT BUILD_RENDERER_VULKAN OR NOT BUILD_RENDERER_GL1 OR NOT USE_RENDERER_DLOPEN)
    return()
endif()

find_path(VULKAN_INCLUDE_DIR vulkan/vulkan.h)
if(NOT VULKAN_INCLUDE_DIR)
    message(STATUS "Vulkan headers not found: renderer_vulkan is not built")
    return()
endif()

include(utils/set_output_dirs)
include(renderer_common)

set(RENDERER_VULKAN_BINARY renderer_vulkan)

add_library(${RENDERER_VULKAN_BINARY} SHARED
    ${RENDERER_COMMON_SOURCES}
    ${RENDERER_GL1_SOURCES}
    ${SDL_RENDERER_SOURCES}
    ${RENDERER_LIBRARY_SOURCES}
    ${DYNAMIC_RENDERER_SOURCES}
    ${SOURCE_DIR}/renderervk/vk_core.c
    ${SOURCE_DIR}/renderervk/vk_gl.c)

target_link_libraries(      ${RENDERER_VULKAN_BINARY} PRIVATE ${RENDERER_LIBRARIES})
target_include_directories( ${RENDERER_VULKAN_BINARY} PRIVATE ${RENDERER_INCLUDE_DIRS} ${VULKAN_INCLUDE_DIR})
target_compile_definitions( ${RENDERER_VULKAN_BINARY} PRIVATE ${RENDERER_DEFINITIONS} USE_VULKAN)
target_compile_options(     ${RENDERER_VULKAN_BINARY} PRIVATE ${RENDERER_COMPILE_OPTIONS})
target_link_options(        ${RENDERER_VULKAN_BINARY} PRIVATE ${RENDERER_LINK_OPTIONS})

set_output_dirs(${RENDERER_VULKAN_BINARY})

install(TARGETS ${RENDERER_VULKAN_BINARY} DESTINATION ${INSTALL_LIBDIR_FULL})
if(MSVC)
    install(FILES $<TARGET_PDB_FILE:${RENDERER_VULKAN_BINARY}> DESTINATION ${INSTALL_LIBDIR_FULL} OPTIONAL)
endif()
