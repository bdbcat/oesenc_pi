##---------------------------------------------------------------------------
## Author:      bdbcat aka. dsr (Dave Register)
## Copyright:
## License:     wxWidgets License
##---------------------------------------------------------------------------


# define minimum cmake version
CMAKE_MINIMUM_REQUIRED(VERSION 2.6.2)
# This should be 2.8.0 to have FindGTK2 module
IF (COMMAND cmake_policy)
  CMAKE_POLICY(SET CMP0003 OLD)
  CMAKE_POLICY(SET CMP0005 OLD)
  CMAKE_POLICY(SET CMP0011 OLD)
ENDIF (COMMAND cmake_policy)

PROJECT(oesenc_pi)

SET(PACKAGE_NAME oesenc_pi)
SET(VERBOSE_NAME oesenc)
SET(PLUGIN_SOURCE_DIR .)
MESSAGE (STATUS "*** Building ${PACKAGE_NAME} ***")

SET(VERSION_MAJOR "0")
SET(VERSION_MINOR "4")
SET(VERSION_PATCH "1405")
SET(PACKAGE_VERSION "${VERSION_MAJOR}.${VERSION_MINOR}.${VERSION_PATCH}" )

#SET(CMAKE_BUILD_TYPE Release)

IF( NOT CMAKE_BUILD_TYPE )
  SET( CMAKE_BUILD_TYPE Release CACHE STRING
       "Choose the type of build, options are: None Debug Release RelWithDebInfo MinSizeRel."
       FORCE )
ENDIF( NOT CMAKE_BUILD_TYPE )

MESSAGE (STATUS "Build type: ${CMAKE_BUILD_TYPE}")

SET(CMAKE_VERBOSE_MAKEFILE ON)

IF (PREFIX)
 SET(CMAKE_INSTALL_PREFIX ${PREFIX})
ENDIF (PREFIX)

SET(PREFIX_BIN bin)
SET(PREFIX_INCLUDE include)
SET(PREFIX_DATA share)
SET(PREFIX_PKGDATA ${PREFIX_DATA}/${PACKAGE_NAME})

IF (NOT WIN32)
  # default
  SET (ARCH "i386")
  IF (UNIX AND NOT APPLE)
    IF (PREFIX)
        SET(CMAKE_INSTALL_PREFIX ${PREFIX})
    ELSE (PREFIX )
        SET(CMAKE_INSTALL_PREFIX "/usr")
    ENDIF (PREFIX)

    MESSAGE (STATUS "*** Will install to ${CMAKE_INSTALL_PREFIX}  ***")

    IF (EXISTS /etc/debian_version)
      SET (PACKAGE_FORMAT "DEB")
      SET (PACKAGE_DEPS "libc6, libwxgtk3.0-0, libglu1-mesa (>= 7.0.0),  zlib1g, bzip2")
      SET (LIB_INSTALL_DIR "lib")
      IF (CMAKE_SIZEOF_VOID_P MATCHES "8")
        SET (ARCH "amd64")
      ELSE (CMAKE_SIZEOF_VOID_P MATCHES "8")
        SET (ARCH "i386")
      ENDIF (CMAKE_SIZEOF_VOID_P MATCHES "8")
    ENDIF (EXISTS /etc/debian_version)
    IF (EXISTS /etc/redhat-release)
      SET (PACKAGE_FORMAT "RPM")
      SET (PACKAGE_DEPS  "opencpn zlib bzip2")
      IF (CMAKE_SIZEOF_VOID_P MATCHES "8")
        SET (ARCH "x86_64")
        SET (LIB_INSTALL_DIR "lib64")
      ELSE (CMAKE_SIZEOF_VOID_P MATCHES "8")
        SET (ARCH "i386")
        SET (LIB_INSTALL_DIR "lib")
      ENDIF (CMAKE_SIZEOF_VOID_P MATCHES "8")
    ENDIF (EXISTS /etc/redhat-release)
  ENDIF (UNIX AND NOT APPLE)
endif(NOT WIN32)

IF (CMAKE_SYSTEM_PROCESSOR MATCHES "arm*")
  SET (ARCH "armhf")
  ADD_DEFINITIONS( -DARMHF )
ENDIF (CMAKE_SYSTEM_PROCESSOR MATCHES "arm*")

MESSAGE (STATUS "*** Build Architecture is ${ARCH}")


INCLUDE_DIRECTORIES(${CMAKE_SOURCE_DIR}/src/wxJSON)
INCLUDE_DIRECTORIES(${CMAKE_SOURCE_DIR}/src)
INCLUDE_DIRECTORIES(${CMAKE_SOURCE_DIR}/src/cpl)
INCLUDE_DIRECTORIES(${CMAKE_SOURCE_DIR}/src/dsa)
INCLUDE_DIRECTORIES(${CMAKE_SOURCE_DIR})


IF(WIN32)
#      INCLUDE_DIRECTORIES("U:/Projects/opencpn_sf/opencpn/include")
ENDIF(WIN32)

IF(UNIX)
#     INCLUDE_DIRECTORIES("/home/dsr/Projects/opencpn_sf/opencpn/include")
ENDIF(UNIX)


# require proper c++
#ADD_DEFINITIONS( "-Wall -ansi -pedantic -Wno-variadic-macros" )
#TODO: Should we use  -fno-stack-protector
#  IF NOT DEBUGGING CFLAGS="-O2 -march=native"
IF(NOT WIN32)
# ADD_DEFINITIONS( "-Wall -g -fexceptions -fvisibility=hidden -O0" )
 ADD_DEFINITIONS( "-Wall -g -fexceptions -O2" )
 IF(CMAKE_BUILD_TYPE MATCHES "Debug")
    ADD_DEFINITIONS( " -O0")
 ENDIF(CMAKE_BUILD_TYPE MATCHES "Debug")

 IF(NOT APPLE)
  SET(CMAKE_SHARED_LINKER_FLAGS "-Wl,-Bsymbolic -g")
 ELSE(NOT APPLE)
  SET(CMAKE_SHARED_LINKER_FLAGS "-Wl -undefined dynamic_lookup")
  ADD_DEFINITIONS( "-Wno-overloaded-virtual" )
 ENDIF(NOT APPLE)

ENDIF(NOT WIN32)

# Add some definitions to satisfy MS
IF(WIN32)
    ADD_DEFINITIONS(-D__MSVC__)
    ADD_DEFINITIONS(-D_CRT_NONSTDC_NO_DEPRECATE -D_CRT_SECURE_NO_DEPRECATE)
ENDIF(WIN32)

# Let cmake find additional modules private
LIST(APPEND CMAKE_MODULE_PATH ${PROJECT_SOURCE_DIR})

SET(wxWidgets_USE_LIBS html base core net )

FIND_PACKAGE(OpenGL)
IF(OPENGL_GLU_FOUND)

    SET(wxWidgets_USE_LIBS ${wxWidgets_USE_LIBS} gl )
    INCLUDE_DIRECTORIES(${OPENGL_INCLUDE_DIR})

    MESSAGE (STATUS "Found OpenGL..." )
    MESSAGE (STATUS "    Lib: " ${OPENGL_LIBRARIES})
    MESSAGE (STATUS "    Include: " ${OPENGL_INCLUDE_DIR})
    ADD_DEFINITIONS(-DocpnUSE_GL)

ELSE(OPENGL_GLU_FOUND)
    MESSAGE (STATUS "OpenGL not found..." )
ENDIF(OPENGL_GLU_FOUND)

ADD_DEFINITIONS(-DTIXML_USE_STL)

SET(BUILD_SHARED_LIBS TRUE)
#SET( wxWidgets_USE_DEBUG ON)

FIND_PACKAGE(wxWidgets REQUIRED)

INCLUDE(${wxWidgets_USE_FILE})

FIND_PACKAGE(Gettext REQUIRED)

#  Do the version.h configuration into the build output directory,
#  thereby allowing building from a read-only source tree.
IF(NOT SKIP_VERSION_CONFIG)
    configure_file(version.h.in ${CMAKE_CURRENT_BINARY_DIR}${CMAKE_FILES_DIRECTORY}/include/version.h)
    INCLUDE_DIRECTORIES(${CMAKE_CURRENT_BINARY_DIR}${CMAKE_FILES_DIRECTORY}/include)
ENDIF(NOT SKIP_VERSION_CONFIG)

# For convenience we define the sources as a variable. You can add
# header files and cpp/c files and CMake will sort them out



SET(SRC_OESENC
            src/oesenc_pi.h
            src/oesenc_pi.cpp
            src/eSENCChart.cpp
            src/eSENCChart.h
            src/Osenc.cpp
            src/Osenc.h
            src/mygeom63.h
            src/mygeom63.cpp
            src/tri.c
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
            src/tinyxml.cpp
            src/tinyxmlerror.cpp
            src/tinyxmlparser.cpp
     )

SET(SRC_CPL
                src/cpl/cpl_config.h
                src/cpl/cpl_conv.h
                src/cpl/cpl_csv.h
                src/cpl/cpl_error.h
                src/cpl/cpl_port.h
                src/cpl/cpl_string.h
                src/cpl/cpl_vsi.h
                src/cpl/cpl_conv.cpp
                src/cpl/cpl_csv.cpp
                src/cpl/cpl_error.cpp
                src/cpl/cpl_findfile.cpp
                src/cpl/cpl_path.cpp
                src/cpl/cpl_string.cpp
                src/cpl/cpl_vsisimple.cpp
        )

SET(SRC_ISO8211
                src/myiso8211/ddffielddefn.cpp
                src/myiso8211/ddfmodule.cpp
                src/myiso8211/ddfrecord.cpp
                src/myiso8211/ddfsubfielddefn.cpp
                src/myiso8211/ddffield.cpp
                src/myiso8211/ddfutils.cpp
        )

SET(SRC_DSA
#                src/dsa/dsa_utils.cpp
#                src/dsa/mp_math.c
#                src/dsa/sha1.c
    )
    
SET(SRC_S57ENC
                src/mygdal/s57classregistrar.cpp
)

SET(SRC_JSON
            src/wxJSON/json_defs.h
            src/wxJSON/jsonreader.h
            src/wxJSON/jsonval.h
            src/wxJSON/jsonwriter.h
            src/wxJSON/jsonreader.cpp
            src/wxJSON/jsonval.cpp
            src/wxJSON/jsonwriter.cpp
        )


ADD_LIBRARY(${PACKAGE_NAME} SHARED ${SRC_OESENC} ${SRC_CPL} ${SRC_ISO8211} ${SRC_DSA} ${SRC_S57ENC} ${SRC_JSON})

IF(WIN32)
#  SET(OPENCPN_IMPORT_LIB "opencpn.lib")
#  SET(OPENCPN_IMPORT_LIB "E:/opencpn_cmake2010/${CMAKE_CFG_INTDIR}/opencpn.lib")
  SET(OPENCPN_IMPORT_LIB "C:/Projects/opencpn/build/${CMAKE_CFG_INTDIR}/opencpn.lib")
  TARGET_LINK_LIBRARIES( ${PACKAGE_NAME} ${OPENCPN_IMPORT_LIB} )
#  ADD_DEPENDENCIES(${PACKAGE_NAME} ${PARENT})
ENDIF(WIN32)

IF(NOT APPLE)
TARGET_LINK_LIBRARIES( ${PACKAGE_NAME} ${wxWidgets_LIBRARIES} )
ENDIF(NOT APPLE)



# Install any special data files or binary helpers here....
# The PlugIn library itself and any "data" directory (with contents) gets installed in PluginInstall.cmake later

IF(UNIX AND NOT APPLE)
SET(PARENT opencpn)
SET(PREFIX_PLUGINS ${LIB_INSTALL_DIR}/${PARENT})
SET(PREFIX_PARENTBIN bin)
SET(PREFIX_PARENTDATA ${PREFIX_DATA}/${PARENT})


IF (ARCH MATCHES "amd64" OR ARCH MATCHES "x86_64")
INSTALL(FILES "buildlinux64/oeserverd/oeserverd"
       PERMISSIONS OWNER_READ OWNER_EXECUTE OWNER_WRITE GROUP_READ GROUP_EXECUTE WORLD_READ WORLD_EXECUTE
       DESTINATION ${PREFIX_PARENTBIN})
ELSE (ARCH MATCHES "amd64" OR ARCH MATCHES "x86_64")

    IF (ARCH MATCHES "arm*" )
        INSTALL(FILES "buildlinuxarm/oeserverd/oeserverd"
          PERMISSIONS OWNER_READ OWNER_EXECUTE OWNER_WRITE GROUP_READ GROUP_EXECUTE WORLD_READ WORLD_EXECUTE
          DESTINATION ${PREFIX_PARENTBIN})
    ELSE (ARCH MATCHES "arm*" )
        INSTALL(FILES "buildlinux/oeserverd/oeserverd"
          PERMISSIONS OWNER_READ OWNER_EXECUTE OWNER_WRITE GROUP_READ GROUP_EXECUTE WORLD_READ WORLD_EXECUTE
          DESTINATION ${PREFIX_PARENTBIN})
    ENDIF (ARCH MATCHES "arm*" )

ENDIF (ARCH MATCHES "amd64" OR ARCH MATCHES "x86_64")


INSTALL(FILES "src/rrc_eula_ChartSetsForOpenCPN-v16-12.txt"
      PERMISSIONS OWNER_READ GROUP_READ WORLD_READ
      DESTINATION ${PREFIX_PARENTDATA}/plugins/${PACKAGE_NAME} )

ENDIF(UNIX AND NOT APPLE)

IF(WIN32)
    INSTALL(FILES "buildwin/oeserverd/oeserverd.exe"
       PERMISSIONS OWNER_EXECUTE OWNER_WRITE OWNER_READ WORLD_EXECUTE
       DESTINATION "plugins\\\\${PACKAGE_NAME}")

    INSTALL(TARGETS ${PACKAGE_NAME} RUNTIME DESTINATION "plugins")
    SET(INSTALL_DIRECTORY "plugins\\\\${PACKAGE_NAME}")

    INSTALL(FILES "src/rrc_eula_ChartSetsForOpenCPN-v16-12.txt"
      PERMISSIONS OWNER_READ GROUP_READ WORLD_READ
      DESTINATION ${INSTALL_DIRECTORY} )

ENDIF(WIN32)

IF(APPLE)
SET(CMAKE_INSTALL_PREFIX "/tmp")

INSTALL(FILES "buildosx/oeserverd/oeserverd"
       PERMISSIONS OWNER_EXECUTE OWNER_WRITE OWNER_READ WORLD_EXECUTE
       DESTINATION "oesenc_pi/oesenc_pi")

INSTALL(FILES "src/rrc_eula_ChartSetsForOpenCPN.txt"
      PERMISSIONS OWNER_READ GROUP_READ WORLD_READ
      DESTINATION "oesenc_pi/oesenc_pi" )

INSTALL(TARGETS ${PACKAGE_NAME} RUNTIME LIBRARY DESTINATION "oesenc_pi/oesenc_pi")
ENDIF(APPLE)

IF(APPLE)
    if (NOT EXISTS "${PROJECT_BINARY_DIR}/data/")
        file (MAKE_DIRECTORY "${PROJECT_BINARY_DIR}/data/")
        message ("Generating data directory")
        endif ()

   FILE(GLOB PACKAGE_DATA_FILES ${CMAKE_SOURCE_DIR}/data/*)

   FOREACH (_currentDataFile ${PACKAGE_DATA_FILES})
        MESSAGE (STATUS "copying: ${_currentDataFile}" )
        configure_file(${_currentDataFile} ${CMAKE_CURRENT_BINARY_DIR}/data COPYONLY)
   ENDFOREACH (_currentDataFile )

ENDIF(APPLE)

SET(CPACK_DEBIAN_PACKAGE_MAINTAINER "dsr")

INCLUDE("cmake/PluginInstall.cmake")
INCLUDE("cmake/PluginLocalization.cmake")
INCLUDE("cmake/PluginPackage.cmake")
