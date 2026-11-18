$ErrorActionPreference = 'Stop'
$sourceDirectory = $PSScriptRoot
$buildDirectory = Join-Path $sourceDirectory 'build-msvc'
$logPath = Join-Path $sourceDirectory 'build-msvc.log'
$buildExitCode = 1
$transcriptStarted = $false

function Find-ExistingFile {
    param([string[]] $Candidates)
    foreach ($candidate in $Candidates) {
        if ($candidate -and (Test-Path -LiteralPath $candidate -PathType Leaf)) {
            return (Get-Item -LiteralPath $candidate).FullName
        }
    }
    return $null
}

function Invoke-BuildCommand {
    param([string] $Executable, [string[]] $CommandArguments)
    & $Executable @CommandArguments
    if ($LASTEXITCODE -ne 0) {
        throw "Command failed with exit code ${LASTEXITCODE}: $Executable"
    }
}

try {
    Start-Transcript -LiteralPath $logPath -Force | Out-Null
    $transcriptStarted = $true
    Write-Host '============================================================'
    Write-Host 'Configuring Storyland and building'
    Write-Host '============================================================'

    $visualStudioDirectory = $null
    $vswhere = Join-Path ${env:ProgramFiles(x86)} 'Microsoft Visual Studio\Installer\vswhere.exe'
    if (Test-Path -LiteralPath $vswhere) {
        $visualStudioDirectory = & $vswhere -latest -products '*' -requires Microsoft.VisualStudio.Component.VC.Tools.x86.x64 -property installationPath
        if ($visualStudioDirectory) {
            $visualStudioDirectory = ([string] $visualStudioDirectory).Trim()
        }
    }

    $cmake = $null
    $cmakeCommand = Get-Command cmake.exe -ErrorAction SilentlyContinue
    if ($cmakeCommand) { $cmake = $cmakeCommand.Source }
    if (-not $cmake -and $visualStudioDirectory) {
        $cmake = Find-ExistingFile -Candidates @(
            (Join-Path $visualStudioDirectory 'Common7\IDE\CommonExtensions\Microsoft\CMake\CMake\bin\cmake.exe')
        )
    }
    if (-not $cmake) {
        $cmake = Find-ExistingFile -Candidates @(
            (Join-Path $env:ProgramFiles 'CMake\bin\cmake.exe')
        )
    }
    if (-not $cmake) {
        throw 'CMake 3.20 or newer was not found. Install CMake or the C++ CMake tools component in Visual Studio Installer.'
    }
    $cmakeVersionText = & $cmake --version
    if ($LASTEXITCODE -ne 0) { throw 'CMake could not start.' }
    $cmakeVersionMatch = [regex]::Match(($cmakeVersionText -join "`n"), 'cmake version (\d+\.\d+\.\d+)')
    if (-not $cmakeVersionMatch.Success -or [version]$cmakeVersionMatch.Groups[1].Value -lt [version]'3.20.0') {
        throw 'CMake 3.20 or newer is required.'
    }

    $vcpkgCandidates = @()
    foreach ($previousBuild in @('build-msvc', 'build-portable', 'build')) {
        $previousCache = Join-Path $sourceDirectory "$previousBuild\CMakeCache.txt"
        if (Test-Path -LiteralPath $previousCache -PathType Leaf) {
            foreach ($cacheLine in Get-Content -LiteralPath $previousCache) {
                if ($cacheLine -match '^CMAKE_TOOLCHAIN_FILE:[^=]+=(.+)$') {
                    $vcpkgCandidates += $Matches[1]
                }
            }
        }
    }
    if ($env:VCPKG_ROOT) {
        $vcpkgCandidates += Join-Path ($env:VCPKG_ROOT.Trim('"')) 'scripts\buildsystems\vcpkg.cmake'
    }
    $vcpkgCommand = Get-Command vcpkg.exe -ErrorAction SilentlyContinue
    if ($vcpkgCommand) {
        $vcpkgCandidates += Join-Path (Split-Path $vcpkgCommand.Source -Parent) 'scripts\buildsystems\vcpkg.cmake'
    }
    $vcpkgCandidates += Join-Path $sourceDirectory 'vcpkg\scripts\buildsystems\vcpkg.cmake'
    $vcpkgCandidates += Join-Path (Split-Path $sourceDirectory -Parent) 'vcpkg\scripts\buildsystems\vcpkg.cmake'
    if ($env:USERPROFILE) {
        $vcpkgCandidates += Join-Path $env:USERPROFILE 'vcpkg\scripts\buildsystems\vcpkg.cmake'
    }
    if ($visualStudioDirectory) {
        $vcpkgCandidates += Join-Path $visualStudioDirectory 'VC\vcpkg\scripts\buildsystems\vcpkg.cmake'
    }
    $toolchain = Find-ExistingFile -Candidates $vcpkgCandidates
    if (-not $toolchain) {
        throw 'vcpkg was not found. Set VCPKG_ROOT to your existing vcpkg folder, or place that folder beside Storyland. Then run build_msvc.bat again.'
    }
    $env:VCPKG_ROOT = Split-Path (Split-Path (Split-Path $toolchain -Parent) -Parent) -Parent
    Write-Host "CMake: $cmake"
    Write-Host "vcpkg: $env:VCPKG_ROOT"
    Write-Host 'Dependency installation and build output follow below.'
    # Always reconfigure once at the beginning of each launch. When using
    # Visual Studio, disable its automatic regeneration during MSBuild;
    # otherwise nested CMake checks run vcpkg install repeatedly and can fail.
    $configureArguments = @(
        '-S', $sourceDirectory, '-B', $buildDirectory,
        '-G', 'Visual Studio 17 2022', '-A', 'x64',
        "-DCMAKE_TOOLCHAIN_FILE=$toolchain",
        '-DVCPKG_TARGET_TRIPLET=x64-windows-static',
        '-DCMAKE_SUPPRESS_REGENERATION=ON'
    )
    Invoke-BuildCommand -Executable $cmake -CommandArguments $configureArguments

    # Compile one project at a time to avoid exhausting RAM during the
    # unusually large Storyland translation units. Allow an explicit override.
    $buildWorkers = 1
    if ($env:STORYLAND_BUILD_WORKERS) {
        $requestedWorkers = 0
        if (-not [int]::TryParse($env:STORYLAND_BUILD_WORKERS, [ref] $requestedWorkers) -or $requestedWorkers -lt 1) {
            throw 'STORYLAND_BUILD_WORKERS must be a positive integer.'
        }
        $buildWorkers = $requestedWorkers
    }
    Write-Host "Building with $buildWorkers parallel worker(s)."
    $buildArguments = @(
        '--build', $buildDirectory, '--config', 'Release', '--parallel', "$buildWorkers"
    )
    if ($env:STORYLAND_CLEAN_BUILD -eq '1') {
        Write-Host 'Explicit clean build requested.'
        $buildArguments += '--clean-first'
    }
    Invoke-BuildCommand -Executable $cmake -CommandArguments $buildArguments
    Write-Host "Built: $(Join-Path $buildDirectory 'Release\Storyland.exe')"
    Write-Host 'OpenGL is the default renderer. Optional DirectX: Storyland.exe --renderer=directx'
    $buildExitCode = 0
}
catch {
    Write-Host ''
    Write-Host "ERROR: $($_.Exception.Message)" -ForegroundColor Red
}
finally {
    if ($transcriptStarted) { Stop-Transcript | Out-Null }
}
exit $buildExitCode
