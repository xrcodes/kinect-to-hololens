cmake_minimum_required(VERSION 3.17)
project(KinectToHololens)

# Include headers from src.
include_directories(${PROJECT_SOURCE_DIR}/src)

# Include headers from telepresence-toolkit.
include_directories(${PROJECT_SOURCE_DIR}/../telepresence-toolkit/src)

# Include headers from vcpkg.
find_path(VCPKG_INCLUDE_DIR NAMES zlib.h)
include_directories(${VCPKG_INCLUDE_DIR})

# Find Azure Kinect directories.
set(AZURE_KINECT_DIR $ENV{PROGRAMFILES}/Azure\ Kinect\ SDK\ v1.4.1)
set(AZURE_KINECT_BIN ${AZURE_KINECT_DIR}/sdk/windows-desktop/amd64/release/bin)
set(AZURE_KINECT_LIB ${AZURE_KINECT_DIR}/sdk/windows-desktop/amd64/release/lib)

# Prepare ${FFMPEG_LIBRARIES} for linking to FFmpeg.
unset(FFMPEG_LIBRARIES CACHE)
foreach(FFMPEG_SUBLIBRARY avcodec avutil)
  find_library(FFMPEG_lib${FFMPEG_SUBLIBRARY}_LIBRARY NAMES ${FFMPEG_SUBLIBRARY})
  list(APPEND FFMPEG_LIBRARIES ${FFMPEG_lib${FFMPEG_SUBLIBRARY}_LIBRARY})
endforeach()

# Prepare Opus::opus.
find_package(Opus CONFIG REQUIRED)

# Prepare imgui::imgui.
find_package(imgui CONFIG REQUIRED)

# Prepare libsoundio::libsoundio.
find_package(libsoundio CONFIG REQUIRED)

# Prepare ${Libvpx_LIB} linking to libvpx.
set(Libvpx_LIB ${VCPKG_INCLUDE_DIR}/../lib/vpxmd.lib)

# Prepare targets including opencv_highgui.
find_package(OpenCV CONFIG REQUIRED)

# Add files in /src.
add_subdirectory(src)

# Add files from telepresence-toolkit.
add_subdirectory(${PROJECT_SOURCE_DIR}/../telepresence-toolkit/src telepresence-toolkit)

# Add files for the applications.
add_subdirectory(apps)
