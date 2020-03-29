add_library(KinectToHololensNative
  kh_udp_socket.h
  kh_udp_socket.cpp
  kh_packet.h
  kh_packet.cpp
  kh_time.h
  kh_soundio.h
  kh_soundio.cpp
  kh_kinect_device.h
  kh_kinect_device.cpp
)
target_include_directories(KinectToHololensNative PRIVATE
  "${AZURE_KINECT_DIR}/sdk/include"
)
target_link_libraries(KinectToHololensNative
  KinectToHololens
  libsoundio::libsoundio
)
set_target_properties(KinectToHololensNative PROPERTIES
  CXX_STANDARD 17
)