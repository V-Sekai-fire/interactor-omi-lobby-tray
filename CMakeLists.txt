cmake_minimum_required(VERSION 3.24)
project(omi_lobby_tray LANGUAGES C CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# Off for a quick test-only build: the list logic and its tests need no SDL.
option(OMI_LOBBY_TRAY_APP "Build the tray program (fetches SDL3)" ON)

enable_testing()

add_library(lobby STATIC src/Lobby.cpp src/Json.cpp src/Overte.cpp)
target_include_directories(lobby PUBLIC src)

add_executable(lobby-tests tests/lobby.cpp)
target_link_libraries(lobby-tests PRIVATE lobby)
target_compile_definitions(lobby-tests PRIVATE LOBBY_FIXTURE="${CMAKE_CURRENT_SOURCE_DIR}/tests/servers.json")
foreach(name IN ITEMS lobby.parses-the-lobby-answer lobby.refuses-what-is-not-a-list
                      lobby.down-and-other-platforms-left-out lobby.labels-show-players lobby.join-links
                      lobby.lobby-choice overte.hifi-command overte.finds-interface)
    add_test(NAME ${name} COMMAND lobby-tests ${name})
endforeach()
# A name the runner does not know must fail, so a renamed test cannot pass by running nothing.
add_test(NAME lobby.control-unknown-test-fails COMMAND lobby-tests lobby.no-such-test)
set_tests_properties(lobby.control-unknown-test-fails PROPERTIES WILL_FAIL TRUE)

if(NOT OMI_LOBBY_TRAY_APP)
    return()
endif()

# SDL3 at the release xr-pilot's tray builds against (3.4), static so the program is one file.
include(FetchContent)
set(SDL_SHARED OFF CACHE BOOL "" FORCE)
set(SDL_STATIC ON CACHE BOOL "" FORCE)
set(SDL_TEST_LIBRARY OFF CACHE BOOL "" FORCE)
set(SDL_AUDIO OFF CACHE BOOL "" FORCE)
set(SDL_CAMERA OFF CACHE BOOL "" FORCE)
set(SDL_JOYSTICK OFF CACHE BOOL "" FORCE)
set(SDL_HAPTIC OFF CACHE BOOL "" FORCE)
set(SDL_HIDAPI OFF CACHE BOOL "" FORCE)
set(SDL_SENSOR OFF CACHE BOOL "" FORCE)
set(SDL_GPU OFF CACHE BOOL "" FORCE)
set(SDL_RENDER OFF CACHE BOOL "" FORCE)
set(SDL_POWER OFF CACHE BOOL "" FORCE)
FetchContent_Declare(SDL3
    GIT_REPOSITORY https://github.com/libsdl-org/SDL.git
    GIT_TAG d9d5536704d585616d4db3c8ba3c4ff6fc2757e1 # release-3.4.8
    GIT_SHALLOW FALSE)
FetchContent_MakeAvailable(SDL3)

# A GUI program, so a start from Explorer or the Start menu opens no console.
add_executable(omi-lobby-tray WIN32 src/main.cpp src/Fetch.cpp)
target_link_libraries(omi-lobby-tray PRIVATE lobby SDL3::SDL3-static)
if(WIN32)
    target_link_libraries(omi-lobby-tray PRIVATE winhttp)
endif()
add_custom_command(TARGET omi-lobby-tray POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E copy_directory ${CMAKE_CURRENT_SOURCE_DIR}/resources $<TARGET_FILE_DIR:omi-lobby-tray>/resources)

install(TARGETS omi-lobby-tray RUNTIME DESTINATION .)
install(DIRECTORY resources/ DESTINATION resources)
