# The 32-bit version of this project is only for the Unity plugin,
# which does not directly relate to Kinects.
# Also, azure_kinect currently imports a 64-bit .lib file.
if(CMAKE_SIZEOF_VOID_P EQUAL 8)
  add_subdirectory(azure_kinect)
endif()

set(SOURCE_FILES
  kh_core.h
  kh_core.cpp
  kh_vp8.h
  kh_vp8_encoder.cpp
  kh_vp8_decoder.cpp
  kh_rvl.h
  kh_rvl.cpp
  kh_message.h
  kh_message.cpp
  kh_sender.h
  kh_sender.cpp
  kh_receiver.h
  kh_receiver.cpp
)

add_library(KinectToHololens ${SOURCE_FILES})

target_link_libraries(KinectToHololens
  ${FFMPEG_LIBRARIES}
)
set_target_properties(KinectToHololens PROPERTIES
  CXX_STANDARD 17
)