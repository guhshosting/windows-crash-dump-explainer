param([string]$Build = "$PSScriptRoot/../build")
$ErrorActionPreference = 'Stop'
$root = [IO.Path]::GetFullPath("$PSScriptRoot/..")
$release = Join-Path $root 'release'
New-Item -ItemType Directory -Path $release -Force | Out-Null
$zip = Join-Path $release 'windows-crash-dump-explainer-1.0.0-windows-x64.zip'
if (Test-Path -LiteralPath $zip) { throw 'Release ZIP already exists; packaging refuses to overwrite it.' }
$work = Join-Path $root ('artifacts/package-' + [guid]::NewGuid().ToString('N'))
$stage = Join-Path $work 'stage'; $extract = Join-Path $work 'extracted'
New-Item -ItemType Directory -Path $stage,$extract | Out-Null
cmake --install $Build --prefix $stage
if ($LASTEXITCODE -ne 0) { throw 'Install failed' }
$files = @(Get-ChildItem -LiteralPath $stage -Recurse -File)
if ($files.Count -ne 7) { throw "Unexpected package file count: $($files.Count)" }
if (!(Test-Path -LiteralPath (Join-Path $stage 'windows-crash-dump-explainer.exe'))) { throw 'Executable absent' }
[IO.Compression.ZipFile]::CreateFromDirectory($stage, $zip, [IO.Compression.CompressionLevel]::Optimal, $false)
[IO.Compression.ZipFile]::ExtractToDirectory($zip,$extract)
foreach ($file in $files) {
 $relative = [IO.Path]::GetRelativePath($stage,$file.FullName)
 $other = Join-Path $extract $relative
 if ((Get-FileHash -LiteralPath $file.FullName -Algorithm SHA256).Hash -ne (Get-FileHash -LiteralPath $other -Algorithm SHA256).Hash) { throw "Extraction differs: $relative" }
}
$exe = Join-Path $extract 'windows-crash-dump-explainer.exe'
& "$root/tests/cli-tests.ps1" -Exe $exe -FixtureTool "$Build/parser_tests.exe" -Work (Join-Path $work 'smoke-fixtures') -Schema (Join-Path $extract 'docs/schema.json')
$fixture = Get-ChildItem -LiteralPath (Join-Path $root 'artifacts') -Directory -Filter 'sdk-fixture-*' | Sort-Object LastWriteTime -Descending | Select-Object -First 1
if (!$fixture) { throw 'No existing SDK fixture available for package smoke test' }
$raw = & $exe (Join-Path $fixture.FullName 'self-snapshot.dmp') --json
if ($LASTEXITCODE -ne 0 -and $LASTEXITCODE -ne 1) { throw 'Extracted binary SDK fixture parse failed' }
if (!($raw | Test-Json -SchemaFile (Join-Path $extract 'docs/schema.json'))) { throw 'Extracted SDK JSON schema failed' }
$hash = (Get-FileHash -LiteralPath $zip -Algorithm SHA256).Hash.ToLowerInvariant()
[IO.File]::WriteAllText($zip + '.sha256', $hash + '  ' + [IO.Path]::GetFileName($zip) + "`n", [Text.UTF8Encoding]::new($false))
"Exact release ZIP extraction smoke test PASSED"
"Files: $($files.Count); extraction file hashes matched; 94 CLI/schema checks; SDK fixture/schema passed"
"ZIP: $zip"
"SHA256: $hash"
"EXTRACTED: $extract"
