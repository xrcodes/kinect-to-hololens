add_library(KinectToHololensNative
  kh_receiver_socket.h
  kh_receiver_socket.cpp
  kh_sender_socket.h
  kh_sender_socket.cpp
  kh_video_packet_collection.h
  kh_video_packet_collection.cpp
  kh_fec_packet_collection.h
  kh_fec_packet_collection.cpp
  kh_video_message.h
  kh_video_message.cpp
  kh_packet.h
  kh_packet.cpp
  kh_audio.h
  kh_audio.cpp
  kh_time.h
  kh_time.cpp
)
target_include_directories(KinectToHololensNative PRIVATE
  "${AZURE_KINECT_DIR}/sdk/include"
)
target_link_libraries(KinectToHololensNative
  KinectToHololens
)
set_target_properties(KinectToHololensNative PROPERTIES
  CXX_STANDARD 17
)