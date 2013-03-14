#include "SettingsDlg.h"

#include "aspell.h"
#include "CommonFunctions.h"
#include "MainDef.h"
#include "Plugin.h"
#include "SpellChecker.h"

#include "resource.h"

#include <uxtheme.h>

void SimpleDlg::init (HINSTANCE hInst, HWND Parent, NppData nppData)
{
  NppDataInstance = nppData;
  return Window::init (hInst, Parent);
}

void SimpleDlg::DisableLanguageCombo (BOOL Disable)
{
  EnableWindow (HComboLanguage, !Disable);
}

// Called from main thread, beware!
BOOL SimpleDlg::AddAvalaibleLanguages (const char *CurrentLanguage)
{
  AspellConfig* aspCfg;
  AspellDictInfoList* dlist;
  AspellDictInfoEnumeration* dels;
  const AspellDictInfo* entry;
  ComboBox_ResetContent (HComboLanguage);

  aspCfg = new_aspell_config();

  /* the returned pointer should _not_ need to be deleted */
  dlist = get_aspell_dict_info_list(aspCfg);

  /* config is no longer needed */
  delete_aspell_config(aspCfg);

  dels = aspell_dict_info_list_elements(dlist);

  if (aspell_dict_info_enumeration_at_end(dels) == TRUE)
  {
    delete_aspell_dict_info_enumeration(dels);
    return FALSE;
  }

  UINT uElementCnt= 0;
  int SelectedIndex = 0;
  int i = 0;
  while ((entry = aspell_dict_info_enumeration_next(dels)) != 0)
  {
    // Well since this strings comes in ansi, the simplest way is too call corresponding function
    // Without using windowsx.h
    if (strcmp (CurrentLanguage, entry->name) == 0)
      SelectedIndex = i;

    i++;
    TCHAR *TBuf = 0;
    SetString (TBuf, entry->name);
    ComboBox_AddString (HComboLanguage, TBuf);
    CLEAN_AND_ZERO_ARR (TBuf);
  }
  ComboBox_SetCurSel (HComboLanguage, SelectedIndex);

  delete_aspell_dict_info_enumeration(dels);
  return TRUE;
}

static HWND CreateToolTip(int toolID, HWND hDlg, PTSTR pszText)
{
  if (!toolID || !hDlg || !pszText)
  {
    return FALSE;
  }
  // Get the window of the tool.
  HWND hwndTool = GetDlgItem(hDlg, toolID);

  // Create the tooltip. g_hInst is the global instance handle.
  HWND hwndTip = CreateWindowEx(NULL, TOOLTIPS_CLASS, NULL,
    WS_POPUP |TTS_ALWAYSTIP | TTS_BALLOON,
    CW_USEDEFAULT, CW_USEDEFAULT,
    CW_USEDEFAULT, CW_USEDEFAULT,
    hDlg, NULL,
    (HINSTANCE) getHModule (), NULL);

  if (!hwndTool || !hwndTip)
  {
    return (HWND)NULL;
  }

  // Associate the tooltip with the tool.
  TOOLINFO toolInfo = { 0 };
  toolInfo.cbSize = sizeof(toolInfo);
  toolInfo.hwnd = hDlg;
  toolInfo.uFlags = TTF_IDISHWND | TTF_SUBCLASS;
  toolInfo.uId = (UINT_PTR)hwndTool;
  toolInfo.lpszText = pszText;
  SendMessage(hwndTip, TTM_ADDTOOL, 0, (LPARAM)&toolInfo);

  return hwndTip;
}

// Called from main thread, beware!
void SimpleDlg::ApplySettings (SpellChecker *SpellCheckerInstance)
{
  int length = GetWindowTextLengthA (HComboLanguage);
  char *LangString = new char[length + 1];
  TCHAR *Buf = 0;
  GetWindowTextA (HComboLanguage, LangString, length + 1);
  SpellCheckerInstance->SetLanguage (LangString);
  SpellCheckerInstance->RecheckVisible ();
  Buf = new TCHAR[DEFAULT_BUF_SIZE];
  Edit_GetText (HSuggestionsNum, Buf, DEFAULT_BUF_SIZE);
  SpellCheckerInstance->SetSuggestionsNum (_ttoi (Buf));
  CLEAN_AND_ZERO_ARR (LangString);
  Edit_GetText (HAspellPath, Buf, DEFAULT_BUF_SIZE);
  SpellCheckerInstance->SetAspellPath (Buf);
  SpellCheckerInstance->SetCheckThose (Button_GetCheck (HCheckOnlyThose) == BST_CHECKED ? 1 : 0);
  Edit_GetText (HFileTypes, Buf, DEFAULT_BUF_SIZE);
  SpellCheckerInstance->SetFileTypes (Buf);
  CLEAN_AND_ZERO_ARR (Buf);
  SpellCheckerInstance->SetCheckComments (Button_GetCheck (HCheckComments) == BST_CHECKED);
  SendEvent (EID_FILL_DIALOGS);
}

void SimpleDlg::FillAspellInfo (BOOL Status, TCHAR *AspellPath)
{
  if (Status)
  {
    AspellStatusColor = RGB (0, 144, 0);
    Static_SetText (HAspellStatus, _T ("Status: Aspell is OK"));
  }
  else
  {
    AspellStatusColor = RGB (225, 0, 0);
    Static_SetText (HAspellStatus, _T ("Status: Aspell is missing"));
  }
  TCHAR *Path = 0;
  GetActualAspellPath (Path, AspellPath);
  Edit_SetText (HAspellPath, Path);
  CLEAN_AND_ZERO_ARR (Path);
}

void SimpleDlg::FillSugestionsNum (int SuggestionsNum)
{
  TCHAR Buf[10];
  _itot_s (SuggestionsNum, Buf, 10);
  Edit_SetText (HSuggestionsNum, Buf);
}

void SimpleDlg::SetFileTypes (BOOL CheckThose, const TCHAR *FileTypes)
{
  if (!CheckThose)
  {
    Button_SetCheck (HCheckNotThose, BST_CHECKED);
    Button_SetCheck (HCheckOnlyThose, BST_UNCHECKED);
    Edit_SetText (HFileTypes, FileTypes);
  }
  else
  {
    Button_SetCheck (HCheckOnlyThose, BST_CHECKED);
    Button_SetCheck (HCheckNotThose, BST_UNCHECKED);
    Edit_SetText (HFileTypes, FileTypes);
  }
}

void SimpleDlg::SetCheckComments (BOOL Value)
{
  Button_SetCheck (HCheckComments, Value ? BST_CHECKED : BST_UNCHECKED);
}

BOOL CALLBACK SimpleDlg::run_dlgProc (UINT message, WPARAM wParam, LPARAM lParam)
{
  char *LangString = NULL;
  int length = 0;
  TCHAR Buf[DEFAULT_BUF_SIZE];
  int x;
  TCHAR *EndPtr;
  HBRUSH DefaultBrush = 0;

  switch (message)
  {
  case WM_INITDIALOG:
    {
      // Retrieving handles of dialog controls
      HComboLanguage = ::GetDlgItem(_hSelf, IDC_COMBO_LANGUAGE);
      HSuggestionsNum = ::GetDlgItem(_hSelf, IDC_SUGGESTIONS_NUM);
      HAspellStatus = ::GetDlgItem (_hSelf, IDC_ASPELL_STATUS);
      HAspellPath = ::GetDlgItem (_hSelf, IDC_ASPELLPATH);
      HCheckNotThose = ::GetDlgItem (_hSelf, IDC_FILETYPES_CHECKNOTTHOSE);
      HCheckOnlyThose = ::GetDlgItem (_hSelf, IDC_FILETYPES_CHECKTHOSE);
      HFileTypes = ::GetDlgItem (_hSelf, IDC_FILETYPES);
      HCheckComments = ::GetDlgItem (_hSelf, IDC_CHECKCOMMENTS);
      DefaultBrush = CreateSolidBrush(GetSysColor(COLOR_BTNFACE));
      return TRUE;
    }
  case WM_CLOSE:
    {
      EndDialog(_hSelf, 0);
      DeleteObject (DefaultBrush);
      return TRUE;
    }
  case WM_COMMAND:
    {
      switch (LOWORD (wParam))
      {
      case IDC_SUGGESTIONS_NUM:
        {
          if (HIWORD (wParam) == EN_CHANGE)
          {
            Edit_GetText (HSuggestionsNum, Buf, DEFAULT_BUF_SIZE);
            if (!*Buf)
              return TRUE;

            x = _tcstol (Buf, &EndPtr, 10);
            if (*EndPtr)
              Edit_SetText (HSuggestionsNum, _T ("5"));
            else if (x > 20)
              Edit_SetText (HSuggestionsNum, _T ("20"));
            else if (x < 1)
              Edit_SetText (HSuggestionsNum, _T ("1"));

            return TRUE;
          }
        }
        break;
      case IDC_RESETASPELLPATH:
        {
          if (HIWORD (wParam) == BN_CLICKED)
          {
            TCHAR *Path = 0;
            GetDefaultAspellPath (Path);
            Edit_SetText (HAspellPath, Path);
            CLEAN_AND_ZERO_ARR (Path);
            return TRUE;
          }
        }
        break;
      case IDC_BROWSEASPELLPATH:
        OPENFILENAME ofn;
        ZeroMemory(&ofn, sizeof(ofn));
        ofn.lStructSize = sizeof(ofn);
        ofn.hwndOwner = _hSelf;
        TCHAR *Buf = new TCHAR [DEFAULT_BUF_SIZE];
        Edit_GetText (HAspellPath, Buf, DEFAULT_BUF_SIZE);
        ofn.lpstrFile = Buf;
        // Set lpstrFile[0] to '\0' so that GetOpenFileName does not
        // use the contents of szFile to initialize itself.
        ofn.lpstrFile[0] = '\0';
        ofn.nMaxFile = DEFAULT_BUF_SIZE;
        ofn.lpstrFilter = _T ("Aspell Library (*.dll)\0*.dll\0");
        ofn.nFilterIndex = 1;
        ofn.lpstrFileTitle = NULL;
        ofn.nMaxFileTitle = 0;
        ofn.lpstrInitialDir = NULL;
        ofn.Flags = OFN_PATHMUSTEXIST | OFN_FILEMUSTEXIST;
        if (GetOpenFileName(&ofn)==TRUE)
          Edit_SetText (HAspellPath, ofn.lpstrFile);

        break;
      }
    }
  case WM_CTLCOLORSTATIC:
    if(GetDlgItem(_hSelf, IDC_ASPELL_STATUS) == (HWND)lParam)
    {
      HDC hDC = (HDC)wParam;
      SetBkColor(hDC, GetSysColor(COLOR_BTNFACE));
      SetTextColor(hDC, AspellStatusColor);
      SetBkMode(hDC, TRANSPARENT);
      return (INT_PTR) DefaultBrush;
    }
    break;
  }
  return FALSE;
}

void AdvancedDlg::FillDelimiters (const char *Delimiters)
{
  TCHAR *TBuf = 0;
  SetStringSUtf8 (TBuf, Delimiters);
  Edit_SetText (HEditDelimiters, TBuf);
  CLEAN_AND_ZERO_ARR (TBuf);
}

void AdvancedDlg::setIgnoreYo (BOOL Value)
{
  Button_SetCheck (HIgnoreYo, Value ? BST_CHECKED : BST_UNCHECKED);
}

BOOL CALLBACK AdvancedDlg::run_dlgProc(UINT message, WPARAM wParam, LPARAM lParam)
{
  TCHAR *EndPtr = 0;
  TCHAR Buf[DEFAULT_BUF_SIZE];
  int x;
  switch (message)
  {
  case WM_INITDIALOG:
    {
      // Retrieving handles of dialog controls
      HEditDelimiters = ::GetDlgItem(_hSelf, IDC_DELIMETERS);
      HDefaultDelimiters = ::GetDlgItem (_hSelf, IDC_DEFAULT_DELIMITERS);
      HIgnoreYo = ::GetDlgItem (_hSelf, IDC_IGNOREYO);
      HRecheckDelay = ::GetDlgItem (_hSelf, IDC_RECHECK_DELAY);

      CreateToolTip (IDC_DELIMETERS, _hSelf, _T ("Standard white-space symbols such as New Line ('\\n'), Carriage Return ('\\r'), Tab ('\\t'), Space (' ') are always counted as delimiters"));
      return TRUE;
    }
  case WM_CLOSE:
    {
      EndDialog(_hSelf, 0);
      return TRUE;
    }
  case WM_COMMAND:
    switch (LOWORD (wParam))
    {
    case IDC_DEFAULT_DELIMITERS:
      if (HIWORD (wParam) == BN_CLICKED)
        SendEvent (EID_DEFAULT_DELIMITERS);
      return TRUE;
    case IDC_RECHECK_DELAY:
      if (HIWORD (wParam) == EN_CHANGE)
      {
        Edit_GetText (HRecheckDelay, Buf, DEFAULT_BUF_SIZE);
        if (!*Buf)
          return TRUE;

        x = _tcstol (Buf, &EndPtr, 10);
        if (*EndPtr)
          Edit_SetText (HRecheckDelay, _T ("0"));
        else if (x > 30000)
          Edit_SetText (HRecheckDelay, _T ("30000"));
        else if (x < 0)
          Edit_SetText (HRecheckDelay, _T ("0"));

        return TRUE;
      }
    }
  }
  return FALSE;
}

void AdvancedDlg::SetDelimetersEdit (TCHAR *Delimiters)
{
  Edit_SetText (HEditDelimiters, Delimiters);
}

void AdvancedDlg::SetRecheckDelay (int Delay)
{
  TCHAR Buf[DEFAULT_BUF_SIZE];
  TCHAR *EndPtr;
  _itot (Delay, Buf, 10);
  Edit_SetText (HRecheckDelay, Buf);
}

int AdvancedDlg::GetRecheckDelay ()
{
  TCHAR Buf[DEFAULT_BUF_SIZE];
  Edit_GetText (HRecheckDelay, Buf, DEFAULT_BUF_SIZE);
  TCHAR *EndPtr;
  int x = _tcstol (Buf, &EndPtr, 10);
  return x;
}

// Called from main thread, beware!
void AdvancedDlg::ApplySettings (SpellChecker *SpellCheckerInstance)
{
  TCHAR *TBuf = 0;
  int Length = Edit_GetTextLength (HEditDelimiters);
  TBuf = new TCHAR[Length + 1];
  Edit_GetText (HEditDelimiters, TBuf, Length + 1);
  char *BufUtf8 = 0;
  SetStringDUtf8 (BufUtf8, TBuf);
  SpellCheckerInstance->SetDelimiters (BufUtf8);
  SpellCheckerInstance->SetIgnoreYo (Button_GetCheck (HIgnoreYo) == BST_CHECKED ? TRUE : FALSE);
  CLEAN_AND_ZERO_ARR (BufUtf8);
}

SimpleDlg *SettingsDlg::GetSimpleDlg ()
{
  return &SimpleDlgInstance;
}

AdvancedDlg *SettingsDlg::GetAdvancedDlg ()
{
  return &AdvancedDlgInstance;
}

void SettingsDlg::init (HINSTANCE hInst, HWND Parent, NppData nppData)
{
  NppDataInstance = nppData;
  return Window::init (hInst, Parent);
}

void SettingsDlg::destroy()
{
  SimpleDlgInstance.destroy();
  AdvancedDlgInstance.destroy();
};

// Send appropriate event and set some npp thread properties
void SettingsDlg::ApplySettings ()
{
  SendEvent (EID_APPLY_SETTINGS);
  SetRecheckDelay (AdvancedDlgInstance.GetRecheckDelay ());
}

BOOL CALLBACK SettingsDlg::run_dlgProc(UINT Message, WPARAM wParam, LPARAM lParam)
{
  switch (Message)
  {
  case WM_INITDIALOG :
    {
      ControlsTabInstance.init (_hInst, _hSelf, false, true, false);
      ControlsTabInstance.setFont(TEXT("Tahoma"), 13);

      SimpleDlgInstance.init(_hInst, _hSelf, NppDataInstance);
      SimpleDlgInstance.create (IDD_SIMPLE, false, false);
      SimpleDlgInstance.display ();
      AdvancedDlgInstance.init(_hInst, _hSelf);
      AdvancedDlgInstance.create (IDD_ADVANCED, false, false);
      AdvancedDlgInstance.SetRecheckDelay (GetRecheckDelay ());
      SendEvent (EID_FILL_DIALOGS);

      WindowVectorInstance.push_back(DlgInfo(&SimpleDlgInstance, TEXT("Simple"), TEXT("Simple Options")));
      WindowVectorInstance.push_back(DlgInfo(&AdvancedDlgInstance, TEXT("Advanced"), TEXT("Advanced Options")));
      ControlsTabInstance.createTabs(WindowVectorInstance);
      ControlsTabInstance.display();
      RECT rc;
      getClientRect(rc);
      ControlsTabInstance.reSizeTo(rc);
      rc.bottom -= 30;

      SimpleDlgInstance.reSizeTo(rc);
      AdvancedDlgInstance.reSizeTo(rc);

      // This stuff is copied from npp source to make tabbed window looked totally nice and white
      ETDTProc enableDlgTheme = (ETDTProc)::SendMessage(_hParent, NPPM_GETENABLETHEMETEXTUREFUNC, 0, 0);
      if (enableDlgTheme)
        enableDlgTheme(_hSelf, ETDT_ENABLETAB);

      return TRUE;
    }

  case WM_NOTIFY :
    {
      NMHDR *nmhdr = (NMHDR *)lParam;
      if (nmhdr->code == TCN_SELCHANGE)
      {
        if (nmhdr->hwndFrom == ControlsTabInstance.getHSelf())
        {
          ControlsTabInstance.clickedUpdate();
          return TRUE;
        }
      }
      break;
    }

  case WM_COMMAND :
    {
      switch (wParam)
      {
      case IDAPPLY:
        ApplySettings ();
        return TRUE;
      case IDOK:
        ApplySettings ();
      case IDCANCEL:
        SendEvent (EID_HIDE_DIALOG);
        return TRUE;

      default :
        ::SendMessage(_hParent, WM_COMMAND, wParam, lParam);
        return TRUE;
      }
    }
  }
  return FALSE;
}

UINT SettingsDlg::DoDialog (void)
{
  if (!isCreated())
  {
    create (IDD_SETTINGS);
    goToCenter ();
  }
  display ();
  return TRUE;
  // return (UINT)::DialogBoxParam(_hInst, MAKEINTRESOURCE(IDD_SETTINGS), _hParent, (DLGPROC)dlgProc, (LPARAM)this);
}
