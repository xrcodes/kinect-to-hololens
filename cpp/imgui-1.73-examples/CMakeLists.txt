add_library(ImguiExamples
  imgui_impl_win32.h
  imgui_impl_win32.cpp
  imgui_impl_dx11.h
  imgui_impl_dx11.cpp
 )
target_link_libraries(ImguiExamples
  d3d11.lib
)
set_target_properties(ImguiExamples PROPERTIES
  CXX_STANDARD 17
)
