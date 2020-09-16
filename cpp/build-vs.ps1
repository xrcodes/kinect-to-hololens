New-Item .\build -ItemType Directory
$CMakeToolchainFile = (Get-Location).path + "\..\telepresence-toolkit\vcpkg\scripts\buildsystems\vcpkg.cmake"
cmake -S . -B .\build -G "Visual Studio 16 2019" -A x64 -DCMAKE_TOOLCHAIN_FILE="$CMakeToolchainFile"
Pause
