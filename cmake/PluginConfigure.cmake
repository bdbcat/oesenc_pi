##---------------------------------------------------------------------------
## Author:      Sean D'Epagnier
## Copyright:
## License:     GPLv3+
##---------------------------------------------------------------------------

# This should be 2.8.0 to have FindGTK2 module

MESSAGE (STATUS "*** Staging to build ${PACKAGE_NAME} ***")

include  ("VERSION.cmake")

#  Do the version.h configuration into the build output directory,
#  thereby allowing building from a read-only source tree.
IF(NOT SKIP_VERSION_CONFIG)
    configure_file(${PROJECT_SOURCE_DIR}/cmake/version.h.in ${CMAKE_CURRENT_BINARY_DIR}${CMAKE_FILES_DIRECTORY}/include/version.h)
    INCLUDE_DIRECTORIES(${CMAKE_CURRENT_BINARY_DIR}${CMAKE_FILES_DIRECTORY}/include)
ENDIF(NOT SKIP_VERSION_CONFIG)

SET(PACKAGE_VERSION "${VERSION_MAJOR}.${VERSION_MINOR}.${VERSION_PATCH}.${VERSION_TWEAK}" )


#SET(CMAKE_BUILD_TYPE Debug)
#SET(CMAKE_VERBOSE_MAKEFILE ON)

INCLUDE_DIRECTORIES(${PROJECT_SOURCE_DIR}/include ${PROJECT_SOURCE_DIR}/src)

# SET(PROFILING 1)

#  IF NOT DEBUGGING CFLAGS="-O2 -march=native"
IF(NOT MSVC)
 IF(PROFILING)
  ADD_DEFINITIONS( "-Wall -g -fprofile-arcs -ftest-coverage -fexceptions" )
 ELSE(PROFILING)
#  ADD_DEFINITIONS( "-Wall -g -fexceptions" )
 ADD_DEFINITIONS( "-Wall -Wno-unused-result -g -fexceptions" )
 ENDIF(PROFILING)

 IF(NOT APPLE)
  SET(CMAKE_SHARED_LINKER_FLAGS "-Wl,-Bsymbolic")
 ELSE(NOT APPLE)
  SET(CMAKE_SHARED_LINKER_FLAGS "-Wl -undefined dynamic_lookup")
 ENDIF(NOT APPLE)

ENDIF(NOT MSVC)

# Add some definitions to satisfy MS
IF(MSVC)
    ADD_DEFINITIONS(-D__MSVC__)
    ADD_DEFINITIONS(-D_CRT_NONSTDC_NO_DEPRECATE -D_CRT_SECURE_NO_DEPRECATE)
ENDIF(MSVC)

IF(NOT DEFINED wxWidgets_USE_FILE)
  SET(wxWidgets_USE_LIBS base core net xml html adv aui)
ENDIF(NOT DEFINED wxWidgets_USE_FILE)


#  QT_ANDROID is a cross-build, so the native FIND_PACKAGE(wxWidgets...) and wxWidgets_USE_FILE is not useful.
#  We add the dependencies manually.
IF(QT_ANDROID)
  ADD_DEFINITIONS(-D__WXQT__)
  ADD_DEFINITIONS(-D__OCPN__ANDROID__)
  ADD_DEFINITIONS(-DOCPN_USE_WRAPPER)
  ADD_DEFINITIONS(-DANDROID)

  SET(CMAKE_CXX_FLAGS "-pthread -fPIC -O2 -g")

  ## Compiler flags
  SET(CMAKE_EXE_LINKER_FLAGS "-s")  ## Strip binary

  ADD_DEFINITIONS(-DocpnUSE_GLES)
  ADD_DEFINITIONS(-DocpnUSE_GL)
  ADD_DEFINITIONS(-DARMHF)

  SET(OPENGLES_FOUND "YES")
  SET(OPENGL_FOUND "YES")
    
  MESSAGE (STATUS "Using GLESv2 for Android")
  ADD_DEFINITIONS(-DUSE_ANDROID_GLES2)
  ADD_DEFINITIONS(-DUSE_GLSL)
  ADD_DEFINITIONS("-Wno-inconsistent-missing-override -Wno-potentially-evaluated-expression")
  SET(QT_LINUX "OFF")
  SET(QT "ON")
  SET(CMAKE_SKIP_BUILD_RPATH  TRUE)
  ADD_DEFINITIONS(-DQT_WIDGETS_LIB)

  IF(_wx_selected_config MATCHES "androideabi-qt-arm64")
   INCLUDE_DIRECTORIES("${OCPN_Android_Common}/qt5/build_arm64_O3/qtbase/include")
   INCLUDE_DIRECTORIES("${OCPN_Android_Common}/qt5/build_arm64_O3/qtbase/include/QtCore")
   INCLUDE_DIRECTORIES("${OCPN_Android_Common}/qt5/build_arm64_O3/qtbase/include/QtWidgets")
   INCLUDE_DIRECTORIES("${OCPN_Android_Common}/qt5/build_arm64_O3/qtbase/include/QtGui")
   INCLUDE_DIRECTORIES("${OCPN_Android_Common}/qt5/build_arm64_O3/qtbase/include/QtOpenGL")
   INCLUDE_DIRECTORIES("${OCPN_Android_Common}/qt5/build_arm64_O3/qtbase/include/QtTest")

   INCLUDE_DIRECTORIES( "${OCPN_Android_Common}/wxWidgets/libarm64/wx/include/arm-linux-androideabi-qt-unicode-static-3.1")
   INCLUDE_DIRECTORIES( "${OCPN_Android_Common}/wxWidgets/include")

   
   SET(OCPN_Core_LIBRARIES
                ${CMAKE_CURRENT_SOURCE_DIR}/${OCPN_Android_Common}/qt5/build_arm64_O3/qtbase/lib/libQt5Core.so
                ${CMAKE_CURRENT_SOURCE_DIR}/${OCPN_Android_Common}/qt5/build_arm64_O3/qtbase/lib/libQt5OpenGL.so
                ${CMAKE_CURRENT_SOURCE_DIR}/${OCPN_Android_Common}/qt5/build_arm64_O3/qtbase/lib/libQt5Widgets.so
                ${CMAKE_CURRENT_SOURCE_DIR}/${OCPN_Android_Common}/qt5/build_arm64_O3/qtbase/lib/libQt5Gui.so
                ${CMAKE_CURRENT_SOURCE_DIR}/${OCPN_Android_Common}/qt5/build_arm64_O3/qtbase/lib/libQt5AndroidExtras.so

                libGLESv2.so
                libEGL.so
                )

  ELSE(_wx_selected_config MATCHES "androideabi-qt-arm64")
   INCLUDE_DIRECTORIES("${OCPN_Android_Common}/qt5/build_arm32_19_O3/qtbase/include")
   INCLUDE_DIRECTORIES("${OCPN_Android_Common}/qt5/build_arm32_19_O3/qtbase/include/QtCore")
   INCLUDE_DIRECTORIES("${OCPN_Android_Common}/qt5/build_arm32_19_O3/qtbase/include/QtWidgets")
   INCLUDE_DIRECTORIES("${OCPN_Android_Common}/qt5/build_arm32_19_O3/qtbase/include/QtGui")
   INCLUDE_DIRECTORIES("${OCPN_Android_Common}/qt5/build_arm32_19_O3/qtbase/include/QtOpenGL")
   INCLUDE_DIRECTORIES("${OCPN_Android_Common}/qt5/build_arm32_19_O3/qtbase/include/QtTest")

   INCLUDE_DIRECTORIES( "${OCPN_Android_Common}/wxWidgets/libarmhf/wx/include/arm-linux-androideabi-qt-unicode-static-3.1")
   INCLUDE_DIRECTORIES( "${OCPN_Android_Common}/wxWidgets/include")

   ADD_DEFINITIONS( -DOCPN_ARMHF )
  
   SET(OCPN_Core_LIBRARIES
                ${CMAKE_CURRENT_SOURCE_DIR}/${OCPN_Android_Common}/qt5/build_arm32_19_O3/qtbase/lib/libQt5Core.so
                ${CMAKE_CURRENT_SOURCE_DIR}/${OCPN_Android_Common}/qt5/build_arm32_19_O3/qtbase/lib/libQt5OpenGL.so
                ${CMAKE_CURRENT_SOURCE_DIR}/${OCPN_Android_Common}/qt5/build_arm32_19_O3/qtbase/lib/libQt5Widgets.so
                ${CMAKE_CURRENT_SOURCE_DIR}/${OCPN_Android_Common}/qt5/build_arm32_19_O3/qtbase/lib/libQt5Gui.so
                ${CMAKE_CURRENT_SOURCE_DIR}/${OCPN_Android_Common}/qt5/build_arm32_19_O3/qtbase/lib/libQt5AndroidExtras.so

                libGLESv2.so
                libEGL.so
                )
  
  ENDIF(_wx_selected_config MATCHES "androideabi-qt-arm64")
ENDIF(QT_ANDROID)


IF(MSYS)
# this is just a hack. I think the bug is in FindwxWidgets.cmake
STRING( REGEX REPLACE "/usr/local" "\\\\;C:/MinGW/msys/1.0/usr/local" wxWidgets_INCLUDE_DIRS ${wxWidgets_INCLUDE_DIRS} )
ENDIF(MSYS)

#  QT_ANDROID is a cross-build, so the native FIND_PACKAGE(OpenGL) is not useful.
#
IF (NOT QT_ANDROID )
FIND_PACKAGE(OpenGL)
IF(OPENGL_GLU_FOUND)

    SET(wxWidgets_USE_LIBS ${wxWidgets_USE_LIBS} gl)
    INCLUDE_DIRECTORIES(${OPENGL_INCLUDE_DIR})

    MESSAGE (STATUS "Found OpenGL..." )
    #MESSAGE (STATUS "    Lib: " ${OPENGL_LIBRARIES})
    #MESSAGE (STATUS "    Include: " ${OPENGL_INCLUDE_DIR})
    ADD_DEFINITIONS(-DocpnUSE_GL)
ELSE(OPENGL_GLU_FOUND)
    MESSAGE (STATUS "OpenGL not found..." )
ENDIF(OPENGL_GLU_FOUND)
ENDIF(NOT QT_ANDROID)


IF (NOT QT_ANDROID )
    if(WXWIDGETS_FORCE_VERSION)
        set (wxWidgets_CONFIG_OPTIONS --version=${WXWIDGETS_FORCE_VERSION})
    endif()
    find_package(wxWidgets COMPONENTS ${wxWidgets_USE_LIBS})
    INCLUDE(${wxWidgets_USE_FILE})
ENDIF (NOT QT_ANDROID )


ADD_DEFINITIONS(-DBUILDING_PLUGIN)


SET(BUILD_SHARED_LIBS TRUE)

FIND_PACKAGE(Gettext REQUIRED)

