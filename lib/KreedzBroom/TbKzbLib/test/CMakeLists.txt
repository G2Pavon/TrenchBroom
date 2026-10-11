add_executable(TbKzbLibTest)
EMBED_UTF8_MANIFEST(TbKzbLibTest)

target_sources(TbKzbLibTest
  PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/src/RunAllTests.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/src/tst_KreedzBroomPreferencePane.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/src/tst_PlayModeRawMouse.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/src/tst_PlayModeShowPre.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/src/tst_PlayModeTimeline.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/src/tst_PlayModeWorld.cpp
)

target_link_libraries(TbKzbLibTest PRIVATE
  CompilerConfig
  Catch2::Catch2WithMain
  Qt6::Test
  Qt6::Widgets
  TbBaseTestUtilsLib
  TbFsTestUtilsLib
  TbKzbLib
  TbMdlTestUtilsLib
  TbUiTestUtilsLib
)

if(TB_XCB_XINPUT_FOUND)
  target_link_libraries(TbKzbLibTest PRIVATE PkgConfig::TB_XCB_XINPUT)
  target_compile_definitions(TbKzbLibTest PRIVATE TB_PLAY_MODE_XINPUT2)
endif()

set_target_properties(TbKzbLibTest PROPERTIES AUTOMOC TRUE)

# Copy fixtures
set(TEST_FIXTURE_SOURCE_DIR ${CMAKE_CURRENT_SOURCE_DIR}/fixture)
set(TEST_FIXTURE_DEST_DIR $<TARGET_FILE_DIR:TbKzbLibTest>/fixture)

add_custom_command(TARGET TbKzbLibTest POST_BUILD
  COMMAND ${CMAKE_COMMAND} -E rm -rf "${TEST_FIXTURE_DEST_DIR}"
  COMMAND ${CMAKE_COMMAND} -E copy_directory "${TEST_FIXTURE_SOURCE_DIR}" "${TEST_FIXTURE_DEST_DIR}/test"
)

include(Catch)
catch_discover_tests(TbKzbLibTest)
