/*
 * Copyright (c) 2005 Massachusetts Institute of Technology
 *
 * Permission is hereby granted, free of charge, to any person
 * obtaining a copy of this software and associated documentation
 * files (the "Software"), to deal in the Software without
 * restriction, including without limitation the rights to use, copy,
 * modify, merge, publish, distribute, sublicense, and/or sell copies
 * of the Software, and to permit persons to whom the Software is
 * furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be
 * included in all copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND,
 * EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF
 * MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND
 * NONINFRINGEMENT. IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT HOLDERS
 * BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER IN AN
 * ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN
 * CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
 * SOFTWARE.
 */

/* $Id$ */

#include<shlwapi.h>
#include<khmapp.h>
#include<netidmgr_intver.h>

#if DEBUG
#include<assert.h>
#endif

HINSTANCE khm_hInstance;
const wchar_t * khm_facility = L"NetIDMgr";
int khm_nCmdShow;
khm_ui_4 khm_commctl_version = 0;

khm_startup_options khm_startup;

khm_version app_version = {KH_VERSION_LIST};

void khm_init_gui(void) {
    khui_init_actions();
    khui_init_rescache();
    khui_init_menu();
    khui_init_toolbar();
    khm_init_notifier();
    khm_init_config();
    khm_init_debug();
}

void khm_exit_gui(void) {
    khm_exit_debug();
    khm_exit_config();
    khm_exit_notifier();
    khui_exit_toolbar();
    khui_exit_menu();
    khui_exit_rescache();
    khui_exit_actions();
}

void khm_parse_commandline(void) {
    LPWSTR wcmdline;
    LPWSTR * wargs;
    int      wargc;
    int i;

    ZeroMemory(&khm_startup, sizeof(khm_startup));

    wcmdline = GetCommandLine();
    wargs = CommandLineToArgvW(wcmdline, &wargc);

    for (i=1; i<wargc; i++) {
        if (!wcscmp(wargs[i], L"-i") ||
            !wcscmp(wargs[i], L"--kinit") ||
            !wcscmp(wargs[i], L"-kinit")) {
            khm_startup.init = TRUE;
            khm_startup.exit = TRUE;
            khm_startup.no_main_window = TRUE;
        }
        else if (!wcscmp(wargs[i], L"-m") ||
                 !wcscmp(wargs[i], L"--import") ||
                 !wcscmp(wargs[i], L"-import")) {
            khm_startup.import = TRUE;
            khm_startup.exit = TRUE;
            khm_startup.no_main_window = TRUE;
        }
        else if (!wcscmp(wargs[i], L"-r") ||
                 !wcscmp(wargs[i], L"--renew") ||
                 !wcscmp(wargs[i], L"-renew")) {
            khm_startup.renew = TRUE;
            khm_startup.exit = TRUE;
            khm_startup.no_main_window = TRUE;
        }
        else if (!wcscmp(wargs[i], L"-d") ||
                 !wcscmp(wargs[i], L"--destroy") ||
                 !wcscmp(wargs[i], L"-destroy")) {
            khm_startup.destroy = TRUE;
            khm_startup.exit = TRUE;
            khm_startup.no_main_window = TRUE;
        }
        else if (!wcscmp(wargs[i], L"-a") ||
                 !wcscmp(wargs[i], L"--autoinit") ||
                 !wcscmp(wargs[i], L"-autoinit")) {
            khm_startup.autoinit = TRUE;
        }
        else {
            wchar_t help[2048];

            LoadString(khm_hInstance, IDS_CMDLINE_HELP,
                       help, ARRAYLENGTH(help));

            MessageBox(NULL, help, L"NetIDMgr", MB_OK);

            khm_startup.error_exit = TRUE;
            break;
        }
    }

    /* special: always enable renew when other options aren't specified */
    if (!khm_startup.exit &&
        !khm_startup.destroy &&
        !khm_startup.init)
        khm_startup.renew = TRUE;
}

void khm_register_window_classes(void) {
    INITCOMMONCONTROLSEX ics;

    ZeroMemory(&ics, sizeof(ics));
    ics.dwSize = sizeof(ics);
    ics.dwICC = 
        ICC_COOL_CLASSES |
        ICC_BAR_CLASSES |
        ICC_DATE_CLASSES |
        ICC_HOTKEY_CLASS |
        ICC_LISTVIEW_CLASSES |
        ICC_TAB_CLASSES |
        ICC_INTERNET_CLASSES |
#if (_WIN32_WINNT >= 0x501)
        ((IS_COMMCTL6())?
         ICC_LINK_CLASS |
         ICC_STANDARD_CLASSES :
         0) |
#endif
        0;

    InitCommonControlsEx(&ics);

    khm_register_main_wnd_class();
    khm_register_credwnd_class();
    khm_register_htwnd_class();
    khm_register_passwnd_class();
    khm_register_newcredwnd_class();
    khm_register_propertywnd_class();
}

void khm_unregister_window_classes(void) {
    khm_unregister_main_wnd_class();
    khm_unregister_credwnd_class();
    khm_unregister_htwnd_class();
    khm_unregister_passwnd_class();
    khm_unregister_newcredwnd_class();
    khm_unregister_propertywnd_class();
}


/* we support up to 16 simutaneous dialogs.  In reality, more than two
   is pretty unlikely.  Property sheets are special and are handled
   separately. */
#define MAX_UI_DIALOGS 16

typedef struct tag_khui_dialog {
    HWND hwnd;
    BOOL active;
} khui_dialog;

static khui_dialog khui_dialogs[MAX_UI_DIALOGS];
static int n_khui_dialogs = 0;
static HWND khui_modal_dialog = NULL;
static BOOL khui_main_window_active;

/* should only be called from the UI thread */
void khm_add_dialog(HWND dlg) {
    if(n_khui_dialogs < MAX_UI_DIALOGS - 1) {
        khui_dialogs[n_khui_dialogs].hwnd = dlg;
        /* we set .active=FALSE for now.  We don't need this to have a
           meaningful value until we enter a modal loop */
        khui_dialogs[n_khui_dialogs].active = FALSE;
        n_khui_dialogs++;
    }
#if DEBUG
    else {
        assert(FALSE);
    }
#endif
}

/* should only be called from the UI thread */
void khm_enter_modal(HWND hwnd) {
    int i;

    for(i=0; i < n_khui_dialogs; i++) {
        if(khui_dialogs[i].hwnd != hwnd) {
            khui_dialogs[i].active = IsWindowEnabled(khui_dialogs[i].hwnd);
            EnableWindow(khui_dialogs[i].hwnd, FALSE);
        }
    }

    khui_main_window_active = khm_is_main_window_active();
    EnableWindow(khm_hwnd_main, FALSE);

    khui_modal_dialog = hwnd;
}

/* should only be called from the UI thread */
void khm_leave_modal(void) {
    int i;

    for(i=0; i < n_khui_dialogs; i++) {
        if(khui_dialogs[i].hwnd != khui_modal_dialog) {
            EnableWindow(khui_dialogs[i].hwnd, khui_dialogs[i].active);
        }
    }

    EnableWindow(khm_hwnd_main, TRUE);
    if (khui_main_window_active)
        SetForegroundWindow(khm_hwnd_main);

    khui_modal_dialog = NULL;
}

/* should only be called from the UI thread */
void khm_del_dialog(HWND dlg) {
    int i;
    for(i=0;i < n_khui_dialogs; i++) {
        if(khui_dialogs[i].hwnd == dlg)
            break;
    }
    
    if(i < n_khui_dialogs)
        n_khui_dialogs--;
    else
        return;

    for(;i < n_khui_dialogs; i++) {
        khui_dialogs[i] = khui_dialogs[i+1];
    }
}

BOOL khm_check_dlg_message(LPMSG pmsg) {
    int i;
    BOOL found = FALSE;
    for(i=0;i<n_khui_dialogs;i++) {
        if(IsDialogMessage(khui_dialogs[i].hwnd, pmsg)) {
            found = TRUE;
            break;
        }
    }

    return found;
}

BOOL khm_is_dialog_active(void) {
    HWND hwnd;
    int i;

    hwnd = GetForegroundWindow();

    for (i=0; i<n_khui_dialogs; i++) {
        if (khui_dialogs[i].hwnd == hwnd)
            return TRUE;
    }

    return FALSE;
}

/* We support at most 256 property sheets simultaneously.  256
   property sheets should be enough for everybody. */
#define MAX_UI_PROPSHEETS 256

khui_property_sheet *_ui_propsheets[MAX_UI_PROPSHEETS];
int _n_ui_propsheets = 0;

void khm_add_property_sheet(khui_property_sheet * s) {
    if(_n_ui_propsheets < MAX_UI_PROPSHEETS)
        _ui_propsheets[_n_ui_propsheets++] = s;
#ifdef DEBUG
    else
        assert(FALSE);
#endif
}

void khm_del_property_sheet(khui_property_sheet * s) {
    int i;

    for(i=0;i < _n_ui_propsheets; i++) {
        if(_ui_propsheets[i] == s)
            break;
    }

    if(i < _n_ui_propsheets)
        _n_ui_propsheets--;
    else
        return;

    for(;i < _n_ui_propsheets; i++) {
        _ui_propsheets[i] = _ui_propsheets[i+1];
    }
}

BOOL khm_check_ps_message(LPMSG pmsg) {
    int i;
    khui_property_sheet * ps;
    for(i=0;i<_n_ui_propsheets;i++) {
        if(khui_ps_check_message(_ui_propsheets[i], pmsg)) {
            if(_ui_propsheets[i]->status == KHUI_PS_STATUS_DONE) {
                ps = _ui_propsheets[i];

                ps->status = KHUI_PS_STATUS_DESTROY;
                kmq_post_message(KMSG_CRED, KMSG_CRED_PP_END, 0, (void *) ps);

                return TRUE;
            }
            return TRUE;
        }
    }

    return FALSE;
}

static HACCEL ha_menu;

WPARAM khm_message_loop_int(khm_boolean * p_exit) {
    int r;
    MSG msg;

    while((r = GetMessage(&msg, NULL, 0,0)) &&
          (p_exit == NULL || *p_exit)) {
        if(r == -1)
            break;
        if(!khm_check_dlg_message(&msg) &&
           !khm_check_ps_message(&msg) &&
           !TranslateAccelerator(khm_hwnd_main, ha_menu, &msg)) {
            TranslateMessage(&msg);
            DispatchMessage(&msg);
        }
    }

    return msg.wParam;
}

WPARAM khm_message_loop(void) {
    WPARAM w;
    ha_menu = khui_create_global_accel_table();
    w = khm_message_loop_int(NULL);
    DestroyAcceleratorTable(ha_menu);
    return w;
}

/* Handles all context closures which have a signalled error state.
   If the context is a top level context, then the errors are
   displayed. */
void KHMAPI
khm_err_ctx_completion_handler(enum kherr_ctx_event evt,
                               kherr_context * c) {
    kherr_event * e;
    khui_alert * a;

    /* we only handle top level contexts here.  For others, we allow
       the child contexts to fold upward silently. */
    if (c->parent || !kherr_is_error_i(c))
        return;

    for(e = kherr_get_first_event(c);
        e;
        e = kherr_get_next_event(e)) {

        if (e->severity != KHERR_ERROR && e->severity != KHERR_WARNING)
            continue;

        kherr_evaluate_event(e);

        /* we only report errors if there is enough information to
           present a message. */
        if (e->short_desc && e->long_desc) {

            khui_alert_create_empty(&a);

            khui_alert_set_severity(a, e->severity);
            khui_alert_set_title(a, e->short_desc);
            khui_alert_set_message(a, e->long_desc);
            if (e->suggestion)
                khui_alert_set_suggestion(a, e->suggestion);

            khui_alert_queue(a);

            khui_alert_release(a);
        }
    }
}

static wchar_t helpfile[MAX_PATH] = L"";

HWND khm_html_help(HWND hwnd, wchar_t * suffix,
                   UINT command, DWORD_PTR data) {

    wchar_t gpath[MAX_PATH + MAX_PATH];

    if (!*helpfile) {
        DWORD dw;
        wchar_t ppath[MAX_PATH];

        dw = GetModuleFileName(NULL, ppath, ARRAYLENGTH(ppath));

        if (dw == 0) {
            StringCbCopy(helpfile, sizeof(helpfile), NIDM_HELPFILE);
        } else {
            PathRemoveFileSpec(ppath);
            PathAppend(ppath, NIDM_HELPFILE);
            StringCbCopy(helpfile, sizeof(helpfile), ppath);
        }
    }

    StringCbCopy(gpath, sizeof(gpath), helpfile);

    if (suffix)
        StringCbCat(gpath, sizeof(gpath), suffix);

    return HtmlHelp(hwnd, gpath, command, data);
}

void khm_load_default_modules(void) {
    kmm_load_default_modules();
}

int WINAPI WinMain(HINSTANCE hInstance,
                   HINSTANCE hPrevInstance,
                   LPSTR lpCmdLine,
                   int nCmdShow) 
{
    int rv = 0;
    HANDLE h_appmutex;
    BOOL slave = FALSE;

    khm_hInstance = hInstance;
    khm_nCmdShow = nCmdShow;

    khm_parse_commandline();

    if (khm_startup.error_exit)
        return 0;

    h_appmutex = CreateMutex(NULL, FALSE, L"Local\\NetIDMgr_GlobalAppMutex");
    if (h_appmutex == NULL)
        return 5;
    if (GetLastError() == ERROR_ALREADY_EXISTS)
        slave = TRUE;

    khc_load_schema(NULL, schema_uiconfig);

 _start_app:

    if(!slave) {

        /* set this so that we don't accidently invoke an API that
           inadvertently puts up the new creds dialog at an
           inopportune moment, like, say, during the new creds dialog
           is open.  This only affects this process, and any child
           processes started by plugins. */
        SetEnvironmentVariable(L"KERBEROSLOGIN_NEVER_PROMPT", L"1");

        khm_version_init();

        khm_commctl_version = khm_get_commctl_version(NULL);

        /* we only open a main window if this is the only instance 
           of the application that is running. */
        kmq_init();
        kmm_init();
        khm_init_gui();

        kmq_set_completion_handler(KMSG_CRED, kmsg_cred_completion);

        kherr_add_ctx_handler(khm_err_ctx_completion_handler,
                              KHERR_CTX_END,
                              0);

        /* load the standard plugins */
        khm_load_default_modules();

        khm_register_window_classes();

        khm_init_request_daemon();

        khm_create_main_window();

        if (!khm_startup.no_main_window)
            khm_show_main_window();

        khm_refresh_config();

        rv = (int) khm_message_loop();

        kmq_set_completion_handler(KMSG_CRED, NULL);

        khm_exit_request_daemon();

        khm_exit_gui();
        khm_unregister_window_classes();
        kmm_exit();
        kmq_exit();

        CloseHandle(h_appmutex);
    } else {
        HWND hwnd = NULL;
        int retries = 5;
        HANDLE hmap;
        wchar_t mapname[256];
        DWORD tid;
        void * xfer;
        khm_query_app_version query_app_version;

        CloseHandle(h_appmutex);

        while (hwnd == NULL && retries) {
            hwnd = FindWindowEx(NULL, NULL, KHUI_MAIN_WINDOW_CLASS, NULL);

            if (hwnd)
                break;

            retries--;
            Sleep(1000);
        }

        if (!hwnd)
            return 2;

        /* first check if the remote instance supports a version
           query */

        StringCbPrintf(mapname, sizeof(mapname),
                       QUERY_APP_VER_MAP_FMT,
                       (tid = GetCurrentThreadId()));

        hmap = CreateFileMapping(INVALID_HANDLE_VALUE,
                                 NULL,
                                 PAGE_READWRITE,
                                 0,
                                 4096,
                                 mapname);

        if (hmap == NULL)
            return 3;

        xfer = MapViewOfFile(hmap, FILE_MAP_WRITE, 0, 0,
                             sizeof(query_app_version));

        ZeroMemory(&query_app_version, sizeof(query_app_version));

        if (xfer) {
            query_app_version.magic = KHM_QUERY_APP_VER_MAGIC;
            query_app_version.code = KHM_ERROR_NOT_IMPLEMENTED;
            query_app_version.ver_caller = app_version;

            query_app_version.request_swap = TRUE;

            memcpy(xfer, &query_app_version, sizeof(query_app_version));

            SendMessage(hwnd, WM_KHUI_QUERY_APP_VERSION,
                        0, (LPARAM) tid);

            memcpy(&query_app_version, xfer, sizeof(query_app_version));

            UnmapViewOfFile(xfer);
            xfer = NULL;
        }

        CloseHandle(hmap);
        hmap = NULL;

        if (query_app_version.code == KHM_ERROR_SUCCESS &&
            query_app_version.request_swap) {
            /* the request for swap was granted.  We can now
               initialize our instance as the master instance. */

            slave = FALSE;
            goto _start_app;
        }

        StringCbPrintf(mapname, sizeof(mapname),
                       COMMANDLINE_MAP_FMT,
                       (tid = GetCurrentThreadId()));

        hmap = CreateFileMapping(INVALID_HANDLE_VALUE,
                                 NULL,
                                 PAGE_READWRITE,
                                 0,
                                 4096,
                                 mapname);

        if (hmap == NULL)
            return 3;

        xfer = MapViewOfFile(hmap,
                             FILE_MAP_WRITE,
                             0, 0,
                             sizeof(khm_startup));

        if (xfer) {
            memcpy(xfer, &khm_startup, sizeof(khm_startup));

            SendMessage(hwnd, WM_KHUI_ASSIGN_COMMANDLINE,
                        0, (LPARAM) tid);
        }

        if (xfer)
            UnmapViewOfFile(xfer);

        if (hmap)
            CloseHandle(hmap);
    }

#if defined(DEBUG) && ( defined(KH_BUILD_PRIVATE) || defined(KH_BUILD_SPECIAL))
    /* writes a report of memory leaks to the specified file.  Should
       only be enabled on development versions. */
    PDUMP("memleak.txt");
#endif

    return rv;
}
