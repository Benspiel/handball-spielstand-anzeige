find_package(libobs REQUIRED)
find_package(obs-frontend-api REQUIRED)
find_package(Qt6 REQUIRED COMPONENTS Core Gui Widgets)

set(CMAKE_AUTOMOC ON)

add_library(handball-scoreboard MODULE
  plugin-main.cpp
  config.cpp
  shared_state.cpp
  camera_capture.cpp
  engine.cpp
  scorebug.cpp
  scorebug_source.cpp
  preview_widget.cpp
  dock.cpp)

target_include_directories(handball-scoreboard PRIVATE ${CMAKE_CURRENT_SOURCE_DIR})
target_link_libraries(handball-scoreboard PRIVATE
  hb_core
  OBS::libobs
  OBS::obs-frontend-api
  Qt6::Core Qt6::Gui Qt6::Widgets)

set(HB_BUILD_ID "dev" CACHE STRING "Kennung des Builds (z.B. Git-Hash), erscheint im Dock und im OBS-Log")
target_compile_definitions(handball-scoreboard PRIVATE HB_BUILD_ID="${HB_BUILD_ID}")

set_target_properties(handball-scoreboard PROPERTIES PREFIX "")

if(MSVC)
  target_compile_options(handball-scoreboard PRIVATE /W3 /utf-8 /permissive-)
else()
  target_compile_options(handball-scoreboard PRIVATE -Wall -Wextra -Wno-unused-parameter)
endif()
