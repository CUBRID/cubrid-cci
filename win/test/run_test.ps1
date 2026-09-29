<#
.SYNOPSIS
  Compiles every test under testcases\ against a driver already built into win\output
  and runs them.

.DESCRIPTION
  win\build.bat calls this for its test target, passing the install prefix and the
  architecture of the build it just made, so the tests always exercise the driver that
  is actually in win\output rather than one found on PATH.

  Each test is built the way anything consuming the package is: include\cas_cci.h to
  compile, the import library in lib\ to link and the DLL in bin\ beside the executable
  to run. A package whose lib cannot be linked therefore fails here rather than
  reaching users.

  Every .c file under testcases\ is a test. It takes the config file as argv[1] and
  returns 0 when it passes, so adding a case is a matter of dropping a file in that
  folder - nothing here needs to change.

  Only the stand-alone driver can be tested. The FOR_OTHER_DRIVER package (cas_cci.dll)
  has no DllMain, so cci_init() is the embedding driver's job and these cases never
  call it; finding that package is reported as skipped rather than run.

  cl.exe only exists inside a developer prompt, so the Visual Studio environment is
  set up here the same way win\external\build_openssl.ps1 does it - VS2017 by
  preference, the newest installed otherwise.

.PARAMETER Prefix
  Install prefix holding bin, lib and include - what build.bat calls BUILD_PREFIX.
  Defaults to win\output\CUBRID_Release_x64_V141, resolved next to this script so it
  works from any working directory.

.PARAMETER Arch
  x86 or x64. Must match the driver in Prefix, since a 32bit process cannot load a
  64bit DLL.

.PARAMETER Config
  Server settings. Defaults to cci_test.conf next to this script, which is the one
  file the tests read their connection details from.

.PARAMETER Name
  Wildcard picking which testcases to run, matched against the file name without .c.
  Defaults to *, meaning every case in the folder.

.PARAMETER BuildOnly
  Compile the tests but do not run them, for checking the build where no server is
  reachable.

.EXAMPLE
  .\run_test.ps1

.EXAMPLE
  .\run_test.ps1 -Prefix ..\output\CUBRID_Debug_x64_V141 -Arch x64

.EXAMPLE
  .\run_test.ps1 -Name basic_test
#>

[CmdletBinding()]
param(
    [string] $Prefix,

    [ValidateSet('x86', 'x64')]
    [string] $Arch = 'x64',

    [string] $Config,

    [string] $Name = '*',

    [switch] $BuildOnly
)

Set-StrictMode -Version Latest
$ErrorActionPreference = 'Stop'

$TestDir = $PSScriptRoot
$CaseDir = Join-Path $TestDir 'testcases'

# $PSScriptRoot is still empty while the param block is bound, so the defaults that
# depend on this script's own location are filled in here.
if (-not $Prefix) {
    $Prefix = Join-Path $TestDir '..\output\CUBRID_Release_x64_V141'
}
if (-not $Config) {
    $Config = Join-Path $TestDir 'cci_test.conf'
}

function Import-VisualStudioEnvironment {
    param([Parameter(Mandatory = $true)][string] $VcvarsArch)

    $vswhere = Join-Path ${env:ProgramFiles(x86)} 'Microsoft Visual Studio\Installer\vswhere.exe'
    if (-not (Test-Path $vswhere)) {
        $vswhere = Join-Path $env:ProgramFiles 'Microsoft Visual Studio\Installer\vswhere.exe'
    }
    if (-not (Test-Path $vswhere)) {
        throw 'vswhere.exe not found. Open a "Native Tools Command Prompt" and run this again.'
    }

    $cppToolset = 'Microsoft.VisualStudio.Component.VC.Tools.x86.x64'

    # VS2017 first, to match the toolset build.bat defaults to.
    $vsPath = & $vswhere -products * -version '[15.0,16.0)' -requires $cppToolset -property installationPath |
              Select-Object -Last 1
    if (-not $vsPath) {
        $vsPath = & $vswhere -latest -products * -requires $cppToolset -property installationPath |
                  Select-Object -Last 1
    }
    if (-not $vsPath) {
        throw 'no Visual Studio with the C++ toolset was found.'
    }

    $vcvars = Join-Path $vsPath 'VC\Auxiliary\Build\vcvarsall.bat'
    if (-not (Test-Path $vcvars)) {
        throw "vcvarsall.bat not found under $vsPath."
    }
    Write-Host "  Visual Studio : $vsPath"

    $dump = & cmd.exe /c "call `"$vcvars`" $VcvarsArch >nul && set"
    if ($LASTEXITCODE -ne 0) {
        throw "vcvarsall.bat $VcvarsArch failed."
    }
    foreach ($line in $dump) {
        if ($line -match '^([^=][^=]*)=(.*)$') {
            [Environment]::SetEnvironmentVariable($matches[1], $matches[2], 'Process')
        }
    }
}

function Get-DriverFile {
    # The package ships exactly one import library and one DLL, but their names depend on
    # how it was built - cascci for the stand-alone driver, cas_cci for the other-driver
    # build, each carrying a _d suffix in Debug - so they are discovered rather than
    # spelled out here.
    param(
        [Parameter(Mandatory = $true)][string] $Dir,
        [Parameter(Mandatory = $true)][string] $Pattern,
        [Parameter(Mandatory = $true)][string] $What
    )

    if (-not (Test-Path $Dir)) {
        throw "[$Dir] is missing. Run build.bat build first. /other builds are not supported here."
    }
    $found = @(Get-ChildItem -Path $Dir -Filter $Pattern -File -ErrorAction SilentlyContinue)
    if ($found.Count -eq 0) {
        throw "no $What ($Pattern) under [$Dir]. Run build.bat build first. /other builds are not supported here."
    }
    if ($found.Count -gt 1) {
        throw "[$Dir] holds more than one $What ($($found.Name -join ', ')) - cannot tell which driver to test."
    }
    return $found[0].FullName
}

try {
    $prefixFull = (Resolve-Path -LiteralPath $Prefix -ErrorAction SilentlyContinue)
    if (-not $prefixFull) {
        throw "the install prefix [$Prefix] does not exist. Run build.bat build first."
    }
    $prefixFull = $prefixFull.Path

    $incDir  = Join-Path $prefixFull 'include'
    $libFile = Get-DriverFile -Dir (Join-Path $prefixFull 'lib') -Pattern '*.lib' -What 'import library'
    $dllFile = Get-DriverFile -Dir (Join-Path $prefixFull 'bin') -Pattern '*.dll' -What 'driver DLL'

    foreach ($needed in $incDir, $Config, $CaseDir) {
        if (-not (Test-Path $needed)) {
            throw "[$needed] is missing. Run build.bat build first."
        }
    }

    $cases = @(Get-ChildItem -Path $CaseDir -Filter "$Name.c" -File | Sort-Object Name)
    if ($cases.Count -eq 0) {
        throw "no test source matching [$Name.c] under [$CaseDir]."
    }
    $caseNames = ($cases | ForEach-Object { $_.BaseName }) -join ', '

    $outDir = Join-Path $TestDir "out\$Arch"
    New-Item -ItemType Directory -Force -Path $outDir | Out-Null

    Write-Host '=========================================================='
    Write-Host " testcases     : $($cases.Count) ($caseNames)"
    Write-Host " driver        : $prefixFull"
    Write-Host "                 $(Split-Path -Leaf $libFile) / $(Split-Path -Leaf $dllFile)"
    Write-Host " architecture  : $Arch"
    Write-Host " config        : $Config"
    Write-Host " build dir     : $outDir"
    Write-Host '=========================================================='

    # FOR_OTHER_DRIVER compiles without CAS_CCI_DL, so that DLL has no DllMain and
    # nothing calls cci_init() when it loads - the driver embedding it is expected to.
    # These testcases call straight into the API, so against that build they would run
    # with uninitialised mutexes. The cas_cci name is the only signal there is for it.
    $dllName = Split-Path -Leaf $dllFile
    if ($dllName -like 'cas_cci*') {
        Write-Host ''
        Write-Host "SKIPPED: [$dllName] is the other-driver build (FOR_OTHER_DRIVER), which is"
        Write-Host '         not supported here. It carries no DllMain, so cci_init() has to be'
        Write-Host '         called by whatever driver embeds it and these testcases do not.'
        Write-Host '         Build without /other and test the stand-alone cascci.dll instead.'
        exit 0
    }

    # A developer prompt targets one architecture, so VSCMD_ARG_TGT_ARCH decides whether
    # the inherited environment can be reused or vcvarsall has to run for this one.
    if ((-not (Get-Command cl.exe -ErrorAction SilentlyContinue)) -or ($env:VSCMD_ARG_TGT_ARCH -ne $Arch)) {
        Write-Host "Setting up the Visual Studio environment for $Arch ..."
        Import-VisualStudioEnvironment -VcvarsArch $Arch
        if (-not (Get-Command cl.exe -ErrorAction SilentlyContinue)) {
            throw 'cl.exe is still unavailable after vcvarsall.'
        }
    }

    # Copied rather than found through PATH so the tests cannot pick up some other driver.
    Copy-Item -Force -Path $dllFile -Destination $outDir

    # cl.exe and the test executables report through stderr, which a 'Stop' preference
    # turns into a terminating NativeCommandError as soon as this script's output is
    # redirected. Exit codes are what decides pass or fail from here on.
    $ErrorActionPreference = 'Continue'

    $results = @()
    foreach ($case in $cases) {
        $exeName = "$($case.BaseName).exe"
        $exePath = Join-Path $outDir $exeName

        Write-Host ''
        Write-Host "---------- $($case.Name) ----------"

        Push-Location $outDir
        try {
            # /MD matches the CRT the DLL is built with. _CRT_SECURE_NO_WARNINGS keeps the
            # C4996 noise for strncpy and friends out of the output.
            & cl.exe /nologo /W3 /MD /D_CRT_SECURE_NO_WARNINGS /I $incDir "/Fe:$exeName" $case.FullName /link $libFile
            $rc = $LASTEXITCODE
        } finally {
            Pop-Location
        }

        if ($rc -ne 0) {
            Write-Host "  compiling $($case.Name) failed (cl exit $rc)."
            $results += [pscustomobject]@{ Name = $case.BaseName; Status = 'BUILD FAILED'; Exit = $rc }
            continue
        }

        if ($BuildOnly) {
            Write-Host "  built $exePath"
            $results += [pscustomobject]@{ Name = $case.BaseName; Status = 'BUILT'; Exit = 0 }
            continue
        }

        Write-Host ''
        & $exePath $Config
        $rc = $LASTEXITCODE
        if ($rc -eq 0) { $status = 'PASS' } else { $status = 'FAIL' }
        $results += [pscustomobject]@{ Name = $case.BaseName; Status = $status; Exit = $rc }
    }

    $ErrorActionPreference = 'Stop'

    Write-Host ''
    Write-Host '========================== summary ======================='
    foreach ($r in $results) {
        if ($r.Exit -ne 0) { $tail = " (exit $($r.Exit))" } else { $tail = '' }
        Write-Host ("  {0,-28} {1}{2}" -f $r.Name, $r.Status, $tail)
    }
    Write-Host '=========================================================='

    $failed = @($results | Where-Object { $_.Exit -ne 0 })
    if ($failed.Count -eq 0) {
        Write-Host "All $($results.Count) test(s) passed."
        exit 0
    }
    Write-Host "$($failed.Count) of $($results.Count) test(s) failed."
    exit 1
} catch {
    Write-Host "ERROR: $($_.Exception.Message)"
    exit 1
}
