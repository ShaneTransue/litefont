# Three examples: rasterize renders text to an image file on the CPU; the
# freeglut example draws text in an old-style OpenGL program (or, with --core,
# in an OpenGL 3.3 core profile context); the Direct3D 11 example draws the
# same scene in a plain Win32 program. The freeglut example is built only when
# GLUT is found (freeglut through vcpkg, a system package or a framework), and
# the Direct3D 11 one only on Windows. The two windowed examples share the
# scene they draw (shared/ExampleScene).
#
# The fonts in LITEFONT_EXAMPLE_FONTS (examples/fonts, open-source Noto fonts,
# by default) are copied next to the windowed examples as fonts/, so they run
# without arguments, from a file manager for instance. The install rules here
# are the examples component, which only the dist target installs.

set(examples litefont_rasterize)

function(litefont_copy_example_fonts target)
	if(LITEFONT_EXAMPLE_FONTS)
		get_filename_component(fonts "${LITEFONT_EXAMPLE_FONTS}" ABSOLUTE BASE_DIR "${PROJECT_SOURCE_DIR}")
		add_custom_command(TARGET ${target} POST_BUILD
			COMMAND ${CMAKE_COMMAND} -E copy_directory "${fonts}" "$<TARGET_FILE_DIR:${target}>/fonts"
			COMMENT "Copying the example fonts next to ${target}")
	endif()
endfunction()

add_executable(litefont_rasterize rasterize/Rasterize.cpp)
target_link_libraries(litefont_rasterize PRIVATE litefont::litefont)
litefont_compile_options(litefont_rasterize)

find_package(GLUT)
if(GLUT_FOUND AND LITEFONT_OPENGL)
	add_executable(litefont_freeglut freeglut/main.cpp shared/ExampleScene.h shared/ExampleScene.cpp)
	target_include_directories(litefont_freeglut PRIVATE shared)
	target_link_libraries(litefont_freeglut PRIVATE litefont::litefont GLUT::GLUT OpenGL::GL)
	litefont_compile_options(litefont_freeglut)
	litefont_copy_example_fonts(litefont_freeglut)
	if(WIN32)
		# freeglut includes windows.h, whose min and max macros would break std::max.
		target_compile_definitions(litefont_freeglut PRIVATE NOMINMAX)
	endif()
	list(APPEND examples litefont_freeglut)
else()
	message(STATUS "litefont: GLUT not found, so the freeglut example is not built")
endif()

if(LITEFONT_D3D11)
	add_executable(litefont_d3d11 d3d11/main.cpp shared/ExampleScene.h shared/ExampleScene.cpp)
	target_include_directories(litefont_d3d11 PRIVATE shared)
	target_link_libraries(litefont_d3d11 PRIVATE litefont::litefont d3d11 d3dcompiler)
	litefont_compile_options(litefont_d3d11)
	litefont_copy_example_fonts(litefont_d3d11)
	list(APPEND examples litefont_d3d11)
endif()

# The examples component: the programs, the DLLs next to them (freeglut's,
# and litefont's in a shared build) and the fonts with their license.
install(TARGETS ${examples} RUNTIME DESTINATION . COMPONENT examples EXCLUDE_FROM_ALL)
install(DIRECTORY "$<TARGET_FILE_DIR:litefont_rasterize>/" DESTINATION . COMPONENT examples EXCLUDE_FROM_ALL
	FILES_MATCHING PATTERN "*.dll" PATTERN "fonts" EXCLUDE)
if(LITEFONT_EXAMPLE_FONTS)
	get_filename_component(fonts "${LITEFONT_EXAMPLE_FONTS}" ABSOLUTE BASE_DIR "${PROJECT_SOURCE_DIR}")
	install(DIRECTORY "${fonts}/" DESTINATION fonts COMPONENT examples EXCLUDE_FROM_ALL)
endif()
set(LITEFONT_EXAMPLE_TARGETS ${examples} PARENT_SCOPE)
