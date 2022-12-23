project(Powar)
cmake_minimum_required(VERSION 3.13)

include_directories(${PROJECT_SOURCE_DIR})
add_executable(powar main.c accel.c eeprom.c interrupts.c lcd.c portb.c rtc.c ssu.c)

if (DEFINED EMSCRIPTEN)
    set_target_properties(powar
        PROPERTIES
        COMPILE_FLAGS
        "-s USE_SDL=2 -fsanitize=undefined"
    )
    set_target_properties(powar
        PROPERTIES
        LINK_FLAGS
        "-s USE_SDL=2 -s STACK_SIZE=512mb -s INITIAL_MEMORY=1024mb -fsanitize=undefined -s EXPORT_ES6 -s EXPORTED_RUNTIME_METHODS=['FS']"
    )
    set_target_properties(powar
        PROPERTIES
        RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/static"
    )
else()
    find_package(SDL2 CONFIG REQUIRED)
    if (DEFINED VCPKG_TARGET_TRIPLET)
        target_link_libraries(powar
            PRIVATE
            $<TARGET_NAME_IF_EXISTS:SDL2::SDL2main>
            $<IF:$<TARGET_EXISTS:SDL2::SDL2>,SDL2::SDL2,SDL2::SDL2-static>
        )
    else()
        include_directories(${SDL2_INCLUDE_DIRS})
        target_link_libraries(powar ${SDL2_LIBRARIES})
    endif()
endif()