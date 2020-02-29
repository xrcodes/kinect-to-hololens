add_library(KinectToHololensNative
  kh_udp_socket.h
  kh_udp_socket.cpp
  kh_packet.h
  kh_packet.cpp
  kh_time.h
)
target_include_directories(KinectToHololensNative PRIVATE
  "${AZURE_KINECT_DIR}/sdk/include"
)
target_link_libraries(KinectToHololensNative
  KinectToHololens
)
set_target_properties(KinectToHololensNative PROPERTIES
  CXX_STANDARD 17
)