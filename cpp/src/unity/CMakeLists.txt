# Source files for the Unity plugin.
set(SOURCE_FILES
  plugin.def
  plugin.cpp
  kh_plugin.cpp
  texture_group.h
  texture_group.cpp
  channel_texture.h
  channel_texture.cpp
  depth_texture.h
  depth_texture.cpp
  interfaces/IUnityGraphics.h
  interfaces/IUnityGraphicsD3D11.h
  interfaces/IUnityInterface.h
)

add_library(KinectToHololensUnity SHARED
  ${SOURCE_FILES}
)
target_link_libraries(KinectToHololensUnity
  KinectToHololens
  Opus::opus
)
set_target_properties(KinectToHololensUnity PROPERTIES
  CXX_STANDARD 17
)