#include <windows.h>
#include <strsafe.h>
#include <msiquery.h>
#include <wcautil.h>
#include <tlhelp32.h>
#include <tchar.h>
#include <msidefs.h>
#include <stdlib.h>
#include <io.h>
#define CA //extern "C" _declspec(dllexport)

struct IDField { DWORD dwVolume, dwIndexHigh, dwIndexLow; };

static bool CalculateFileId(const TCHAR* path, struct IDField* id)
{
	BY_HANDLE_FILE_INFORMATION file_info = { 0 };
	HANDLE file;
	bool ret;

	file = CreateFile(path, 0, 0, NULL, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, NULL);
	if (file == INVALID_HANDLE_VALUE)
		return false;
	ret = GetFileInformationByHandle(file, &file_info);
	CloseHandle(file);
	if (!ret)
		return false;
	id->dwVolume = file_info.dwVolumeSerialNumber;
	id->dwIndexHigh = file_info.nFileIndexHigh;
	id->dwIndexLow = file_info.nFileIndexLow;
	return true;
}

static bool GetBinPath(MSIHANDLE hInstall, WCHAR* path, DWORD* dwSize) {
	WCHAR productCode[MAX_GUID_CHARS + 1];
	DWORD dwPCSize = _countof(productCode);
	UINT ret;
	ret = MsiGetProperty(hInstall, L"ProductCode", productCode, &dwPCSize);
	if (ret != ERROR_SUCCESS) {
		return false;
	}
	MsiGetComponentPath(productCode, L"{5E369341-4051-4BD3-AB1A-ED7D7B21F784}", path, dwSize);
	return true;
}

static bool GetInstallPath(MSIHANDLE hInstall, WCHAR* path, DWORD* dwSize) {
	WCHAR productCode[MAX_GUID_CHARS + 1];
	DWORD dwPCSize = _countof(productCode);
	UINT ret;
	ret = MsiGetProperty(hInstall, L"ProductCode", productCode, &dwPCSize);
	if (ret != ERROR_SUCCESS) {
		return false;
	}
	ret = MsiGetProductInfo(productCode, INSTALLPROPERTY_INSTALLLOCATION, path, dwSize);
	return ret == ERROR_SUCCESS;
}


CA UINT __stdcall KillProcesses(MSIHANDLE hInstall)
{
	HRESULT hr = S_OK;
	UINT er = ERROR_SUCCESS;

	hr = WcaInitialize(hInstall, "KillProcesses");
	ExitOnFailure(hr, "Failed to initialize KillProcesses()");

	WcaLog(LOGMSG_STANDARD, "Initialized KillProcesses().");

	WCHAR binPath[MAX_PATH];
	memset(binPath, 0, sizeof(binPath));
	DWORD dwBinPathSize = _countof(binPath);
	if (!GetBinPath(hInstall, binPath, &dwBinPathSize) || wcslen(binPath) == 0)
		goto LExit;

	HANDLE hSnapshot, hProcess;
	PROCESSENTRY32 entry;
	entry.dwSize = sizeof(PROCESSENTRY32);
	TCHAR processPath[MAX_PATH + 1];
	DWORD dwProcessPathLen = _countof(processPath);
	struct IDField appFileId, fileId;

	if (!CalculateFileId(binPath, &appFileId))
		goto LExit;

	hSnapshot = CreateToolhelp32Snapshot(TH32CS_SNAPPROCESS, 0);
	if (hSnapshot == INVALID_HANDLE_VALUE)
		goto LExit;
	for (bool ret = Process32First(hSnapshot, &entry); ret; ret = Process32Next(hSnapshot, &entry)) {
		if (_tcsicmp(entry.szExeFile, TEXT("svcctl.exe")))
			continue;
		hProcess = OpenProcess(PROCESS_TERMINATE | PROCESS_QUERY_LIMITED_INFORMATION, false, entry.th32ProcessID);
		if (!hProcess)
			continue;

		if (!QueryFullProcessImageName(hProcess, 0, processPath, &dwProcessPathLen))
			goto next;
		if (!CalculateFileId(processPath, &fileId))
			goto next;
		ret = false;
		if (!memcmp(&fileId, &appFileId, sizeof(fileId))) {
			ret = true;
		}
		if (!ret)
			goto next;
		if (TerminateProcess(hProcess, 0)) {
			WaitForSingleObject(hProcess, INFINITE);
		}
	next:
		CloseHandle(hProcess);
	}
	CloseHandle(hSnapshot);

LExit:
	er = SUCCEEDED(hr) ? ERROR_SUCCESS : ERROR_INSTALL_FAILURE;
	return WcaFinalize(er);
}

static UINT InsertServiceControl(MSIHANDLE hView, const TCHAR* szServiceName)
{
	static unsigned int index = 0;
	UINT ret;
	MSIHANDLE record = NULL;
	TCHAR row_identifier[_countof(TEXT("svcctl_service_control_4294967296"))];

	if (_sntprintf_s(row_identifier, _countof(row_identifier), TEXT("svcctl_service_control_%u"), ++index) >= _countof(row_identifier)) {
		ret = ERROR_INSTALL_FAILURE;
		goto LExit;
	}
	record = MsiCreateRecord(5);
	if (!record) {
		ret = ERROR_INSTALL_FAILURE;
		goto LExit;
	}

	MsiRecordSetString(record, 1/*ServiceControl*/, row_identifier);
	MsiRecordSetString(record, 2/*Name          */, szServiceName);
	MsiRecordSetInteger(record, 3/*Event         */, msidbServiceControlEventStop | msidbServiceControlEventUninstallStop | msidbServiceControlEventUninstallDelete);
	MsiRecordSetString(record, 4/*Component_    */, TEXT("MainApplication"));
	MsiRecordSetInteger(record, 5/*Wait          */, 1); /* Waits 30 seconds. */
	WcaLog(LOGMSG_STANDARD, "Uninstalling service: %s", szServiceName);
	ret = MsiViewExecute(hView, record);
	if (ret != ERROR_SUCCESS) {
		WcaLog(LOGMSG_STANDARD, "MsiViewExecute failed(%d).", ret);
		goto LExit;
	}

LExit:
	if (record)
		MsiCloseHandle(record);
	return ret;
}

CA UINT __stdcall EvaluateCtlServices(MSIHANDLE hInstall)
{
	HRESULT hr = S_OK;
	UINT ret = ERROR_INSTALL_FAILURE;
	hr = WcaInitialize(hInstall, "EvaluateCtlServices");
	if (!SUCCEEDED(hr))
		return WcaFinalize(ERROR_INSTALL_FAILURE);

	WCHAR binPath[MAX_PATH];
	DWORD dwBinPathSize = _countof(binPath);
	MSIHANDLE hDb = NULL, hView = NULL;
	SC_HANDLE hScm = NULL;
	SC_HANDLE hService = NULL;
	ENUM_SERVICE_STATUS_PROCESS* serviceStatus = NULL;
	QUERY_SERVICE_CONFIG* serviceConfig = NULL;
	DWORD dwServiceStatusResume = 0;
	enum { SERVICE_STATUS_PROCESS_SIZE = 0x10000, SERVICE_CONFIG_SIZE = 8000 };

	memset(binPath, 0, sizeof(binPath));
	if (!GetBinPath(hInstall, binPath, &dwBinPathSize) || wcslen(binPath) == 0)
		goto LExit;

	hDb = MsiGetActiveDatabase(hInstall);
	if (!hDb) {
		WcaLog(LOGMSG_STANDARD, "MsiGetActiveDatabase failed.");
		goto LExit;
	}
	ret = MsiDatabaseOpenView(hDb,
		TEXT("INSERT INTO `ServiceControl` (`ServiceControl`, `Name`, `Event`, `Component_`, `Wait`) VALUES(?, ?, ?, ?, ?) TEMPORARY"),
		&hView);
	if (ret != ERROR_SUCCESS) {
		WcaLog(LOGMSG_STANDARD, "MsiDatabaseOpenView failed.");
		goto LExit;
	}
	hScm = OpenSCManager(NULL, SERVICES_ACTIVE_DATABASE, SC_MANAGER_CONNECT | SC_MANAGER_ENUMERATE_SERVICE);
	if (!hScm) {
		ret = GetLastError();
		WcaLog(LOGMSG_STANDARD, "MsiDatabaseOpenView failed(%d).", ret);
		goto LExit;
	}

	serviceStatus = (ENUM_SERVICE_STATUS_PROCESS*)LocalAlloc(LMEM_FIXED, SERVICE_STATUS_PROCESS_SIZE);
	serviceConfig = (QUERY_SERVICE_CONFIG*)LocalAlloc(LMEM_FIXED, SERVICE_CONFIG_SIZE);
	if (!serviceStatus || !serviceConfig) {
		ret = GetLastError();
		WcaLog(LOGMSG_STANDARD, "LocalAlloc failed(%d).", ret);
		goto LExit;
	}

	for (bool more_services = true; more_services;) {
		DWORD dwServiceStatusSize = 0, serviceStatusCount = 0;
		DWORD dwBytesNeeded;
		if (EnumServicesStatusEx(hScm, SC_ENUM_PROCESS_INFO, SERVICE_WIN32, SERVICE_STATE_ALL, (LPBYTE)serviceStatus,
			SERVICE_STATUS_PROCESS_SIZE, &dwServiceStatusSize, &serviceStatusCount,
			&dwServiceStatusResume, NULL))
			more_services = false;
		else {
			ret = GetLastError();
			if (ret != ERROR_MORE_DATA) {
				WcaLog(LOGMSG_STANDARD, "EnumServicesStatusEx failed(%d).", ret);
				break;
			}
		}

		for (DWORD i = 0; i < serviceStatusCount; ++i) {
			hService = OpenService(hScm, serviceStatus[i].lpServiceName, SERVICE_QUERY_CONFIG);
			if (hService == NULL) {
				continue;
			}
			if (!QueryServiceConfig(hService, serviceConfig, SERVICE_CONFIG_SIZE, &dwBytesNeeded)) {
				continue;
			}

			if (_tcsnicmp(serviceConfig->lpBinaryPathName, binPath, wcslen(binPath)) && _tcsnicmp(serviceConfig->lpBinaryPathName + 1, binPath, wcslen(binPath)))
				continue;
			InsertServiceControl(hView, serviceStatus[i].lpServiceName);
		}
	}
	ret = ERROR_SUCCESS;

LExit:
	if (serviceStatus) {
		LocalFree(serviceStatus);
	}
	if (serviceConfig) {
		LocalFree(serviceConfig);
	}
	if (hScm)
		CloseServiceHandle(hScm);
	if (hView)
		MsiCloseHandle(hView);
	if (hDb)
		MsiCloseHandle(hDb);
	return WcaFinalize(ret == ERROR_SUCCESS ? ret : ERROR_INSTALL_FAILURE);
}

CA UINT __stdcall StartCtlServices(MSIHANDLE hInstall) {
    UINT ret = ERROR_INSTALL_FAILURE;
    ret = WcaInitialize(hInstall, "StartCtlServices");
    ExitOnFailure(ret, "Failed to initialize StartCtlServices()");
    WcaLog(LOGMSG_STANDARD, "Initialized StartCtlServices().");

	WCHAR binPath[MAX_PATH];
	DWORD dwBinPathSize = _countof(binPath);
	memset(binPath, 0, sizeof(binPath));
	if (!GetBinPath(hInstall, binPath, &dwBinPathSize) || wcslen(binPath) == 0) {
		goto LExit;
	}
	WCHAR token[MAX_GUID_CHARS];
	memset(token, 0, sizeof(token));
	GetEnvironmentVariable(TEXT("SVCCTL_TOKEN"), token, _countof(token));
    WCHAR cmd[MAX_PATH];
    _sntprintf_s(cmd, _countof(cmd), TEXT("\"%s\" -install -token=%s & net start svcctl"), binPath, token);
    _wsystem(cmd);
    ret = ERROR_SUCCESS;
LExit:
    return WcaFinalize(ret == ERROR_SUCCESS ? ret : ERROR_INSTALL_FAILURE);
}

// DllMain - Initialize and cleanup WiX custom action utils.
extern "C" BOOL WINAPI DllMain(
	__in HINSTANCE hInst,
	__in ULONG ulReason,
	__in LPVOID
	)
{
	switch(ulReason)
	{
	case DLL_PROCESS_ATTACH:
		WcaGlobalInitialize(hInst);
		break;

	case DLL_PROCESS_DETACH:
		WcaGlobalFinalize();
		break;
	}

	return TRUE;
}