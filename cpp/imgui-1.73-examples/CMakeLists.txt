add_library(ImguiExamples
  imgui_impl_glfw.h
  imgui_impl_glfw.cpp
  imgui_impl_opengl3.h
  imgui_impl_opengl3.cpp
 )
target_link_libraries(ImguiExamples
  glfw
  unofficial::gl3w::gl3w
)
set_target_properties(ImguiExamples PROPERTIES
  CXX_STANDARD 17
)
