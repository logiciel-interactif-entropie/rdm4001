project('glad', 'c')

inc = include_directories('include')
libglad = static_library('glad',
                         ['src/gl.c',
                          'src/egl.c',
   'include/glad/egl.h',
   'include/glad/gl.h',
   'include/EGL/eglplatform.h',
   'include/KHR/khrplatform.h'],
  include_directories: inc)

libglad_dep = declare_dependency(include_directories: inc, link_with: libglad)
