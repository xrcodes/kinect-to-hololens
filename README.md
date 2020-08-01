# Kinect to HoloLens
Seeing through a Kinect from a HoloLens.

![a figure from paper](kinect-to-hololens.jpg)

# Requirement
- A Windows 10 computer, an Azure Kinect, and a HoloLens (v1).
- CMake, Unity3D 2019.3, and Visual Studio 2019 (with C++ and UWP components).

# How to Build
1. git config core.symlinks true (allow your git using symbolic links)
2. git clone --recursive https://github.com/hanseuljun/kinect-to-hololens
3. Using vcpkg, install these libraries: asio, ffmpeg, imgui, libsoundio, libvpx, ms-gsl, opencv, and opus.
```powershell
.\vcpkg.exe install asio:x86-windows asio:x64-windows ffmpeg:x86-windows ffmpeg:x64-windows imgui:x86-windows imgui:x64-windows libsoundio:x86-windows libsoundio:x64-windows libvpx:x86-windows libvpx:x64-windows ms-gsl:x86-windows ms-gsl:x64-windows opencv:x86-windows opencv:x64-windows opus:x86-windows opus:x64-windows
```
4. Install Kinect for Azure Kinect Sensor SDK 1.4.0 (https://docs.microsoft.com/en-us/azure/Kinect-dk/sensor-sdk-download). (TODO: use vcpkg)
5. Run run-cmake.ps1 in directory /cpp to build Visual Studio solutions.
6. Run build-plugin.ps1 to build the Unity3D plugin and copy it into the Unity3D project in directory /unity/KinectViewer.
7. Build applications with the Visual Studio solution in /cpp/build/x64 and the Unity3D project in /unity/KinectViewer.

# How to Use
Download the kinect-sender-v0.3 and kinect-viewer-v0.3 from https://github.com/hanseuljun/kinect-to-hololens/releases/tag/v0.3.  

## From the Kinect-side
1. Connect a Kinect for Azure to a PC.
2. Run KinectSender.exe. Allow connection to both private and public networks.

## From the HoloLens-side
1. Install the app package in kinect-viewer-v0.3 to your HoloLens (see https://www.microsoft.com/en-us/p/microsoft-hololens/9nblggh4qwnx) and run the installed app--Kinect Viewer.
2. Access the HoloLens through the Windows Device Portal for virtual key input.
3. Using the virtual key input, enter IP address of KinectSender.exe and press enter.
4. Place your HoloLens beneth your Kinect on the floor and press space using the portal.
5. Use arrow keys to adjust the position of the scene and d key to hide the visuals for setup.

## Check firewall when connections don't work.
Since the sender/controller often gets turned on in a machine that is not built to function as a server, incoming connections to the sender/controller often gets blocked. Actually, they get blocked by default when the firewall is on. If a connection does not work, first turn off the whole firewall and try it again. If this makes the connection to happen, it means the firewall was blocking the connection. Now, turn on the firewall again, as turning off the firewall is terrible for your security, and add inbound firewall rules for both sender and controller (this would mean Unity3D if you are running it as a Unity3D project) allowing domain/private/public (choose the ones that belong to your environment; pick all if you are not sure) connections. (For adding inbound firewall rules, see https://www.howtogeek.com/112564/how-to-create-advanced-firewall-rules-in-the-windows-firewall/)

# Additional Notes

## Floor Detection
Files for floor detection in /cpp/azure-kinect-samples comes from https://github.com/microsoft/Azure-Kinect-Samples/tree/master/body-tracking-samples/floor_detector_sample.

## Imgui
Usage of imgui requires imgui_impl files in /cpp/imgui-1.73-examples.

## Spamming Messages from Kinect Viewer in Visual Studio Console
The current implementation of .Net handles socket errors using exceptions that can be caught in C#. However, Visual Studio prints a message to the output window whenever there is an exception by default. This leaves a lot of scary messages in the Visual Studio console when testing Kinect Viewer and prevents from seeing other exception messages. While this is not the best solution, it is possible to turn off exception messages using output's settings. Unfortunately, this turns off all exceptions.

# Paper
Jun, H., Bailenson, J.N., Fuchs, H., & Wetzstein, G. (2020). An Easy-to-use Pipeline for an RGBD Camera and an AR Headset. *PRESENCE: Teleoperators and Virtual Environments*.
