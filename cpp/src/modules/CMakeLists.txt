add_library(KinectToHololensModules
  video_pipeline.h
  video_pipeline.cpp
)
target_include_directories(KinectToHololensModules PRIVATE
  "${AZURE_KINECT_DIR}/sdk/include"
)
target_link_libraries(KinectToHololensModules
  KinectToHololensNative
  AzureKinectSamples
)
set_target_properties(KinectToHololensModules PROPERTIES
  CXX_STANDARD 17
)