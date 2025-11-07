cmake_minimum_required(VERSION 3.21)
project(enet)

set(ENET_SOURCES
    callbacks.c
    compress.c
    host.c
    list.c
    packet.c
    peer.c
    protocol.c
    win32.c
)

add_library(enet STATIC ${ENET_SOURCES})

target_include_directories(enet PUBLIC 
    ${CMAKE_CURRENT_SOURCE_DIR}/include
)

if(WIN32)
    target_link_libraries(enet PUBLIC ws2_32 winmm ws2_32)
    target_compile_definitions(enet PUBLIC -DHAS_SOCKLEN_T)
endif()

if(MSVC)
    target_compile_options(enet PRIVATE /W0)
else()
    target_compile_options(enet PRIVATE -w)
endif()