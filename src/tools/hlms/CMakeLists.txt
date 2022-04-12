
set( TARGET_NAME hlms )

#Add in the shared sources
add_sources( ${SHARED_SRCS} )

#Add sources
add_sources(
	lodepng.cpp
	osdemo.cpp
)

add_subdirectory( ../../engine/renderer/studiomodel ${CMAKE_CURRENT_BINARY_DIR}/engine/renderer/studiomodel )
add_subdirectory( ../../engine/shared ${CMAKE_CURRENT_BINARY_DIR}/engine/shared )
add_subdirectory( ../../lib ${CMAKE_CURRENT_BINARY_DIR}/lib )
add_subdirectory( ../../graphics ${CMAKE_CURRENT_BINARY_DIR}/graphics )

preprocess_sources()

find_package( OpenGL REQUIRED )

if( NOT OPENGL_FOUND )
	MESSAGE( FATAL_ERROR "Could not locate OpenGL library" )
endif()

add_definitions(-DIS_LITTLE_ENDIAN=1)

add_executable( ${TARGET_NAME} ${PREP_SRCS} )

check_winxp_support( ${TARGET_NAME} )

target_include_directories( ${TARGET_NAME} SYSTEM BEFORE PRIVATE
	${OPENGL_INCLUDE_DIR}
	${SHARED_INCLUDEPATHS}
)

if( WIN32 )
	find_library( GLEW glew32 PATHS ${CMAKE_SOURCE_DIR}/external/GLEW/lib )
elseif()
	find_library( GLEW libGLEW.so.2.0.0 PATHS ${CMAKE_SOURCE_DIR}/external/GLEW/lib )
endif()

if(EMSCRIPTEN)
	set(CMAKE_EXECUTABLE_SUFFIX ".html")
	
	set(SHELL_FILE ${CMAKE_CURRENT_SOURCE_DIR}/shell.html)
	set(LNK_FLAGS "-s TOTAL_MEMORY=67108864 \
		-s LEGACY_GL_EMULATION=1 \
		-s GL_FFP_ONLY=1 \
		-s ASYNCIFY=1 \
		-s EXTRA_EXPORTED_RUNTIME_METHODS=[\"ccall\"] \
		--shell-file ${SHELL_FILE}")
	set_target_properties(${TARGET_NAME} PROPERTIES LINK_FLAGS ${LNK_FLAGS})
	
	target_link_directories(${TARGET_NAME} PUBLIC ${CMAKE_CURRENT_BINARY_DIR}/../../../bin)
	target_link_libraries(${TARGET_NAME} -lGL -lHLStdLib -lHLCore)
else()
	target_link_libraries( ${TARGET_NAME}
		HLCore
		Keyvalues
		#${GLEW}
		#${OPENGL_LIBRARIES}
		${SHARED_DEPENDENCIES}
		-lOSMesa
		-lGLU
		-lGL
		-lm
	)
endif()


#Create filters
create_source_groups( "${CMAKE_CURRENT_SOURCE_DIR}" )

clear_sources()

if( WIN32 )
	copy_dependencies( ${TARGET_NAME} external/GLEW/lib glew32.dll )
else()
	#copy_dependencies( ${TARGET_NAME} external/GLEW/lib libGLEW.so.2.0.0 )
endif()

set_property( DIRECTORY ${CMAKE_SOURCE_DIR} PROPERTY VS_STARTUP_PROJECT ${TARGET_NAME} )
