find_program(COMPILER glslangValidator)

function(add_shader TNAME OUTDIR SRC)
	set(SRCFILE ${CMAKE_CURRENT_SOURCE_DIR}/${SRC})
	set(OUTFILE ${OUTDIR}/${TNAME}.spv)

	add_custom_command(OUTPUT ${OUTFILE}
		MAIN_DEPENDENCY ${SRCFILE}
		COMMAND ${COMPILER} -V ${SRCFILE} -o ${OUTFILE}
		COMMENT "Compiling shader: ${SRC}"
	)

	add_custom_target(${TNAME} ALL DEPENDS ${OUTFILE})
endfunction()

add_shader(vertex_shader ${CMAKE_BINARY_DIR} shader.vert)
add_shader(fragment_shader ${CMAKE_BINARY_DIR} shader.frag)
