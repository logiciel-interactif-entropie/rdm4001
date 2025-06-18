project('rdm4001', 'c', 'cpp', default_options: ['cpp_std=c++20'])

if get_option('vcpkg').enabled()
  vcpkg = find_program('vcpkg', required : true)
  run_command(vcpkg, 'install', '--x-wait-for-lock',
                                '--x-manifest-root=' + meson.current_source_dir())
endif

if get_option('buildtype').startswith('release')
   add_project_arguments('-Ofast', language:['cpp'])
endif

sdl2 = dependency('sdl2')
sdl2_ttf = dependency('sdl2_ttf')
glm = dependency('glm')
openal = dependency('openal')
assimp = dependency('assimp')
enet = dependency('libenet')
easy_profiler = dependency('easy_profiler')
bullet = dependency('Bullet')
libsndfile = dependency('sndfile')
curl = dependency('libcurl')
boost_dep = dependency('boost', modules: ['program_options'])
readline = dependency('readline')

common_proj = subproject('common')
common_dep = common_proj.get_variable('libcommon_dep')
obz_proj = subproject('obz', required: false)
obz_options = []
if not obz_proj.found()
  obz_dep = declare_dependency()
  obz_options = ['-DDISABLE_OBZ']
else
  obz_dep = obz_proj.get_variable('libobz_dep')
endif

options = ['-DDISABLE_EASY_PROFILER']
#options = []

glad_proj = subproject('glad')
glad_dep = glad_proj.get_variable('libglad_dep')

inc = include_directories(['.', 'gfx/imgui', '/usr/include/bullet', 'subprojects/common'])

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

  'gfx/gui/api.cpp',
  'gfx/gui/api.hpp',
  'gfx/gui/gui.cpp',
  'gfx/gui/gui.hpp',
  'gfx/gui/ngui.cpp',
  'gfx/gui/ngui.hpp',
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

  'gfx/base_context.cpp',
  'gfx/base_context.hpp',
  'gfx/base_device.cpp',
  'gfx/base_device.hpp',
  'gfx/base_types.cpp',
  'gfx/base_types.hpp',

  'gfx/gl_context.cpp',
  'gfx/gl_context.hpp',
  'gfx/gl_device.cpp',
  'gfx/gl_device.hpp',
  'gfx/gl_types.cpp',
  'gfx/gl_types.hpp',
], cpp_args: options + obz_options, include_directories: [inc], dependencies: [libsndfile, boost_dep, enet, common_dep, glad_dep, obz_dep, sdl2, sdl2_ttf, glm, openal, assimp, bullet, easy_profiler, curl, readline])

rdm4001_dep = declare_dependency(include_directories: inc,
				 link_with: gamelib)

executable('launcher', [
  'launcher.cpp'
], include_directories: [inc], dependencies: [common_dep], link_with: gamelib)

executable('raymarcher', [
  'raymarcher/main.cpp',
  'raymarcher/rgame.hpp',
  'raymarcher/rgame.cpp'
], include_directories: [inc], dependencies: [common_dep, sdl2, glm], link_with: gamelib)

b3geometry_dep = declare_dependency(link_args : ['-lBullet3Geometry', '-lBullet3Common'] ) 
executable('roadtrip', [
  'roadtrip/main.cpp',
  'roadtrip/roadtrip.hpp',
  'roadtrip/roadtrip.cpp',
  'roadtrip/america.hpp',
  'roadtrip/america.cpp',
  'roadtrip/pawn.hpp',
  'roadtrip/pawn.cpp',
], cpp_args: options, include_directories: [inc], dependencies: [common_dep, sdl2, glm, b3geometry_dep, easy_profiler], link_with: gamelib)
