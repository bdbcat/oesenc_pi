# ~~~
# Author:      bdcat <Dave Register>
# Copyright:
# License:     GPLv2+
# ~~~

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

# -------  Plugin setup --------
#
include("VERSION.cmake")

set(PKG_NAME oesenc_pi)
set(PKG_VERSION ${OCPN_VERSION})
set(PKG_PRERELEASE "")  # Empty, or a tag like 'beta'

set(DISPLAY_NAME oesenc)    # Dialogs, installer artifacts, ...
set(PLUGIN_API_NAME oeSENC) # As of GetCommonName() in plugin API
set(CPACK_PACKAGE_CONTACT "Dave Register")
set(PKG_SUMMARY
  "OpenCPN support for encrypted OCPN Vector Charts "
)
set(PKG_DESCRIPTION [=[
OCPN Vector Charts licensed and sourced from chart providers like
Hydrographic Offices.  

While the charts are not officially approved official ENC charts they
are based on the same data -- the legal conditions are described in the
EULA. The charts has world-wide coverage and provides a cost-effective
way to access the national chart databases. Charts are encrypted and 
can only be used after purchasing decryption keys from o-charts.org.
]=])

set(PKG_AUTHOR "Dave register")

set(PKG_HOMEPAGE https://github.com/bdbcat/oesenc_pi)
set(PKG_INFO_URL https://opencpn.org/OpenCPN/plugins/oesenc.html)

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

set(PKG_API_LIB api-16)  #  A directory in libs/ e. g., api-17 or api-16

macro(late_init)
  # Perform initialization after the PACKAGE_NAME library, compilers
  # and ocpn::api is available.
  if (ARCH STREQUAL "armhf")
    target_compile_definitions(${PACKAGE_NAME} PRIVATE OCPN_ARMHF)
  endif ()      
  target_include_directories(${PACKAGE_NAME} PRIVATE libs/gdal/src src)
endmacro ()

macro(add_plugin_libraries)

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

  add_subdirectory("libs/opencpn-glu")
  target_link_libraries(${PACKAGE_NAME} opencpn::glu)
 
  if( ("${plugin_target}" STREQUAL  "mingw") OR ("${plugin_target}" STREQUAL  "msvc") )  
    option(OCPN_USE_BUNDLED_WXCURL "Use wxCurl libraries" ON)
    message(STATUS "Building bundled wxCurl libraries")
  else( ("${plugin_target}" STREQUAL  "mingw") OR ("${plugin_target}" STREQUAL  "msvc") ) 
    message(STATUS "Using system wxCurl libraries")
    INCLUDE_DIRECTORIES("libs/wxcurl/src")
  endif( ("${plugin_target}" STREQUAL  "mingw") OR ("${plugin_target}" STREQUAL  "msvc") )  

  if (NOT QT_ANDROID)
    if(OCPN_USE_BUNDLED_WXCURL)
        add_subdirectory("libs/wxcurl")
        target_link_libraries(${PACKAGE_NAME} ocpn::wxcurl)
    endif(OCPN_USE_BUNDLED_WXCURL)
  endif ()
  
  add_subdirectory("libs/oeserverd")

endmacro ()
