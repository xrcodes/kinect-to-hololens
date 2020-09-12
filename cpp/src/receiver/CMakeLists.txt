add_library(KinectToHololensReceiverModules
  audio_receiver.h
  sender_packet_classifier.h
  video_receiver_storage.h
  video_renderer.h
)
target_link_libraries(KinectToHololensReceiverModules
  KinectToHololensNative
)
set_target_properties(KinectToHololensReceiverModules PROPERTIES
  CXX_STANDARD 17
)