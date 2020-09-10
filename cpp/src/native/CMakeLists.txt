add_library(KinectToHololensNative
  kh_kinect.h
  kh_kinect.cpp
  kh_native.h
  kh_packet.h
  kh_packet.cpp
  kh_occlusion_remover.h
  kh_occlusion_remover.cpp
  kh_soundio.h
  kh_soundio.cpp
  kh_udp_socket.h
  kh_udp_socket.cpp
  kh_vp8_encoder.h
  kh_vp8_encoder.cpp
  imgui_wrapper.h
  profiler.h
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
)
set_target_properties(KinectToHololensNative PROPERTIES
  CXX_STANDARD 17
)