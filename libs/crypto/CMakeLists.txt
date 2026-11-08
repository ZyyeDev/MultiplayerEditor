cmake_minimum_required(VERSION 3.21)
project(crypto)

set(CRYPTO_SOURCES
    PasswordHash.cpp
)

add_library(crypto STATIC ${CRYPTO_SOURCES})

target_include_directories(crypto
    PUBLIC
        ${CMAKE_CURRENT_SOURCE_DIR}/include
    PRIVATE
        ${CMAKE_CURRENT_SOURCE_DIR}
)

if(WIN32)
    target_link_libraries(crypto PUBLIC bcrypt)
endif()

if(MSVC)
    target_compile_options(crypto PRIVATE /W0)
else()
    target_compile_options(crypto PRIVATE -w)
endif()