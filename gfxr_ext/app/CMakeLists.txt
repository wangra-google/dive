#
# Copyright 2025 Google LLC
#
# Licensed under the Apache License, Version 2.0 (the "License");
# you may not use this file except in compliance with the License.
# You may obtain a copy of the License at
#
# http://www.apache.org/licenses/LICENSE-2.0
#
# Unless required by applicable law or agreed to in writing, software
# distributed under the License is distributed on an "AS IS" BASIS,
# WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
# See the License for the specific language governing permissions and
# limitations under the License.
#

cmake_minimum_required(VERSION 3.4.1)
project(dive_replay)
set(CMAKE_CXX_STANDARD 17)

add_library(dive_replay SHARED dive_android_main.cpp)

target_link_libraries(dive_replay
    gfxrecon-replay
    gfxr_decode_ext_lib
    gfxrecon_application
    gfxrecon_decode
    gfxrecon_graphics
    gfxrecon_format
    gfxrecon_util
    native_app_glue
    android
    log
)

target_include_directories(dive_replay PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}
    ../../third_party/gfxreconstruct/tools/replay
    ../../third_party/gfxreconstruct/framework
)