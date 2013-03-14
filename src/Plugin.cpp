//this file is part of notepad++
//Copyright (C)2003 Don HO <donho@altern.org>
//
//This program is free software; you can redistribute it and/or
//modify it under the terms of the GNU General Public License
//as published by the Free Software Foundation; either
//version 2 of the License, or (at your option) any later version.
//
//This program is distributed in the hope that it will be useful,
//but WITHOUT ANY WARRANTY; without even the implied warranty of
//MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
//GNU General Public License for more details.
//
//You should have received a copy of the GNU General Public License
//along with this program; if not, write to the Free Software
//Foundation, Inc., 675 Mass Ave, Cambridge, MA 02139, USA.

#include "Plugin.h"
#include "menuCmdID.h"

//
// put the headers you need here
//

#include "aspell.h"
#include "CommonFunctions.h"
#include "MainDef.h"
#include "SpellChecker.h"
#include "Suggestions.h"

const TCHAR configFileName[] = _T ("DSpellCheck.ini");

#ifdef UNICODE
#define generic_itoa _itow
#else
#define generic_itoa itoa
#endif

FuncItem funcItem[nbFunc];
BOOL ResourcesInited = FALSE;

FuncItem *get_funcItem ()
{
  // Well maybe we should lock mutex here
  return funcItem;
}

//
// The data of Notepad++ that you can use in your plugin commands
//
NppData nppData;
TCHAR IniFilePath[MAX_PATH];
bool doCloseTag = false;
BOOL AutoCheckText = false;
SpellChecker *SpellCheckerInstance;
SettingsDlg *SettingsDlgInstance;
Suggestions *SuggestionsInstance;
HANDLE hThread = NULL;
HANDLE hEvent[EID_MAX]  = {NULL};
HANDLE hModule = NULL;
HHOOK HMouseHook = NULL;

//
// Initialize your plug-in data here
// It will be called while plug-in loading
LRESULT CALLBACK MouseProc (_In_  int nCode,
                            _In_  WPARAM wParam,
                            _In_  LPARAM lParam)
{
  switch (wParam)
  {
  case WM_MOUSEMOVE:
    SendEvent (EID_INIT_SUGGESTIONS_BOX);
    break;
  }
  return CallNextHookEx(HMouseHook, nCode, wParam, lParam);;
}

void pluginInit(HANDLE hModuleArg)
{
  hModule = hModuleArg;
  // Init it all dialog classes:
}

HANDLE getHModule ()
{
  return hModule;
}

DWORD WINAPI ThreadMain (LPVOID lpParam)
{
  DWORD dwWaitResult    = EID_MAX;
  SpellChecker *SpellCheckerInstance = (SpellChecker*)lpParam;

  BOOL bRun = SpellCheckerInstance->NotifyEvent(EID_MAX);

  MSG Msg;

  // Creating thread message queue
  PeekMessage (&Msg, NULL, WM_USER, WM_USER, PM_NOREMOVE);

  while (bRun)
  {
    dwWaitResult = WaitForMultipleObjects(EID_MAX, hEvent, FALSE, INFINITE);
    if (dwWaitResult == (unsigned int) - 1)
    {
      SpellCheckerInstance->ErrorMsgBox (_T ("Thread has died"));
      break;
    }

    bRun = SpellCheckerInstance->NotifyEvent(dwWaitResult);
  }

  SendEvent (EID_THREADKILLED);
  return 0;
}

void CreateThreadResources ()
{
  DWORD dwThreadId = 0;

  /* create events */
  for (int i = 0; i < EID_MAX; i++)
    hEvent[i] = ::CreateEvent(NULL, FALSE, FALSE, NULL);

  /* create thread */
  hThread = CreateThread (NULL, 0, ThreadMain, SpellCheckerInstance, 0, &dwThreadId);
  SetThreadPriority (hThread, THREAD_PRIORITY_BELOW_NORMAL);

  ResourcesInited = TRUE;
}

void CreateHooks ()
{
  HMouseHook = SetWindowsHookEx (WH_MOUSE, MouseProc, 0, GetCurrentThreadId ());
}

void KillThreadResources ()
{
  SendEvent (EID_KILLTHREAD);

  WaitForEvent (EID_THREADKILLED);

  CloseHandle(hThread);

  /* kill events */
  for (int i = 0; i < EID_MAX; i++)
    CloseHandle(hEvent[i]);
}

//
// Here you can do the clean up, save the parameters (if any) for the next session
//
void pluginCleanUp ()
{
  UnhookWindowsHookEx(HMouseHook);
  KillThreadResources ();
  CLEAN_AND_ZERO (SpellCheckerInstance);
  CLEAN_AND_ZERO (SettingsDlgInstance);
  CLEAN_AND_ZERO (funcItem[1]._pShKey);
}

void inline SendEvent (EventId Event)
{
  if (ResourcesInited)
    SetEvent(hEvent[Event]);
}

void PostMessageToMainThread (UINT Msg, WPARAM WParam, LPARAM LParam)
{
  if (ResourcesInited)
  {
    PostThreadMessage (GetThreadId (hThread), Msg, WParam, LParam);
  }
}

void WaitForEvent (EventId Event, DWORD WaitTime)
{
  if (ResourcesInited)
    WaitForSingleObject (hEvent[Event], WaitTime);
}

void SwitchAutoCheckText ()
{
  SendEvent (EID_SWITCH_AUTOCHECK);
}

void GetSuggestions ()
{
  //SendEvent (EID_INITSUGGESTIONS);
}

void StartSettings ()
{
  SettingsDlgInstance->DoDialog ();
}

void LoadSettings ()
{
  SendEvent (EID_LOAD_SETTINGS);
}

void RecheckVisible ()
{
  SendEvent (EID_RECHECK_VISIBLE);
}

void FindNextMistake ()
{
  SendEvent (EID_FIND_NEXT_MISTAKE);
}

void FindPrevMistake ()
{
  SendEvent (EID_FIND_PREV_MISTAKE);
}

//
// Initialization of your plug-in commands
// You should fill your plug-ins commands here
void commandMenuInit()
{
  //
  // Firstly we get the parameters from your plugin config file (if any)
  //

  // get path of plugin configuration
  ::SendMessage(nppData._nppHandle, NPPM_GETPLUGINSCONFIGDIR, MAX_PATH, (LPARAM) IniFilePath);

  // if config path doesn't exist, we create it
  if (PathFileExists(IniFilePath) == FALSE)
  {
    ::CreateDirectory(IniFilePath, NULL);
  }

  // make your plugin config file full file path name
  PathAppend (IniFilePath, configFileName);

  TCHAR Buf[DEFAULT_BUF_SIZE];
  TCHAR *EndPtr;
  int x;
  GetPrivateProfileString (_T ("SpellCheck"), _T ("Recheck_Delay"), _T ("500"), Buf, DEFAULT_BUF_SIZE, IniFilePath);
  x = _tcstol (Buf, &EndPtr, 10);
  if (*EndPtr)
    SetRecheckDelay (500);
  else
    SetRecheckDelay (x);

  //--------------------------------------------//
  //-- STEP 3. CUSTOMIZE YOUR PLUGIN COMMANDS --//
  //--------------------------------------------//
  // with function :
  // setCommand(int index,                      // zero based number to indicate the order of command
  //            TCHAR *commandName,             // the command name that you want to see in plugin menu
  //            PFUNCPLUGINCMD functionPointer, // the symbol of function (function pointer) associated with this command. The body should be defined below. See Step 4.
  //            ShortcutKey *shortcut,          // optional. Define a shortcut to trigger this command
  //            bool check0nInit                // optional. Make this menu item be checked visually
  //            );
  setCommand(0, TEXT("Auto-check document"), SwitchAutoCheckText, NULL, false);
  ShortcutKey *shKey = new ShortcutKey;
  shKey->_isAlt = true;
  shKey->_isCtrl = false;
  shKey->_isShift = false;
  shKey->_key = 0x41 + 'n' - 'a';
  setCommand(1, TEXT("Find Next Misspelling"), FindNextMistake, shKey, false);
  shKey = new ShortcutKey;
  shKey->_isAlt = true;
  shKey->_isCtrl = false;
  shKey->_isShift = false;
  shKey->_key = 0x41 + 'b' - 'a';
  setCommand(2, TEXT("Find Previous Misspelling"), FindPrevMistake, shKey, false);
  setCommand(3, TEXT("---"), NULL, NULL, false);

  setCommand(4, TEXT("Settings..."), StartSettings, NULL, false);
}

void InitClasses ()
{
  SuggestionsInstance = new Suggestions;
  SuggestionsInstance->init ((HINSTANCE) hModule, nppData._nppHandle);
  SuggestionsInstance->DoDialog ();

  SettingsDlgInstance = new SettingsDlg;
  SettingsDlgInstance->init((HINSTANCE) hModule, nppData._nppHandle, nppData);
  SpellCheckerInstance = new SpellChecker (IniFilePath, SettingsDlgInstance, &nppData, SuggestionsInstance);
}

void commandMenuCleanUp()
{
  CLEAN_AND_ZERO (funcItem[2]._pShKey);
  // We should deallocate shortcuts here
}

BOOL GetAutoCheckText ()
{
  return AutoCheckText;
}

//
// Function that initializes plug-in commands
//
bool setCommand(size_t index, TCHAR *cmdName, PFUNCPLUGINCMD pFunc, ShortcutKey *sk, bool check0nInit)
{
  if (index >= nbFunc)
    return false;

  if (!pFunc)
    return false;

  lstrcpy(funcItem[index]._itemName, cmdName);
  funcItem[index]._pFunc = pFunc;
  funcItem[index]._init2Check = check0nInit;
  funcItem[index]._pShKey = sk;

  return true;
}