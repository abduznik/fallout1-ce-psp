param(
    [string]$PPSSPP = "F:\emulators\ppsspp\PPSSPPWindows64.exe",
    [string]$GameDir = "F:\emulators\ppsspp\memstick\PSP\GAME\FOUT00001",
    [string]$DebugFile = "F:\emulators\ppsspp\memstick\psp_debug.txt"
)

Add-Type @"
    using System;
    using System.Runtime.InteropServices;
    public class Win32 {
        [DllImport("user32.dll")] public static extern bool SetForegroundWindow(IntPtr hWnd);
        [DllImport("user32.dll")] public static extern IntPtr GetForegroundWindow();
        [DllImport("user32.dll")] public static extern void mouse_event(uint dwFlags, uint dx, uint dy, uint dwData, int dwExtraInfo);
        [DllImport("user32.dll")] public static extern bool SetCursorPos(int X, int Y);
    }
"@
Add-Type -AssemblyName System.Windows.Forms

Remove-Item $DebugFile -Force -ErrorAction SilentlyContinue

Write-Host "=== Launching PPSSPP (with game DIR, not EBOOT) ==="
Write-Host "Command: $PPSSPP $GameDir"
$proc = Start-Process -FilePath $PPSSPP -ArgumentList "$GameDir" -WindowStyle Normal -PassThru

# Step 1: Wait for PPSSPP menu to appear
Write-Host "Waiting 8s for PPSSPP menu..."
Start-Sleep -Seconds 8

# Step 2: Activate window
$psp = Get-Process | Where-Object { $_.ProcessName -eq "PPSSPPWindows64" } | Select-Object -First 1
if ($psp) {
    Write-Host "Activating window..."
    [Win32]::SetForegroundWindow($psp.MainWindowHandle) | Out-Null
    Start-Sleep -Seconds 1
}

# Step 3: Navigate menu and launch game (same as smoke test)
Write-Host "Selecting game with Enter..."
[System.Windows.Forms.SendKeys]::SendWait("{ENTER}")
Start-Sleep -Seconds 3

# Step 4: Wait for game to boot and show dialog  
Write-Host "Waiting 5s for game boot..."
Start-Sleep -Seconds 5

# Step 5: Dismiss system dialog with Cross (X)
Write-Host "Dismissing dialog with X..."
[System.Windows.Forms.SendKeys]::SendWait("x")
Start-Sleep -Seconds 1
[System.Windows.Forms.SendKeys]::SendWait("x")
Start-Sleep -Seconds 1
[System.Windows.Forms.SendKeys]::SendWait("{ENTER}")
Start-Sleep -Seconds 1
[System.Windows.Forms.SendKeys]::SendWait("c")

# Step 6: Wait for game init + svga_init + render
Write-Host "Waiting 15s for game init..."
Start-Sleep -Seconds 15

# Step 7: Check debug file
Write-Host "`n=== psp_debug.txt ==="
if (Test-Path $DebugFile) {
    $content = Get-Content $DebugFile -Raw
    Write-Host $content
    Write-Host "Size: $((Get-Item $DebugFile).Length) bytes"
    if ($content.Length -gt 32) { Write-Host ">>> NEW DATA CAPTURED <<<" }
} else { Write-Host "NOT FOUND" }

# Kill
Write-Host "Killing PPSSPP..."
$proc.Kill() 2>$null
Stop-Process -Name "PPSSPPWindows64" -Force 2>$null
Write-Host "=== DONE ==="
