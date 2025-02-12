#include <Windows.h>

namespace windows
{
	namespace chrome
	{
		bool IsChromeInstalledViaRegistry()
		{
			HKEY hKey;
			LONG result = RegOpenKeyEx(HKEY_LOCAL_MACHINE, TEXT("SOFTWARE\\Google\\Chrome\\BLBeacon"), 0, KEY_READ, &hKey);

			if (result == ERROR_SUCCESS) {
				RegCloseKey(hKey);
				return true;
			}

			result = RegOpenKeyEx(HKEY_CURRENT_USER, TEXT("SOFTWARE\\Google\\Chrome\\BLBeacon"), 0, KEY_READ, &hKey);

			if (result == ERROR_SUCCESS) {
				RegCloseKey(hKey);
				return true;
			}

			return false;
		}

		std::string GetChromePath(bool pathOnly)
		{
			HKEY hKey;
			char path[MAX_PATH] = { 0 };
			DWORD pathSize = sizeof(path);
			DWORD valueType;
			std::string res;

			// 打开注册表键
			if (RegOpenKeyEx(HKEY_LOCAL_MACHINE, TEXT("SOFTWARE\\Microsoft\\Windows\\CurrentVersion\\App Paths\\chrome.exe"), 0, KEY_READ, &hKey) == ERROR_SUCCESS) {
				// 查询默认值
				if (RegQueryValueExA(hKey, pathOnly ? "Path" : "", NULL, &valueType, (LPBYTE)path, &pathSize) == ERROR_SUCCESS) {
					if (valueType == REG_SZ) {
						res.assign(path, pathSize);
						RegCloseKey(hKey);
						return res;
					}
				}
				RegCloseKey(hKey);
			}
			if (RegOpenKeyEx(HKEY_CURRENT_USER, TEXT("SOFTWARE\\Microsoft\\Windows\\CurrentVersion\\App Paths\\chrome.exe"), 0, KEY_READ, &hKey) == ERROR_SUCCESS) {
				// 查询默认值
				if (RegQueryValueExA(hKey, pathOnly ? "Path" : "", NULL, &valueType, (LPBYTE)path, &pathSize) == ERROR_SUCCESS) {
					if (valueType == REG_SZ) {
						res.assign(path, pathSize);
						RegCloseKey(hKey);
						return res;
					}
				}
				RegCloseKey(hKey);
			}
			return res; // 如果没有找到路径，返回空字符串
		}

#if QT_VERSION && QT_VERSION >= QT_VERSION_CHECK(5, 10, 0)
		QString OpenInChromeByPath(const QString& url, const QStringList& args)
		{
			zInfo("ComFunction", "OpenInChromeByPath") << "url:" << url;
			QProcess process;
			QString output;
			do {
				auto chromePath = QString::fromStdString(GetChromePath());
				if (chromePath.endsWith("\0")) {
					chromePath.remove(chromePath.length() - 1, 1);
				}
				auto newArgs = QStringList() << args << url;
				QString command = "\"" + chromePath + "\" ";
				auto res = process.startDetached(command, newArgs);
				output = res ? "success" : "failed";
				zInfo("ComFunction", "OpenInChromeByPath") << "output:" << output;
			} while (false);
			return output;
		}

		QString OpenInChrome(const QString& url, const QStringList& args)
		{
			if (OpenInChromeByPath(url, args) == "success") {
				return "success";
			}

			zInfo("ComFunction", "OpenInChrome") << "url:" << url;
			QProcess process;
			QString output;
			do {
				auto newArgs = QStringList() << "/c"
											 << "start chrome" << args;
				auto strArgs = newArgs.join(" ") + " \"" + url + "\"";
				process.setNativeArguments(strArgs);
				process.start("cmd.exe");
				if (!process.waitForStarted()) {
					output = "Failed to start process: " + QString::fromLocal8Bit(process.readAllStandardError());
					break;
				}
				if (!process.waitForFinished()) {
					output = "Process failed to finish: " + QString::fromLocal8Bit(process.readAllStandardError());
					break;
				}
				auto res = QString::fromLocal8Bit(process.readAllStandardOutput());
				output = res.isEmpty() ? "success" : res;
			} while (false);
			return output;
		}
#endif
	} // namespace chrome

	namespace admin
	{
		bool IsRunAsAdmin()
		{
			BOOL fIsRunAsAdmin = FALSE;
			DWORD dwError = ERROR_SUCCESS;
			PSID pAdministratorsGroup = NULL;
			do {
				SID_IDENTIFIER_AUTHORITY NtAuthority = SECURITY_NT_AUTHORITY;
				if (!AllocateAndInitializeSid(&NtAuthority, 2, SECURITY_BUILTIN_DOMAIN_RID,
						DOMAIN_ALIAS_RID_ADMINS, 0, 0, 0, 0, 0, 0,
						&pAdministratorsGroup)) {
					dwError = GetLastError();
					break;
				}

				if (!CheckTokenMembership(NULL, pAdministratorsGroup, &fIsRunAsAdmin)) {
					dwError = GetLastError();
					break;
				}
			} while (false);

			if (pAdministratorsGroup) {
				FreeSid(pAdministratorsGroup);
				pAdministratorsGroup = NULL;
			}

			return fIsRunAsAdmin;
		}

		VOID ManagerRun(const std::string& exe, const std::string& param)
		{
			SHELLEXECUTEINFOA ShExecInfo = { 0 };
			ShExecInfo.cbSize = sizeof(SHELLEXECUTEINFO);
			ShExecInfo.fMask = SEE_MASK_NOCLOSEPROCESS;
			ShExecInfo.hwnd = NULL;
			ShExecInfo.lpVerb = "runas";
			ShExecInfo.lpFile = exe.c_str();
			ShExecInfo.lpParameters = param.c_str();
			ShExecInfo.lpDirectory = NULL;
			ShExecInfo.nShow = SW_SHOW;
			ShExecInfo.hInstApp = NULL;

			BOOL ret = ShellExecuteExA(&ShExecInfo);
			if (!ret) {
				DWORD dwError = GetLastError();
				std::cerr << "无法提升权限，错误代码: " << dwError << std::endl;
			}
			else {
				// 等待新进程启动完成
				WaitForSingleObject(ShExecInfo.hProcess, INFINITE);
				CloseHandle(ShExecInfo.hProcess);
			}
		}

	} // namespace admin

	namespace service
	{
		constexpr auto TAG{ "zServiceUtils" };
		constexpr auto timeout{ 5000 };

		inline bool IsServiceExists(const QString& serviceName)
		{
			ARGS(serviceName)

			// 服务控制管理器句柄
			SC_HANDLE schSCManager = ::OpenSCManager(NULL, NULL, SC_MANAGER_ENUMERATE_SERVICE);
			if (schSCManager == NULL) {
				LOGE << "Failed to OpenSCManager: " << GetLastError() << std::endl;
				return false;
			}

			// 尝试打开服务
			SC_HANDLE schService = OpenService(schSCManager, serviceName.toStdWString().c_str(), SERVICE_QUERY_STATUS);
			if (schService == NULL) {
				// 服务不存在
				CloseServiceHandle(schSCManager);
				return false;
			}

			// 服务存在
			CloseServiceHandle(schService);
			CloseServiceHandle(schSCManager);
			return true;
		}

		/**
		 * 安装守护服务
		 * @param exePath 守护服务可执行文件的路径
		 * @return 如果成功安装服务，返回true；否则返回false
		 */
		inline bool InstallGuardianService(const QString& exePath)
		{
			ARGS(exePath)

			QStringList arguments;
			arguments << "install";

			QProcess process;
			process.start(exePath, arguments);

			if (!process.waitForFinished(timeout)) {
				LOGE << "Failed to installService: " << GetLastError() << std::endl;
				return false;
			}
			else {
				LOGD << "InstallService successfully." << std::endl;
				return true;
			}
		}

		/**
		 * 卸载守护服务
		 * @param exePath 守护服务可执行文件的路径
		 * @return 如果卸载成功，则返回true；否则返回false
		 */
		inline bool UninstallGuardianService(const QString& exePath)
		{
			ARGS(exePath)

			QStringList arguments;
			arguments << "uninstall";

			QProcess process;
			process.start(exePath, arguments);

			if (!process.waitForFinished(timeout)) {
				LOGE << "Failed to uninstallService: " << GetLastError() << std::endl;
				return false;
			}
			else {
				LOGD << "UninstallService successfully." << std::endl;
				return true;
			}
		}

		/**
		 * 开始守护服务
		 */
		inline bool StartGuardianService()
		{
			// 服务控制管理器句柄
			SC_HANDLE schSCManager = ::OpenSCManager(NULL, NULL, SC_MANAGER_ALL_ACCESS);
			if (schSCManager == NULL) {
				LOGE << "Failed to OpenSCManager: " << GetLastError() << std::endl;
				return false;
			}

			// 服务句柄
			SC_HANDLE schService = OpenService(schSCManager, L"ZYBGuardianService", SERVICE_START);
			if (schService == NULL) {
				LOGE << "Failed to OpenService: " << GetLastError() << std::endl;
				CloseServiceHandle(schSCManager);
				return false;
			}

			// 获取当前可执行文件的路径
			TCHAR szPath[MAX_PATH];
			GetModuleFileName(NULL, szPath, MAX_PATH);

			// 提取文件名
			QFileInfo fileInfo(QCoreApplication::applicationFilePath());
			QString fileName = fileInfo.fileName();

			std::wstring wstr = fileName.toStdWString();
			wchar_t* wchars = new wchar_t[wstr.size() + 1];
			if (wcscpy_s(wchars, wstr.size() + 1, wstr.c_str()) != 0) {
				delete[] wchars;
				return false;
			}

			// 参数数组
			LPCWSTR args[] = { L"guard", wchars, szPath };

			// 参数数量
			DWORD dwNumArgs = _countof(args);

			// 启动服务
			if (!StartService(schService, dwNumArgs, args)) {
				// 启动服务失败
				LOGE << "Failed to StartService: " << GetLastError() << std::endl;
				CloseServiceHandle(schService);
				CloseServiceHandle(schSCManager);
				delete[] wchars;
				return false;
			}

			// 成功启动服务后清理资源
			CloseServiceHandle(schService);
			CloseServiceHandle(schSCManager);
			delete[] wchars;
			return true;
		}

		/**
		 * 停止守护服务
		 */
		inline bool StopGuardianService()
		{
			// 服务控制管理器句柄
			SC_HANDLE schSCManager = ::OpenSCManager(NULL, NULL, SC_MANAGER_ALL_ACCESS);
			if (schSCManager == NULL) {
				LOGE << "Failed to OpenSCManager: " << GetLastError() << std::endl;
				return false;
			}

			// 打开服务
			SC_HANDLE schService = OpenService(schSCManager, L"ZYBGuardianService", SERVICE_STOP | SERVICE_QUERY_STATUS);
			if (schService == NULL) {
				LOGE << "Failed to OpenService: " << GetLastError() << std::endl;
				CloseServiceHandle(schSCManager);
				return false;
			}

			SERVICE_STATUS ssStatus;
			if (!ControlService(schService, SERVICE_CONTROL_STOP, &ssStatus)) {
				LOGE << "Failed to ControlService: " << GetLastError() << std::endl;
				CloseServiceHandle(schService);
				CloseServiceHandle(schSCManager);
				return false;
			}

			CloseServiceHandle(schService);
			CloseServiceHandle(schSCManager);
			return true;
		}
	} // namespace service
} // namespace windows