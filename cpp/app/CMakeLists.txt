add_executable(KinectReader
  kinect_reader.cpp
  helper/opencv_helper.h
)
target_include_directories(KinectReader PRIVATE
  "${AZURE_KINECT_DIR}/sdk/include"
)
target_link_libraries(KinectReader
  ${OpenCV_LIBS}
  "${AZURE_KINECT_DIR}/sdk/windows-desktop/amd64/release/lib/k4a.lib"
  KinectToHololens
  ${Libvpx_LIB}
)
set_target_properties(KinectReader PROPERTIES
  CXX_STANDARD 17
)
add_custom_command(TARGET KinectReader POST_BUILD
  COMMAND ${CMAKE_COMMAND} -E copy_if_different "${AZURE_KINECT_DIR}/sdk/windows-desktop/amd64/release/bin/depthengine_2_0.dll"
  $<TARGET_FILE_DIR:KinectReader>
)
add_custom_command(TARGET KinectReader POST_BUILD
  COMMAND ${CMAKE_COMMAND} -E copy_if_different "${AZURE_KINECT_DIR}/sdk/windows-desktop/amd64/release/bin/k4a.dll"
  $<TARGET_FILE_DIR:KinectReader>
)

add_executable(KinectSender
  kinect_sender.cpp
  helper/kinect_helper.h
)
target_include_directories(KinectSender PRIVATE
  "${AZURE_KINECT_DIR}/sdk/include"
)
target_link_libraries(KinectSender
  "${AZURE_KINECT_DIR}/sdk/windows-desktop/amd64/release/lib/k4a.lib"
  KinectToHololens
  ${Libvpx_LIB}
)
set_target_properties(KinectSender PROPERTIES
  CXX_STANDARD 17
)
add_custom_command(TARGET KinectSender POST_BUILD
  COMMAND ${CMAKE_COMMAND} -E copy_if_different "${AZURE_KINECT_DIR}/sdk/windows-desktop/amd64/release/bin/depthengine_2_0.dll"
  $<TARGET_FILE_DIR:KinectSender>
)
add_custom_command(TARGET KinectSender POST_BUILD
  COMMAND ${CMAKE_COMMAND} -E copy_if_different "${AZURE_KINECT_DIR}/sdk/windows-desktop/amd64/release/bin/k4a.dll"
  $<TARGET_FILE_DIR:KinectSender>
)

add_executable(KinectReceiver
  kinect_receiver.cpp
  helper/opencv_helper.h
)
target_link_libraries(KinectReceiver
  ${OpenCV_LIBS}
  KinectToHololens
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
  KinectToHololens
)
set_target_properties(KinectListener PROPERTIES
  CXX_STANDARD 17
)

add_executable(KinectAudioSender
  kinect_audio_sender.cpp
  helper/soundio_helper.h
)
target_include_directories(KinectAudioSender PRIVATE
  "${AZURE_KINECT_DIR}/sdk/include"
)
target_link_libraries(KinectAudioSender
  KinectToHololens
  Opus::opus
)
set_target_properties(KinectAudioSender PROPERTIES
  CXX_STANDARD 17
)

add_executable(KinectAudioReceiver
  kinect_audio_receiver.cpp
  helper/soundio_helper.h
)
target_include_directories(KinectAudioReceiver PRIVATE
  "${AZURE_KINECT_DIR}/sdk/include"
)
target_link_libraries(KinectAudioReceiver
  KinectToHololens
  Opus::opus
)
set_target_properties(KinectAudioReceiver PROPERTIES
  CXX_STANDARD 17
)