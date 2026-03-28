add_rules("mode.debug", "mode.release")

set_languages("cxx17")

add_requires("flecs", "glad", "glfw", "nanovg", "tracy")
add_requires("zlib-ng", { configs = { zlib_compat = true } })

target("event_sourcing")
  set_kind("static")
  add_files("event_sourcing/src/*.cpp")
  add_defines("NANOVG_GL2_IMPLEMENTATION", { public = true })
  add_includedirs("event_sourcing/include", { public = true })
  add_packages("flecs", "zlib-ng", { public = true })

target("neo_timeline")
  set_kind("binary")
  add_files("neo_timeline/src/*.cpp")
  add_deps("event_sourcing")
  add_packages("flecs", "glad", "glfw", "nanovg", "tracy")
