#
# Install the pre-compiled oeserverd DRM module + helper libraries.
#
macro (install_libs libs)
  if (APPLE)
    set(_oe_libdir "OpenCPN.app/Contents/SharedSupport/plugins/${PACKAGE_NAME}")
  elseif (WIN32)
    set(_oe_libdir "plugins")
  elseif (UNIX)
    set(_oe_libdir "lib/opencpn/")
  endif ()
  foreach(lib ${ARGV})
    install(
      FILES ${lib}
      PERMISSIONS OWNER_READ OWNER_EXECUTE OWNER_WRITE GROUP_READ GROUP_EXECUTE
                  WORLD_READ WORLD_EXECUTE
      DESTINATION ${CMAKE_INSTALL_PREFIX}/${_oe_libdir}
    )
  endforeach ()
endmacro () 

if (EXISTS "/etc/udev/rules.d")
  install(
    FILES "linux/98-sglock.rules"
    PERMISSIONS OWNER_READ OWNER_WRITE GROUP_READ WORLD_READ
    DESTINATION "${CMAKE_INSTALL_PREFIX}/etc/udev/rules.d"
  )
endif ()

if (QT_ANDROID)
  install_libs(${CMAKE_CURRENT_LIST_DIR}/oeserverda_arm/oeserverda)
elseif (APPLE)
  install_libs("osx/oeserverd" "osx/libsglmac-2.29.0.0.dylib.x86_64")
  # We also need to copy the helper utility and required dylibs to the binary
  # build directory so that the PACKAGES scripts will find it.
  configure_file(
    ${CMAKE_CURRENT_LIST_DIR}/osx/oeserverd
    ${CMAKE_BINARY_DIR}/oeserverd COPYONLY
  )
  configure_file(
    ${CMAKE_CURRENT_LIST_DIR}/osx/libsglmac-2.29.0.0.dylib.x86_64
    ${CMAKE_BINARY_DIR}/libsglmac-2.29.0.0.dylib.x86_64 COPYONLY
  )
elseif (UNIX AND ARCH  STREQUAL "x86_64")
  install_libs(${CMAKE_CURRENT_LIST_DIR}/linux64/oeserverd)
  install_libs(${CMAKE_CURRENT_LIST_DIR}/linux64/libsgllnx64-2.29.02.so)
elseif (UNIX AND ARCH STREQUAL "armhf")
  install_libs(${CMAKE_CURRENT_LIST_DIR}/linuxarm/oeserverd)
  install_libs(${CMAKE_CURRENT_LIST_DIR}/linuxarm/libsglarmhf32-2.30.0.0.so)
elseif (UNIX AND ARCH STREQUAL "armh64")
  install_libs(${CMAKE_CURRENT_LIST_DIR}/linuxarm64/oeserverd)
  install_libs(${CMAKE_CURRENT_LIST_DIR}/linuxarm/libsglarm64-2.31.0.0.so)
elseif (UNIX)
  # 32-bit amd64: is this really useful?
  install_libs(${CMAKE_CURRENT_LIST_DIR}/linux/oeserverd)
  install_libs(${CMAKE_CURRENT_LIST_DIR}/linux/libsgllnx-2.29.0.1.so)
elseif (WIN32)
  file(GLOB vcdll_files "${CMAKE_CURRENT_LIST}/win//*.dll")
  install_libs(${vcdll_files} "win/oeserverd.exe")
endif ()
