PKG_CHECK_MODULES(COMMON_DEP
    dlog
    REQUIRED
    )

SET(COMMON_PATH ${PROJECT_SOURCE_DIR}/src/server)

SET(COMMON_SOURCES
    ${COMMON_PATH}/common/protocols.cpp
    ${COMMON_PATH}/common/message-buffer.cpp
    ${COMMON_PATH}/common/smack-check.cpp
    ${COMMON_PATH}/dpl/log/src/abstract_log_provider.cpp
    ${COMMON_PATH}/dpl/log/src/dlog_log_provider.cpp
    ${COMMON_PATH}/dpl/log/src/log.cpp
    ${COMMON_PATH}/dpl/log/src/old_style_log_provider.cpp
    ${COMMON_PATH}/dpl/log/src/audit-smack-log.cpp
    ${COMMON_PATH}/dpl/core/src/assert.cpp
    ${COMMON_PATH}/dpl/core/src/binary_queue.cpp
    ${COMMON_PATH}/dpl/core/src/colors.cpp
    ${COMMON_PATH}/dpl/core/src/exception.cpp
    ${COMMON_PATH}/dpl/core/src/noncopyable.cpp
    ${COMMON_PATH}/dpl/core/src/serialization.cpp
    ${COMMON_PATH}/dpl/core/src/singleton.cpp
    )

INCLUDE_DIRECTORIES(SYSTEM
    ${COMMON_DEP_INCLUDE_DIRS}
    )

INCLUDE_DIRECTORIES(
    ${COMMON_PATH}/common
    ${COMMON_PATH}/dpl/core/include
    ${COMMON_PATH}/dpl/log/include
    )

ADD_LIBRARY(${TARGET_SERVER_COMMON} SHARED ${COMMON_SOURCES})

SET_TARGET_PROPERTIES(
    ${TARGET_SERVER_COMMON}
    PROPERTIES
        COMPILE_FLAGS "-D_GNU_SOURCE -fPIC -fvisibility=default"
        SOVERSION 1.0.0
        VERSION 1.0.0
    )

TARGET_LINK_LIBRARIES(${TARGET_SERVER_COMMON}
    ${COMMON_DEP_LIBRARIES}
    )

################################################################################

INSTALL(TARGETS ${TARGET_SERVER_COMMON} DESTINATION lib)
