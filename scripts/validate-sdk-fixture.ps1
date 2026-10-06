param([string]$Exe = "$PSScriptRoot/../build/windows-crash-dump-explainer.exe")
$ErrorActionPreference = 'Stop'
$root = [System.IO.Path]::GetFullPath("$PSScriptRoot/..")
$work = Join-Path $root ('artifacts/sdk-fixture-' + [guid]::NewGuid().ToString('N'))
New-Item -ItemType Directory -Path $work | Out-Null
$fixture = Join-Path $work 'self-snapshot.dmp'
& "$root/build-sdk-fixture/sdk_fixture.exe" $fixture
if ($LASTEXITCODE -ne 0) { throw 'SDK writer failed' }
$before = (Get-FileHash -LiteralPath $fixture -Algorithm SHA256).Hash
$raw = & $Exe $fixture --json
$code = $LASTEXITCODE
if ($code -ne 0 -and $code -ne 1) { throw "SDK fixture parse failed with exit $code : $raw" }
if (!($raw | Test-Json -SchemaFile "$root/docs/schema.json")) { throw 'SDK fixture schema failed' }
$j = $raw | ConvertFrom-Json
if ($j.system.architecture -ne 'x64' -or $j.exception.name -ne 'ACCESS_VIOLATION' -or $j.modules.Count -lt 3 -or $j.threads.Count -lt 1 -or $j.exception_location.module_paths.Count -ne 1) { throw 'SDK fixture semantic checks failed' }
if ((Get-FileHash -LiteralPath $fixture -Algorithm SHA256).Hash -ne $before) { throw 'Fixture was modified' }
$raw | Set-Content -Encoding utf8 (Join-Path $work 'report.json')
@{path=$fixture;exit_code=$code;modules=$j.modules.Count;threads=$j.threads.Count;system=$j.system;exception_location=$j.exception_location;warnings=$j.warnings;sha256=$before;no_crash=$true} | ConvertTo-Json -Depth 10
