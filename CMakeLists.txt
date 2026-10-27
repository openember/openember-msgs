cmake_minimum_required(VERSION 3.16)

project(openember_msgs VERSION 0.1.0 LANGUAGES CXX)

option(OPENEMBER_MSGS_BUILD_CPP "Generate and build C++ protobuf bindings" OFF)

set(OPENEMBER_MSGS_PROTO_ROOT "${CMAKE_CURRENT_SOURCE_DIR}/proto")

set(OPENEMBER_MSGS_PROTO_FILES
    "openember/msgs/common/v1/common.proto"
    "openember/msgs/lifecycle/v1/lifecycle.proto"
    "openember/msgs/node/v1/node.proto"
    "openember/msgs/diagnostics/v1/diagnostics.proto"
    "openember/msgs/parameter/v1/parameter.proto"
    "openember/msgs/log/v1/log.proto"
    "openember/msgs/device/v1/device.proto"
    "openember/msgs/runtime/v1/runtime.proto"
    "openember/msgs/sensor/v1/sensor.proto"
    "openember/msgs/actuator/v1/actuator.proto"
    "openember/msgs/power/v1/power.proto"
    "openember/msgs/geometry/v1/geometry.proto"
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

    target_link_libraries(openember_msgs_cpp PUBLIC protobuf::libprotobuf)
    target_include_directories(openember_msgs_cpp PUBLIC
        "$<BUILD_INTERFACE:${CMAKE_CURRENT_BINARY_DIR}/generated>"
        "$<INSTALL_INTERFACE:include>"
    )
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
