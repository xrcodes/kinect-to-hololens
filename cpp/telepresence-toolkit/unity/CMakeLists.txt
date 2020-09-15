# Source files for the Unity plugin.
set(SOURCE_FILES
  graphics.def
  channel_texture.h
  channel_texture.cpp
  depth_texture.h
  depth_texture.cpp
  plugin.cpp
  graphics.cpp
  texture_group.h
  texture_group.cpp
  two_channel_texture.h
  two_channel_texture.cpp
  external/IUnityGraphics.h
  external/IUnityGraphicsD3D11.h
  external/IUnityInterface.h
)

add_library(TelepresenceToolkitUnity SHARED
  ${SOURCE_FILES}
)
target_include_directories(TelepresenceToolkitUnity PRIVATE
  "${OPUS_DIR}/include"
)
target_link_libraries(TelepresenceToolkitUnity
  TelepresenceToolkitCore
)
set_target_properties(TelepresenceToolkitUnity PROPERTIES
  CXX_STANDARD 17
)