cmake_minimum_required(VERSION 3.13)
project(KinectToHololens)

set(AZURE_KINECT_DIR $ENV{PROGRAMFILES}/Azure\ Kinect\ SDK\ v1.4.1)
if(CMAKE_SIZEOF_VOID_P GREATER 4)
	set(AZURE_KINECT_BIN ${AZURE_KINECT_DIR}/sdk/windows-desktop/amd64/release/bin)
	set(AZURE_KINECT_LIB ${AZURE_KINECT_DIR}/sdk/windows-desktop/amd64/release/lib)
else()
	set(AZURE_KINECT_BIN ${AZURE_KINECT_DIR}/sdk/windows-desktop/x86/release/bin)
	set(AZURE_KINECT_LIB ${AZURE_KINECT_DIR}/sdk/windows-desktop/x86/release/lib)
endif()

message(STATUS ${AZURE_KINECT_BIN})

# To keep asio quite from leaving a lot of warnings.
# reference: https://github.com/chriskohlhoff/asio/issues/290
add_definitions(-D_SILENCE_CXX17_ALLOCATOR_VOID_DEPRECATION_WARNING)

# Assign the top directory as a directory to include.
# This allows including header files without the ".."s.
include_directories("${PROJECT_SOURCE_DIR}/src")

# By adding the directory containing asio.hpp, that is the directory containing
# all header files of vcpkg, header files from vcpkg can be included to this project.
find_path(VCPKG_INCLUDE_DIR NAMES asio.hpp PATH_SUFFIXES asio)
include_directories(${VCPKG_INCLUDE_DIR})

set(Libvpx_LIB ${VCPKG_INCLUDE_DIR}/../lib/vpxmd.lib)

# Set ${FFMPEG_LIBRARIES} for FFmpeg.
unset(FFMPEG_LIBRARIES CACHE)
foreach(FFMPEG_SUBLIBRARY avcodec avutil)
  find_library(FFMPEG_lib${FFMPEG_SUBLIBRARY}_LIBRARY NAMES ${FFMPEG_SUBLIBRARY})
  list(APPEND FFMPEG_LIBRARIES ${FFMPEG_lib${FFMPEG_SUBLIBRARY}_LIBRARY})
endforeach()

# Set ${OpenCV_LIBS} for OpenCV.
find_package(OpenCV REQUIRED)

find_package(libsoundio CONFIG REQUIRED)

find_package(imgui CONFIG REQUIRED)

find_package(Opus CONFIG REQUIRED)

set(AZURE_KINECT_SAMPLES_DIR ${PROJECT_SOURCE_DIR}/azure-kinect-samples)
set(IMGUI_EXAMPLES_DIR ${PROJECT_SOURCE_DIR}/imgui-1.73-examples)

# The directory for general source files.
add_subdirectory(src)
# The directory for files from Azure Kinect samples (i.e., floor detection).
add_subdirectory(azure-kinect-samples)
add_subdirectory(imgui-1.73-examples)
# The directory for cpp files corresponding to exe files.
add_subdirectory(apps)
