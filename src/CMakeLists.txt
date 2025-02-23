add_library(vma framework/vma.cpp)

add_library(framework
	framework/shader.cpp
	framework/query.cpp
)

if ("${CMAKE_BUILD_TYPE}" STREQUAL "Debug")
	target_compile_options(framework PRIVATE -Wall -Wextra -Wpedantic -O0 -g)
	message(STATUS "Debugging enabled")
endif()

target_link_libraries(framework PUBLIC
	${Vulkan_LIBRARIES}
	-lcfgtk_parser
	-lcfgtk_lexer
	-lglfw
	vma
)

add_executable(sigil main.cpp initialize.cpp cli.cpp update.cpp render.cpp)
target_link_libraries(sigil framework)
set_target_properties(sigil PROPERTIES
	RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}
)

file(CREATE_LINK ${CMAKE_BINARY_DIR}/sigil ${CMAKE_BINARY_DIR}/run SYMBOLIC)

add_subdirectory(glsl)
