param(
    [string]$Compiler = "clang",
    [string]$ConfigDirectory = "build/config"
)
$ErrorActionPreference = "Stop"
$testRoot = Split-Path $PSScriptRoot -Parent
Push-Location $testRoot
$testExe = Join-Path ([IO.Path]::GetTempPath()) ("snapheater-safety-" + [guid]::NewGuid().ToString("N") + ".exe")
try {
    if (!(Test-Path -LiteralPath (Join-Path $ConfigDirectory "sdkconfig.h"))) {
        throw "Build the firmware first, or supply -ConfigDirectory pointing to generated sdkconfig.h."
    }
    $compilerArgs = @("-std=c11", "-D_CRT_SECURE_NO_WARNINGS", "-Wall", "-Wextra", "-Werror",
        "-Itests/safety_stubs", "-Itests/stubs", "-Imain", "-I$ConfigDirectory",
        "-I$env:IDF_PATH/components/json/cJSON",
        "main/app_state.c", "main/control_lease.c", "main/safety_latch.c",
        "tests/safety_state_host_test.c", "-o", $testExe)
    & $Compiler @compilerArgs
    if ($LASTEXITCODE -ne 0) { throw "Host compilation failed." }
    & $testExe
    if ($LASTEXITCODE -ne 0) { throw "Safety regression failed." }
} finally {
    if (Test-Path -LiteralPath $testExe) { Remove-Item -LiteralPath $testExe }
    Pop-Location
}
