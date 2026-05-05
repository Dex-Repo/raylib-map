cmake_minimum_required(VERSION 3.16)

project(raylib-map VERSION 0.1.0 LANGUAGES C)

set(CMAKE_C_STANDARD 11)
set(CMAKE_C_STANDARD_REQUIRED ON)

set(RMP_TARGET rMP)
set(RMP_SOURCES ${CMAKE_CURRENT_SOURCE_DIR}/rMP.c)

if(APPLE)
  add_executable(${RMP_TARGET} MACOSX_BUNDLE ${RMP_SOURCES})
elseif(WIN32)
  add_executable(${RMP_TARGET} WIN32 ${RMP_SOURCES})
  if(MSVC)
    set_property(TARGET ${RMP_TARGET} APPEND_STRING PROPERTY LINK_FLAGS " /ENTRY:mainCRTStartup")
  endif()
else()
  add_executable(${RMP_TARGET} ${RMP_SOURCES})
endif()

target_include_directories(${RMP_TARGET} PRIVATE ${CMAKE_CURRENT_SOURCE_DIR})

find_package(raylib QUIET CONFIG)
set(RAYLIB_LINKED FALSE)

if(raylib_FOUND)
  if(TARGET raylib::raylib)
    target_link_libraries(${RMP_TARGET} PRIVATE raylib::raylib)
    set(RAYLIB_LINKED TRUE)
  elseif(TARGET raylib)
    target_link_libraries(${RMP_TARGET} PRIVATE raylib)
    set(RAYLIB_LINKED TRUE)
  elseif(TARGET Raylib::Raylib)
    target_link_libraries(${RMP_TARGET} PRIVATE Raylib::Raylib)
    set(RAYLIB_LINKED TRUE)
  endif()
endif()

if(NOT RAYLIB_LINKED)
  find_package(PkgConfig REQUIRED)
  pkg_check_modules(RAYLIB QUIET raylib)
  if(RAYLIB_FOUND)
    target_include_directories(${RMP_TARGET} PRIVATE ${RAYLIB_INCLUDE_DIRS})
    target_link_libraries(${RMP_TARGET} PRIVATE ${RAYLIB_LIBRARIES})
    if(DEFINED RAYLIB_CFLAGS)
      string(REPLACE ";" " " RAYLIB_CFLAGS_STR "${RAYLIB_CFLAGS}")
      separate_arguments(RAYLIB_CFLAGS_LIST UNIX_COMMAND "${RAYLIB_CFLAGS_STR}")
      target_compile_options(${RMP_TARGET} PRIVATE ${RAYLIB_CFLAGS_LIST})
    endif()
    set(RAYLIB_LINKED TRUE)
  endif()
endif()

if(NOT RAYLIB_LINKED)
  if(DEFINED ENV{HOMEBREW_PREFIX})
    set(HB_PREFIX $ENV{HOMEBREW_PREFIX})
  elseif(EXISTS "/opt/homebrew")
    set(HB_PREFIX "/opt/homebrew")
  elseif(EXISTS "/usr/local")
    set(HB_PREFIX "/usr/local")
  else()
    set(HB_PREFIX "")
  endif()

  # Include CMAKE_PREFIX_PATH in search paths (for CI builds)
  set(SEARCH_PREFIXES ${CMAKE_PREFIX_PATH} ${HB_PREFIX} /usr/local /usr)

  find_path(RAYLIB_INCLUDE_DIR raylib.h PATHS
    ${SEARCH_PREFIXES}
    PATH_SUFFIXES include opt/raylib/include
  )
  find_library(RAYLIB_LIBRARY NAMES raylib PATHS
    ${SEARCH_PREFIXES}
    PATH_SUFFIXES lib opt/raylib/lib
  )

  if(RAYLIB_INCLUDE_DIR AND RAYLIB_LIBRARY)
    target_include_directories(${RMP_TARGET} PRIVATE ${RAYLIB_INCLUDE_DIR})
    target_link_libraries(${RMP_TARGET} PRIVATE ${RAYLIB_LIBRARY})
    set(RAYLIB_LINKED TRUE)
    message(STATUS "Found raylib: ${RAYLIB_INCLUDE_DIR} / ${RAYLIB_LIBRARY}")
  else()
    message(STATUS "raylib not found in: ${SEARCH_PREFIXES}")
    message(STATUS "  Include: ${RAYLIB_INCLUDE_DIR}")
    message(STATUS "  Library: ${RAYLIB_LIBRARY}")
  endif()
endif()

if(NOT RAYLIB_LINKED)
  message(FATAL_ERROR "raylib not found. Install raylib and pkg-config or provide CMAKE_PREFIX_PATH. Official repo: https://github.com/raysan5/raylib")
endif()

# Platform-specific dependencies for Raylib
if(APPLE)
    target_link_libraries(${RMP_TARGET} PRIVATE
        "-framework CoreVideo"
        "-framework IOKit"
        "-framework Cocoa"
        "-framework GLUT"
        "-framework OpenGL"
    )
elseif(WIN32)
    target_link_libraries(${RMP_TARGET} PRIVATE winmm gdi32 opengl32 shell32)
else() # Linux / UNIX
    target_link_libraries(${RMP_TARGET} PRIVATE m pthread dl rt X11 GL)
endif()

set_target_properties(${RMP_TARGET} PROPERTIES
  RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/bin
)

if(EXISTS ${CMAKE_SOURCE_DIR}/logo.png)
  configure_file(${CMAKE_SOURCE_DIR}/logo.png ${CMAKE_BINARY_DIR}/bin/logo.png COPYONLY)
  install(FILES ${CMAKE_SOURCE_DIR}/logo.png DESTINATION .)
endif()

if(APPLE)
  if(EXISTS ${CMAKE_SOURCE_DIR}/logo.icns)
    set_source_files_properties(${CMAKE_SOURCE_DIR}/logo.icns PROPERTIES MACOSX_PACKAGE_LOCATION Resources)
    target_sources(${RMP_TARGET} PRIVATE ${CMAKE_SOURCE_DIR}/logo.icns)
    set_target_properties(${RMP_TARGET} PROPERTIES MACOSX_BUNDLE_ICON_FILE logo.icns)
  endif()
  if(EXISTS ${CMAKE_SOURCE_DIR}/logo.png)
    set_source_files_properties(${CMAKE_SOURCE_DIR}/logo.png PROPERTIES MACOSX_PACKAGE_LOCATION Resources)
    target_sources(${RMP_TARGET} PRIVATE ${CMAKE_SOURCE_DIR}/logo.png)
  endif()
endif()

if(WIN32)
  if(EXISTS ${CMAKE_SOURCE_DIR}/logo.ico)
    configure_file(${CMAKE_SOURCE_DIR}/logo.ico ${CMAKE_BINARY_DIR}/logo.ico COPYONLY)
  endif()
  if(EXISTS ${CMAKE_SOURCE_DIR}/app_icon.rc)
    target_sources(${RMP_TARGET} PRIVATE ${CMAKE_SOURCE_DIR}/app_icon.rc)
  endif()
endif()

if(UNIX AND NOT APPLE)
  set(DESKTOP_FILE ${CMAKE_BINARY_DIR}/raylib-map.desktop)
  file(WRITE ${DESKTOP_FILE}
"[Desktop Entry]\n"
"Type=Application\n"
"Name=raylib-map\n"
"Comment=Tilemap editor in raylib\n"
"Exec=rMP\n"
"Icon=logo\n"
"Terminal=false\n"
"Categories=Development;Graphics;\n")
  install(FILES ${DESKTOP_FILE} DESTINATION share/applications)
endif()

if(APPLE)
  install(TARGETS ${RMP_TARGET} BUNDLE DESTINATION . RUNTIME DESTINATION bin)
else()
  install(TARGETS ${RMP_TARGET} RUNTIME DESTINATION bin)
endif()

set(CPACK_PACKAGE_NAME "raylib-map")
set(CPACK_PACKAGE_VENDOR "rMP")
set(CPACK_PACKAGE_VERSION ${PROJECT_VERSION})
set(CPACK_PACKAGE_FILE_NAME "raylib-map-${PROJECT_VERSION}-${CMAKE_SYSTEM_NAME}")

if(APPLE)
  set(CPACK_GENERATOR "DragNDrop")
elseif(WIN32)
  set(CPACK_GENERATOR "ZIP")
else()
  set(CPACK_GENERATOR "TGZ")
endif()

include(CPack)
