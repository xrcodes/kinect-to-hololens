add_library(KinectToHololensNative
  kh_native.h
  packets.h
  packets.cpp
  profiler.h
  udp_socket.h
  udp_socket.cpp
  vp8_encoder.h
  vp8_encoder.cpp
)
target_include_directories(KinectToHololensNative PRIVATE
  "${AZURE_KINECT_DIR}/sdk/include"
)
target_link_libraries(KinectToHololensNative
  TelepresenceToolkitCore
  ${Libvpx_LIB}
)
set_target_properties(KinectToHololensNative PROPERTIES
  CXX_STANDARD 17
)