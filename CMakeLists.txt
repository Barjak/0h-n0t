cmake_minimum_required(VERSION 3.5.1)
include(ExternalProject)
set(CMAKE_BUILD_TYPE Release)
set(PROOT ${CMAKE_CURRENT_LIST_DIR})

## Compile
####################
project(Board C)

set(CMAKE_POSITION_INDEPENDENT_CODE ON)
add_library(simple_solver STATIC c_board/simple_solver/Problem.c)

add_library(Board SHARED c_board/Board.c)
set_property(TARGET Board PROPERTY WINDOWS_EXPORT_ALL_SYMBOLS 1)
set_property(TARGET Board PROPERTY LIBRARY_OUTPUT_DIRECTORY ${PROOT})
target_link_libraries(Board simple_solver)

## Package
####################
set(final_prefix "0hn0t")

# Produce 0hn0t.love
add_custom_command(OUTPUT 0hn0t.love
                   COMMAND ${CMAKE_COMMAND} -E tar cf ${PROOT}/0hn0t.love --format=zip --files-from=${PROOT}/manifest
                   WORKING_DIRECTORY ${PROOT}/game/)
add_custom_target(love-file DEPENDS 0hn0t.love)

# Make for Windows
# assume x86_64 for now
if(${CMAKE_HOST_SYSTEM_NAME} STREQUAL "Windows")

        set(final_game_dir ${PROOT}/${final_prefix}-win64)
        ExternalProject_Add(love-win64
                            URL "https://bitbucket.org/rude/love/downloads/love-0.10.2-win64.zip"
                            CONFIGURE_COMMAND "" BUILD_COMMAND "" INSTALL_COMMAND ""
                            PREFIX .love_bin)
        add_custom_target(win64
                          COMMAND ${CMAKE_COMMAND} -E make_directory    ${final_game_dir}
                          COMMAND ${CMAKE_COMMAND} -E copy_directory .love_bin/src/love-win64/ ${final_game_dir}
                          COMMAND ${CMAKE_COMMAND} -E copy $<TARGET_FILE:Board> ${final_game_dir}
                          COMMAND copy /b "\"${final_game_dir}\\love.exe\"" + "\"${PROOT}\\0hn0t.love\"" "\"${final_game_dir}\\0hn0t.exe\""
                          COMMAND ${CMAKE_COMMAND} -E copy ${PROOT}/c_board/Board.h ${final_game_dir}
                          DEPENDS love-win64 love-file)
# Make for *nix
else()
        set(final_game_dir ${PROOT}/${final_prefix}-unix)
        add_custom_target(unix
                          COMMAND ${CMAKE_COMMAND} -E make_directory ${final_game_dir}
                          COMMAND ${CMAKE_COMMAND} -E copy $<TARGET_FILE:Board> ${PROOT}/c_board/Board.h ${PROOT}/0hn0t.love ${final_game_dir}
                  DEPENDS love-file)
endif()
