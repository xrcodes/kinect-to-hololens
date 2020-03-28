add_executable(KinectReader
  kinect_reader.cpp
  helper/opencv_helper.h
)
target_include_directories(KinectReader PRIVATE
  "${AZURE_KINECT_DIR}/sdk/include"
  "${OPUS_DIR}/include"
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
  helper/kinect_helper.h
  helper/soundio_helper.h
  helper/shadow_remover.h
)
target_include_directories(KinectSender PRIVATE
  "${AZURE_KINECT_DIR}/sdk/include"
  "${OPUS_DIR}/include"
)
target_link_libraries(KinectSender
  "${AZURE_KINECT_LIB}/k4a.lib"
  KinectToHololensNative
  ${Libvpx_LIB}
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
  receiver_modules.h
)
target_include_directories(KinectReceiver PRIVATE
  "${AZURE_KINECT_DIR}/sdk/include"
  "${OPUS_DIR}/include"
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
  "${OPUS_DIR}/include"
)
target_link_libraries(KinectListener
  KinectToHololensNative
)
set_target_properties(KinectListener PROPERTIES
  CXX_STANDARD 17
)