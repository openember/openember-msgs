cmake_minimum_required(VERSION 3.16)

project(openember_msgs VERSION 0.1.0 LANGUAGES CXX)

option(OPENEMBER_MSGS_BUILD_CPP "Generate and build C++ protobuf bindings" OFF)
option(OPENEMBER_MSGS_BUILD_TESTS "Build protocol compatibility tests" OFF)
set(OPENEMBER_MSGS_NANOPB_ROOT "" CACHE PATH "Optional existing Nanopb source for C wire tests")

set(OPENEMBER_MSGS_PROTO_ROOT "${CMAKE_CURRENT_SOURCE_DIR}/proto")

set(OPENEMBER_MSGS_PROTO_FILES
    "openember/msgs/common/v1/common.proto"
    "openember/msgs/lifecycle/v1/lifecycle.proto"
    "openember/msgs/node/v1/node.proto"
    "openember/msgs/diagnostics/v1/diagnostics.proto"
    "openember/msgs/parameter/v1/parameter.proto"
    "openember/msgs/log/v1/log.proto"
    "openember/msgs/alerts/v1/alerts.proto"
    "openember/msgs/device/v1/device.proto"
    "openember/msgs/runtime/v1/runtime.proto"
    "openember/msgs/system/v1/system.proto"
    "openember/msgs/sensor/v1/sensor.proto"
    "openember/msgs/sensor/v1/imu.proto"
    "openember/msgs/sensor/v1/observation.proto"
    "openember/msgs/sensor/v1/camera.proto"
    "openember/msgs/sensor/v1/point_cloud.proto"
    "openember/msgs/sensor/v1/lidar.proto"
    "openember/msgs/actuator/v1/actuator.proto"
    "openember/msgs/actuator/v2/actuator.proto"
    "openember/msgs/motion/v1/motion.proto"
    "openember/msgs/power/v1/power.proto"
    "openember/msgs/geometry/v1/geometry.proto"
    "openember/msgs/host/v1/host.proto"
    "openember/msgs/network/v1/network.proto"
)

set(OPENEMBER_MSGS_PROTO_ABS_FILES)
foreach(_proto IN LISTS OPENEMBER_MSGS_PROTO_FILES)
    list(APPEND OPENEMBER_MSGS_PROTO_ABS_FILES "${OPENEMBER_MSGS_PROTO_ROOT}/${_proto}")
endforeach()

add_library(openember_msgs_proto INTERFACE)
target_include_directories(openember_msgs_proto INTERFACE
    "$<BUILD_INTERFACE:${OPENEMBER_MSGS_PROTO_ROOT}>"
    "$<INSTALL_INTERFACE:include>"
)

add_library(openember::msgs_proto ALIAS openember_msgs_proto)
set_property(TARGET openember_msgs_proto PROPERTY OPENEMBER_NETWORK_CONTRACT_VERSION 2)
set_property(TARGET openember_msgs_proto PROPERTY OPENEMBER_HOST_CONTRACT_VERSION 2)
set_property(TARGET openember_msgs_proto PROPERTY OPENEMBER_SYSTEM_CONTRACT_VERSION 1)
set_property(TARGET openember_msgs_proto PROPERTY OPENEMBER_MOTION_CONTRACT_VERSION 1)
set_property(TARGET openember_msgs_proto PROPERTY OPENEMBER_ALERTS_CONTRACT_VERSION 1)
set_property(TARGET openember_msgs_proto PROPERTY OPENEMBER_IMU_CONTRACT_VERSION 1)
set_property(TARGET openember_msgs_proto PROPERTY OPENEMBER_IMU_CONTROL_CONTRACT_VERSION 1)
set_property(TARGET openember_msgs_proto PROPERTY OPENEMBER_POWER_CONTROL_CONTRACT_VERSION 2)
set_property(TARGET openember_msgs_proto PROPERTY OPENEMBER_ACTUATOR_CONTRACT_VERSION 2)
set_property(TARGET openember_msgs_proto PROPERTY OPENEMBER_PROTO_ROOT "${OPENEMBER_MSGS_PROTO_ROOT}")

if(OPENEMBER_MSGS_BUILD_CPP)
    find_package(Protobuf REQUIRED)

    if(TARGET protobuf::protoc)
        set(_OPENEMBER_MSGS_PROTOC "$<TARGET_FILE:protobuf::protoc>")
    else()
        set(_OPENEMBER_MSGS_PROTOC "${Protobuf_PROTOC_EXECUTABLE}")
    endif()

    set(OPENEMBER_MSGS_GENERATED_SRCS)
    set(OPENEMBER_MSGS_GENERATED_HDRS)

    foreach(_proto IN LISTS OPENEMBER_MSGS_PROTO_FILES)
        get_filename_component(_proto_dir "${_proto}" DIRECTORY)
        get_filename_component(_proto_name "${_proto}" NAME_WE)
        set(_out_dir "${CMAKE_CURRENT_BINARY_DIR}/generated/${_proto_dir}")
        set(_out_src "${_out_dir}/${_proto_name}.pb.cc")
        set(_out_hdr "${_out_dir}/${_proto_name}.pb.h")

        file(MAKE_DIRECTORY "${_out_dir}")

        add_custom_command(
            OUTPUT "${_out_src}" "${_out_hdr}"
            COMMAND "${_OPENEMBER_MSGS_PROTOC}"
                    --cpp_out "${CMAKE_CURRENT_BINARY_DIR}/generated"
                    -I "${OPENEMBER_MSGS_PROTO_ROOT}"
                    "${_proto}"
            DEPENDS ${OPENEMBER_MSGS_PROTO_ABS_FILES}
            WORKING_DIRECTORY "${OPENEMBER_MSGS_PROTO_ROOT}"
            COMMENT "Running cpp protocol buffer compiler on ${_proto}"
            VERBATIM
        )

        list(APPEND OPENEMBER_MSGS_GENERATED_SRCS "${_out_src}")
        list(APPEND OPENEMBER_MSGS_GENERATED_HDRS "${_out_hdr}")
    endforeach()

    add_library(openember_msgs_cpp
        ${OPENEMBER_MSGS_GENERATED_SRCS}
        ${OPENEMBER_MSGS_GENERATED_HDRS}
    )
    add_library(openember::msgs_cpp ALIAS openember_msgs_cpp)
    set_property(TARGET openember_msgs_cpp PROPERTY OPENEMBER_NETWORK_CONTRACT_VERSION 2)
    set_property(TARGET openember_msgs_cpp PROPERTY OPENEMBER_HOST_CONTRACT_VERSION 2)
    set_property(TARGET openember_msgs_cpp PROPERTY OPENEMBER_SYSTEM_CONTRACT_VERSION 1)
    set_property(TARGET openember_msgs_cpp PROPERTY OPENEMBER_MOTION_CONTRACT_VERSION 1)
    set_property(TARGET openember_msgs_cpp PROPERTY OPENEMBER_ALERTS_CONTRACT_VERSION 1)
    set_property(TARGET openember_msgs_cpp PROPERTY OPENEMBER_IMU_CONTRACT_VERSION 1)
    set_property(TARGET openember_msgs_cpp PROPERTY OPENEMBER_IMU_CONTROL_CONTRACT_VERSION 1)
    set_property(TARGET openember_msgs_cpp PROPERTY OPENEMBER_POWER_CONTRACT_VERSION 1)
    set_property(TARGET openember_msgs_cpp PROPERTY OPENEMBER_POWER_CONTROL_CONTRACT_VERSION 2)
    set_property(TARGET openember_msgs_cpp PROPERTY OPENEMBER_ACTUATOR_CONTRACT_VERSION 2)

    target_link_libraries(openember_msgs_cpp PUBLIC protobuf::libprotobuf)
    target_include_directories(openember_msgs_cpp PUBLIC
        "$<BUILD_INTERFACE:${CMAKE_CURRENT_BINARY_DIR}/generated>"
        "$<INSTALL_INTERFACE:include>"
    )
endif()

if(OPENEMBER_MSGS_BUILD_TESTS)
    if(NOT OPENEMBER_MSGS_BUILD_CPP)
        message(FATAL_ERROR "Protocol tests require OPENEMBER_MSGS_BUILD_CPP=ON")
    endif()
    enable_testing()
    add_executable(sensor_observation_contract_test tests/sensor_observation_contract_test.cpp)
    target_compile_features(sensor_observation_contract_test PRIVATE cxx_std_17)
    target_link_libraries(sensor_observation_contract_test PRIVATE openember::msgs_cpp)
    add_test(NAME sensor_observation_wire_contract COMMAND sensor_observation_contract_test)
    add_executable(motion_contract_test tests/motion_contract_test.cpp)
    target_compile_features(motion_contract_test PRIVATE cxx_std_17)
    target_link_libraries(motion_contract_test PRIVATE openember::msgs_cpp)
    add_test(NAME motion_wire_contract COMMAND motion_contract_test
        "${CMAKE_CURRENT_SOURCE_DIR}/nanopb/openember_msgs.options")
    add_executable(alerts_contract_test tests/alerts_contract_test.cpp)
    target_compile_features(alerts_contract_test PRIVATE cxx_std_17)
    target_link_libraries(alerts_contract_test PRIVATE openember::msgs_cpp)
    add_test(NAME alerts_wire_contract COMMAND alerts_contract_test)
    add_executable(imu_contract_test tests/imu_contract_test.cpp)
    target_compile_features(imu_contract_test PRIVATE cxx_std_17)
    target_link_libraries(imu_contract_test PRIVATE openember::msgs_cpp)
    add_test(NAME imu_wire_compatibility COMMAND imu_contract_test)
    add_executable(power_contract_test tests/power_contract_test.cpp)
    target_compile_features(power_contract_test PRIVATE cxx_std_17)
    target_link_libraries(power_contract_test PRIVATE openember::msgs_cpp)
    add_test(NAME power_wire_compatibility COMMAND power_contract_test)
    add_executable(actuator_contract_test tests/actuator_contract_test.cpp)
    target_compile_features(actuator_contract_test PRIVATE cxx_std_17)
    target_link_libraries(actuator_contract_test PRIVATE openember::msgs_cpp)
    add_test(NAME actuator_wire_contract COMMAND actuator_contract_test
        "${CMAKE_CURRENT_SOURCE_DIR}/nanopb/openember_msgs.options")
    if(OPENEMBER_MSGS_NANOPB_ROOT)
        enable_language(C)
        find_package(Python3 COMPONENTS Interpreter REQUIRED)
        add_test(NAME imu_nanopb_roundtrip
            COMMAND "${Python3_EXECUTABLE}" "${CMAKE_CURRENT_SOURCE_DIR}/tests/nanopb_roundtrip.py"
                "${OPENEMBER_MSGS_NANOPB_ROOT}" "${CMAKE_CURRENT_SOURCE_DIR}" "${CMAKE_C_COMPILER}")
        set_tests_properties(imu_nanopb_roundtrip PROPERTIES TIMEOUT 30)
    endif()
endif()

include(GNUInstallDirs)

install(DIRECTORY "${OPENEMBER_MSGS_PROTO_ROOT}/"
    DESTINATION "${CMAKE_INSTALL_INCLUDEDIR}"
    FILES_MATCHING PATTERN "*.proto"
)

install(FILES "${CMAKE_CURRENT_SOURCE_DIR}/nanopb/openember_msgs.options"
    DESTINATION "${CMAKE_INSTALL_DATADIR}/openember-msgs/nanopb"
)

install(TARGETS openember_msgs_proto EXPORT openember_msgsTargets)
install(EXPORT openember_msgsTargets
    NAMESPACE openember::
    DESTINATION "${CMAKE_INSTALL_LIBDIR}/cmake/openember-msgs"
)
