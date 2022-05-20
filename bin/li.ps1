icd.exe $env:TEMP\icd_output
$icd_output = Get-Content "$env:TEMP\icd_output" -Raw
cd $icd_output