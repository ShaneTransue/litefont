# The tests are compiled straight into one executable, so the self-registering
# test objects are never dropped by the linker. Run it from the project folder
# (CTest does) so the golden images are found; pass --fonts <folder> to run
# the tests that need real fonts, which otherwise leave a note and skip.

add_executable(litefont_tests
	TestKit.h
	TestKit.cpp
	TestMain.cpp
	RecordingDevice.h
	RecordingDevice.cpp
	TestKitTests.cpp
	ColorTests.cpp
	HashTests.cpp
	MathTests.cpp
	ByteReaderTests.cpp
	ByteWriterTests.cpp
	FileTests.cpp
	ImageTests.cpp
	BlurTests.cpp
	AtlasTests.cpp
	DeviceConformance.h
	DeviceConformance.cpp
	GraphicsTests.cpp
	OutlineTests.cpp
	RasterizerTests.cpp
	DilationTests.cpp
	TestFontBuilder.h
	TestFontBuilder.cpp
	BlockFont.h
	FontTests.cpp
	FontFaceTests.cpp
	FontFuzzTests.cpp
	FontOracleTests.cpp
	CffTableTests.cpp
	GposTableTests.cpp
	HinterTests.cpp
	FontFamilyTests.cpp
	FontCatalogTests.cpp
	Utf8Tests.cpp
	TextTests.cpp
	TextLayoutTests.cpp
	TextCursorTests.cpp
	GlyphRasterizerTests.cpp
	TextRendererTests.cpp
	TextPainterTests.cpp)
target_link_libraries(litefont_tests PRIVATE litefont::litefont)
litefont_compile_options(litefont_tests)

if(LITEFONT_TEST_FONTS)
	add_test(NAME litefont_tests COMMAND litefont_tests --fonts ${LITEFONT_TEST_FONTS} WORKING_DIRECTORY ${PROJECT_SOURCE_DIR})
else()
	add_test(NAME litefont_tests COMMAND litefont_tests WORKING_DIRECTORY ${PROJECT_SOURCE_DIR})
endif()
