if (EMSCRIPTEN)
	add_subdirectory( hlms )
else()
	add_subdirectory( hlmv )
	add_subdirectory( spriteviewer )
endif()

