param(
    [Parameter(ValueFromRemainingArguments = $true)]
    [string[]]$BuildArguments
)

$ErrorActionPreference = "Stop"
$ProjectDirectory = (Resolve-Path (Join-Path $PSScriptRoot "..")).Path
$OutputDirectory = Join-Path $ProjectDirectory "build-android"
$LogPath = Join-Path $OutputDirectory "build_android.log"
$BuildScript = Join-Path $PSScriptRoot "build_apk.py"
$ExitCode = 0
$TranscriptStarted = $false

try {
    New-Item -ItemType Directory -Force -Path $OutputDirectory | Out-Null
    Start-Transcript -Path $LogPath -Force | Out-Null
    $TranscriptStarted = $true

    Write-Host "Storyland Android build" -ForegroundColor Magenta
    Write-Host "Source: $ProjectDirectory"
    Write-Host "Build log: $LogPath"
    Write-Host ""

    $PythonCandidates = @(
        @{ Name = "py.exe"; Arguments = @("-3") },
        @{ Name = "python.exe"; Arguments = @() },
        @{ Name = "python3.exe"; Arguments = @() }
    )

    $PythonPath = $null
    $PythonPrefix = @()

    foreach ($Candidate in $PythonCandidates) {
        $Command = Get-Command $Candidate.Name -ErrorAction SilentlyContinue
        if ($null -eq $Command -or $Command.CommandType -ne "Application") {
            continue
        }

        $CheckArguments = @($Candidate.Arguments) + @("-c", "import sys; assert sys.version_info >= (3, 8)")
        $PreviousErrorActionPreference = $ErrorActionPreference
        $ErrorActionPreference = "Continue"
        try {
            & $Command.Source @CheckArguments *> $null
            $CheckResult = $LASTEXITCODE
        }
        finally {
            $ErrorActionPreference = $PreviousErrorActionPreference
        }

        if ($CheckResult -eq 0) {
            $PythonPath = $Command.Source
            $PythonPrefix = @($Candidate.Arguments)
            break
        }
    }

    if (-not $PythonPath) {
        throw "Python 3.8+ was not found. Install Python 3 and enable 'Add python.exe to PATH', then rerun build_android.bat."
    }

    Write-Host "Python: $PythonPath $($PythonPrefix -join ' ')"
    Write-Host ""

    $ErrorActionPreference = "Continue"
    & $PythonPath @PythonPrefix $BuildScript @BuildArguments
    $PythonExitCode = $LASTEXITCODE
    $ErrorActionPreference = "Stop"

    if ($PythonExitCode -ne 0) {
        throw "APK build exited with code $PythonExitCode. Review the error above and the build log."
    }

    Write-Host ""
    Write-Host "APK BUILT SUCCESSFULLY" -ForegroundColor Green
    Get-ChildItem -Path $OutputDirectory -Filter "*.apk" -File | ForEach-Object {
        Write-Host $_.FullName -ForegroundColor Green
    }
}
catch {
    $ExitCode = 1
    Write-Host ""
    Write-Host "APK BUILD FAILED" -ForegroundColor Red
    Write-Host $_.Exception.Message -ForegroundColor Red
    Write-Host "Build log: $LogPath"
}
finally {
    if ($TranscriptStarted) {
        Stop-Transcript | Out-Null
    }
}

exit $ExitCode
