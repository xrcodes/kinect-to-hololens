set(SOURCE_FILES
  azure_kinect.h
  azure_kinect.cpp
  azure_kinect_capture.cpp
  azure_kinect_device.cpp
  azure_kinect_image.cpp
)

add_library(AzureKinect ${SOURCE_FILES})

target_include_directories(AzureKinect PRIVATE
  "${AZURE_KINECT_DIR}/sdk/include"
)
target_link_libraries(AzureKinect
  "${AZURE_KINECT_DIR}/sdk/windows-desktop/amd64/release/lib/k4a.lib"
  "${AZURE_KINECT_DIR}/sdk/windows-desktop/amd64/release/lib/k4arecord.lib"
)
set_target_properties(AzureKinect PROPERTIES
  CXX_STANDARD 17
)