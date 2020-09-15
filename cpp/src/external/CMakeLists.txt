add_library(AzureKinectSamples
  FloorDetector.h
  FloorDetector.cpp
  PointCloudGenerator.h
  PointCloudGenerator.cpp
  SampleMathTypes.h
 )
target_include_directories(AzureKinectSamples PRIVATE
  "${AZURE_KINECT_DIR}/sdk/include"
)
set_target_properties(AzureKinectSamples PROPERTIES
  CXX_STANDARD 17
)
