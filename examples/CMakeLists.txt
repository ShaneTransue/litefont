# Two examples: rasterize renders text to an image file on the CPU, and the
# freeglut example draws text in an old-style OpenGL program. The second one
# is built only when GLUT is found (freeglut through vcpkg, a system package
# or a framework).

add_executable(litefont_rasterize rasterize/Rasterize.cpp)
target_link_libraries(litefont_rasterize PRIVATE litefont::litefont)
litefont_compile_options(litefont_rasterize)

find_package(GLUT)
if(GLUT_FOUND AND LITEFONT_OPENGL)
	add_executable(litefont_freeglut freeglut/main.cpp)
	target_link_libraries(litefont_freeglut PRIVATE litefont::litefont GLUT::GLUT OpenGL::GL)
	litefont_compile_options(litefont_freeglut)
	if(WIN32)
		# freeglut includes windows.h, whose min and max macros would break std::max.
		target_compile_definitions(litefont_freeglut PRIVATE NOMINMAX)
	endif()
else()
	message(STATUS "litefont: GLUT not found, so the freeglut example is not built")
endif()
