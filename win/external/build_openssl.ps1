<#
.SYNOPSIS
  Downloads and builds the OpenSSL that win\build.bat links against.

.DESCRIPTION
  The Visual Studio environment is set up per architecture with vswhere and vcvarsall -
  VS2017 by preference, the newest installed otherwise - so neither a developer prompt
  nor nmake on PATH is needed.

  VS2017 (v141) and later are fine even though the folders are named v140: MSVC has
  kept a stable ABI across v140/v141/v142, and win\build.bat already defaults to the
  VS2017 generator while linking these same archives.

  Requirements: Perl (Strawberry Perl), Visual Studio and NASM 2.15 or later. NASM is
  mandatory - see the nasm check in Invoke-OpensslBuild for why no-asm is not an option.

  jom is optional but worth installing: nmake compiles OpenSSL's ~1100 source files one
  at a time, so the compile is effectively the whole build, and jom runs the same
  makefile across every core.

.PARAMETER Arch
  all (the default) builds Win32 and x64, 32 builds Win32 only, 64 builds x64 only.
  The /all, /32 and /64 spellings are accepted as well, so build_openssl.bat can pass
  its arguments straight through.

.PARAMETER Keep
  Keep the work directory (openssl_build) instead of deleting it.

.PARAMETER NoParallel
  Build with nmake even when jom is on PATH, and drop the note about installing it.

.EXAMPLE
  .\build_openssl.ps1

  Builds both architectures.

.EXAMPLE
  .\build_openssl.ps1 /64 (default : all)

.EXAMPLE
  .\build_openssl.ps1 /all /keep
#>

[CmdletBinding()]
param(
    [Parameter(Position = 0)]
    [string] $Arch,

    [switch] $Keep,

    [switch] $NoParallel,

    [switch] $Help,

    [Parameter(ValueFromRemainingArguments = $true)]
    [string[]] $Rest
)

Set-StrictMode -Version Latest
$ErrorActionPreference = 'Stop'

$OpensslVersion = '3.5.7'
$OpensslUrl     = "https://github.com/CUBRID/3rdparty/raw/develop/openssl/openssl-$OpensslVersion.tar.gz"
$OpensslSha256  = 'a8c0d28a529ca480f9f36cf5792e2cd21984552a3c8e4aa11a24aa31aeac98e8'
$NasmMinVersion = [version]'2.15'

$ExternalDir = $PSScriptRoot
$WorkDir     = Join-Path $ExternalDir 'openssl_build'
$IncludeDir  = Join-Path $ExternalDir 'openssl\include'


foreach ($name in @('LC_ALL', 'LANG', 'LANGUAGE', 'LC_CTYPE', 'LC_NUMERIC', 'LC_COLLATE',
                    'LC_TIME', 'LC_MONETARY', 'LC_MESSAGES')) {
    if ($null -ne [Environment]::GetEnvironmentVariable($name, 'Process')) {
        [Environment]::SetEnvironmentVariable($name, $null, 'Process')
    }
}

$BaselineEnv = @{}
foreach ($entry in [Environment]::GetEnvironmentVariables('Process').GetEnumerator()) {
    $BaselineEnv[$entry.Key] = $entry.Value
}

function Show-Usage {
    Write-Host @"

Usage: build_openssl.ps1 [-Arch all|32|64] [-Keep] [-NoParallel]

  -Arch all  build Win32 and x64 - the default, so plain build_openssl.ps1 does this
  -Arch 32   build for Win32 only
  -Arch 64   build for x64 only
  -Keep      keep the work directory (openssl_build) instead of deleting it
  -NoParallel  build with nmake even when jom is on PATH

The /all, /32, /64 and /keep spellings of the old batch script are accepted too, which
is what build_openssl.bat forwards.

Downloads openssl-$OpensslVersion, builds it statically and copies libssl.lib,
libcrypto.lib and the headers into openssl\lib_v140 | lib64_v140 | include.
"@
}

function Reset-EnvironmentToBaseline {
    foreach ($name in @([Environment]::GetEnvironmentVariables('Process').Keys)) {
        if (-not $BaselineEnv.ContainsKey($name)) {
            [Environment]::SetEnvironmentVariable($name, $null, 'Process')
        }
    }
    foreach ($entry in $BaselineEnv.GetEnumerator()) {
        [Environment]::SetEnvironmentVariable($entry.Key, $entry.Value, 'Process')
    }
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

    # VS2017 comes first: win\build.bat defaults to the "Visual Studio 15 2017" generator,
    # so the archives dropped into lib_v140 / lib64_v140 come from the toolset that links
    # them. -latest on its own hands back VS2019 or VS2022 whenever one sits next to
    # VS2017, hence the version range, with -latest kept only as the fallback.
    $vsPath = & $vswhere -products * -version '[15.0,16.0)' -requires $cppToolset -property installationPath |
              Select-Object -Last 1
    if (-not $vsPath) {
        Write-Host '  WARNING: VS2017 was not found - falling back to the newest Visual Studio installed.'
        $vsPath = & $vswhere -latest -products * -requires $cppToolset -property installationPath |
                  Select-Object -Last 1
    }
    if (-not $vsPath) {
        throw 'no Visual Studio with the C++ toolset was found.'
    }
    Write-Host "  Visual Studio: $vsPath"

    $vcvars = Join-Path $vsPath 'VC\Auxiliary\Build\vcvarsall.bat'
    if (-not (Test-Path $vcvars)) {
        throw "vcvarsall.bat not found under $vsPath."
    }

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

function Resolve-MakeTool {
    # nmake compiles one file at a time and OpenSSL has over a thousand of them, so the
    # compile phase is effectively the whole build. jom is Qt's drop-in nmake replacement
    # that drives the same makefile across every core; without it the build still works,
    # just serially.
    $script:MakeExe   = 'nmake'
    $script:MakeArgs  = @()
    $script:MakeLabel = 'nmake, one file at a time'

    if ($NoParallel) { return }

    $jom = Get-Command jom -ErrorAction SilentlyContinue
    if ($jom) {
        $jobs = [Environment]::ProcessorCount
        $script:MakeExe   = $jom.Source
        $script:MakeArgs  = @('-j', "$jobs")
        $script:MakeLabel = "jom -j $jobs"
        return
    }

    Write-Host ''
    Write-Host 'NOTE: jom was not found, so OpenSSL is compiled one file at a time while'
    Write-Host "      $([Environment]::ProcessorCount) cores sit idle. jom is a drop-in nmake replacement that builds"
    Write-Host '      the same makefile in parallel - unpack jom.exe from'
    Write-Host '        https://download.qt.io/official_releases/jom/jom.zip'
    Write-Host '      onto PATH. Pass -NoParallel to skip this note.'
    Write-Host ''
}

function Get-NasmRequirementMessage {
    param([Parameter(Mandatory = $true)][string] $Reason)

    return @"
$Reason

NASM $NasmMinVersion or later is required. OpenSSL needs it to assemble the AES-NI and
SHA-NI code paths; building with no-asm would leave the bundled archives measurably
slower at runtime, so the build stops here rather than degrading silently.

  Download : https://www.nasm.us/pub/nasm/releasebuilds/
             pick a release >= $NasmMinVersion and run nasm-<version>-installer-x64.exe
  Or       : winget install --id NASM.NASM

Add the install directory (C:\Program Files\NASM by default) to PATH, open a new
shell and run this script again.
"@
}

function Invoke-OpensslBuild {
    param([Parameter(Mandatory = $true)][ValidateSet('32', '64')][string] $Target)

    if ($Target -eq '64') {
        $configureTarget = 'VC-WIN64A'
        $libDir          = Join-Path $ExternalDir 'openssl\lib64_v140'
        $vcvarsArch      = 'x64'
    } else {
        $configureTarget = 'VC-WIN32'
        $libDir          = Join-Path $ExternalDir 'openssl\lib_v140'
        $vcvarsArch      = 'x86'
    }

    # no-apps and no-makedepend are build-time only - what gets bundled is the two static
    # libraries plus the headers, nothing else. no-makedepend is the expensive one: with
    # dependency tracking on, OpenSSL compiles every source file twice, once for real and
    # once more as a "cl /Zs /showIncludes" pass inside its own cmd.exe just to write a .d
    # file, and a tree configured from scratch and deleted afterwards never reads one back.
    #
    # /FS is what makes a parallel build possible at all. The VC targets put debug info
    # for every object into one ossl_static.pdb (see /Fd in LIB_CFLAGS), so concurrent
    # cl.exe processes fight over it and die with "fatal error C1041". /FS routes those
    # writes through mspdbsrv. It is set unconditionally rather than only alongside jom,
    # because a flag that depends on the make tool is a trap for whoever changes it next.
    $configureOpts = @('no-shared', 'no-module', 'no-docs', 'no-tests',
                       'no-apps', 'no-makedepend', '/FS')

    Write-Host '=========================================================='
    Write-Host " OpenSSL      : $OpensslVersion ($configureTarget)"
    Write-Host " work dir     : $WorkDir"
    Write-Host " libraries to : $libDir"
    Write-Host " headers to   : $IncludeDir"
    Write-Host '=========================================================='

    if ((-not (Get-Command nmake -ErrorAction SilentlyContinue)) -or
        ($env:VSCMD_ARG_TGT_ARCH -ne $vcvarsArch)) {
        Write-Host "Setting up the Visual Studio environment for $vcvarsArch ..."
        Import-VisualStudioEnvironment -VcvarsArch $vcvarsArch
        if (-not (Get-Command nmake -ErrorAction SilentlyContinue)) {
            throw 'nmake is still unavailable after vcvarsall.'
        }
    }

    if (-not (Get-Command perl -ErrorAction SilentlyContinue)) {
        throw 'perl not found. Install Strawberry Perl and put it on PATH.'
    }

    # A no-asm build drops OpenSSL's AES-NI and SHA-NI implementations, which costs real
    # TLS throughput in every driver that links these archives. Missing or outdated NASM
    # therefore fails the build instead of quietly producing a slower library.
    $nasm = Get-Command nasm -ErrorAction SilentlyContinue
    if (-not $nasm) {
        throw (Get-NasmRequirementMessage 'nasm was not found on PATH.')
    }

    $nasmBanner = (& nasm -v) -join ' '
    $nasmMatch  = [regex]::Match($nasmBanner, 'NASM version (\d+(?:\.\d+)+)')
    if (-not $nasmMatch.Success) {
        throw (Get-NasmRequirementMessage "the NASM version could not be read from '$nasmBanner'.")
    }

    $nasmText    = $nasmMatch.Groups[1].Value
    $nasmVersion = [version]$nasmText
    if ($nasmVersion -lt $NasmMinVersion) {
        throw (Get-NasmRequirementMessage "NASM $nasmText at $($nasm.Source) is too old.")
    }
    Write-Host " NASM         : $nasmText ($($nasm.Source))"

    if (-not (Get-Command tar.exe -ErrorAction SilentlyContinue)) {
        throw 'tar.exe not found - it ships with Windows 10 1803 and later.'
    }

    New-Item -ItemType Directory -Force -Path $WorkDir | Out-Null
    $tarball = Join-Path $WorkDir "openssl-$OpensslVersion.tar.gz"

    if (Test-Path $tarball) {
        Write-Host 'Using the tarball already downloaded.'
    } else {
        Write-Host "Downloading $OpensslUrl ..."
        # Windows PowerShell 5.1 still negotiates TLS 1.0 by default and github.com refuses
        # that, and the progress bar alone makes Invoke-WebRequest several times slower.
        [Net.ServicePointManager]::SecurityProtocol =
            [Net.ServicePointManager]::SecurityProtocol -bor [Net.SecurityProtocolType]::Tls12
        $partial     = "$tarball.part"
        $oldProgress = $ProgressPreference
        $ProgressPreference = 'SilentlyContinue'
        try {
            Invoke-WebRequest -Uri $OpensslUrl -OutFile $partial -UseBasicParsing
            Move-Item -Force -Path $partial -Destination $tarball
        } catch {
            if (Test-Path $partial) { Remove-Item -Force $partial }
            throw "download failed: $($_.Exception.Message)"
        } finally {
            $ProgressPreference = $oldProgress
        }
    }

    Write-Host 'Verifying SHA256 ...'
    $actualSha256 = (Get-FileHash -Path $tarball -Algorithm SHA256).Hash
    if ($actualSha256 -ne $OpensslSha256) {
        Remove-Item -Force $tarball
        throw "SHA256 mismatch - expected $OpensslSha256, got $actualSha256. The tarball was deleted; run again."
    }

    # Always start from a clean source tree: a 32-bit build cannot reuse a 64-bit one.
    $srcDir = Join-Path $WorkDir "openssl-$OpensslVersion"
    if (Test-Path $srcDir) { Remove-Item -Recurse -Force $srcDir }
    Write-Host 'Extracting ...'
    & tar.exe -xzf $tarball -C $WorkDir
    if ($LASTEXITCODE -ne 0) { throw 'extract failed.' }

    $prefix = Join-Path $WorkDir "install_$Target"
    Push-Location $srcDir
    try {
        Write-Host 'Configuring ...'
        & perl Configure $configureTarget @configureOpts "--prefix=$prefix"
        if ($LASTEXITCODE -ne 0) { throw 'Configure failed.' }

        # install_dev pulls in build_libs and nothing else, then installs the headers,
        # libcrypto.lib, libssl.lib and ossl_static.pdb - precisely what is copied below.
        # Bare "nmake" is the all target, which also builds apps\openssl.exe, and
        # install_sw walks the engine, module and runtime targets that never ship here.
        Write-Host "Building and installing ($script:MakeLabel) ..."
        $makeArgs = $script:MakeArgs
        & $script:MakeExe @makeArgs install_dev
        if ($LASTEXITCODE -ne 0) { throw 'build failed.' }
    } finally {
        Pop-Location
    }

    Write-Host 'Copying into the bundled folders ...'
    New-Item -ItemType Directory -Force -Path $libDir | Out-Null
    foreach ($lib in 'libssl.lib', 'libcrypto.lib') {
        Copy-Item -Force -Path (Join-Path $prefix "lib\$lib") -Destination $libDir
    }
    $pdb = Join-Path $prefix 'lib\ossl_static.pdb'
    if (Test-Path $pdb) {
        Copy-Item -Force -Path $pdb -Destination $libDir
    }

    # Every generated header is identical for both architectures except configuration.h,
    # which carries the word size - THIRTY_TWO_BIT plus BN_LLONG for Win32 against
    # SIXTY_FOUR_BIT for x64 - and bn.h turns that into the public BN_ULONG typedef.
    # Sharing one copy would leave whichever architecture built last with a BN_ULONG that
    # silently disagrees with its own libcrypto, so each keeps its own file and
    # configuration.h becomes a stub that picks between them at compile time.
    $dstInclude = Join-Path $IncludeDir 'openssl'
    $otherArch  = if ($Target -eq '64') { '32' } else { '64' }
    $otherFile  = Join-Path $dstInclude "configuration_$otherArch.h"
    $otherSaved = if (Test-Path $otherFile) { [IO.File]::ReadAllBytes($otherFile) } else { $null }

    if (Test-Path $dstInclude) { Remove-Item -Recurse -Force $dstInclude }
    New-Item -ItemType Directory -Force -Path $IncludeDir | Out-Null
    Copy-Item -Recurse -Force -Path (Join-Path $prefix 'include\openssl') -Destination $dstInclude

    Move-Item -Force -Path (Join-Path $dstInclude 'configuration.h') -Destination (Join-Path $dstInclude "configuration_$Target.h")
    if ($otherSaved) { [IO.File]::WriteAllBytes($otherFile, $otherSaved) }

    # A configuration_NN.h that is absent because that architecture was never built makes
    # the include below fail by name, rather than the build quietly using the wrong one.
    Set-Content -Path (Join-Path $dstInclude 'configuration.h') -Encoding ascii -Value @(
        '/* Generated by build_openssl.ps1 - do not edit. */',
        '#if defined(_WIN64) || defined(_M_X64) || defined(_M_AMD64) || defined(_M_ARM64)',
        '# include <openssl/configuration_64.h>',
        '#else',
        '# include <openssl/configuration_32.h>',
        '#endif'
    )

    Write-Host ''
    Write-Host "$configureTarget done - $libDir now holds openssl-$OpensslVersion."
    Write-Host ''
}

# The batch wrapper forwards its arguments untouched, so /all and /keep arrive here as
# positional values rather than as PowerShell parameters. Both spellings fold into the
# same two settings below.
$targets = @()
$tokens  = @()
if ($Arch) { $tokens += $Arch }
if ($Rest) { $tokens += $Rest }

foreach ($token in $tokens) {
    switch -Regex ($token) {
        '^[/-]?all$'            { $targets += @('32', '64'); continue }
        '^[/-]?(32|x86|win32)$' { $targets += '32'; continue }
        '^[/-]?(64|x64)$'       { $targets += '64'; continue }
        '^[/-]keep$'            { $Keep = $true; continue }
        '^[/-]no-?parallel$'    { $NoParallel = $true; continue }
        '^[/-](h|\?|help)$'     { $Help = $true; continue }
        default {
            Write-Host "Unknown option: $token"
            Show-Usage
            exit 1
        }
    }
}

if ($Help) {
    Show-Usage
    exit 0
}

# all is the default: the two bundled folders are only consistent with each other when
# both were built from the same source tree, and building one alone quietly leaves the
# other on whatever version it held before.
if (-not $targets) { $targets = @('32', '64') }
$targets = @($targets | Select-Object -Unique)

try {
    Resolve-MakeTool
    foreach ($target in $targets) {
        Reset-EnvironmentToBaseline
        Invoke-OpensslBuild -Target $target
    }

    if ($Keep) {
        Write-Host "Work directory kept at $WorkDir."
    } elseif (Test-Path $WorkDir) {
        Write-Host 'Removing the work directory ...'
        Remove-Item -Recurse -Force $WorkDir
    }

    Write-Host ''
    Write-Host "Done. openssl-$OpensslVersion is bundled for: $($targets -join ', ')."
    Write-Host 'Build the driver with'
    Write-Host '  win\build.bat /vs2017 /64 release'
} catch {
    Write-Host "ERROR: $($_.Exception.Message)"
    exit 1
}
