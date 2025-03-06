li-win.exe -1 $env:TEMP\li_output
$li_output = Get-Content "$env:TEMP\li_output" -Raw
Set-Location $li_output