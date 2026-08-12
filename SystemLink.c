// Compile with cl.exe SystemLink.c  for the x64 architecture.

#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <userenv.h>
#include <wtsapi32.h>
#include <stdio.h>

#pragma comment(lib, "userenv.lib")
#pragma comment(lib, "wtsapi32.lib")

static void enable_privilege(HANDLE token, const wchar_t* name)
{
  TOKEN_PRIVILEGES privileges;
  ZeroMemory(&privileges, sizeof(privileges));
  privileges.PrivilegeCount = 1;
  if (!LookupPrivilegeValueW(NULL, name, &privileges.Privileges[0].Luid))
        return;
      privileges.Privileges[0].Attributes = SE_PRIVILEGE_ENABLED;
      AdjustTokenPrivileges(token, FALSE, &privileges, sizeof(privileges),
      NULL, NULL);
}

static DWORD active_session(void)
{
  PWTS_SESSION_INFOW sessions = NULL;
  DWORD count = 0;
  DWORD session_id = WTSGetActiveConsoleSessionId();
  DWORD i;

  if (WTSEnumerateSessionsW(WTS_CURRENT_SERVER_HANDLE, 0, 1,
      &sessions, &count)) {
      for (i = 0; i < count; ++i) {
           if (sessions[i].State == WTSActive) {
               session_id = sessions[i].SessionId;
               break;
           }
      }
        WTSFreeMemory(sessions);
  }
    return session_id;
}

int wmain(int argc, wchar_t** argv)
{
  if (argc > 1 && wcscmp(argv[1], L"run") == 0) {
      HANDLE process_token = NULL;
      HANDLE primary_token = NULL;
      LPVOID environment = NULL;
      DWORD session_id = active_session();
      DWORD error = ERROR_SUCCESS;
      DWORD flags = CREATE_NEW_CONSOLE;
      STARTUPINFOW startup;
      PROCESS_INFORMATION process;
      wchar_t command[] = L"C:\\Windows\\System32\\cmd.exe /k whoami";

    ZeroMemory(&startup, sizeof(startup));
    ZeroMemory(&process, sizeof(process));
    startup.cb = sizeof(startup);
    wchar_t desktop[] = L"winsta0\\default";
    startup.lpDesktop = desktop;

    if (session_id == 0xFFFFFFFF) {
        error = ERROR_NO_SUCH_LOGON_SESSION;
        goto failed;
    }
    if (!OpenProcessToken(GetCurrentProcess(), TOKEN_QUERY | TOKEN_DUPLICATE |
        TOKEN_ASSIGN_PRIMARY | TOKEN_ADJUST_PRIVILEGES,
        &process_token)) {
        error = GetLastError();
        goto failed;
    }

    enable_privilege(process_token, L"SeAssignPrimaryTokenPrivilege");
    enable_privilege(process_token, L"SeIncreaseQuotaPrivilege");
    enable_privilege(process_token, L"SeTcbPrivilege");

    if (!DuplicateTokenEx(process_token, MAXIMUM_ALLOWED, NULL,
        SecurityImpersonation, TokenPrimary,
        &primary_token)) {
        error = GetLastError();
        goto failed;
    }
    if (!SetTokenInformation(primary_token, TokenSessionId, &session_id,
        sizeof(session_id))) {
        error = GetLastError();
        goto failed;
    }
    if (CreateEnvironmentBlock(&environment, primary_token, FALSE))
    flags |= CREATE_UNICODE_ENVIRONMENT;

    if (!CreateProcessAsUserW(primary_token,
        NULL, command,
        NULL, NULL, FALSE, flags, environment,
        L"C:\\Windows\\System32", &startup,
        &process)) {
        error = GetLastError();
        goto failed;
    }

    CloseHandle(process.hThread);
    CloseHandle(process.hProcess);
    if (environment) DestroyEnvironmentBlock(environment);
    CloseHandle(primary_token);
    CloseHandle(process_token);

        
        system("schtasks /delete /tn \"TempSystemRunner\" /f >nul 2>&1");

        return 0;

    failed:
     if (environment) DestroyEnvironmentBlock(environment);
     if (primary_token) CloseHandle(primary_token);
     if (process_token) CloseHandle(process_token);

   
    //system("schtasks /delete /tn \"TempSystemRunner\" /f >nul 2>&1");

        return (int)error;
    }
  else
  {
    wchar_t exe_path[MAX_PATH];
    GetModuleFileNameW(NULL, exe_path, MAX_PATH);

    wchar_t schtasks_cmd[512];
    swprintf_s(schtasks_cmd, 512,
        L"schtasks /create /tn \"TempSystemRunner\" /tr \"\\\"%s\\\" run\" /sc ONCE /st 00:00 /ru \"NT AUTHORITY\\SYSTEM\" /f >nul 2>&1", exe_path);

    _wsystem(schtasks_cmd);
    wchar_t launcher_cmd[512];
    swprintf_s(launcher_cmd, 512,
        L"cmd.exe /c schtasks /run /tn \"TempSystemRunner\" >nul 2>&1");

    STARTUPINFOW si;
    PROCESS_INFORMATION pi;
    ZeroMemory(&si, sizeof(si));
    si.cb = sizeof(si);
    si.dwFlags = STARTF_USESHOWWINDOW;
    si.wShowWindow = SW_HIDE;
    ZeroMemory(&pi, sizeof(pi));

    CreateProcessW(NULL, launcher_cmd, NULL, NULL, FALSE, 0, NULL, NULL, &si, &pi);

    CloseHandle(pi.hThread);
    CloseHandle(pi.hProcess);

        return 0;
  }
}
