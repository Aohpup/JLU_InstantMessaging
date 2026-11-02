param([string]$InputDoc,[string]$OutputPdf)
$ErrorActionPreference='Stop'
$taskWord=New-Object -ComObject Word.Application
$taskWord.Visible=$false
$taskWord.DisplayAlerts=0
$taskWord.AutomationSecurity=3
try {
 $taskDoc=$taskWord.Documents.Open($InputDoc,$false,$true)
 $null=$taskDoc.Fields.Update()
 foreach($section in $taskDoc.Sections) { foreach($footer in $section.Footers) { $null=$footer.Range.Fields.Update() } }
 $taskDoc.Repaginate()
 $taskDoc.ExportAsFixedFormat($OutputPdf,17)
 $taskDoc.Close(0)
} finally { try { $taskWord.Quit() } catch { Write-Warning 'Word 已结束，检查实际输出文件。' } }
if (-not (Test-Path -LiteralPath $OutputPdf)) { throw 'Word PDF export missing' }
Write-Output $OutputPdf
