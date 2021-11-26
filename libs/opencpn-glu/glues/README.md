The glues directory is from https://github.com/lunixbochs/glues, verbatim.

The CMakeLists.txt in this directory is used instead of the  very old 
one in the original sources. It exports a glues::glu link target which 
includes the "GL/glu.h" header. 

Only libtess and libutil are built. Addding libnurbs works out of the box.
The top-level glues functions whichm depends on GL/gl.h, not available on
Android. It might tbe possible to build against GLES/gl.h
