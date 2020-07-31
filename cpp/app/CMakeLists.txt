add_executable(KinectReader
  kinect_reader.cpp
  helper/opencv_helper.h
)
target_include_directories(KinectReader PRIVATE
  "${AZURE_KINECT_DIR}/sdk/include"
)
target_link_libraries(KinectReader
  ${OpenCV_LIBS}
  "${AZURE_KINECT_LIB}/k4a.lib"
  KinectToHololensNative
  ${Libvpx_LIB}
)
set_target_properties(KinectReader PROPERTIES
  CXX_STANDARD 17
)
add_custom_command(TARGET KinectReader POST_BUILD
  COMMAND ${CMAKE_COMMAND} -E copy_if_different "${AZURE_KINECT_BIN}/depthengine_2_0.dll"
  $<TARGET_FILE_DIR:KinectReader>
)
add_custom_command(TARGET KinectReader POST_BUILD
  COMMAND ${CMAKE_COMMAND} -E copy_if_different "${AZURE_KINECT_BIN}/k4a.dll"
  $<TARGET_FILE_DIR:KinectReader>
)

add_executable(KinectSender
  kinect_sender.cpp
  helper/soundio_helper.h
  sender/kinect_audio_sender.h
  sender/kinect_video_sender.h
  sender/kinect_video_sender.cpp
  sender/receiver_packet_receiver.h
  sender/video_sender_utils.h
  sender/remote_receiver.h
  helper/imgui_helper.h
  "${PROJECT_SOURCE_DIR}/resources/KinectSender.rc"
)
target_include_directories(KinectSender PRIVATE
  "${AZURE_KINECT_DIR}/sdk/include"
  ${AZURE_KINECT_SAMPLES_DIR}
  ${IMGUI_EXAMPLES_DIR}
)
target_link_libraries(KinectSender
  "${AZURE_KINECT_LIB}/k4a.lib"
  KinectToHololensNative
  ${Libvpx_LIB}
  AzureKinectSamples
  ImguiExamples
  imgui::imgui
)
set_target_properties(KinectSender PROPERTIES
  CXX_STANDARD 17
)
add_custom_command(TARGET KinectSender POST_BUILD
  COMMAND ${CMAKE_COMMAND} -E copy_if_different "${AZURE_KINECT_BIN}/depthengine_2_0.dll"
  $<TARGET_FILE_DIR:KinectSender>
)
add_custom_command(TARGET KinectSender POST_BUILD
  COMMAND ${CMAKE_COMMAND} -E copy_if_different "${AZURE_KINECT_BIN}/k4a.dll"
  $<TARGET_FILE_DIR:KinectSender>
)
add_executable(KinectReceiver
  kinect_receiver.cpp
  helper/opencv_helper.h
  helper/soundio_helper.h
  receiver/audio_packet_receiver.h
  receiver/sender_packet_receiver.h
  receiver/video_message_assembler.h
  receiver/video_message_assembler.cpp
  receiver/video_renderer.h
  receiver/video_renderer_state.h
)
target_include_directories(KinectReceiver PRIVATE
  "${AZURE_KINECT_DIR}/sdk/include"
)
target_link_libraries(KinectReceiver
  ${OpenCV_LIBS}
  KinectToHololensNative
)
set_target_properties(KinectReceiver PROPERTIES
  CXX_STANDARD 17
)

add_executable(KinectListener
  kinect_listener.cpp
  helper/soundio_helper.h
)
target_include_directories(KinectListener PRIVATE
  "${AZURE_KINECT_DIR}/sdk/include"
)
target_link_libraries(KinectListener
  KinectToHololensNative
)
set_target_properties(KinectListener PROPERTIES
  CXX_STANDARD 17
)