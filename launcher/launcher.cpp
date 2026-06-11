#include <windows.h>
#include <winhttp.h>
#include <shellapi.h>
#include <shlobj.h>

#include <algorithm>
#include <atomic>
#include <cmath>
#include <filesystem>
#include <string>
#include <thread>

#pragma comment(lib, "winhttp.lib")
#pragma comment(lib, "shell32.lib")

namespace {

constexpr wchar_t kWindowClass[] = L"TWEInstallerWindow";
constexpr wchar_t kDownloadUrl[] =
    L"https://github.com/cluckegy/TWE/releases/latest/download/TWE-runtime.zip";
constexpr UINT kProgressMessage = WM_APP + 1;
constexpr UINT kStatusMessage = WM_APP + 2;
constexpr UINT kFinishedMessage = WM_APP + 3;

HWND g_window = nullptr;
std::atomic<int> g_progress{0};
std::wstring g_status = L"Preparing download...";

std::wstring runtimeDownloadUrl()
{
    const DWORD required = GetEnvironmentVariableW(
        L"TWE_RUNTIME_URL", nullptr, 0);
    if (required == 0)
        return kDownloadUrl;

    std::wstring value(required, L'\0');
    const DWORD written = GetEnvironmentVariableW(
        L"TWE_RUNTIME_URL", value.data(), required);
    if (written == 0)
        return kDownloadUrl;
    value.resize(written);
    return value;
}

std::filesystem::path installDirectory()
{
    const DWORD overrideLength = GetEnvironmentVariableW(
        L"TWE_INSTALL_ROOT", nullptr, 0);
    if (overrideLength > 0) {
        std::wstring overridePath(overrideLength, L'\0');
        const DWORD written = GetEnvironmentVariableW(
            L"TWE_INSTALL_ROOT", overridePath.data(), overrideLength);
        if (written > 0) {
            overridePath.resize(written);
            return std::filesystem::path(overridePath) / L"CL" / L"TWE";
        }
    }

    PWSTR localLow = nullptr;
    if (FAILED(SHGetKnownFolderPath(FOLDERID_LocalAppDataLow, 0, nullptr,
                                    &localLow))) {
        return {};
    }
    const std::filesystem::path result =
        std::filesystem::path(localLow) / L"CL" / L"TWE";
    CoTaskMemFree(localLow);
    return result;
}

bool launchInstalledApp()
{
    const auto executable = installDirectory() / L"TWE.exe";
    if (!std::filesystem::exists(executable))
        return false;

    const auto result = reinterpret_cast<INT_PTR>(ShellExecuteW(
        nullptr, L"open", executable.c_str(), nullptr,
        executable.parent_path().c_str(), SW_SHOWNORMAL));
    return result > 32;
}

void setStatus(const wchar_t *text)
{
    g_status = text;
    PostMessageW(g_window, kStatusMessage, 0, 0);
}

bool downloadRuntime(const std::filesystem::path &target)
{
    const std::wstring downloadUrl = runtimeDownloadUrl();
    HINTERNET session = WinHttpOpen(
        L"TWE Launcher/1.0", WINHTTP_ACCESS_TYPE_AUTOMATIC_PROXY,
        WINHTTP_NO_PROXY_NAME, WINHTTP_NO_PROXY_BYPASS, 0);
    if (!session)
        return false;

    URL_COMPONENTS components{};
    components.dwStructSize = sizeof(components);
    components.dwSchemeLength = static_cast<DWORD>(-1);
    components.dwHostNameLength = static_cast<DWORD>(-1);
    components.dwUrlPathLength = static_cast<DWORD>(-1);
    components.dwExtraInfoLength = static_cast<DWORD>(-1);
    if (!WinHttpCrackUrl(downloadUrl.c_str(), 0, 0, &components)) {
        WinHttpCloseHandle(session);
        return false;
    }

    const std::wstring host(components.lpszHostName, components.dwHostNameLength);
    std::wstring path(components.lpszUrlPath, components.dwUrlPathLength);
    if (components.dwExtraInfoLength)
        path.append(components.lpszExtraInfo, components.dwExtraInfoLength);

    HINTERNET connection = WinHttpConnect(
        session, host.c_str(), components.nPort, 0);
    HINTERNET request = connection
        ? WinHttpOpenRequest(connection, L"GET", path.c_str(), nullptr,
                             WINHTTP_NO_REFERER, WINHTTP_DEFAULT_ACCEPT_TYPES,
                             components.nScheme == INTERNET_SCHEME_HTTPS
                                 ? WINHTTP_FLAG_SECURE
                                 : 0)
        : nullptr;

    DWORD redirectPolicy = WINHTTP_OPTION_REDIRECT_POLICY_ALWAYS;
    if (request) {
        WinHttpSetOption(request, WINHTTP_OPTION_REDIRECT_POLICY,
                         &redirectPolicy, sizeof(redirectPolicy));
    }

    const bool sent = request
        && WinHttpSendRequest(request, WINHTTP_NO_ADDITIONAL_HEADERS, 0,
                              WINHTTP_NO_REQUEST_DATA, 0, 0, 0)
        && WinHttpReceiveResponse(request, nullptr);
    if (!sent) {
        if (request) WinHttpCloseHandle(request);
        if (connection) WinHttpCloseHandle(connection);
        WinHttpCloseHandle(session);
        return false;
    }

    DWORD statusCode = 0;
    DWORD statusSize = sizeof(statusCode);
    WinHttpQueryHeaders(request,
                        WINHTTP_QUERY_STATUS_CODE | WINHTTP_QUERY_FLAG_NUMBER,
                        WINHTTP_HEADER_NAME_BY_INDEX, &statusCode, &statusSize,
                        WINHTTP_NO_HEADER_INDEX);
    if (statusCode < 200 || statusCode >= 300) {
        WinHttpCloseHandle(request);
        WinHttpCloseHandle(connection);
        WinHttpCloseHandle(session);
        return false;
    }

    ULONGLONG totalSize = 0;
    DWORD sizeLength = sizeof(totalSize);
    WinHttpQueryHeaders(request,
                        WINHTTP_QUERY_CONTENT_LENGTH | WINHTTP_QUERY_FLAG_NUMBER64,
                        WINHTTP_HEADER_NAME_BY_INDEX, &totalSize, &sizeLength,
                        WINHTTP_NO_HEADER_INDEX);

    HANDLE file = CreateFileW(target.c_str(), GENERIC_WRITE, 0, nullptr,
                              CREATE_ALWAYS, FILE_ATTRIBUTE_NORMAL, nullptr);
    if (file == INVALID_HANDLE_VALUE) {
        WinHttpCloseHandle(request);
        WinHttpCloseHandle(connection);
        WinHttpCloseHandle(session);
        return false;
    }

    BYTE buffer[64 * 1024];
    ULONGLONG downloaded = 0;
    bool success = true;
    while (true) {
        DWORD bytesRead = 0;
        if (!WinHttpReadData(request, buffer, sizeof(buffer), &bytesRead)) {
            success = false;
            break;
        }
        if (bytesRead == 0)
            break;

        DWORD bytesWritten = 0;
        if (!WriteFile(file, buffer, bytesRead, &bytesWritten, nullptr)
            || bytesWritten != bytesRead) {
            success = false;
            break;
        }

        downloaded += bytesRead;
        if (totalSize > 0) {
            g_progress = (std::min)(99, static_cast<int>(downloaded * 100 / totalSize));
            PostMessageW(g_window, kProgressMessage, 0, 0);
        }
    }

    CloseHandle(file);
    WinHttpCloseHandle(request);
    WinHttpCloseHandle(connection);
    WinHttpCloseHandle(session);
    return success && downloaded > 0;
}

bool extractRuntime(const std::filesystem::path &archive,
                    const std::filesystem::path &destination)
{
    const std::wstring command =
        L"powershell.exe -NoLogo -NoProfile -NonInteractive "
        L"-ExecutionPolicy Bypass -Command \"Expand-Archive -LiteralPath '"
        + archive.wstring() + L"' -DestinationPath '"
        + destination.wstring() + L"' -Force\"";

    STARTUPINFOW startup{};
    startup.cb = sizeof(startup);
    startup.dwFlags = STARTF_USESHOWWINDOW;
    startup.wShowWindow = SW_HIDE;
    PROCESS_INFORMATION process{};
    std::wstring mutableCommand = command;
    if (!CreateProcessW(nullptr, mutableCommand.data(), nullptr, nullptr, FALSE,
                        CREATE_NO_WINDOW, nullptr, nullptr, &startup, &process)) {
        return false;
    }

    WaitForSingleObject(process.hProcess, INFINITE);
    DWORD exitCode = 1;
    GetExitCodeProcess(process.hProcess, &exitCode);
    CloseHandle(process.hThread);
    CloseHandle(process.hProcess);
    return exitCode == 0;
}

void install()
{
    const auto directory = installDirectory();
    if (directory.empty()) {
        setStatus(L"Unable to access LocalAppData.");
        PostMessageW(g_window, kFinishedMessage, FALSE, 0);
        return;
    }

    std::error_code error;
    std::filesystem::create_directories(directory, error);
    const auto archive = directory / L"TWE-runtime.zip";

    setStatus(L"Downloading TWE files...");
    if (!downloadRuntime(archive)) {
        std::filesystem::remove(archive, error);
        setStatus(L"Download failed. Check your internet connection.");
        PostMessageW(g_window, kFinishedMessage, FALSE, 0);
        return;
    }

    g_progress = 100;
    PostMessageW(g_window, kProgressMessage, 0, 0);
    setStatus(L"Installing...");
    if (!extractRuntime(archive, directory)) {
        std::filesystem::remove(archive, error);
        setStatus(L"Installation failed.");
        PostMessageW(g_window, kFinishedMessage, FALSE, 0);
        return;
    }

    std::filesystem::remove(archive, error);
    if (!launchInstalledApp()) {
        setStatus(L"TWE.exe was not found in the downloaded package.");
        PostMessageW(g_window, kFinishedMessage, FALSE, 0);
        return;
    }
    PostMessageW(g_window, kFinishedMessage, TRUE, 0);
}

void paintWindow(HWND window)
{
    PAINTSTRUCT paint{};
    HDC dc = BeginPaint(window, &paint);
    RECT client{};
    GetClientRect(window, &client);

    HBRUSH background = CreateSolidBrush(RGB(246, 250, 248));
    FillRect(dc, &client, background);
    DeleteObject(background);

    SetBkMode(dc, TRANSPARENT);
    SetTextColor(dc, RGB(24, 61, 54));

    HFONT titleFont = CreateFontW(
        26, 0, 0, 0, FW_BOLD, FALSE, FALSE, FALSE, DEFAULT_CHARSET,
        OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS, CLEARTYPE_QUALITY,
        DEFAULT_PITCH, L"Segoe UI");
    HFONT bodyFont = CreateFontW(
        15, 0, 0, 0, FW_NORMAL, FALSE, FALSE, FALSE, DEFAULT_CHARSET,
        OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS, CLEARTYPE_QUALITY,
        DEFAULT_PITCH, L"Segoe UI");
    HFONT percentFont = CreateFontW(
        28, 0, 0, 0, FW_BOLD, FALSE, FALSE, FALSE, DEFAULT_CHARSET,
        OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS, CLEARTYPE_QUALITY,
        DEFAULT_PITCH, L"Segoe UI");

    RECT titleRect{0, 22, client.right, 58};
    SelectObject(dc, titleFont);
    DrawTextW(dc, L"TWE", -1, &titleRect, DT_CENTER | DT_SINGLELINE);

    RECT circle{115, 72, 245, 202};
    HPEN trackPen = CreatePen(PS_SOLID, 10, RGB(220, 234, 229));
    HPEN progressPen = CreatePen(PS_SOLID, 10, RGB(39, 127, 108));
    HGDIOBJ oldPen = SelectObject(dc, trackPen);
    HGDIOBJ oldBrush = SelectObject(dc, GetStockObject(NULL_BRUSH));
    Ellipse(dc, circle.left, circle.top, circle.right, circle.bottom);
    SelectObject(dc, progressPen);
    const int progress = std::clamp(g_progress.load(), 0, 100);
    const int sweep = static_cast<int>(progress * 3.6);
    if (sweep > 0) {
        const double startAngle = -90.0;
        const double endAngle = startAngle + sweep;
        const int centerX = (circle.left + circle.right) / 2;
        const int centerY = (circle.top + circle.bottom) / 2;
        const int radius = (circle.right - circle.left) / 2;
        constexpr double pi = 3.14159265358979323846;
        POINT start{
            centerX + static_cast<int>(radius * cos(startAngle * pi / 180.0)),
            centerY - static_cast<int>(radius * sin(startAngle * pi / 180.0))};
        POINT end{
            centerX + static_cast<int>(radius * cos(endAngle * pi / 180.0)),
            centerY - static_cast<int>(radius * sin(endAngle * pi / 180.0))};
        Arc(dc, circle.left, circle.top, circle.right, circle.bottom,
            start.x, start.y, end.x, end.y);
    }

    SelectObject(dc, percentFont);
    const std::wstring percent = std::to_wstring(progress) + L"%";
    RECT percentRect{circle.left, circle.top + 42, circle.right, circle.bottom};
    DrawTextW(dc, percent.c_str(), -1, &percentRect, DT_CENTER | DT_SINGLELINE);

    SelectObject(dc, bodyFont);
    SetTextColor(dc, RGB(100, 122, 117));
    RECT statusRect{25, 222, client.right - 25, 262};
    DrawTextW(dc, g_status.c_str(), -1, &statusRect,
              DT_CENTER | DT_SINGLELINE | DT_END_ELLIPSIS);

    SelectObject(dc, oldBrush);
    SelectObject(dc, oldPen);
    DeleteObject(trackPen);
    DeleteObject(progressPen);
    DeleteObject(titleFont);
    DeleteObject(bodyFont);
    DeleteObject(percentFont);
    EndPaint(window, &paint);
}

LRESULT CALLBACK windowProcedure(HWND window, UINT message,
                                 WPARAM wParam, LPARAM lParam)
{
    switch (message) {
    case WM_PAINT:
        paintWindow(window);
        return 0;
    case kProgressMessage:
    case kStatusMessage:
        InvalidateRect(window, nullptr, FALSE);
        return 0;
    case kFinishedMessage:
        if (wParam)
            DestroyWindow(window);
        else
            InvalidateRect(window, nullptr, FALSE);
        return 0;
    case WM_DESTROY:
        PostQuitMessage(0);
        return 0;
    default:
        return DefWindowProcW(window, message, wParam, lParam);
    }
}

} // namespace

int WINAPI wWinMain(HINSTANCE instance, HINSTANCE, PWSTR, int)
{
    if (launchInstalledApp())
        return 0;

    WNDCLASSEXW windowClass{};
    windowClass.cbSize = sizeof(windowClass);
    windowClass.hInstance = instance;
    windowClass.lpfnWndProc = windowProcedure;
    windowClass.lpszClassName = kWindowClass;
    windowClass.hCursor = LoadCursorW(nullptr, IDC_ARROW);
    windowClass.hIcon = LoadIconW(instance, MAKEINTRESOURCEW(1));
    windowClass.hIconSm = windowClass.hIcon;
    RegisterClassExW(&windowClass);

    const int width = 360;
    const int height = 300;
    const int x = (GetSystemMetrics(SM_CXSCREEN) - width) / 2;
    const int y = (GetSystemMetrics(SM_CYSCREEN) - height) / 2;
    g_window = CreateWindowExW(
        0, kWindowClass, L"TWE Setup", WS_OVERLAPPED | WS_CAPTION | WS_SYSMENU,
        x, y, width, height, nullptr, nullptr, instance, nullptr);
    if (!g_window)
        return 1;

    ShowWindow(g_window, SW_SHOW);
    UpdateWindow(g_window);
    std::thread(install).detach();

    MSG message{};
    while (GetMessageW(&message, nullptr, 0, 0) > 0) {
        TranslateMessage(&message);
        DispatchMessageW(&message);
    }
    return static_cast<int>(message.wParam);
}
