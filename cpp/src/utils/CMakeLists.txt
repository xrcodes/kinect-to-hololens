add_library(KinectToHololensUtils
  filesystem_utils.h
  soundio_utils.h
)
target_link_libraries(KinectToHololensUtils
  KinectToHololensNative
)
set_target_properties(KinectToHololensUtils PROPERTIES
  CXX_STANDARD 17
)