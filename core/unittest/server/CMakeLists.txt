#-------------------------------------------------------------------------------
# Copyright (C) 2019-2020 Zilliz. All rights reserved.
#
# Licensed under the Apache License, Version 2.0 (the "License"); you may not use this file except in compliance
# with the License. You may obtain a copy of the License at
#
# http://www.apache.org/licenses/LICENSE-2.0
#
# Unless required by applicable law or agreed to in writing, software distributed under the License
# is distributed on an "AS IS" BASIS, WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express
# or implied. See the License for the specific language governing permissions and limitations under the License.
#-------------------------------------------------------------------------------

set(test_files
        ${CMAKE_CURRENT_SOURCE_DIR}/test_cache.cpp
        ${CMAKE_CURRENT_SOURCE_DIR}/test_check.cpp
        ${CMAKE_CURRENT_SOURCE_DIR}/test_config.cpp
        ${CMAKE_CURRENT_SOURCE_DIR}/test_rpc.cpp
        ${CMAKE_CURRENT_SOURCE_DIR}/test_web.cpp
        ${CMAKE_CURRENT_SOURCE_DIR}/test_util.cpp
        ${CMAKE_CURRENT_SOURCE_DIR}/utils.cpp)

include_directories("${CUDA_TOOLKIT_ROOT_DIR}/include")
link_directories("${CUDA_TOOLKIT_ROOT_DIR}/lib64")

set(util_files
        ${MILVUS_ENGINE_SRC}/utils/StringHelpFunctions.cpp
        ${MILVUS_ENGINE_SRC}/utils/LogUtil.cpp
        ${MILVUS_ENGINE_SRC}/utils/SignalHandler.cpp)

set(server_test_files
        ${common_files}
        ${server_files}
        ${server_init_files}
        ${grpc_server_files}
        ${server_delivery_files}
        ${web_server_files}
        ${util_files}
        ${entry_file}
        ${test_files}
        )

add_executable(test_server ${server_test_files})



target_link_libraries(test_server
        knowhere
        metrics
        stdc++
        # ${grpc_lib}
        ${unittest_libs}
        oatpp
        )

install(TARGETS test_server DESTINATION unittest)
