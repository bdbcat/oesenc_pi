#
# One Debian/armhf bundle libGLU.so with the plugin to resolve
# an indirect dependency. Since the dependency is indirect, there
# is no need to link to headers or library. Despite this, an
# empty interface library is defined for symmetry.
#
# Exports
#    ocpn:glu       Interface link library (empty).
#
# Bundles:
#    libGLU.so on raspbian

cmake_minimum_required(VERSION 3.1)

include(PluginSetup)

if (TARGET ocpn::glu)
    return ()
endif ()

add_library(_LIBGLU INTERFACE)
add_library(ocpn::glu ALIAS _LIBGLU)

if (OPENGL_GLU_FOUND)
  target_link_libraries(_LIBGLU INTERFACE OpenGL::GLU)
else ()
  message(WARNING "OpenGL GLU library not found.")
endif ()

if ("${plugin_target}" STREQUAL  "raspbian")
  file(GLOB _glulibs /usr/lib/*/libGLU.so.* /usr/lib*/libGLU.so.*)
  if (NOT "${_glulibs}" STREQUAL "")
    install(CODE
      "execute_process(COMMAND cmake -E copy ${_glulibs} app/files/lib)"
    )
    message(STATUS "Bundling libGLU.so on Raspbian build.")
  else ()
    message(STATUS "Raspbian: Cannot locate libGLU library to bundle")
  endif ()
endif ()
