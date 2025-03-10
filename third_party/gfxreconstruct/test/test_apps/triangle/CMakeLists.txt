###############################################################################
# Copyright (c) 2018-2024 LunarG, Inc.
# All rights reserved
#
# Permission is hereby granted, free of charge, to any person obtaining a copy
# of this software and associated documentation files (the "Software"), to
# deal in the Software without restriction, including without limitation the
# rights to use, copy, modify, merge, publish, distribute, sublicense, and/or
# sell copies of the Software, and to permit persons to whom the Software is
# furnished to do so, subject to the following conditions:
#
# The above copyright notice and this permission notice shall be included in
# all copies or substantial portions of the Software.
#
# THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
# IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
# FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
# AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
# LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING
# FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS
# IN THE SOFTWARE.
#
# Author: LunarG Team
# Description: CMake script for triangle test app
###############################################################################

add_executable(gfxrecon-testapp-triangle "")

target_sources(gfxrecon-testapp-triangle
        PRIVATE
        ${CMAKE_CURRENT_LIST_DIR}/app.cpp
        ${CMAKE_CURRENT_LIST_DIR}/../common/test_app_base.cpp)

target_include_directories(gfxrecon-testapp-triangle PUBLIC
        ${CMAKE_BINARY_DIR}
        ${CMAKE_CURRENT_LIST_DIR}/../common)

target_link_libraries(gfxrecon-testapp-triangle
        gfxrecon_application
        gfxrecon_decode
        gfxrecon_graphics
        gfxrecon_format
        gfxrecon_util
        SDL3::SDL3
        platform_specific)

if (MSVC)
    # Force inclusion of "gfxrecon_disable_popup_result" variable in linking.
    # On 32-bit windows, MSVC prefixes symbols with "_" but on 64-bit windows it doesn't.
    if (CMAKE_SIZEOF_VOID_P EQUAL 4)
        target_link_options(gfxrecon-replay PUBLIC "LINKER:/Include:_gfxrecon_disable_popup_result")
    else ()
        target_link_options(gfxrecon-replay PUBLIC "LINKER:/Include:gfxrecon_disable_popup_result")
    endif ()
endif ()

common_build_directives(gfxrecon-testapp-triangle)

add_custom_command(
        TARGET gfxrecon-testapp-triangle
        POST_BUILD
        COMMAND
        ${CMAKE_COMMAND} -E copy_directory
        ${CMAKE_CURRENT_LIST_DIR}/shaders ${CMAKE_CURRENT_BINARY_DIR}
        DEPENDS ${CMAKE_CURRENT_BINARY_DIR}/${CMAKE_BUILD_TYPE})
if (WIN32)
    add_custom_command(TARGET gfxrecon-testapp-triangle POST_BUILD
            COMMAND ${CMAKE_COMMAND} -E copy_if_different $<TARGET_RUNTIME_DLLS:gfxrecon-testapp-triangle> $<TARGET_FILE_DIR:gfxrecon-testapp-triangle>
            COMMAND_EXPAND_LISTS)
endif ()

install(TARGETS gfxrecon-testapp-triangle RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR})

add_dependencies(gfxrecon-testapps gfxrecon-testapp-triangle)