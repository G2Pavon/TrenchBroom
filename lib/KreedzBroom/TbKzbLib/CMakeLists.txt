# TbKzbLib: KreedzBroom's play mode in the editor (Counter-Strike 1.6 movement from
# TbCsLib), its mouse capture and its preference pane. TrenchBroom calls into it through
# the hooks kzb::initialize installs.
add_library(TbKzbLib STATIC)

target_sources(TbKzbLib
  PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/src/KreedzBroom.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/src/KreedzBroomPreferencePane.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/src/PlayMode.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/src/PlayModeController.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/src/PlayModeRawMouse.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/src/PlayModeShowPre.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/src/PlayModeSound.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/src/PlayModeTimeline.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/src/PlayModeWorld.cpp

  PUBLIC FILE_SET headers TYPE HEADERS
    BASE_DIRS ${CMAKE_CURRENT_SOURCE_DIR}/include
    FILES
    ${CMAKE_CURRENT_SOURCE_DIR}/include/kzb/KreedzBroom.h
    ${CMAKE_CURRENT_SOURCE_DIR}/include/kzb/KreedzBroomPreferencePane.h
    ${CMAKE_CURRENT_SOURCE_DIR}/include/kzb/PlayMode.h
    ${CMAKE_CURRENT_SOURCE_DIR}/include/kzb/PlayModeController.h
    ${CMAKE_CURRENT_SOURCE_DIR}/include/kzb/PlayModeRawMouse.h
    ${CMAKE_CURRENT_SOURCE_DIR}/include/kzb/PlayModeShowPre.h
    ${CMAKE_CURRENT_SOURCE_DIR}/include/kzb/PlayModeSound.h
    ${CMAKE_CURRENT_SOURCE_DIR}/include/kzb/PlayModeTimeline.h
    ${CMAKE_CURRENT_SOURCE_DIR}/include/kzb/PlayModeWorld.h
    ${CMAKE_CURRENT_SOURCE_DIR}/include/kzb/Preferences.h
)

target_link_libraries(TbKzbLib
  PRIVATE
    CompilerConfig
  PUBLIC
    Qt6::OpenGLWidgets
    Qt6::Widgets
    TbCsLib
    TbUiLib
)

# Play mode mouse capture: XInput2 raw motion + confined grab on X11 / XWayland, pointer
# lock + relative motion on Wayland. Both are optional; without them play mode falls back
# to accelerated cursor input.

# Play mode sounds (footsteps etc.) need Qt Multimedia; without it play mode is silent.
find_package(Qt6 QUIET COMPONENTS Multimedia)
if(TARGET Qt6::Multimedia)
  target_link_libraries(TbKzbLib PRIVATE Qt6::Multimedia)
  target_compile_definitions(TbKzbLib PRIVATE TB_PLAY_MODE_SOUND)
else()
  message(STATUS "Qt6 Multimedia not found: play mode will be silent")
endif()

if(UNIX AND NOT APPLE)
  find_package(PkgConfig QUIET)
  if(PkgConfig_FOUND)
    pkg_check_modules(TB_XCB_XINPUT QUIET IMPORTED_TARGET xcb xcb-xinput xcb-xfixes)
    pkg_check_modules(TB_WAYLAND_CLIENT QUIET IMPORTED_TARGET wayland-client)
  endif()

  if(TB_XCB_XINPUT_FOUND)
    target_link_libraries(TbKzbLib PRIVATE PkgConfig::TB_XCB_XINPUT)
    target_compile_definitions(TbKzbLib PRIVATE TB_PLAY_MODE_XINPUT2)
  else()
    message(STATUS "xcb-xinput / xcb-xfixes not found: no raw mouse input on X11")
  endif()

  find_program(TB_WAYLAND_SCANNER wayland-scanner)
  # Only QNativeInterface::Private::QWaylandWindow (the window's wl_surface) is used.
  set(QT_NO_PRIVATE_MODULE_WARNING ON)
  find_package(Qt6 QUIET COMPONENTS GuiPrivate)
  if(TB_WAYLAND_CLIENT_FOUND AND TB_WAYLAND_SCANNER AND TARGET Qt6::GuiPrivate)
    set(TB_WAYLAND_GENERATED_DIR "${CMAKE_CURRENT_BINARY_DIR}/wayland")
    file(MAKE_DIRECTORY "${TB_WAYLAND_GENERATED_DIR}")
    set(TB_WAYLAND_PROTOCOL_SOURCES)
    foreach(protocol pointer-constraints-unstable-v1 relative-pointer-unstable-v1)
      set(xml "${CMAKE_CURRENT_SOURCE_DIR}/wayland/${protocol}.xml")
      set(header "${TB_WAYLAND_GENERATED_DIR}/${protocol}-client-protocol.h")
      set(code "${TB_WAYLAND_GENERATED_DIR}/${protocol}-protocol.c")
      add_custom_command(
        OUTPUT "${header}"
        COMMAND "${TB_WAYLAND_SCANNER}" client-header "${xml}" "${header}"
        DEPENDS "${xml}")
      add_custom_command(
        OUTPUT "${code}"
        COMMAND "${TB_WAYLAND_SCANNER}" private-code "${xml}" "${code}"
        DEPENDS "${xml}")
      list(APPEND TB_WAYLAND_PROTOCOL_SOURCES "${header}" "${code}")
    endforeach()

    # generated C code, kept out of TbKzbLib so it does not get the C++ warning flags
    add_library(TbWaylandProtocols OBJECT ${TB_WAYLAND_PROTOCOL_SOURCES})
    target_link_libraries(TbWaylandProtocols PUBLIC PkgConfig::TB_WAYLAND_CLIENT)
    target_include_directories(TbWaylandProtocols PUBLIC "${TB_WAYLAND_GENERATED_DIR}")

    target_sources(TbKzbLib PRIVATE $<TARGET_OBJECTS:TbWaylandProtocols>)
    target_link_libraries(TbKzbLib PRIVATE TbWaylandProtocols Qt6::GuiPrivate)
    target_compile_definitions(TbKzbLib PRIVATE TB_PLAY_MODE_WAYLAND)
  else()
    message(STATUS "wayland-client / wayland-scanner / Qt6::GuiPrivate not found: no pointer lock on Wayland")
  endif()
endif()

set_target_properties(TbKzbLib PROPERTIES AUTOMOC TRUE)

add_subdirectory(test)
