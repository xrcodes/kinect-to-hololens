# The 32-bit version of this project is only for the Unity plugin,
# which does not directly relate to Kinects.
# Also, azure_kinect currently imports a 64-bit .lib file.

set(SOURCE_FILES
  kh_core.h
  kh_core.cpp
  kh_vp8.h
  kh_vp8_encoder.cpp
  kh_vp8_decoder.cpp
  kh_rvl.h
  kh_rvl.cpp
  kh_trvl.h
  kh_trvl.cpp
  kh_sender_udp.h
  kh_sender_udp.cpp
  kh_receiver_udp.h
  kh_receiver_udp.cpp
)

add_library(KinectToHololens ${SOURCE_FILES})

target_include_directories(KinectToHololens PRIVATE
  "${AZURE_KINECT_DIR}/sdk/include"
)
target_link_libraries(KinectToHololens
  ${FFMPEG_LIBRARIES}
)
set_target_properties(KinectToHololens PROPERTIES
  CXX_STANDARD 17
)