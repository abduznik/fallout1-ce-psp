# Fallout CE PSP Smoke Test Script
# Launches PPSSPP, takes screenshots, sends keystrokes

param(
    [string]$PPSSPP = "F:\emulators\ppsspp\PPSSPPWindows64.exe",
    [string]$GameDir = "F:\emulators\ppsspp\memstick\PSP\GAME\FOUT00001",
    [string]$OutputDir = "L:\projects\fallout1-ce-psp\smoke-test-screenshots",
    [string]$LogFile = "F:\emulators\ppsspp\ppsspplog.txt"
)

# Load WinForms for SendKeys and Bitmap
Add-Type -AssemblyName System.Windows.Forms
Add-Type -AssemblyName System.Drawing

# Ensure output dir exists
New-Item -ItemType Directory -Force -Path $OutputDir | Out-Null

$sn = 0
function Take-Screenshot {
    param([string]$Label)
    $script:sn++
    # Capture the entire screen
    $bounds = [System.Windows.Forms.Screen]::PrimaryScreen.Bounds
    $bitmap = New-Object System.Drawing.Bitmap $bounds.Width, $bounds.Height
    $graphics = [System.Drawing.Graphics]::FromImage($bitmap)
    $graphics.CopyFromScreen($bounds.X, $bounds.Y, 0, 0, $bounds.Size)
    $graphics.Dispose()
    
    $path = Join-Path $OutputDir "$($script:sn)-$Label.png"
    $bitmap.Save($path, [System.Drawing.Imaging.ImageFormat]::Png)
    $bitmap.Dispose()
    Write-Host "Screenshot saved: $path"
}

function Send-Key {
    param([string]$Key)
    Write-Host "Sending key: $Key"
    [System.Windows.Forms.SendKeys]::SendWait($Key)
    Start-Sleep -Milliseconds 300
}

# Clean previous log
Remove-Item $LogFile -Force -ErrorAction SilentlyContinue

Write-Host "=== Launching PPSSPP with Fallout CE ==="
$proc = Start-Process -FilePath $PPSSPP -ArgumentList "$GameDir" -PassThru
Start-Sleep -Seconds 3

# The game needs time to boot
Write-Host "Waiting for boot (8 seconds)..."
Start-Sleep -Seconds 8
Take-Screenshot "01-boot-screen"

# Wait a bit more for main menu
Write-Host "Waiting for main menu (additional 5 seconds)..."
Start-Sleep -Seconds 5
Take-Screenshot "02-main-menu-idle"

# Wait longer to see if animations/transitions happen
Write-Host "Waiting idle (additional 10 seconds)..."
Start-Sleep -Seconds 10
Take-Screenshot "03-idle-10s-later"

# === STEP 2: Cursor Test ===
Write-Host "`n=== CURSOR TEST ==="

# Try using arrow keys to move cursor (Fallout uses keyboard by default)
# Note: PSP controls map via SDL_GameController - in PPSSPP, keyboard maps to PSP buttons
# The Fallout cursor should respond to D-pad or analog stick
# In PPSSPP, arrow keys = D-pad, WASD = analog stick by default
# Let's try both

# First, check if cursor responds - press up several times
Write-Host "Testing UP direction (press 5 times)..."
1..5 | ForEach-Object { Send-Key "{UP}"; Start-Sleep -Milliseconds 200 }
Take-Screenshot "04-cursor-up"

Write-Host "Testing DOWN direction (press 5 times)..."
1..5 | ForEach-Object { Send-Key "{DOWN}"; Start-Sleep -Milliseconds 200 }
Take-Screenshot "05-cursor-down-back"

Write-Host "Testing LEFT direction (press 5 times)..."
1..5 | ForEach-Object { Send-Key "{LEFT}"; Start-Sleep -Milliseconds 200 }
Take-Screenshot "06-cursor-left"

Write-Host "Testing RIGHT direction (press 5 times)..."
1..5 | ForEach-Object { Send-Key "{RIGHT}"; Start-Sleep -Milliseconds 200 }
Take-Screenshot "07-cursor-right-back"

# === STEP 3: Button Tests ===
Write-Host "`n=== BUTTON TEST ==="

Write-Host "Pressing ENTER (default confirm)..."
Send-Key "{ENTER}"
Take-Screenshot "08-enter-press"

Start-Sleep -Seconds 2
Take-Screenshot "09-after-enter"

# Try ESC
Write-Host "Pressing ESC..."
Send-Key "{ESC}"
Take-Screenshot "10-esc-press"

Start-Sleep -Seconds 2
Take-Screenshot "11-after-esc"

# Try Space
Write-Host "Pressing SPACE..."
Send-Key " "
Take-Screenshot "12-space-press"
Start-Sleep -Seconds 2
Take-Screenshot "13-after-space"

# Tab
Write-Host "Pressing TAB..."
Send-Key "{TAB}"
Take-Screenshot "14-tab-press"
Start-Sleep -Seconds 1
Take-Screenshot "15-after-tab"

# Try letter keys in case menus respond to keyboard input
Write-Host "Pressing 'N' (New Game)..."
Send-Key "n"
Take-Screenshot "16-n-key"
Start-Sleep -Seconds 3
Take-Screenshot "17-after-n"

# Try 'L' (Load Game)
Write-Host "Pressing 'L'..."
Send-Key "l"
Take-Screenshot "18-l-key"
Start-Sleep -Seconds 2
Take-Screenshot "19-after-l"

# Try to navigate to New Game using arrows and enter
Write-Host "Trying Enter again..."
Send-Key "{ENTER}"
Start-Sleep -Seconds 3
Take-Screenshot "20-enter-try-again"

# Number keys (Fallout 1 uses F1-F10 and number keys for quick save/load)
Write-Host "Pressing F1 (help)..."
Send-Key "{F1}"
Take-Screenshot "21-f1-key"
Start-Sleep -Seconds 2
Take-Screenshot "22-after-f1"

# === STEP 4: Try to start a game ===
Write-Host "`n=== NEW GAME ATTEMPT ==="
Write-Host "Trying multiple approaches to reach new game..."

# Try mouse click (PPSSPP might forward mouse clicks)
Write-Host "Clicking at center of screen..."
$cursorPos = [System.Windows.Forms.Cursor]::Position
Write-Host "Current cursor: X=$($cursorPos.X) Y=$($cursorPos.Y)"

# Get PPSSPP window info
$pspWindow = Get-Process | Where-Object { $_.ProcessName -eq "PPSSPPWindows64" } | Select-Object -First 1
if ($pspWindow) {
    Write-Host "PPSSPP window found: MainWindowHandle=$($pspWindow.MainWindowHandle)"
    $windowBounds = $pspWindow.MainWindowRectangle
    Write-Host "Window bounds: Left=$($windowBounds.Left) Top=$($windowBounds.Top) Width=$($windowBounds.Width) Height=$($windowBounds.Height)"
    
    # Activate the window
    $pspWindow.Activate() | Out-Null
    Start-Sleep -Seconds 1
    
    # Click in the center of the PPSSPP window (where the game renders)
    $centerX = $windowBounds.Left + ($windowBounds.Width / 2)
    $centerY = $windowBounds.Top + ($windowBounds.Height / 2)
    
    # Use SendKeys to simulate mouse click doesn't work well
    # Instead, let's use [System.Windows.Forms.Cursor]::Position + mouse_event
    
    Write-Host "Moving mouse to game center: X=$centerX Y=$centerY"
    [System.Windows.Forms.Cursor]::Position = New-Object System.Drawing.Point($centerX, $centerY)
    Start-Sleep -Milliseconds 500
    
    # Click (mouse_event: 0x2 = down, 0x4 = up, 0x8002 = absolute)
    Add-Type @"
        using System;
        using System.Runtime.InteropServices;
        public class MouseOp {
            [DllImport("user32.dll")] public static extern void mouse_event(uint dwFlags, uint dx, uint dy, uint dwData, int dwExtraInfo);
            [DllImport("user32.dll")] public static extern bool SetCursorPos(int X, int Y);
        }
"@
    [MouseOp]::mouse_event(0x0002, 0, 0, 0, 0) # left down
    Start-Sleep -Milliseconds 100
    [MouseOp]::mouse_event(0x0004, 0, 0, 0, 0) # left up
    
    Take-Screenshot "23-mouse-click-center"
    Start-Sleep -Seconds 3
    Take-Screenshot "24-after-mouse-click"
}

# Try clicking the "NEW GAME" area (typically in a menu quadrant)
Write-Host "`nTrying to click various menu areas..."

# NEW GAME is usually in the middle-left area
$clickAreas = @(
    @{X=0.25; Y=0.30; Label="new-game-area-top"},      # Top options
    @{X=0.25; Y=0.45; Label="new-game-area-mid"},       # Mid options
    @{X=0.25; Y=0.55; Label="new-game-area-lower-mid"}, # Lower options
    @{X=0.50; Y=0.50; Label="center-try2"},             # Dead center
    @{X=0.30; Y=0.35; Label="new-game-area-2"},         # Another guess
    @{X=0.25; Y=0.60; Label="lower-section"}            # Lower section
)

$pspWindow = Get-Process | Where-Object { $_.ProcessName -eq "PPSSPPWindows64" } | Select-Object -First 1
if ($pspWindow) {
    $pspWindow.Activate() | Out-Null
    Start-Sleep -Milliseconds 500
    
    $windowBounds = $pspWindow.MainWindowRectangle
    $winWidth = $windowBounds.Width
    $winHeight = $windowBounds.Height
    
    foreach ($area in $clickAreas) {
        $clickX = $windowBounds.Left + [int]($winWidth * $area.X)
        $clickY = $windowBounds.Top + [int]($winHeight * $area.Y)
        
        Write-Host "Clicking at $($area.Label): X=$clickX Y=$clickY"
        [MouseOp]::SetCursorPos($clickX, $clickY)
        Start-Sleep -Milliseconds 200
        [MouseOp]::mouse_event(0x0002, 0, 0, 0, 0)
        Start-Sleep -Milliseconds 100
        [MouseOp]::mouse_event(0x0004, 0, 0, 0, 0)
        Start-Sleep -Milliseconds 500
        
        # Take screenshot after each click
        $sc = $script:sn + 1
        Take-Screenshot "25-click-$($area.Label)"
        Start-Sleep -Seconds 2
    }
}

# Final idle screenshot
Start-Sleep -Seconds 5
Take-Screenshot "26-final-idle"

# Close PPSSPP
Write-Host "`n=== Closing PPSSPP ==="
$proc.Kill() 2>$null
Stop-Process -Name "PPSSPPWindows64" -Force 2>$null
Start-Sleep -Seconds 2

Write-Host "`n=== Smoke test complete ==="
Write-Host "Screenshots saved to: $OutputDir"
