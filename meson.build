project('rdm4001', 'c', 'cpp', default_options: ['cpp_std=c++20'], meson_version: '>=1.2.0')

if get_option('vcpkg').enabled()
  vcpkg = find_program('vcpkg', required : true)
  run_command(vcpkg, 'install', '--x-wait-for-lock',
                                '--x-manifest-root=' + meson.current_source_dir())
endif

if get_option('buildtype').startswith('release')
   add_project_arguments('-Ofast', language:['cpp'])
endif

libtomcrypt = dependency('libtomcrypt', version: '>=1.18.2')
sdl2 = dependency('sdl2')
sdl2_ttf = dependency('SDL2_ttf')
openal = dependency('openal')
assimp = dependency('assimp')
enet = dependency('libenet')
bullet = dependency('bullet')
libsndfile = dependency('sndfile')
curl = dependency('libcurl')
mpv = dependency('mpv', required: get_option('mpv'))

common_proj = subproject('common')
common_dep = common_proj.get_variable('libcommon_dep')
obz_proj = subproject('obz', required: get_option('obz'))
obz_options = []
if not obz_proj.found()
  obz_dep = declare_dependency()
  obz_options = ['-DDISABLE_OBZ']
else
  obz_dep = obz_proj.get_variable('libobz_dep')
endif

shadercompiler_dep = subproject('shadercompiler')

options = ['-DDISABLE_EASY_PROFILER']
#options = []


inc_dirs = ['.', 'gfx/imgui', 'subprojects/common']

readline = dependency('', required: false)
game_deps = [libsndfile, enet, common_dep, obz_dep, sdl2, sdl2_ttf, openal, assimp, bullet, curl, libtomcrypt]

# HACK because bullet cant find LinearMath without this
if build_machine.system() == 'haiku'
  warning('Haiku experimental!')
  inc_dirs += ['/boot/system/develop/headers/bullet']
elif build_machine.system() == 'linux'
  if target_machine.system() == 'windows'
    readline = dependency('wineditline')
    inc_dirs += ['/ucrt64/include/bullet']
  else
    readline = dependency('libeditline')
    inc_dirs += ['/usr/include/bullet']
  endif
elif build_machine.system() == 'windows'
  readline = dependency('wineditline')
endif
game_deps += readline

if target_machine.system() == 'windows'
  cpp = meson.get_compiler('cpp')
  game_deps += cpp.find_library('shlwapi', required: true)
endif

if mpv.found()
  game_deps += mpv
  add_project_arguments('-DRDM4001_FEATURE_MPV', language: ['cpp'])
endif

inc = include_directories(inc_dirs)

xtra_src = []

if get_option('graphics').enabled()
  vulkan = dependency('vulkan', required: get_option('vulkan'))
  if vulkan.found() 
    game_deps += [vulkan]
    xtra_src += [
      'gfx/vk_context.cpp',
      'gfx/vk_context.hpp',
      'gfx/vk_device.cpp',
      'gfx/vk_device.hpp',
      'gfx/vk_types.cpp',
      'gfx/vk_types.hpp'
    ]
    add_project_arguments('-DRDM4001_FEATURE_VULKAN', language: ['cpp'])
  endif

  if get_option('openglmodern').enabled()
    glad_proj = subproject('glad')
    glad_dep = glad_proj.get_variable('libglad_dep')
    game_deps += [glad_dep]
    xtra_src += [
      'gfx/gl_context.cpp',
      'gfx/gl_context.hpp',
      'gfx/gl_device.cpp',
      'gfx/gl_device.hpp',
      'gfx/gl_types.cpp',
      'gfx/gl_types.hpp',
    ]
    add_project_arguments('-DRDM4001_FEATURE_GLMODERN', language: ['cpp'])
  endif

  add_project_arguments('-DGRAPHICS_ENABLED', language: ['cpp'])
endif

gamelib = static_library('game', [ 
  'console.cpp',
  'console.hpp',
  'game.cpp',
  'game.hpp',
  'graph.cpp',
  'graph.hpp',
  'http.cpp',
  'http.hpp',
  'state.cpp',
  'state.hpp',
  'input.cpp',
  'input.hpp',
  'scheduler.cpp',
  'scheduler.hpp',
  'security.cpp',
  'security.hpp',
  'settings.cpp',
  'settings.hpp',
  'signal.cpp',
  'signal.hpp',
  'world.cpp',
  'world.hpp',
  'sound.cpp',
  'sound.hpp',
  'physics.cpp',
  'physics.hpp',
  'profiler.cpp',
  'profiler.hpp',
  'resource.cpp',
  'resource_model.cpp',
  'resource_texture.cpp',
  'resource.hpp',
  'fun.cpp',
  'fun.hpp',

  'script/api.cpp',
  'script/api.hpp',
  'script/my_basic.c',
  'script/my_basic.h',
  'script/script.cpp',
  'script/script.hpp',
  'script/context.cpp',
  'script/context.hpp',

  'putil/fpscontroller.cpp',
  'putil/fpscontroller.hpp',

  'network/bitstream.cpp',
  'network/bitstream.hpp',
  'network/network.cpp',
  'network/network.hpp',
  'network/entity.cpp',
  'network/entity.hpp',
  'network/player.cpp',
  'network/player.hpp',

  'gfx/base_context.cpp',
  'gfx/base_context.hpp',
  'gfx/base_device.cpp',
  'gfx/base_device.hpp',
  'gfx/base_types.cpp',
  'gfx/base_types.hpp',

  'gfx/gui/api.cpp',
  'gfx/gui/api.hpp',
  'gfx/gui/gui.cpp',
  'gfx/gui/gui.hpp',
  'gfx/gui/ngui.cpp',
  'gfx/gui/ngui.hpp',
  'gfx/gui/ngui_window.cpp',
  'gfx/gui/ngui_window.hpp',
  'gfx/gui/font.cpp',
  'gfx/gui/font.hpp',

  'gfx/imgui/imconfig.h',
  'gfx/imgui/imstb_truetype.h',
  'gfx/imgui/imstb_textedit.h',
  'gfx/imgui/imstb_rectpack.h',
  'gfx/imgui/imgui_internal.h',
  'gfx/imgui/imgui.h',
  'gfx/imgui/imgui.cpp',
  'gfx/imgui/imgui_widgets.cpp',
  'gfx/imgui/imgui_tables.cpp',
  'gfx/imgui/imgui_draw.cpp',
  'gfx/imgui/backends/imgui_impl_opengl3.cpp',
  'gfx/imgui/backends/imgui_impl_sdl2.cpp',

  'gfx/apis.cpp',
  'gfx/apis.hpp',
  'gfx/camera.cpp',
  'gfx/camera.hpp',
  'gfx/engine.cpp',
  'gfx/engine.hpp',
  'gfx/entity.cpp',
  'gfx/entity.hpp',
  'gfx/rendercommand.cpp',
  'gfx/rendercommand.hpp',
  'gfx/renderpass.cpp',
  'gfx/renderpass.hpp',
  'gfx/heightmap.cpp',
  'gfx/heightmap.hpp',
  'gfx/material.cpp',
  'gfx/material.hpp',
  'gfx/mesh.cpp',
  'gfx/mesh.hpp',
  'gfx/video.cpp',
  'gfx/video.hpp',
  'gfx/viewport.cpp',
  'gfx/viewport.hpp',
  'gfx/stb_image.h',

  xtra_src
], cpp_args: options + obz_options, include_directories: [inc], dependencies: game_deps)

rdm4001_dep = declare_dependency(include_directories: inc,
				 link_with: gamelib,
                                 dependencies: game_deps)

suite = executable('testsuite',
           [ 'test/main.cpp',
           'test/testsystem.cpp',
           'test/testsystem.hpp',
           'test/testgame.cpp',
           'test/testgame.hpp',
           
           'test/render.cpp',
           ],
           dependencies: [rdm4001_dep], link_with: gamelib)
test('Base', suite, args: ['--group=Base'])
test('Render', suite, args: ['--group=Render'])

if get_option('compile_experiments').enabled()
  executable('raymarcher', [
    'raymarcher/main.cpp',
    'raymarcher/rgame.hpp',
    'raymarcher/rgame.cpp'
  ], include_directories: [inc], dependencies: [common_dep, sdl2], link_with: gamelib)

  b3geometry_dep = declare_dependency(link_args : ['-lBullet3Geometry', '-lBullet3Common'] ) 
  executable('roadtrip', [
    'roadtrip/main.cpp',
    'roadtrip/roadtrip.hpp',
    'roadtrip/roadtrip.cpp',
    'roadtrip/america.hpp',
    'roadtrip/america.cpp',
    'roadtrip/pawn.hpp',
    'roadtrip/pawn.cpp',
  ], cpp_args: options, include_directories: [inc], dependencies: [common_dep, sdl2, b3geometry_dep], link_with: gamelib)
endif 
