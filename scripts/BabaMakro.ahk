; ============================================================
;  Baba Makro - Amaran'in baba makrosu
;  AutoHotkey v1.1+ Uyumlu Makro Uygulamasi
;  Ozellikler:
;    - Interval Mode: Secili tuslari belirli araliklarla basar
;    - Stick Mode: Secili tuslari fiziksel olarak basili tutar
;    - Koyu tema arayuz
;    - Gercek zamanli istatistikler
;    - Oyun uyumlulugu (SendInput + Admin)
; ============================================================

; === Yonetici olarak calistir (oyunlar icin gerekli) ===
if not A_IsAdmin
{
    Run *RunAs "%A_ScriptFullPath%"
    ExitApp
}

#NoEnv
#SingleInstance Force
#Persistent
#InstallKeybdHook
#InstallMouseHook
#UseHook

SetWorkingDir %A_ScriptDir%
SetBatchLines -1
SetKeyDelay, -1, -1
SetMouseDelay, -1
SendMode Input
CoordMode, Mouse, Screen
Process, Priority, , High

; ===== Global Degiskenler =====
global IsRunning := false
global IsStickMode := false
global TimesPressed := 0
global LastPressText := "---"
global StartTime := 0
global ElapsedSeconds := 0
global NextPressTime := 0

; Tus durumlari
global chkE := 0, chkQ := 0, chkW := 0, chkA := 0, chkS := 0, chkD := 0
global chkF := 0, chkG := 0, chkSpace := 0, chkLMB := 0, chkRMB := 0
global chk1 := 0, chk2 := 0, chk3 := 0

; Key release zamanlayici
global KeysCurrentlyDown := false
global ReleaseTime := 0

; Stick mode tus izleme
global StickKeysDown := {}

; ===== Renk Sabitleri =====
cBg        := "1a1a2e"
cAccent    := "4488FF"
cTextWhite := "FFFFFF"
cTextGray  := "888888"
cGreen     := "44CC66"
cRed       := "FF4444"

; ===== GUI Olusturma =====
Gui, Main:New, +AlwaysOnTop +HWNDhMainWnd
Gui, Main:Color, %cBg%
Gui, Main:Margin, 0, 0

; === Baslik ===
Gui, Main:Font, s16 c%cTextWhite% Bold, Segoe UI
Gui, Main:Add, Text, x0 y15 w480 Center, Baba Makro
Gui, Main:Font, s9 c%cTextGray% Normal
Gui, Main:Add, Text, x0 y42 w480 Center, Amaran'in baba makrosu

; === Keys Bolumu ===
Gui, Main:Font, s10 c%cTextGray% Bold
Gui, Main:Add, Text, x25 y75, Keys:

; Satir 1: E Q W A S D
Gui, Main:Font, s10 c%cTextWhite% Normal
xStart := 25
yRow1 := 100
spacing := 75

Gui, Main:Add, Checkbox, x%xStart% y%yRow1% vchkE gOnKeyToggle, E
x2 := xStart + spacing
Gui, Main:Add, Checkbox, x%x2% y%yRow1% vchkQ gOnKeyToggle, Q
x3 := xStart + spacing * 2
Gui, Main:Add, Checkbox, x%x3% y%yRow1% vchkW gOnKeyToggle, W
x4 := xStart + spacing * 3
Gui, Main:Add, Checkbox, x%x4% y%yRow1% vchkA gOnKeyToggle, A
x5 := xStart + spacing * 4
Gui, Main:Add, Checkbox, x%x5% y%yRow1% vchkS gOnKeyToggle, S
x6 := xStart + spacing * 5
Gui, Main:Add, Checkbox, x%x6% y%yRow1% vchkD gOnKeyToggle, D

; Satir 2: F G Space LMB RMB 1
yRow2 := 125
Gui, Main:Add, Checkbox, x%xStart% y%yRow2% vchkF gOnKeyToggle, F
Gui, Main:Add, Checkbox, x%x2% y%yRow2% vchkG gOnKeyToggle, G
Gui, Main:Add, Checkbox, x%x3% y%yRow2% vchkSpace gOnKeyToggle, Space
Gui, Main:Add, Checkbox, x%x4% y%yRow2% vchkLMB gOnKeyToggle, LMB
Gui, Main:Add, Checkbox, x%x5% y%yRow2% vchkRMB gOnKeyToggle, RMB
Gui, Main:Add, Checkbox, x%x6% y%yRow2% vchk1 gOnKeyToggle, 1

; Satir 3: 2 3
yRow3 := 150
Gui, Main:Add, Checkbox, x%xStart% y%yRow3% vchk2 gOnKeyToggle, 2
Gui, Main:Add, Checkbox, x%x2% y%yRow3% vchk3 gOnKeyToggle, 3

; === Ayirici Cizgi ===
Gui, Main:Add, Text, x20 y178 w440 h1 0x10

; === Interval Mode Bolumu ===
Gui, Main:Font, s12 c%cAccent% Bold
Gui, Main:Add, Text, x25 y190, [+] Interval Mode

Gui, Main:Font, s10 c%cTextWhite% Normal
Gui, Main:Add, Text, x25 y222, Interval (sec):
Gui, Main:Add, Edit, x135 y219 w70 h24 vIntervalSec Center, 10
Gui, Main:Font, s9 c%cTextGray%
Gui, Main:Add, Text, x215 y222, e.g. 10`, 2.5

Gui, Main:Font, s10 c%cTextWhite% Normal
Gui, Main:Add, Text, x25 y252, Hold (sec):
Gui, Main:Add, Edit, x135 y249 w70 h24 vHoldSec Center, 1
Gui, Main:Font, s9 c%cTextGray%
Gui, Main:Add, Text, x215 y252, e.g. 1`, 0.5

; Start / Stop Butonlari
Gui, Main:Font, s10 c%cTextWhite% Bold
Gui, Main:Add, Button, x25 y290 w100 h32 gStartMacro vBtnStart, [>] Start
Gui, Main:Add, Button, x135 y290 w100 h32 gStopMacro vBtnStop, [=] Stop

; === Ayirici Cizgi ===
Gui, Main:Add, Text, x20 y335 w440 h1 0x10

; === Stick Mode Bolumu ===
Gui, Main:Font, s12 c%cAccent% Bold
Gui, Main:Add, Text, x25 y348, [+] Stick Mode
Gui, Main:Font, s9 c%cTextGray% Normal
Gui, Main:Add, Text, x25 y370, Check to hold selected keys down. Uncheck to release.

Gui, Main:Font, s10 c%cTextWhite% Normal
Gui, Main:Add, Checkbox, x25 y395 vIsStickMode gToggleStickMode, Stick Mode  --  hold keys physically down

; === Zamanlayici Gostergesi ===
Gui, Main:Font, s28 c%cAccent% Bold, Consolas
Gui, Main:Add, Text, x0 y430 w480 Center vTimerDisplay, --:--

; === Istatistikler ===
Gui, Main:Font, s10 c%cTextGray% Normal, Segoe UI
Gui, Main:Add, Text, x60 y478 w180 vStatPressed, Times pressed: 0
Gui, Main:Add, Text, x260 y478 w200 vStatLastPress, Last press: ---

; === Durum Gostergesi ===
Gui, Main:Font, s10 c%cRed% Bold
Gui, Main:Add, Text, x0 y505 w480 Center vStatusDisplay, [X] Idle

; === Pencereyi Goster ===
Gui, Main:Show, w480 h535, Key Macro

; === Hotkey: F6 ile Start/Stop ===
Hotkey, F6, ToggleRunning
; === Hotkey: F7 ile Acil Durdurma ===
Hotkey, F7, EmergencyStop

return

; ============================================================
;                    OLAY ISLEYICILERI
; ============================================================

OnKeyToggle:
    Gui, Main:Submit, NoHide
    if (IsStickMode)
        UpdateStickKeys()
return

; ============================================================
;                    INTERVAL MODE
; ============================================================

StartMacro:
    if (IsRunning)
        return

    Gui, Main:Submit, NoHide

    ; En az bir tus secili mi kontrol et
    if (!HasSelectedKeys()) {
        MsgBox, 48, Uyari, Lutfen en az bir tus secin!
        return
    }

    ; Interval degerlerini dogrula
    if IntervalSec is not number
    {
        MsgBox, 48, Uyari, Gecerli bir interval (saniye) degeri girin!
        return
    }
    if HoldSec is not number
    {
        MsgBox, 48, Uyari, Gecerli bir hold (saniye) degeri girin!
        return
    }
    if (IntervalSec <= 0 || HoldSec <= 0) {
        MsgBox, 48, Uyari, Interval ve Hold degerleri 0'dan buyuk olmalidir!
        return
    }

    IsRunning := true
    TimesPressed := 0
    LastPressText := "---"
    StartTime := A_TickCount
    ElapsedSeconds := 0
    KeysCurrentlyDown := false

    ; GUI guncelle
    GuiControl, Main:, StatPressed, Times pressed: 0
    GuiControl, Main:, StatLastPress, Last press: ---

    ; Durum guncelle
    Gui, Main:Font, s10 c%cGreen% Bold
    GuiControl, Main:Font, StatusDisplay
    GuiControl, Main:, StatusDisplay, [>] Running

    ; Zamanlayicilari baslat
    SetTimer, UpdateTimer, 1000
    SetTimer, MacroTick, 50

    ; Ilk basisi hemen yap
    NextPressTime := A_TickCount
return

StopMacro:
    if (!IsRunning)
        return

    IsRunning := false

    ; Zamanlayicilari durdur
    SetTimer, MacroTick, Off
    SetTimer, UpdateTimer, Off

    ; Basili tuslari birak
    if (KeysCurrentlyDown) {
        ReleaseSelectedKeys()
        KeysCurrentlyDown := false
    }

    ; Durum guncelle
    Gui, Main:Font, s10 c%cRed% Bold
    GuiControl, Main:Font, StatusDisplay
    GuiControl, Main:, StatusDisplay, [X] Idle

    ; Zamanlayici sifirla
    GuiControl, Main:, TimerDisplay, --:--
return

; === Ana Makro Dongusu (Non-blocking) ===
MacroTick:
    if (!IsRunning)
        return

    currentTime := A_TickCount

    ; Tuslar basili ve birakma zamani geldiyse birak
    if (KeysCurrentlyDown && currentTime >= ReleaseTime) {
        ReleaseSelectedKeys()
        KeysCurrentlyDown := false
    }

    ; Yeni basma zamani geldiyse bas
    if (!KeysCurrentlyDown && currentTime >= NextPressTime) {
        Gui, Main:Submit, NoHide

        ; Tuslari bas
        PressSelectedKeys()
        KeysCurrentlyDown := true
        ReleaseTime := currentTime + (HoldSec * 1000)

        ; Sonraki basma zamanini ayarla
        NextPressTime := currentTime + (IntervalSec * 1000)

        ; Istatistikleri guncelle
        TimesPressed++
        FormatTime, nowTime, , HH:mm:ss
        LastPressText := nowTime

        GuiControl, Main:, StatPressed, % "Times pressed: " . TimesPressed
        GuiControl, Main:, StatLastPress, % "Last press: " . LastPressText
    }
return

UpdateTimer:
    if (!IsRunning)
        return

    elapsed := (A_TickCount - StartTime) // 1000
    mins := Floor(elapsed / 60)
    secs := Mod(elapsed, 60)
    timeStr := Format("{:02d}:{:02d}", mins, secs)
    GuiControl, Main:, TimerDisplay, %timeStr%
return

; ============================================================
;                    STICK MODE
; ============================================================

ToggleStickMode:
    Gui, Main:Submit, NoHide
    if (IsStickMode) {
        UpdateStickKeys()
    } else {
        ReleaseAllStickKeys()
    }
return

UpdateStickKeys() {
    global
    Gui, Main:Submit, NoHide

    ReleaseAllStickKeys()

    if (!IsStickMode)
        return

    if (chkE) {
        SendInput, {e down}
        StickKeysDown["e"] := true
    }
    if (chkQ) {
        SendInput, {q down}
        StickKeysDown["q"] := true
    }
    if (chkW) {
        SendInput, {w down}
        StickKeysDown["w"] := true
    }
    if (chkA) {
        SendInput, {a down}
        StickKeysDown["a"] := true
    }
    if (chkS) {
        SendInput, {s down}
        StickKeysDown["s"] := true
    }
    if (chkD) {
        SendInput, {d down}
        StickKeysDown["d"] := true
    }
    if (chkF) {
        SendInput, {f down}
        StickKeysDown["f"] := true
    }
    if (chkG) {
        SendInput, {g down}
        StickKeysDown["g"] := true
    }
    if (chkSpace) {
        SendInput, {Space down}
        StickKeysDown["Space"] := true
    }
    if (chkLMB) {
        SendInput, {LButton down}
        StickKeysDown["LMB"] := true
    }
    if (chkRMB) {
        SendInput, {RButton down}
        StickKeysDown["RMB"] := true
    }
    if (chk1) {
        SendInput, {1 down}
        StickKeysDown["1"] := true
    }
    if (chk2) {
        SendInput, {2 down}
        StickKeysDown["2"] := true
    }
    if (chk3) {
        SendInput, {3 down}
        StickKeysDown["3"] := true
    }
}

ReleaseAllStickKeys() {
    global StickKeysDown

    for key, val in StickKeysDown {
        if (key = "LMB") {
            SendInput, {LButton up}
        } else if (key = "RMB") {
            SendInput, {RButton up}
        } else if (key = "Space") {
            SendInput, {Space up}
        } else {
            SendInput, {%key% up}
        }
    }
    StickKeysDown := {}
}

; ============================================================
;                    YARDIMCI FONKSIYONLAR
; ============================================================

HasSelectedKeys() {
    global
    Gui, Main:Submit, NoHide
    return (chkE || chkQ || chkW || chkA || chkS || chkD
         || chkF || chkG || chkSpace || chkLMB || chkRMB
         || chk1 || chk2 || chk3)
}

PressSelectedKeys() {
    global
    Gui, Main:Submit, NoHide

    if (chkE)
        SendInput, {e down}
    if (chkQ)
        SendInput, {q down}
    if (chkW)
        SendInput, {w down}
    if (chkA)
        SendInput, {a down}
    if (chkS)
        SendInput, {s down}
    if (chkD)
        SendInput, {d down}
    if (chkF)
        SendInput, {f down}
    if (chkG)
        SendInput, {g down}
    if (chkSpace)
        SendInput, {Space down}
    if (chkLMB)
        SendInput, {LButton down}
    if (chkRMB)
        SendInput, {RButton down}
    if (chk1)
        SendInput, {1 down}
    if (chk2)
        SendInput, {2 down}
    if (chk3)
        SendInput, {3 down}
}

ReleaseSelectedKeys() {
    global
    Gui, Main:Submit, NoHide

    if (chkE)
        SendInput, {e up}
    if (chkQ)
        SendInput, {q up}
    if (chkW)
        SendInput, {w up}
    if (chkA)
        SendInput, {a up}
    if (chkS)
        SendInput, {s up}
    if (chkD)
        SendInput, {d up}
    if (chkF)
        SendInput, {f up}
    if (chkG)
        SendInput, {g up}
    if (chkSpace)
        SendInput, {Space up}
    if (chkLMB)
        SendInput, {LButton up}
    if (chkRMB)
        SendInput, {RButton up}
    if (chk1)
        SendInput, {1 up}
    if (chk2)
        SendInput, {2 up}
    if (chk3)
        SendInput, {3 up}
}

; ============================================================
;                    HOTKEY ISLEYICILERI
; ============================================================

ToggleRunning:
    if (IsRunning)
        GoSub, StopMacro
    else
        GoSub, StartMacro
return

EmergencyStop:
    ; Acil durdurma - her seyi birak
    IsRunning := false
    SetTimer, MacroTick, Off
    SetTimer, UpdateTimer, Off

    ; Tum tuslari birak
    if (KeysCurrentlyDown) {
        ReleaseSelectedKeys()
        KeysCurrentlyDown := false
    }
    ReleaseAllStickKeys()

    ; Stick mode kapat
    IsStickMode := false
    GuiControl, Main:, IsStickMode, 0

    ; Durum guncelle
    Gui, Main:Font, s10 c%cRed% Bold
    GuiControl, Main:Font, StatusDisplay
    GuiControl, Main:, StatusDisplay, [X] Idle
    GuiControl, Main:, TimerDisplay, --:--

    ToolTip, ACIL DURDURMA! Tum tuslar birakildi., , , 1
    SetTimer, RemoveToolTip, -2000
return

RemoveToolTip:
    ToolTip, , , , 1
return

; ============================================================
;                    PENCERE OLAYLARI
; ============================================================

MainGuiClose:
MainGuiEscape:
    ; Stick mode aktifse tuslari birak
    if (IsStickMode)
        ReleaseAllStickKeys()

    ; Makro calisiyorsa durdur
    if (IsRunning) {
        IsRunning := false
        SetTimer, MacroTick, Off
        SetTimer, UpdateTimer, Off
        if (KeysCurrentlyDown) {
            ReleaseSelectedKeys()
            KeysCurrentlyDown := false
        }
    }
    ExitApp
return
