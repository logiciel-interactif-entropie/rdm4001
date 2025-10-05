project('common', 'cpp')

inc = include_directories('.')
liblzma = dependency('liblzma')

libcommon = static_library('common',
  ['filesystem.cpp',
   'filesystem.hpp',
   'pak_file.cpp',
   'pak_file.hpp',
   'guid.cpp',
   'guid.hpp',
   'json.hpp',
   'logging.cpp',
   'logging.hpp',
   'random.cpp',
   'random.hpp',
   'rapidxml.hpp',
   'rapidxml_iterators.hpp',
   'rapidxml_print.hpp',
   'rapidxml_utils.hpp'],
  include_directories: inc, dependencies: liblzma)

omp = dependency('openmp')

pakfilecreator = executable('pak_file_creator',
                            'pak_file_creator.cpp',
                            link_with: libcommon, dependencies: omp)
libcommon_dep = declare_dependency(include_directories: inc, link_with: libcommon)
