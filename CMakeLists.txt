# ~~~
# Author:      bdcat <Dave Register>
# Copyright:
# License:     GPLv2+
# ~~~

# -------- Cmake setup ---------
#
cmake_minimum_required(VERSION 3.12.0)

cmake_policy(SET CMP0042 NEW)

if (POLICY CMP0072)
  cmake_policy(SET CMP0072 NEW)
endif ()

if (POLICY CMP0077)
  cmake_policy(SET CMP0077 NEW)
endif ()

# Locations where cmake looks for cmake modules.
set(CMAKE_MODULE_PATH ${CMAKE_SOURCE_DIR}/build ${CMAKE_SOURCE_DIR}/cmake)

if (WIN32)
  list(APPEND CMAKE_MODULE_PATH ${CMAKE_SOURCE_DIR}/buildwin)
endif ()
message(STATUS "Cmake version: ${CMAKE_VERSION}.")


# -------- Build setup (mostly in PluginCompiler) --------
#
set(CMAKE_CXX_STANDARD 11)
set(CMAKE_CXX_EXTENSIONS OFF)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

set(CMAKE_POSITION_INDEPENDENT_CODE ON)
message(STATUS "Building: ${BUILD_TYPE}")

add_compile_options(
  -Wno-inconsistent-missing-override
  -Wno-potentially-evaluated-expression
)

# -------- Options ----------

set(OCPN_TEST_REPO
    "david-register/ocpn-plugins-unstable"
    CACHE STRING "Default repository for untagged builds"
)
set(OCPN_BETA_REPO
    "david-register/ocpn-plugins-unstable"
    CACHE STRING 
    "Default repository for tagged builds matching 'beta'"
)
set(OCPN_RELEASE_REPO
    "david-register/ocpn-plugins-stable"
    CACHE STRING 
    "Default repository for tagged builds not matching 'beta'"
)

set(OCPN_TARGET_TUPLE "" CACHE STRING
  "Target spec: \"platform;version;arch\""
)

string(TOLOWER "${OCPN_TARGET_TUPLE}" _lc_target)
if ("${_lc_target}" MATCHES "android*")
  set(QT_ANDROID ON)
  set(OCPN_USE_CURL OFF)
  # Until we have a proper toolchain file:
  set(CMAKE_CROSSCOMPILING ON)
else ()
  set(QT_ANDROID OFF)
  option(OCPN_USE_CURL "Use Curl libraries" ON)
endif ()


# -------  Plugin setup --------
#
project(oesenc_pi VERSION 4.2.18.2)

set(DISPLAY_NAME oesenc)    # Dialogs, installer artifacts, ...
set(PLUGIN_API_NAME oeSENC) # As of GetCommonName() in plugin API
set(CPACK_PACKAGE_CONTACT "Dave Register")
set(CPACK_PACKAGE_DESCRIPTION_SUMMARY
  "OpenCPN support for encrypted OCPN Vector Charts "
)
set(CPACK_PACKAGE_DESCRIPTION [=[
OCPN Vector Charts licensed and sourced from chart providers like
Hydrographic Offices.  

While the charts are not officially approved official ENC charts they
are based on the same data -- the legal conditions are described in the
EULA. The charts has world-wide coverage and provides a cost-effective
way to access the national chart databases. Charts are encrypted and 
can only be used after purchasing decryption keys from o-charts.org.
]=])

set(PKG_RELEASE "1")
set(PKG_AUTHOR "Dave register")

set(SRC
    src/oesenc_pi.h
    src/oesenc_pi.cpp
    src/eSENCChart.cpp
    src/eSENCChart.h
    src/InstallDirs.cpp
    src/InstallDirs.h
    src/Osenc.cpp
    src/Osenc.h
    src/mygeom.h
    src/mygeom63.cpp
    src/TexFont.cpp
    src/TexFont.h
    src/s52plib.cpp
    src/s57RegistrarMgr.cpp
    src/chartsymbols.cpp
    src/s52cnsy.cpp
    src/viewport.cpp
    src/s52utils.cpp
    src/bbox.cpp
    src/OCPNRegion.cpp
    src/LLRegion.cpp
    src/cutil.cpp
    src/razdsparser.cpp
    src/pi_DepthFont.cpp
    src/ochartShop.cpp
    src/ochartShop.h
    libs/gdal/src/s57classregistrar.cpp
)


# -------- Setup completed, build the plugin --------
#
add_library(${CMAKE_PROJECT_NAME} SHARED ${SRC})
add_subdirectory("libs/api-16")
target_link_libraries(${CMAKE_PROJECT_NAME} ocpn::api)

set(VERSION_MAJOR ${PROJECT_VERSION_MAJOR})
set(VERSION_MINOR ${PROJECT_VERSION_MINOR})
set(VERSION_PATCH ${PROJECT_VERSION_PATCH})
set(PACKAGE_NAME ${CMAKE_PROJECT_NAME})

# Create the minimim ocpn version id string like ov50
string(REGEX REPLACE "\([0-9]\)\.\([0-9]\).*" "ov\\1\\2"
       OCPN_MIN_VERSION ${API_OCPN_MIN_VERSION})


# Set up targets. Targets sets up a recursive call with BUILD_TYPE set to
# 'flatpak', 'pkg' or 'tarball'. The initial call without BUILD_TYPE ends
# here.
#
include(Targets)
create_targets(
  ${PROJECT_SOURCE_DIR}/flatpak/org.opencpn.OpenCPN.Plugin.oesenc.yaml
)
if ("${BUILD_TYPE}" STREQUAL "")
  return ()
endif ()

if (NOT ${BUILD_TYPE} STREQUAL "flatpak")
  # Build package as required (flatpak already dealt with).
  #
  include(PluginCompiler)
  if (NOT QT_ANDROID)
    include(PluginLibs)
  endif ()
  include(PluginLocalization)
  include(PluginInstall)
  include(PluginPackage)

  include_directories(BEFORE ${CMAKE_BINARY_DIR}/include)
  include_directories(${CMAKE_SOURCE_DIR}/src)
  include_directories(${CMAKE_SOURCE_DIR}/include)
  include_directories(${CMAKE_SOURCE_DIR}/libs/gdal/src)
  if (QT_ANDROID)
    include(libs/AndroidLibs.cmake)
  endif ()
  add_subdirectory("libs/cpl")
  target_link_libraries(${PACKAGE_NAME} ocpn::cpl)
  add_subdirectory("libs/dsa")
  target_link_libraries(${PACKAGE_NAME} ocpn::dsa)
  add_subdirectory("libs/wxJSON")
  target_link_libraries(${PACKAGE_NAME} ocpn::wxjson)
  add_subdirectory("libs/iso8211")
  target_link_libraries(${PACKAGE_NAME} ocpn::iso8211)
  add_subdirectory("libs/tinyxml")
  target_link_libraries(${PACKAGE_NAME} ocpn::tinyxml)
  add_subdirectory("libs/libglu")
  target_link_libraries(${PACKAGE_NAME} ocpn::glu)
  if (OCPN_USE_CURL)
    add_subdirectory("libs/wxcurl")
    target_link_libraries(${PACKAGE_NAME} ocpn::wxcurl)
  endif ()
  add_subdirectory("libs/oeserverd")
  if (APPLE)
    # Copy some items needed by the .img package
    execute_process(
      COMMAND cp -R ${CMAKE_SOURCE_DIR}/data ${CMAKE_BINARY_DIR}
    )
    execute_process(
      COMMAND cp -a ${CMAKE_SOURCE_DIR}/data/rrc_eula_ChartSetsForOpenCPN.txt
      ${CMAKE_SOURCE_DIR}/buildosx/InstallOSX/SGLockInstall.scptd
      ${CMAKE_SOURCE_DIR}/buildosx/InstallOSX/sgli
      ${CMAKE_BINARY_DIR}
    )
  endif ()
endif ()

configure_file(
  ${CMAKE_SOURCE_DIR}/config.h.in ${CMAKE_BINARY_DIR}/include//config.h
  @ONLY
)

configure_file(
  # The cloudsmith upload script
  ${CMAKE_SOURCE_DIR}/ci/upload.sh.in ${CMAKE_BINARY_DIR}/upload.sh
  @ONLY
)
configure_file(
  # The cloudsmith upload script, windows bat file.
  ${CMAKE_SOURCE_DIR}/ci/upload.bat.in ${CMAKE_BINARY_DIR}/upload.bat
  @ONLY
)

set(checksum "@checksum@")
configure_file(
  # The XML metadata file
  ${CMAKE_SOURCE_DIR}/oesenc-plugin.xml.in
  ${CMAKE_BINARY_DIR}/${pkg_displayname}.xml.in
  @ONLY
)
