# Source files for the Unity plugin.
set(SOURCE_FILES
  plugin.def
  plugin.cpp
  kh_plugin.cpp
  texture_group.h
  texture_group.cpp
  channel_texture.h
  channel_texture.cpp
  two_channel_texture.h
  two_channel_texture.cpp
  depth_texture.h
  depth_texture.cpp
  interfaces/IUnityGraphics.h
  interfaces/IUnityGraphicsD3D11.h
  interfaces/IUnityInterface.h
)

add_library(KinectToHololensUnity SHARED
  ${SOURCE_FILES}
)
target_include_directories(KinectToHololensUnity PRIVATE
  "${OPUS_DIR}/include"
)
target_link_libraries(KinectToHololensUnity
  KinectToHololens
)
set_target_properties(KinectToHololensUnity PROPERTIES
  CXX_STANDARD 17
)