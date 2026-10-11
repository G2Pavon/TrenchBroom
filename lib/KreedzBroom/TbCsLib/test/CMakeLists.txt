add_executable(TbCsLibTest)
EMBED_UTF8_MANIFEST(TbCsLibTest)

target_sources(TbCsLibTest PRIVATE
  ${CMAKE_CURRENT_SOURCE_DIR}/src/GroundTruth.cpp
  ${CMAKE_CURRENT_SOURCE_DIR}/src/GroundTruth.h
  ${CMAKE_CURRENT_SOURCE_DIR}/src/tst_GroundTruth.cpp
  ${CMAKE_CURRENT_SOURCE_DIR}/src/tst_ScsApi.cpp
)

target_link_libraries(TbCsLibTest
  PRIVATE
    CompilerConfig
    Catch2::Catch2WithMain
    TbCsLib
)

if(TB_CS_X87_MATH)
  target_compile_definitions(TbCsLibTest PRIVATE TB_CS_X87_MATH)
endif()

# Copy fixtures
set(TEST_FIXTURE_SOURCE_DIR ${CMAKE_CURRENT_SOURCE_DIR}/fixture)
set(TEST_FIXTURE_DEST_DIR $<TARGET_FILE_DIR:TbCsLibTest>/fixture)

add_custom_command(TARGET TbCsLibTest POST_BUILD
  COMMAND ${CMAKE_COMMAND} -E rm -rf "${TEST_FIXTURE_DEST_DIR}"
  COMMAND ${CMAKE_COMMAND} -E copy_directory "${TEST_FIXTURE_SOURCE_DIR}" "${TEST_FIXTURE_DEST_DIR}/test"
)

include(Catch)
catch_discover_tests(TbCsLibTest)
