Install-Module VSSetup -Scope CurrentUser

$vsInstance = Get-VSSetupInstance `
            | Select-VSSetupInstance -Version '[16.0, 17.0)' -Latest
$vsPath = $vsInstance.InstallationPath + "\MSBuild"
$msBuilds = Get-ChildItem $vsPath -recurse -filter "MSBuild.exe"
$msBuild = $msBuilds[0].FullName

$configuration = "RelWithDebInfo"
$buildPath = (Get-Location).path + "\cpp\build"

&$msBuild ("/t:KinectToHololensUnity", "/p:Configuration=$configuration", "/p:Platform=Win32", "$buildPath\x86\KinectToHololens.sln")
&$msBuild ("/t:KinectToHololensUnity", "/p:Configuration=$configuration", "/p:Platform=x64", "$buildPath\x64\KinectToHololens.sln")

$assetsPath = (Get-Location).path + "\unity\KhViewer\Assets"

Copy-Item "$buildPath\x86\src\unity\$configuration\KinectToHololensUnity.dll" -Destination "$assetsPath\Plugins\WSA"
Copy-Item "$buildPath\x64\src\unity\$configuration\KinectToHololensUnity.dll" -Destination "$assetsPath\Editor"

$binPath = (Get-Location).path + "\bin"
Copy-Item "$binPath\msvcp140.dll" -Destination "$assetsPath\Plugins\WSA"
Copy-Item "$binPath\vcruntime140.dll" -Destination "$assetsPath\Plugins\WSA"

$vcpkgPath = (Get-Location).path + "\vcpkg"
$ffmpegX86Path = "$vcpkgPath\packages\ffmpeg_x86-windows\bin"
$ffmpegX64Path = "$vcpkgPath\packages\ffmpeg_x64-windows\bin"
$opusX86Path = "$vcpkgPath\packages\opus_x86-windows\bin"
$opusX64Path = "$vcpkgPath\packages\opus_x64-windows\bin"

Copy-Item "$ffmpegX86Path\avcodec-58.dll" -Destination "$assetsPath\Plugins\WSA"
Copy-Item "$ffmpegX86Path\avutil-56.dll" -Destination "$assetsPath\Plugins\WSA"
Copy-Item "$opusX86Path\opus.dll" -Destination "$assetsPath\Plugins\WSA"

Copy-Item "$ffmpegX64Path\avcodec-58.dll" -Destination "$assetsPath\Editor"
Copy-Item "$ffmpegX64Path\avutil-56.dll" -Destination "$assetsPath\Editor"
Copy-Item "$opusX64Path\opus.dll" -Destination "$assetsPath\Editor"

Pause