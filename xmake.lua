add_rules("mode.debug", "mode.release")

add_rules("plugin.compile_commands.autoupdate")

set_languages("cxx17")

add_requires("flecs", "glad", "glfw", "nanovg", "tracy")
add_requires("zlib-ng", { configs = { zlib_compat = true } })

target("snapshot")
  set_kind("static")
  add_files("snapshot/source/*.cpp")
  add_includedirs("snapshot/include", { public = true })
  add_packages("flecs", { public = true })
  add_packages("zlib-ng")

target("event_sourcing")
  set_kind("static")
  add_files("event_sourcing/src/*.cpp")
  add_includedirs("event_sourcing/include")
  add_deps("snapshot")
  add_defines("NANOVG_GL2_IMPLEMENTATION", { public = true })

target("neo_timeline")
  set_kind("binary")
  add_files("neo_timeline/src/*.cpp")
  add_includedirs("neo_timeline/include")
  add_deps("event_sourcing")
  add_packages("flecs", "glad", "glfw", "nanovg", "tracy")
  set_configdir("$(builddir)/$(plat)/$(arch)/$(mode)")
  add_configfiles("neo_timeline/(assets/**.png)", { onlycopy = true })
  add_configfiles("neo_timeline/(assets/**.ttf)", { onlycopy = true })
  