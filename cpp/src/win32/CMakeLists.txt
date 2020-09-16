add_library(KinectToHololensWin32
  kh_kinect.h
  kh_kinect.cpp
  imgui_wrapper.h
  opencv_utils.h
  opencv_utils.cpp
  soundio_utils.h
  soundio_utils.cpp
)
target_include_directories(KinectToHololensWin32 PRIVATE
  "${AZURE_KINECT_DIR}/sdk/include"
)
target_link_libraries(KinectToHololensWin32
  KinectToHololensNative
  ${AZURE_KINECT_LIB}/k4a.lib
  ${AZURE_KINECT_LIB}/k4arecord.lib
  libsoundio::libsoundio
  opencv_highgui
)
set_target_properties(KinectToHololensWin32 PROPERTIES
  CXX_STANDARD 17
)