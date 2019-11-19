# Source files for the demos.
# Both the KinectReaderDemo and the KinectSenderDemo
# cannot be built for 32-bit since they require access to
# libfreenect2, which we have no 32-bit DLL file.
if(CMAKE_SIZEOF_VOID_P EQUAL 8)
  add_executable(KinectReaderDemo
    kinect_reader_demo.cpp
    helper/opencv_helper.h
  )
  target_include_directories(KinectReaderDemo PRIVATE
    "${AZURE_KINECT_DIR}/sdk/include"
  )
  target_link_libraries(KinectReaderDemo
    ${OpenCV_LIBS}
    "${AZURE_KINECT_DIR}/sdk/windows-desktop/amd64/release/lib/k4a.lib"
    "${AZURE_KINECT_DIR}/sdk/windows-desktop/amd64/release/lib/k4arecord.lib"
    KinectToHololens
    ${Libvpx_LIB}
  )
  set_target_properties(KinectReaderDemo PROPERTIES
    CXX_STANDARD 17
  )
  add_custom_command(TARGET KinectReaderDemo POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E copy_if_different "${AZURE_KINECT_DIR}/sdk/windows-desktop/amd64/release/bin/depthengine_2_0.dll"
    $<TARGET_FILE_DIR:KinectReaderDemo>
  )
  add_custom_command(TARGET KinectReaderDemo POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E copy_if_different "${AZURE_KINECT_DIR}/sdk/windows-desktop/amd64/release/bin/k4a.dll"
    $<TARGET_FILE_DIR:KinectReaderDemo>
  )

  add_executable(KinectSenderDemo
    kinect_sender_demo.cpp
  )
  target_include_directories(KinectSenderDemo PRIVATE
    "${AZURE_KINECT_DIR}/sdk/include"
  )
  target_link_libraries(KinectSenderDemo
    "${AZURE_KINECT_DIR}/sdk/windows-desktop/amd64/release/lib/k4a.lib"
    "${AZURE_KINECT_DIR}/sdk/windows-desktop/amd64/release/lib/k4arecord.lib"
    KinectToHololens
    ${Libvpx_LIB}
  )
  set_target_properties(KinectSenderDemo PROPERTIES
    CXX_STANDARD 17
  )
  add_custom_command(TARGET KinectSenderDemo POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E copy_if_different "${AZURE_KINECT_DIR}/sdk/windows-desktop/amd64/release/bin/depthengine_2_0.dll"
    $<TARGET_FILE_DIR:KinectSenderDemo>
  )
  add_custom_command(TARGET KinectSenderDemo POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E copy_if_different "${AZURE_KINECT_DIR}/sdk/windows-desktop/amd64/release/bin/k4a.dll"
    $<TARGET_FILE_DIR:KinectSenderDemo>
  )
  
  add_executable(KinectPlayerDemo
    kinect_player_demo.cpp
  )
  target_include_directories(KinectPlayerDemo PRIVATE
    "${AZURE_KINECT_DIR}/sdk/include"
    "${JPEG_INCLUDE_DIR}"
  )
  target_link_libraries(KinectPlayerDemo PRIVATE
    "${AZURE_KINECT_DIR}/sdk/windows-desktop/amd64/release/lib/k4a.lib"
    "${AZURE_KINECT_DIR}/sdk/windows-desktop/amd64/release/lib/k4arecord.lib"
    KinectToHololens
    ${Libvpx_LIB}
    ${JPEG_LIBRARY_RELEASE}
    ${TURBO_JPEG_LIBRARY}
  )
  set_target_properties(KinectPlayerDemo PROPERTIES
    CXX_STANDARD 17
  )
  add_custom_command(TARGET KinectPlayerDemo POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E copy_if_different "${AZURE_KINECT_DIR}/sdk/windows-desktop/amd64/release/bin/depthengine_2_0.dll"
    $<TARGET_FILE_DIR:KinectPlayerDemo>
  )
  add_custom_command(TARGET KinectPlayerDemo POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E copy_if_different "${AZURE_KINECT_DIR}/sdk/windows-desktop/amd64/release/bin/k4a.dll"
    $<TARGET_FILE_DIR:KinectPlayerDemo>
  )
  add_custom_command(TARGET KinectPlayerDemo POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E copy_if_different "${AZURE_KINECT_DIR}/sdk/windows-desktop/amd64/release/bin/k4arecord.dll"
    $<TARGET_FILE_DIR:KinectPlayerDemo>
  )
endif()

add_executable(KinectReceiverDemo
  kinect_receiver_demo.cpp
)
target_link_libraries(KinectReceiverDemo
  ${OpenCV_LIBS}
  KinectToHololens
)
set_target_properties(KinectReceiverDemo PROPERTIES
  CXX_STANDARD 17
)
