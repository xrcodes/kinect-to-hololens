add_library(TelepresenceToolkitCore
  tt_core.h
  opus_utils.h
  opus_utils.cpp
  rvl.h
  rvl.cpp
  time.h
  trvl.h
  trvl.cpp
  vp8_decoder.h
  vp8_decoder.cpp
  yuv.h
  yuv.cpp
 )
target_include_directories(TelepresenceToolkitCore PRIVATE
  "${AZURE_KINECT_DIR}/sdk/include"
)
target_link_libraries(TelepresenceToolkitCore PRIVATE
  ${FFMPEG_LIBRARIES}
  Opus::opus
)
set_target_properties(TelepresenceToolkitCore PROPERTIES
  CXX_STANDARD 17
)
