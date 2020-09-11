add_library(KinectToHololensSenderModules
  occlusion_remover.h
  occlusion_remover.cpp
  receiver_packet_classifier.h
  remote_receiver.h
  video_sender_storage.h
  video_pipeline.h
  video_pipeline.cpp
)
target_include_directories(KinectToHololensSenderModules PRIVATE
  "${AZURE_KINECT_DIR}/sdk/include"
)
target_link_libraries(KinectToHololensSenderModules
  KinectToHololensNative
  AzureKinectSamples
)
set_target_properties(KinectToHololensSenderModules PROPERTIES
  CXX_STANDARD 17
)