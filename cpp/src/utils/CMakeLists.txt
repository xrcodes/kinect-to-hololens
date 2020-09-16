add_library(KinectToHololensUtils
  filesystem_utils.h
)
target_link_libraries(KinectToHololensUtils
  KinectToHololensWin32
)
set_target_properties(KinectToHololensUtils PROPERTIES
  CXX_STANDARD 17
)