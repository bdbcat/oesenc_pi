#
#  Export variables used in plugin setup: GIT_HASH, GIT_COMMIT,
#  PKG_TARGET, PKG_TARGET_VERSION and PKG_NVR

execute_process(
  COMMAND git log -1 --format=%h
  WORKING_DIRECTORY ${CMAKE_SOURCE_DIR}
  OUTPUT_VARIABLE GIT_HASH
  OUTPUT_STRIP_TRAILING_WHITESPACE
)

execute_process(
  COMMAND git log -1 --format=%ci
  WORKING_DIRECTORY ${CMAKE_SOURCE_DIR}
  OUTPUT_VARIABLE GIT_COMMIT_DATE
  OUTPUT_STRIP_TRAILING_WHITESPACE
)

if (OCPN_FLATPAK)
    set(PKG_TARGET "flatpak")
    set(PKG_TARGET_VERSION "18.08")    # As of flatpak/*yaml
elseif (MINGW)
    set(PKG_TARGET "mingw")
    if (CMAKE_SYSTEM_VERSION)
        set(PKG_TARGET_VERSION ${CMAKE_SYSTEM_VERSION})
    else ()
	set(PKG_TARGET_VERSION 10)
    endif ()
elseif (MSVC)
    set(PKG_TARGET "msvc")
    if (CMAKE_SYSTEM_VERSION)
        set(PKG_TARGET_VERSION ${CMAKE_SYSTEM_VERSION})
    elseif (CMAKE_VS_WINDOWS_TARGET_PLATFORM_VERSION)
        set(PKG_TARGET_VERSION ${CMAKE_VS_WINDOWS_TARGET_PLATFORM_VERSION})
    else ()
        set(PKG_TARGET_VERSION 10)
    endif ()
elseif (APPLE)
    set(PKG_TARGET "darwin")
    execute_process(COMMAND "sw_vers" "-productVersion"
                    OUTPUT_VARIABLE PKG_TARGET_VERSION)
elseif (UNIX)
    # Some linux dist:
    execute_process(COMMAND "lsb_release" "-is"
                    OUTPUT_VARIABLE PKG_TARGET)
    execute_process(COMMAND "lsb_release" "-rs"
                    OUTPUT_VARIABLE PKG_TARGET_VERSION)
else ()
    set(PKG_TARGET "unknown")
    set(PKG_TARGET_VERSION 1)
endif ()


string(STRIP "${PKG_TARGET}" PKG_TARGET)
string(TOLOWER "${PKG_TARGET}" PKG_TARGET)
string(STRIP "${PKG_TARGET_VERSION}" PKG_TARGET_VERSION)
string(TOLOWER "${PKG_TARGET_VERSION}" PKG_TARGET_VERSION)
set(PKG_TARGET_NVR "${PKG_TARGET}-${PKG_TARGET_VERSION}")
if (NOT DEFINED wxWidgets_LIBRARIES)
  message(FATAL_ERROR "PluginSetup: required wxWidgets_LIBRARIES missing")
elseif ("${wxWidgets_LIBRARIES}" MATCHES "gtk3u" AND PKG_TARGET STREQUAL "ubuntu")
  set(PKG_TARGET "${PKG_TARGET}-gtk3")
endif ()
