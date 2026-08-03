[CmdletBinding()]
param(
    [ValidateSet("setup", "test", "3dsx", "cia", "old3ds", "all")]
    [string]$Target = "setup"
)

$ErrorActionPreference = "Stop"

$ProjectRoot = (Resolve-Path (Join-Path $PSScriptRoot "..")).Path
$BaseImage = "devkitpro/devkitarm@sha256:116afba8df8453961de2936ffab20dd441edf4d682856c1ec8b0e53d7ed0bbf5"
$Image = "clouds-music-dev:devkitarm-20260717"
$DockerDesktop = "C:\Program Files\Docker\Docker\Docker Desktop.exe"
$DockerCli = "C:\Program Files\Docker\Docker\resources\bin\docker.exe"

function Resolve-DockerCli {
    $command = Get-Command docker.exe -ErrorAction SilentlyContinue
    if ($command) {
        return $command.Source
    }
    if (Test-Path $DockerCli) {
        return $DockerCli
    }
    throw "Docker CLI was not found. Install Docker Desktop, then rerun this script."
}

function Initialize-Submodules {
    if (Test-Path (Join-Path $ProjectRoot "external\minimp3\minimp3_ex.h")) {
        return
    }

    $git = Get-Command git.exe -ErrorAction SilentlyContinue
    if (-not $git) {
        throw "Git was not found. Install Git for Windows, then rerun this script."
    }

    # Git submodule is a shell script. Keep Git for Windows' Unix tools visible
    # even when this script is launched from an IDE with a reduced PATH.
    $gitCmdDirectory = Split-Path $git.Source
    $gitRoot = Split-Path $gitCmdDirectory
    $savedPath = $env:Path
    try {
        $env:Path = "$gitRoot\mingw64\bin;$gitRoot\usr\bin;$savedPath"
        & $git.Source -C $ProjectRoot submodule update --init --recursive
        if ($LASTEXITCODE -ne 0) {
            throw "git submodule update failed with exit code $LASTEXITCODE."
        }
    }
    finally {
        $env:Path = $savedPath
    }
}

function Test-DockerServer {
    & $script:DockerPath info *> $null
    return $LASTEXITCODE -eq 0
}

function Start-DockerServer {
    if (Test-DockerServer) {
        return
    }

    if (Test-Path $DockerDesktop) {
        Write-Host "Starting Docker Desktop..."
        Start-Process -FilePath $DockerDesktop -WindowStyle Hidden | Out-Null
        for ($attempt = 0; $attempt -lt 60; $attempt++) {
            Start-Sleep -Seconds 2
            if (Test-DockerServer) {
                return
            }
        }
    }

    throw "Docker Engine is not ready. Start Docker Desktop and wait for it to finish, then rerun this script."
}

function Invoke-DockerChecked {
    param([string[]]$Arguments)

    & $script:DockerPath @Arguments
    if ($LASTEXITCODE -ne 0) {
        throw "docker failed with exit code $LASTEXITCODE."
    }
}

function Test-BuildImage {
    & $script:DockerPath image inspect $Image *> $null
    return $LASTEXITCODE -eq 0
}

function Install-BuildImage {
    Invoke-DockerChecked @("pull", $BaseImage)
    Invoke-DockerChecked @(
        "build",
        "--build-arg", "DEVKITARM_IMAGE=$BaseImage",
        "--tag", $Image,
        "--file", (Join-Path $ProjectRoot "tools\docker\Dockerfile.windows"),
        (Join-Path $ProjectRoot "tools\docker")
    )
}

function Invoke-ContainerMake {
    param([string[]]$Goals)

    $mount = "type=bind,source=$ProjectRoot,target=/project"
    $arguments = @(
        "run", "--rm",
        "--mount", $mount,
        "-w", "/project",
        $Image,
        "make"
    ) + $Goals
    Invoke-DockerChecked $arguments
}

function Install-CiaTools {
    $bannerTool = Join-Path $ProjectRoot ".tools\cia-tools\Linux-x86_64\bin\bannertool"
    if (-not (Test-Path $bannerTool)) {
        Invoke-ContainerMake @("cia-tools")
    }
}

Initialize-Submodules
$script:DockerPath = Resolve-DockerCli
$dockerDirectory = Split-Path $script:DockerPath
if (($env:Path -split ";") -notcontains $dockerDirectory) {
    $env:Path = "$dockerDirectory;$env:Path"
}
Start-DockerServer

if ($Target -eq "setup" -or -not (Test-BuildImage)) {
    Install-BuildImage
}
if ($Target -in @("setup", "3dsx", "cia", "old3ds", "all")) {
    Install-CiaTools
}

switch ($Target) {
    "setup" {
        Invoke-DockerChecked @(
            "run", "--rm", $Image,
            "/opt/devkitpro/devkitARM/bin/arm-none-eabi-gcc", "--version"
        )
        Write-Host "ClouDS Music build environment is ready."
    }
    "test" {
        Invoke-ContainerMake @("repo-check", "host-test")
    }
    "3dsx" {
        Invoke-ContainerMake @("-j2", "emulator-build")
    }
    "cia" {
        Invoke-ContainerMake @("-j2", "cia")
    }
    "old3ds" {
        Invoke-ContainerMake @("-j2", "old3ds-stress")
    }
    "all" {
        Invoke-ContainerMake @("repo-check", "host-test")
        Invoke-ContainerMake @("-j2", "emulator-build")
    }
}
