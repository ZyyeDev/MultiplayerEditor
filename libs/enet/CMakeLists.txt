cmake_minimum_required(VERSION 3.21)
project(enet)

set(ENET_SOURCES
    compress.c
    callbacks.c
    host.c
    list.c
    packet.c
    peer.c
    protocol.c
)

if(WIN32)
    list(APPEND ENET_SOURCES win32.c)
else()
    list(APPEND ENET_SOURCES unix.c)
endif()

add_library(enet STATIC ${ENET_SOURCES})

target_include_directories(enet 
    PUBLIC 
        ${CMAKE_CURRENT_SOURCE_DIR}/include
    PRIVATE 
        ${CMAKE_CURRENT_SOURCE_DIR}
        ${CMAKE_CURRENT_SOURCE_DIR}/include
)

target_compile_definitions(enet PRIVATE ENET_BUILDING_LIB)

if(WIN32)
    target_link_libraries(enet PUBLIC ws2_32 winmm ws2_32)
    target_compile_definitions(enet PUBLIC -DHAS_SOCKLEN_T)
else()
    target_compile_definitions(enet PRIVATE HAS_FCNTL HAS_POLL HAS_GETHOSTBYNAME HAS_GETHOSTBYADDR HAS_SOCKLEN_T)
endif()

if(MSVC)
    target_compile_options(enet PRIVATE /W0)
else()
    target_compile_options(enet PRIVATE -w)
endif()