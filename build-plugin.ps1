Install-Module VSSetup -Scope CurrentUser

$vsInstance = Get-VSSetupInstance `
            | Select-VSSetupInstance -Version '[16.0, 17.0)' -Latest
$vsPath = $vsInstance.InstallationPath + "\MSBuild"
$msBuilds = Get-ChildItem $vsPath -recurse -filter "MSBuild.exe"
$msBuild = $msBuilds[0].FullName

$configuration = "RelWithDebInfo"
$buildPath = (Get-Location).path + "\telepresence-toolkit\build"

&$msBuild ("/t:TelepresenceToolkitUnity", "/p:Configuration=$configuration", "/p:Platform=Win32", "$buildPath\x86\TelepresenceToolkit.sln")
&$msBuild ("/t:TelepresenceToolkitUnity", "/p:Configuration=$configuration", "/p:Platform=x64", "$buildPath\x64\TelepresenceToolkit.sln")

$packagePath = (Get-Location).path + "\unity\TelepresenceToolkit"

$x86Path = "$buildPath\x86\src\unity\$configuration"
$x64Path = "$buildPath\x64\src\unity\$configuration"
$editorPath = "$packagePath\Editor\Plugins"
$uwpPluginPath = "$packagePath\Runtime\Plugins\UWP\x86"

Copy-Item "$x86Path\TelepresenceToolkitUnity.dll" -Destination $uwpPluginPath
Copy-Item "$x64Path\TelepresenceToolkitUnity.dll" -Destination $editorPath

$binPath = (Get-Location).path + "\bin"
Copy-Item "$binPath\msvcp140.dll" -Destination $uwpPluginPath
Copy-Item "$binPath\vcruntime140.dll" -Destination $uwpPluginPath
Copy-Item "$binPath\avcodec-58.dll.meta" -Destination $uwpPluginPath
Copy-Item "$binPath\avutil-56.dll.meta" -Destination $uwpPluginPath
Copy-Item "$binPath\msvcp140.dll.meta" -Destination $uwpPluginPath
Copy-Item "$binPath\opus.dll.meta" -Destination $uwpPluginPath
Copy-Item "$binPath\swresample-3.dll.meta" -Destination $uwpPluginPath
Copy-Item "$binPath\TelepresenceToolkitUnity.dll.meta" -Destination $uwpPluginPath
Copy-Item "$binPath\vcruntime140.dll.meta" -Destination $uwpPluginPath

Copy-Item "$x86Path\avcodec-58.dll" -Destination $uwpPluginPath
Copy-Item "$x86Path\avutil-56.dll" -Destination $uwpPluginPath
Copy-Item "$x86Path\swresample-3.dll" -Destination $uwpPluginPath
Copy-Item "$x86Path\opus.dll" -Destination $uwpPluginPath

Copy-Item "$x64Path\avcodec-58.dll" -Destination $editorPath
Copy-Item "$x64Path\avutil-56.dll" -Destination $editorPath
Copy-Item "$x64Path\swresample-3.dll" -Destination $editorPath
Copy-Item "$x64Path\opus.dll" -Destination $editorPath

Pause