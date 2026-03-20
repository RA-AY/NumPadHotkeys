// NumPad Hotkeys — resource and control IDs
// Copyright (c) 2026 rainyApps.com — MIT License
#pragma once

// Icon resource IDs
#define IDI_APPICON     101
#define IDI_DISABLED    102

// Menu IDs
#define IDM_TOGGLE_HOOK     201
#define IDM_CONFIGURE       202
#define IDM_PROFILE_BASE    300   // 300..399 reserved for profile submenu items
#define IDM_PROFILE_MAX     399
#define IDM_AUTOSTART       400
#define IDM_EXIT            401

// Dialog IDs
#define IDD_CONFIG          501
#define IDD_NEW_PROFILE     502
#define IDD_RECORD_KEY      503

// Control IDs (config dialog)
#define IDC_PROFILE_COMBO   601
#define IDC_NUMPAD_VIZ      602
#define IDC_LABEL_EDIT      603
#define IDC_ACTION_KEYSTROKE 604
#define IDC_ACTION_TEXT     605
#define IDC_ACTION_APP      606
#define IDC_ACTION_MEDIA    607
#define IDC_ACTION_DISABLED 608
#define IDC_SHORTCUT_EDIT   609
#define IDC_RECORD_BTN      610
#define IDC_SAVE_BTN        611
#define IDC_CANCEL_BTN      612
#define IDC_RESET_BTN       613
#define IDC_SELECTED_KEY_LABEL 614
#define IDC_BROWSE_BTN      615
#define IDC_NEW_PROFILE_BTN 616
#define IDC_DEL_PROFILE_BTN 617

// Control IDs (new profile dialog)
#define IDC_PROFILE_NAME_EDIT 701

// WM_APP messages
#define WM_APP_TRAY         (WM_APP + 0)   // tray icon callback
#define WM_APP_SHOW_CONFIG  (WM_APP + 1)   // second instance → show config
#define WM_APP_HOOK_KEY     (WM_APP + 2)   // hook → main thread key event
