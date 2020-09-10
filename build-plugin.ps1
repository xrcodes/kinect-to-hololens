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

$x86Path = "$buildPath\x86\telepresence-toolkit\unity\$configuration"
$x64Path = "$buildPath\x64\telepresence-toolkit\unity\$configuration"
$editorPath = "$assetsPath\Editor"
$uwpPluginPath = "$assetsPath\Plugins\WSA"

Copy-Item "$x86Path\TelepresenceToolkitUnity.dll" -Destination $uwpPluginPath
Copy-Item "$x64Path\TelepresenceToolkitUnity.dll" -Destination $editorPath

$binPath = (Get-Location).path + "\bin"
Copy-Item "$binPath\msvcp140.dll" -Destination $uwpPluginPath
Copy-Item "$binPath\vcruntime140.dll" -Destination $uwpPluginPath

Copy-Item "$x86Path\avcodec-58.dll" -Destination $uwpPluginPath
Copy-Item "$x86Path\avutil-56.dll" -Destination $uwpPluginPath
Copy-Item "$x86Path\swresample-3.dll" -Destination $uwpPluginPath
Copy-Item "$x86Path\opus.dll" -Destination $uwpPluginPath

Copy-Item "$x64Path\avcodec-58.dll" -Destination $editorPath
Copy-Item "$x64Path\avutil-56.dll" -Destination $editorPath
Copy-Item "$x64Path\swresample-3.dll" -Destination $editorPath
Copy-Item "$x64Path\opus.dll" -Destination $editorPath

Pause