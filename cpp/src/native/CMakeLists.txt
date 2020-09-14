add_library(KinectToHololensNative
  imgui_wrapper.h
  kh_kinect.h
  kh_kinect.cpp
  kh_native.h
  kh_opencv.h
  kh_opencv.cpp
  packet.h
  packet.cpp
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
  "${AZURE_KINECT_LIB}/k4a.lib"
  "${AZURE_KINECT_LIB}/k4arecord.lib"
  libsoundio::libsoundio
  ${Libvpx_LIB}
  ${OpenCV_LIBS}
)
set_target_properties(KinectToHololensNative PROPERTIES
  CXX_STANDARD 17
)