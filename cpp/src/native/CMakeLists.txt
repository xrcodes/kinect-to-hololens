add_library(KinectToHololensNative
  imgui_wrapper.h
  kh_native.h
  opencv_utils.h
  opencv_utils.cpp
  packets.h
  packets.cpp
  profiler.h
  soundio_utils.h
  soundio_utils.cpp
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
  libsoundio::libsoundio
  ${Libvpx_LIB}
  opencv_highgui
)
set_target_properties(KinectToHololensNative PROPERTIES
  CXX_STANDARD 17
)