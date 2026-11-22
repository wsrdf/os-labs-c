; Лабораторная 3: логирование работы lab.exe.
; Запуск: AutoIt3.exe lab3.au3 [путь_к_lab.exe]
; Лог: lab3.log рядом со скриптом (строки: время | событие | описание).
Global Const $EXTRA_KEY = "5"   ; клавиша, которую скрипт отправляет в программу
Global Const $TRIGGER_VK = 0x58 ; клавиша X: при её нажатии отправляется $EXTRA_KEY
Global Const $TITLE = "Lab3Target"

Global $sExe = @ScriptDir & "\lab.exe"
If $CmdLine[0] > 0 Then $sExe = $CmdLine[1]
Global $sLog = @ScriptDir & "\lab3.log"

Global $hLog = FileOpen($sLog, 1 + 8 + 128) ; дозапись, создать каталог, UTF-8
If $hLog = -1 Then Exit MsgBox(16, "lab3", "Cannot open log file: " & $sLog)

Func Stamp()
	Local $t = DllStructCreate("word y;word m;word dow;word d;word h;word min;word s;word ms")
	DllCall("kernel32.dll", "none", "GetLocalTime", "struct*", $t)
	Return StringFormat("%04d-%02d-%02d %02d:%02d:%02d.%03d", $t.y, $t.m, $t.d, $t.h, $t.min, $t.s, $t.ms)
EndFunc

Func WriteLog($sEvent, $sText)
	FileWriteLine($hLog, Stamp() & " | " & $sEvent & " | " & $sText)
	FileFlush($hLog)
EndFunc

Func KeyName($iVk)
	If $iVk >= 0x30 And $iVk <= 0x39 Then Return Chr($iVk)
	If $iVk >= 0x41 And $iVk <= 0x5A Then Return Chr($iVk)
	If $iVk >= 0x60 And $iVk <= 0x69 Then Return "Num" & ($iVk - 0x60)
	If $iVk >= 0x70 And $iVk <= 0x87 Then Return "F" & ($iVk - 0x6F)
	Switch $iVk
		Case 0x08
			Return "Backspace"
		Case 0x09
			Return "Tab"
		Case 0x0D
			Return "Enter"
		Case 0x1B
			Return "Esc"
		Case 0x20
			Return "Space"
		Case 0x25 To 0x28
			Return StringSplit("Left,Up,Right,Down", ",")[$iVk - 0x24]
		Case 0xA0 To 0xA5
			Return StringSplit("LShift,RShift,LCtrl,RCtrl,LAlt,RAlt", ",")[$iVk - 0x9F]
	EndSwitch
	Return "VK_0x" & Hex($iVk, 2)
EndFunc

; --- запуск программы в отдельном окне консоли с известным заголовком ---
If Not FileExists($sExe) Then
	WriteLog("ERROR", "Program not found: " & $sExe)
	Exit MsgBox(16, "lab3", "Program not found: " & $sExe)
EndIf

Global $sExeName = StringRegExpReplace($sExe, "^.*\\", "")
Global $iPid = Run('conhost.exe cmd /c "title ' & $TITLE & ' & "' & $sExe & '""', @ScriptDir)
Global $hWnd = WinWait("[REGEXPTITLE:^" & $TITLE & "]", "", 10)
If Not $hWnd Then
	WriteLog("ERROR", "Program window did not appear")
	Exit MsgBox(16, "lab3", "Program window did not appear")
EndIf

Global $iProg = 0
For $i = 1 To 50
	$iProg = ProcessExists($sExeName)
	If $iProg Then ExitLoop
	Sleep(100)
Next
WriteLog("START", $sExeName & " started, PID=" & $iProg)

; --- низкоуровневый хук клавиатуры: события кладутся в очередь, обрабатываются в основном цикле ---
Global Const $WM_KEYDOWN = 0x100, $WM_SYSKEYDOWN = 0x104
Global $aDown[256]
Global $aQueue[256][3], $iQHead = 0, $iQTail = 0 ; [время, vk, активно ли окно программы]

Func KbdProc($nCode, $wParam, $lParam)
	If $nCode = 0 Then
		Local $tKb = DllStructCreate("dword vk;dword scan;dword flags;dword time;ulong_ptr extra", $lParam)
		Local $vk = $tKb.vk
		If $wParam = $WM_KEYDOWN Or $wParam = $WM_SYSKEYDOWN Then
			If Not $aDown[$vk] Then ; отсекаем автоповтор
				$aDown[$vk] = True
				$aQueue[$iQTail][0] = Stamp()
				$aQueue[$iQTail][1] = $vk
				$aQueue[$iQTail][2] = (WinActive($hWnd) <> 0)
				$iQTail = Mod($iQTail + 1, 256)
			EndIf
		Else
			$aDown[$vk] = False
		EndIf
	EndIf
	Local $aRet = DllCall("user32.dll", "lresult", "CallNextHookEx", "handle", 0, "int", $nCode, "wparam", $wParam, "lparam", $lParam)
	Return $aRet[0]
EndFunc

Global $hCallback = DllCallbackRegister("KbdProc", "lresult", "int;wparam;lparam")
Global $hHook = DllCall("user32.dll", "handle", "SetWindowsHookExW", "int", 13, "ptr", DllCallbackGetPtr($hCallback), _
		"handle", DllCall("kernel32.dll", "handle", "GetModuleHandleW", "ptr", 0)[0], "dword", 0)[0]
OnAutoItExitRegister("Cleanup")

Func Cleanup()
	DllCall("user32.dll", "bool", "UnhookWindowsHookEx", "handle", $hHook)
	DllCallbackFree($hCallback)
EndFunc

Func ProcessQueue()
	While $iQHead <> $iQTail
		Local $sTime = $aQueue[$iQHead][0], $iVk = $aQueue[$iQHead][1], $bActive = $aQueue[$iQHead][2]
		$iQHead = Mod($iQHead + 1, 256)
		If Not $bActive Then ContinueLoop ; интересуют только клавиши, нажатые в окне программы

		FileWriteLine($hLog, $sTime & " | KEY | " & KeyName($iVk) & " pressed")
		If $iVk = $TRIGGER_VK Then
			ControlSend($hWnd, "", "", $EXTRA_KEY)
			WriteLog("SENT", $EXTRA_KEY & " sent to program (trigger: X)")
		EndIf
		FileFlush($hLog)
	WEnd
EndFunc

; --- основной цикл, пока работает программа ---
While ProcessExists($iProg) And WinExists($hWnd)
	ProcessQueue()
	Sleep(10)
WEnd
ProcessQueue() ; клавиши, нажатые прямо перед завершением (например, Esc)

WriteLog("STOP", $sExeName & " finished")
FileClose($hLog)
