$ErrorActionPreference = "Stop"

$projectRoot = Split-Path -Parent $PSScriptRoot
$installedEngineRoot = "C:\Program Files\Epic Games\UE_5.7"
$sourceEngineRoot = "C:\Users\PC\OneDrive\Desktop\MOBA MMO\UE_5.7.4_Source"
$engineRoot = if (Test-Path (Join-Path $installedEngineRoot "Engine\Build\BatchFiles\RunUAT.bat")) {
  $installedEngineRoot
} else {
  $sourceEngineRoot
}
$uat = Join-Path $engineRoot "Engine\Build\BatchFiles\RunUAT.bat"
$dotnet = Join-Path $engineRoot "Engine\Binaries\ThirdParty\DotNet\8.0.412\win-x64\dotnet.exe"
$ubt = Join-Path $engineRoot "Engine\Binaries\DotNET\UnrealBuildTool\UnrealBuildTool.dll"
$projectFile = Join-Path $projectRoot "MOBAMMO.uproject"
$stageDir = Join-Path $projectRoot "Saved\StagedBuilds\WindowsGame"
$cookDir = Join-Path $projectRoot "Saved\Cooked\Windows"
$projectBin = Join-Path $projectRoot "Binaries\Win64"

if (-not (Test-Path $uat)) {
  throw "RunUAT.bat not found: $uat"
}

if (-not (Test-Path $dotnet)) {
  throw "Bundled dotnet not found: $dotnet"
}

if (-not (Test-Path $ubt)) {
  throw "UnrealBuildTool not found: $ubt"
}

if (-not (Test-Path $projectFile)) {
  throw "Project file not found: $projectFile"
}

$openEditors = Get-Process -Name "UnrealEditor" -ErrorAction SilentlyContinue
if ($openEditors) {
  throw "UnrealEditor is running. Close the editor before staging the client so the MOBAMMOEditor module can be rebuilt."
}

if (Test-Path $stageDir) {
  Remove-Item -LiteralPath $stageDir -Recurse -Force
}

if (Test-Path $cookDir) {
  Remove-Item -LiteralPath $cookDir -Recurse -Force
}

& $dotnet $ubt `
  MOBAMMOEditor Win64 Development `
  -Project="$projectFile" `
  -WaitMutex `
  -NoHotReloadFromIDE `
  -NoUBA

if ($LASTEXITCODE -ne 0) {
  throw "Editor module build failed with exit code $LASTEXITCODE"
}

$runtimeCopies = @(
  @{
    Source = Join-Path $engineRoot "Engine\Binaries\Win64\D3D12\x64"
    Target = Join-Path $projectBin "D3D12\x64"
    Files = @("D3D12Core.dll", "d3d12SDKLayers.dll")
  },
  @{
    Source = Join-Path $engineRoot "Engine\Binaries\Win64\DML\x64"
    Target = Join-Path $projectBin "DML\x64"
    Files = @("DirectML.dll")
  },
  @{
    Source = Join-Path $engineRoot "Engine\Binaries\Win64"
    Target = $projectBin
    Files = @("tbb12.dll", "tbbmalloc.dll")
  }
)

foreach ($copy in $runtimeCopies) {
  New-Item -ItemType Directory -Force -Path $copy.Target | Out-Null

  foreach ($file in $copy.Files) {
    $sourceFile = Join-Path $copy.Source $file
    if (Test-Path $sourceFile) {
      Copy-Item -LiteralPath $sourceFile -Destination (Join-Path $copy.Target $file) -Force
    }
  }
}

& $uat BuildCookRun `
  -project="$projectFile" `
  -build `
  -nocompileeditor `
  -cook `
  -stage `
  -platform=Win64 `
  -clientconfig=Development `
  -map=/Game/ThirdPerson/Lvl_ThirdPerson `
  -unattended `
  -noxge `
  -NoUBA `
  -stagingdirectory="$stageDir"
