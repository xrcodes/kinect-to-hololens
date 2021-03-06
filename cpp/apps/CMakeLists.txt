add_executable(KinectToHololensReaderApp
  kh_reader.cpp
)
target_include_directories(KinectToHololensReaderApp PRIVATE
  "${AZURE_KINECT_DIR}/sdk/include"
)
target_link_libraries(KinectToHololensReaderApp
  KinectToHololensSenderModules
)
set_target_properties(KinectToHololensReaderApp PROPERTIES
  CXX_STANDARD 17
)
add_custom_command(TARGET KinectToHololensReaderApp POST_BUILD
  COMMAND ${CMAKE_COMMAND} -E copy_if_different "${AZURE_KINECT_BIN}/depthengine_2_0.dll"
  $<TARGET_FILE_DIR:KinectToHololensReaderApp>
)
add_custom_command(TARGET KinectToHololensReaderApp POST_BUILD
  COMMAND ${CMAKE_COMMAND} -E copy_if_different "${AZURE_KINECT_BIN}/k4a.dll"
  $<TARGET_FILE_DIR:KinectToHololensReaderApp>
)
add_custom_command(TARGET KinectToHololensReaderApp POST_BUILD
  COMMAND ${CMAKE_COMMAND} -E copy_if_different "${AZURE_KINECT_BIN}/k4arecord.dll"
  $<TARGET_FILE_DIR:KinectToHololensReaderApp>
)

add_executable(KinectToHololensListenerApp
  kh_listener.cpp
)
target_include_directories(KinectToHololensListenerApp PRIVATE
  "${AZURE_KINECT_DIR}/sdk/include"
)
target_link_libraries(KinectToHololensListenerApp
  KinectToHololensWin32
)
set_target_properties(KinectToHololensListenerApp PROPERTIES
  CXX_STANDARD 17
)

add_executable(KinectToHololensSenderApp
  kh_sender.cpp
  resources/kh_sender.rc
)
target_include_directories(KinectToHololensSenderApp PRIVATE
  "${AZURE_KINECT_DIR}/sdk/include"
)
target_link_libraries(KinectToHololensSenderApp
  KinectToHololensSenderModules
  ${Libvpx_LIB}
  imgui::imgui
  d3d11.lib
)
set_target_properties(KinectToHololensSenderApp PROPERTIES
  CXX_STANDARD 17
)
add_custom_command(TARGET KinectToHololensSenderApp POST_BUILD
  COMMAND ${CMAKE_COMMAND} -E copy_if_different "${AZURE_KINECT_BIN}/depthengine_2_0.dll"
  $<TARGET_FILE_DIR:KinectToHololensSenderApp>
)
add_custom_command(TARGET KinectToHololensSenderApp POST_BUILD
  COMMAND ${CMAKE_COMMAND} -E copy_if_different "${AZURE_KINECT_BIN}/k4a.dll"
  $<TARGET_FILE_DIR:KinectToHololensSenderApp>
)
add_custom_command(TARGET KinectToHololensSenderApp POST_BUILD
  COMMAND ${CMAKE_COMMAND} -E copy_if_different "${AZURE_KINECT_BIN}/k4arecord.dll"
  $<TARGET_FILE_DIR:KinectToHololensSenderApp>
)

add_executable(KinectToHololensReceiverApp
  kh_receiver.cpp
)
target_include_directories(KinectToHololensReceiverApp PRIVATE
  "${AZURE_KINECT_DIR}/sdk/include"
)
target_link_libraries(KinectToHololensReceiverApp
  KinectToHololensWin32
)
set_target_properties(KinectToHololensReceiverApp PROPERTIES
  CXX_STANDARD 17
)
