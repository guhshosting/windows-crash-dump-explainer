param([Parameter(Mandatory)][string]$Exe, [Parameter(Mandatory)][string]$FixtureTool, [Parameter(Mandatory)][string]$Work, [Parameter(Mandatory)][string]$Schema)
$ErrorActionPreference = 'Stop'
New-Item -ItemType Directory -Path $Work -Force | Out-Null
& $FixtureTool --write-fixtures $Work
if ($LASTEXITCODE -ne 0) { throw 'Fixture generation failed' }
$script:checks = 0
function Check([bool]$Value, [string]$Name) { $script:checks++; if (!$Value) { throw "Check failed: $Name" } }
function Run([string[]]$Arguments) {
 $si = [System.Diagnostics.ProcessStartInfo]::new()
 $si.FileName = $Exe; $si.UseShellExecute = $false; $si.CreateNoWindow = $true
 $si.RedirectStandardOutput = $true; $si.RedirectStandardError = $true
 $si.StandardOutputEncoding = [System.Text.UTF8Encoding]::new($false)
 foreach ($arg in $Arguments) { $si.ArgumentList.Add($arg) }
 $proc = [System.Diagnostics.Process]::Start($si)
 $outTask = $proc.StandardOutput.ReadToEndAsync(); $errTask = $proc.StandardError.ReadToEndAsync()
 if (!$proc.WaitForExit(60000)) { $proc.Kill(); throw 'CLI timeout' }
 $r = @{Code=$proc.ExitCode; Text=$outTask.GetAwaiter().GetResult(); Error=$errTask.GetAwaiter().GetResult()}
 $proc.Dispose(); return $r
}
function JsonCheck($r, [int]$Code) {
 Check ($r.Code -eq $Code) "exit $Code"
 Check ($r.Error -eq '') 'no stray stderr'
 Check ($r.Text | Test-Json -SchemaFile $Schema) 'JSON schema'
 $j = $r.Text | ConvertFrom-Json
 Check ($j.exit_code -eq $Code) 'JSON exit code'
 return $j
}
$complete = Join-Path $Work 'complete.dmp'
$before = (Get-FileHash -LiteralPath $complete -Algorithm SHA256).Hash
$r = Run @($complete,'--json'); $j = JsonCheck $r 0
Check ($j.exception.name -eq 'ACCESS_VIOLATION') 'exception decode'
Check ($j.exception.memory_operation -eq 'write') 'operation'
Check ($j.modules.Count -eq 2) 'two modules'
Check ($j.unloaded_modules.Count -eq 1) 'unloaded module'
Check ($j.modules[0].file_version -eq '1.2.3.4') 'version'
Check ($j.process.process_id -eq 123) 'process ID'
Check ($j.exception_location.module_paths[0] -eq 'C:\test\sample.dll') 'location'
Check ($j.findings -join ' ' -match 'not proof of root cause') 'conservative explanation'
$r2 = Run @($complete,'--json'); Check ($r.Text -ceq $r2.Text) 'byte deterministic JSON'
foreach ($opt in @('--summary','--modules','--threads','--exception')) {
 $t = Run @($complete,$opt); Check ($t.Code -eq 0) "selector $opt"
 Check ($t.Text -match 'Crash location and root cause differ') 'selector retains caveat'
 $t = Run @($complete,$opt,'--json'); Check ($t.Text -ceq $r.Text) 'JSON selector independent'
}
$null = JsonCheck (Run @((Join-Path $Work 'minimal.dmp'),'--json')) 1
$null = JsonCheck (Run @((Join-Path $Work 'malformed.dmp'),'--json')) 2
$null = JsonCheck (Run @((Join-Path $Work 'missing.dmp'),'--json')) 3
$null = JsonCheck (Run @('--json')) 4
foreach ($argsCase in @(@($complete,'--bogus','--json'),@($complete,'--summary','--threads','--json'),@($complete,'--json','--json'),@($complete,$complete,'--json'))) { $null = JsonCheck (Run $argsCase) 4 }
foreach ($pathCase in @('\\not-a-server\share\crash.dmp','\\?\C:\crash.dmp','\\.\PhysicalDrive0',($complete + ':stream'))) { $null = JsonCheck (Run @($pathCase,'--json')) 3 }
$t = Run @('--version'); Check ($t.Code -eq 0 -and $t.Text.Trim() -eq 'windows-crash-dump-explainer 1.0.0') 'version'
$t = Run @('--help'); Check ($t.Code -eq 0 -and $t.Text -match 'Usage:') 'help'
$t = Run @(); Check ($t.Code -eq 4) 'no arguments'
$unicode = Join-Path $Work 'unicode space-試験.dmp'; Copy-Item -LiteralPath $complete -Destination $unicode -Force
$null = JsonCheck (Run @($unicode,'--json')) 0
$dash = Join-Path $Work '-dash.dmp'; Copy-Item -LiteralPath $complete -Destination $dash -Force
$null = JsonCheck (Run @('--json','--',$dash)) 0
$locked = [System.IO.File]::Open($complete,[System.IO.FileMode]::Open,[System.IO.FileAccess]::ReadWrite,[System.IO.FileShare]::None)
try { $null = JsonCheck (Run @($complete,'--json')) 3 } finally { $locked.Dispose() }
# Dedicated test-only junction, never followed by the analyzer.
$target = Join-Path $Work 'junction-target'; New-Item -ItemType Directory -Path $target -Force | Out-Null
Copy-Item -LiteralPath $complete -Destination (Join-Path $target 'fixture.dmp') -Force
$link = Join-Path $Work 'junction-link'
if (!(Test-Path -LiteralPath $link)) { New-Item -ItemType Junction -Path $link -Target $target | Out-Null }
$null = JsonCheck (Run @((Join-Path $link 'fixture.dmp'),'--json')) 3
Check ((Get-FileHash -LiteralPath $complete -Algorithm SHA256).Hash -eq $before) 'dump unchanged'
# Ensure a structurally invalid response is rejected by the independent schema validator.
$bad = $r.Text | ConvertFrom-Json; $bad.exception.address = 'invalid'; $badJson = $bad | ConvertTo-Json -Depth 40 -Compress
Check (!(Test-Json -Json $badJson -SchemaFile $Schema -ErrorAction SilentlyContinue)) 'schema negative control'
"$script:checks CLI/schema/read-only checks passed"
