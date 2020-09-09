Install-Module VSSetup -Scope CurrentUser

$vsInstance = Get-VSSetupInstance `
            | Select-VSSetupInstance -Version '[16.0, 17.0)' -Latest
$vsPath = $vsInstance.InstallationPath + "\MSBuild"
$msBuilds = Get-ChildItem $vsPath -recurse -filter "MSBuild.exe"
$msBuild = $msBuilds[0].FullName

$configuration = "RelWithDebInfo"
$buildPath = (Get-Location).path + "\cpp\build"

&$msBuild ("/t:TelepresenceToolkitUnity", "/p:Configuration=$configuration", "/p:Platform=Win32", "$buildPath\x86\KinectToHololens.sln")
&$msBuild ("/t:TelepresenceToolkitUnity", "/p:Configuration=$configuration", "/p:Platform=x64", "$buildPath\x64\KinectToHololens.sln")

$assetsPath = (Get-Location).path + "\unity\KhViewer\Assets"

$X86Path = "$buildPath\x86\telepresence-toolkit\unity\$configuration"
$X64Path = "$buildPath\x64\telepresence-toolkit\unity\$configuration"
Copy-Item "$X86Path\TelepresenceToolkitUnity.dll" -Destination "$assetsPath\Plugins\WSA"
Copy-Item "$X64Path\TelepresenceToolkitUnity.dll" -Destination "$assetsPath\Editor"

$binPath = (Get-Location).path + "\bin"
Copy-Item "$binPath\msvcp140.dll" -Destination "$assetsPath\Plugins\WSA"
Copy-Item "$binPath\vcruntime140.dll" -Destination "$assetsPath\Plugins\WSA"

Copy-Item "$X86Path\avcodec-58.dll" -Destination "$assetsPath\Plugins\WSA"
Copy-Item "$X86Path\avutil-56.dll" -Destination "$assetsPath\Plugins\WSA"
Copy-Item "$X86Path\swresample-3.dll" -Destination "$assetsPath\Plugins\WSA"
Copy-Item "$X86Path\opus.dll" -Destination "$assetsPath\Plugins\WSA"

Copy-Item "$X64Path\avcodec-58.dll" -Destination "$assetsPath\Editor"
Copy-Item "$X64Path\avutil-56.dll" -Destination "$assetsPath\Editor"
Copy-Item "$X64Path\swresample-3.dll" -Destination "$assetsPath\Editor"
Copy-Item "$X64Path\opus.dll" -Destination "$assetsPath\Editor"

Pause