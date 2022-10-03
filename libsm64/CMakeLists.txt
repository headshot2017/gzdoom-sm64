cmake_minimum_required(VERSION 2.8.7)

make_release_only()

set(CMAKE_MODULE_PATH ${CMAKE_CURRENT_SOURCE_DIR}/cmake)
set(CMAKE_C_FLAGS_DEBUG -g)
set(CMAKE_CXX_FLAGS_DEBUG -g)
set(CMAKE_C_FLAGS_RELEASE -O1)
set(CMAKE_CXX_FLAGS_RELEASE -O1)
set(CMAKE_C_FLAGS_RELWITHDEBINFO "-O1 -g")
set(CMAKE_CXX_FLAGS_RELWITHDEBINFO "-O1 -g")

file(GLOB SOURCES
	src/*.c
	src/sha1/*.c
	src/decomp/*.c
	src/decomp/engine/*.c
	src/decomp/game/*.c
	src/decomp/tools/*.c
	src/decomp/audio/*.c
	src/decomp/pc/*.c
	src/decomp/pc/audio/*.cpp
	src/decomp/pc/audio/*.c
)

find_package(PythonInterp 3 REQUIRED)

if (NOT EXISTS ${CMAKE_CURRENT_SOURCE_DIR}/src/decomp/mario)
	file(MAKE_DIRECTORY "${CMAKE_CURRENT_SOURCE_DIR}/src/decomp/mario")
	execute_process(
		COMMAND ${PYTHON_EXECUTABLE} ${CMAKE_CURRENT_SOURCE_DIR}/import-mario-geo.py
		WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}
		COMMENT "Download mario geo.inc and model.inc"
	)
endif()

file(GLOB MARIO_SOURCES src/decomp/mario/*.c)

add_library(sm64 SHARED ${SOURCES} ${MARIO_SOURCES})

include_directories(src/decomp/include)
include_directories(src/decomp/mario)

target_compile_options(sm64 PRIVATE -Wall)
target_compile_definitions(sm64 PRIVATE SM64_LIB_EXPORT VERSION_US NO_SEGMENTED_MEMORY GBI_FLOATS)

if (WIN32)
	target_link_options(sm64 PRIVATE -mwindows -static -lole32 -lstdc++)

elseif (APPLE)
	find_package(SDL2 REQUIRED)
	target_compile_definitions(sm64 PRIVATE USE_SDL2)
	target_link_libraries(sm64 SDL2)

elseif (UNIX)
	find_package(ALSA REQUIRED)
	find_package(PulseAudio REQUIRED)
	target_compile_options(sm64 PRIVATE -fPIC)
	target_compile_definitions(sm64 PRIVATE USE_ALSA USE_PULSEAUDIO)
	target_link_options(sm64 PRIVATE -fPIC)
	target_link_libraries(sm64 asound pulse)

endif()

target_link_libraries( sm64 )
