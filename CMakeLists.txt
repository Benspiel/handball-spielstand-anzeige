cmake_minimum_required(VERSION 3.22)
project(handball-scoreboard VERSION 0.1.0 LANGUAGES C CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)
set(CMAKE_POSITION_INDEPENDENT_CODE ON)

option(HB_BUILD_PLUGIN "OBS-Plugin bauen (benötigt libobs, obs-frontend-api und Qt6)" ON)
option(HB_BUILD_TESTS "Tests für den Erkennungskern bauen" ON)

# --- Erkennungskern: keine Abhängigkeiten, überall baubar --------------------------------------
add_library(hb_core STATIC src/core/seven_segment.cpp src/core/match_tracker.cpp)
target_include_directories(hb_core PUBLIC src/core)
if(MSVC)
  target_compile_options(hb_core PRIVATE /W4 /utf-8)
else()
  target_compile_options(hb_core PRIVATE -Wall -Wextra)
endif()

if(HB_BUILD_TESTS)
  enable_testing()
  add_executable(hb_tests tests/test_main.cpp)
  target_link_libraries(hb_tests PRIVATE hb_core)
  add_test(NAME core COMMAND hb_tests)

  # Erzeugt künstliche Tafel-Bilder zum Ausprobieren (siehe README)
  add_executable(hb_make_demo_board tests/make_demo_board.cpp)
  target_link_libraries(hb_make_demo_board PRIVATE hb_core)
endif()

if(HB_BUILD_PLUGIN)
  add_subdirectory(src/plugin)
endif()
