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
  "${AZURE_KINECT_DIR}/sdk/windows-desktop/amd64/release/lib/k4arecord.lib"
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

add_executable(KinectSenderTcp
  kinect_sender_tcp.cpp
)
target_include_directories(KinectSenderTcp PRIVATE
  "${AZURE_KINECT_DIR}/sdk/include"
)
target_link_libraries(KinectSenderTcp
  "${AZURE_KINECT_DIR}/sdk/windows-desktop/amd64/release/lib/k4a.lib"
  "${AZURE_KINECT_DIR}/sdk/windows-desktop/amd64/release/lib/k4arecord.lib"
  KinectToHololens
  ${Libvpx_LIB}
)
set_target_properties(KinectSenderTcp PROPERTIES
  CXX_STANDARD 17
)
add_custom_command(TARGET KinectSenderTcp POST_BUILD
  COMMAND ${CMAKE_COMMAND} -E copy_if_different "${AZURE_KINECT_DIR}/sdk/windows-desktop/amd64/release/bin/depthengine_2_0.dll"
  $<TARGET_FILE_DIR:KinectSenderTcp>
)
add_custom_command(TARGET KinectSenderTcp POST_BUILD
  COMMAND ${CMAKE_COMMAND} -E copy_if_different "${AZURE_KINECT_DIR}/sdk/windows-desktop/amd64/release/bin/k4a.dll"
  $<TARGET_FILE_DIR:KinectSenderTcp>
)

add_executable(KinectReceiverTcp
  kinect_receiver_tcp.cpp
)
target_link_libraries(KinectReceiverTcp
  ${OpenCV_LIBS}
  KinectToHololens
)
set_target_properties(KinectReceiverTcp PROPERTIES
  CXX_STANDARD 17
)

add_executable(KinectSenderUdp
  kinect_sender_udp.cpp
)
target_include_directories(KinectSenderUdp PRIVATE
  "${AZURE_KINECT_DIR}/sdk/include"
)
target_link_libraries(KinectSenderUdp
  "${AZURE_KINECT_DIR}/sdk/windows-desktop/amd64/release/lib/k4a.lib"
  "${AZURE_KINECT_DIR}/sdk/windows-desktop/amd64/release/lib/k4arecord.lib"
  KinectToHololens
  ${Libvpx_LIB}
)
set_target_properties(KinectSenderUdp PROPERTIES
  CXX_STANDARD 17
)
add_custom_command(TARGET KinectSenderUdp POST_BUILD
  COMMAND ${CMAKE_COMMAND} -E copy_if_different "${AZURE_KINECT_DIR}/sdk/windows-desktop/amd64/release/bin/depthengine_2_0.dll"
  $<TARGET_FILE_DIR:KinectSenderUdp>
)
add_custom_command(TARGET KinectSenderUdp POST_BUILD
  COMMAND ${CMAKE_COMMAND} -E copy_if_different "${AZURE_KINECT_DIR}/sdk/windows-desktop/amd64/release/bin/k4a.dll"
  $<TARGET_FILE_DIR:KinectSenderUdp>
)

add_executable(KinectReceiverUdp
  kinect_receiver_udp.cpp
)
target_link_libraries(KinectReceiverUdp
  ${OpenCV_LIBS}
  KinectToHololens
)
set_target_properties(KinectReceiverUdp PROPERTIES
  CXX_STANDARD 17
)
