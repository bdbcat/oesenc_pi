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

if (WIN32 OR MINGW)
  if (OPENGL_GLU_FOUND)
    target_link_libraries(_LIBGLU INTERFACE OpenGL::GLU)
  else ()
    message(WARNING "OpenGL GLU library not found.")
  endif ()
endif ()
