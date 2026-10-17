cmake_minimum_required(VERSION 3.20)
project(litefont VERSION 1.0.0 DESCRIPTION "A small font engine with OpenGL 1.1 and 3.3 renderers" LANGUAGES CXX)
include(GNUInstallDirs)

# Built on its own, litefont also builds its tests; added to another project
# with add_subdirectory, only the library is built unless asked.
if(CMAKE_SOURCE_DIR STREQUAL PROJECT_SOURCE_DIR)
	set(LITEFONT_TOP_LEVEL ON)
else()
	set(LITEFONT_TOP_LEVEL OFF)
endif()

option(LITEFONT_OPENGL "Build the OpenGL renderers (links the system OpenGL library)" ON)
option(LITEFONT_BUILD_TESTS "Build the tests" ${LITEFONT_TOP_LEVEL})
option(LITEFONT_BUILD_EXAMPLES "Build the examples (the freeglut example needs GLUT)" OFF)
option(LITEFONT_INSTALL "Generate the install rules" ${LITEFONT_TOP_LEVEL})
option(LITEFONT_WARNINGS_AS_ERRORS "Treat compiler warnings as errors" OFF)
option(LITEFONT_SANITIZE "Build with AddressSanitizer" OFF)
set(LITEFONT_TEST_FONTS "" CACHE PATH "A folder of real fonts for the tests that need them (optional)")

# A shared build on Windows puts the programs next to the DLL, so they run.
if(LITEFONT_TOP_LEVEL AND BUILD_SHARED_LIBS AND WIN32 AND NOT CMAKE_RUNTIME_OUTPUT_DIRECTORY)
	set(CMAKE_RUNTIME_OUTPUT_DIRECTORY ${PROJECT_BINARY_DIR}/bin)
endif()

set(LITEFONT_SOURCES
	source/AtlasPacker.cpp
	source/Blur.cpp
	source/ByteReader.cpp
	source/ByteWriter.cpp
	source/CffTable.cpp
	source/Dilation.cpp
	source/File.cpp
	source/Font.cpp
	source/FontCatalog.cpp
	source/FontFamily.cpp
	source/GlyphRasterizer.cpp
	source/GlyphSource.cpp
	source/GposTable.cpp
	source/Hash.cpp
	source/Hinter.cpp
	source/Image.cpp
	source/Outline.cpp
	source/Rasterizer.cpp
	source/SoftwareDevice.cpp
	source/SpriteAtlas.cpp
	source/Text.cpp
	source/TextCursor.cpp
	source/TextLayout.cpp
	source/TextRenderer.cpp
	source/Utf8.cpp)
if(LITEFONT_OPENGL)
	list(APPEND LITEFONT_SOURCES source/OpenGLFunctions.cpp source/OpenGLDevice.cpp source/OpenGL11Device.cpp
		source/OpenGL33Device.cpp source/OpenGLRenderer.cpp)
endif()

add_library(litefont ${LITEFONT_SOURCES})
add_library(litefont::litefont ALIAS litefont)
target_compile_features(litefont PUBLIC cxx_std_20)
set_target_properties(litefont PROPERTIES CXX_EXTENSIONS OFF VERSION ${PROJECT_VERSION} SOVERSION ${PROJECT_VERSION_MAJOR})
target_include_directories(litefont PUBLIC
	$<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
	$<INSTALL_INTERFACE:${CMAKE_INSTALL_INCLUDEDIR}>)
if(BUILD_SHARED_LIBS AND WIN32)
	# No export annotations in the sources: export every symbol instead.
	set_target_properties(litefont PROPERTIES WINDOWS_EXPORT_ALL_SYMBOLS ON)
endif()

# One place for compiler flags, used by the library, tests and examples.
# Floating-point contraction stays off, so the software device gives the same
# pixels on every compiler.
function(litefont_compile_options target)
	if(MSVC)
		# C4458 (a parameter hides a member) is off on purpose: parameters share
		# their member's name, and members are always reached through this->.
		target_compile_options(${target} PRIVATE /W4 /wd4458 /permissive- /utf-8 /Zc:__cplusplus /fp:precise
			/external:anglebrackets /external:W0)
		if(LITEFONT_WARNINGS_AS_ERRORS)
			target_compile_options(${target} PRIVATE /WX)
		endif()
	else()
		target_compile_options(${target} PRIVATE -Wall -Wextra -Wpedantic -ffp-contract=off)
		if(LITEFONT_WARNINGS_AS_ERRORS)
			target_compile_options(${target} PRIVATE -Werror)
		endif()
	endif()
endfunction()
litefont_compile_options(litefont)
if(LITEFONT_SANITIZE)
	if(MSVC)
		target_compile_options(litefont PUBLIC /fsanitize=address /Zi)
	else()
		target_compile_options(litefont PUBLIC -fsanitize=address,undefined)
		target_link_options(litefont PUBLIC -fsanitize=address,undefined)
	endif()
endif()

if(LITEFONT_OPENGL)
	find_package(OpenGL REQUIRED)
	# OpenGL functions are found at run time (source/OpenGLFunctions.cpp), so
	# no OpenGL header is compiled; dlsym needs libdl where it is separate.
	target_link_libraries(litefont PRIVATE OpenGL::GL ${CMAKE_DL_LIBS})
endif()

if(LITEFONT_BUILD_TESTS)
	enable_testing()
	add_subdirectory(tests)
endif()
if(LITEFONT_BUILD_EXAMPLES)
	add_subdirectory(examples)
endif()

if(LITEFONT_INSTALL)
	include(CMakePackageConfigHelpers)
	install(TARGETS litefont EXPORT litefontTargets
		ARCHIVE DESTINATION ${CMAKE_INSTALL_LIBDIR}
		LIBRARY DESTINATION ${CMAKE_INSTALL_LIBDIR}
		RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR})
	install(DIRECTORY include/litefont DESTINATION ${CMAKE_INSTALL_INCLUDEDIR})
	install(EXPORT litefontTargets NAMESPACE litefont:: DESTINATION ${CMAKE_INSTALL_LIBDIR}/cmake/litefont)
	configure_package_config_file(cmake/litefontConfig.cmake.in ${CMAKE_CURRENT_BINARY_DIR}/litefontConfig.cmake
		INSTALL_DESTINATION ${CMAKE_INSTALL_LIBDIR}/cmake/litefont)
	write_basic_package_version_file(${CMAKE_CURRENT_BINARY_DIR}/litefontConfigVersion.cmake COMPATIBILITY SameMajorVersion)
	install(FILES ${CMAKE_CURRENT_BINARY_DIR}/litefontConfig.cmake ${CMAKE_CURRENT_BINARY_DIR}/litefontConfigVersion.cmake
		DESTINATION ${CMAKE_INSTALL_LIBDIR}/cmake/litefont)
	install(FILES LICENSE README.md DESTINATION ${CMAKE_INSTALL_DATADIR}/litefont)
endif()
