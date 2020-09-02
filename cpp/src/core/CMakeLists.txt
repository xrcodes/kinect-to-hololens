add_library(KinectToHololensCore
  kh_core.h
  kh_opus.h
  kh_opus.cpp
  kh_rvl.h
  kh_rvl.cpp
  kh_trvl.h
  kh_trvl.cpp
  kh_vp8_decoder.h
  kh_vp8_decoder.cpp
  kh_yuv.h
  kh_yuv.cpp
 )
target_include_directories(KinectToHololensCore PRIVATE
  "${AZURE_KINECT_DIR}/sdk/include"
)
target_link_libraries(KinectToHololensCore PRIVATE
  ${FFMPEG_LIBRARIES}
  Opus::opus
)
set_target_properties(KinectToHololensCore PROPERTIES
  CXX_STANDARD 17
)
