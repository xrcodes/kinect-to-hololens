set(SOURCE_FILES
  azure_kinect.h
  azure_kinect.cpp
)

add_library(AzureKinect ${SOURCE_FILES})

target_include_directories(AzureKinect PRIVATE
  "${AZURE_KINECT_DIR}/sdk/include"
)
target_link_libraries(AzureKinect
  "${AZURE_KINECT_DIR}/sdk/windows-desktop/amd64/release/lib/k4a.lib"
)
set_target_properties(AzureKinect PROPERTIES
  CXX_STANDARD 17
)