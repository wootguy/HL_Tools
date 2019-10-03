if (EMSCRIPTEN OR RASPI)
	add_subdirectory( hlms )
else()
	add_subdirectory( hlmv )
	add_subdirectory( spriteviewer )
endif()

