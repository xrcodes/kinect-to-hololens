add_library(KinectToHololens
  kh_opus.h
  kh_opus.cpp
  kh_rvl.h
  kh_rvl.cpp
  kh_trvl.h
  kh_trvl.cpp
  kh_vp8.h
  kh_vp8_decoder.cpp
  kh_vp8_encoder.cpp
  kh_yuv.h
  kh_yuv.cpp
 )
target_include_directories(KinectToHololens PRIVATE
  "${AZURE_KINECT_DIR}/sdk/include"
  "${OPUS_DIR}/include"
)
target_link_libraries(KinectToHololens
  ${FFMPEG_LIBRARIES}
  debug ${OPUS_BUILD_DIR}/debug/opus.lib optimized ${OPUS_BUILD_DIR}/relwithdebinfo/opus.lib
)
set_target_properties(KinectToHololens PROPERTIES
  CXX_STANDARD 17
)

# The directory that contains files required to build the DLL file for native applications. 
add_subdirectory(native)

# The directory that contains files required to build the DLL file for Unity. 
add_subdirectory(unity)