#include "Win32Window.h"
#include "Logger.h"

namespace anxiety::platform {
    static constexpr std::string_view k_category = "Win32Window";
    static constexpr wchar_t          k_class[]  = L"Anxiety_Win32Window";

    // Se registra una vez por proceso — la destrucción la gestiona UnregisterClass en el destructor
    // de Win32Window de la última ventana viva (no se rastrea aquí; el SO recupera la clase cuando
    // el proceso termina).
    static bool s_class_registered = false;

    // Constructor / Destructor -------------------------------------------------------------------
    Win32Window::Win32Window(const IWindow::Desc& desc) : m_width(desc.width) , m_height(desc.height) {
        const HINSTANCE hInst = GetModuleHandleW(nullptr);

        // Registrar la clase de ventana (una vez por proceso) ---------------------------------------
        if (!s_class_registered) {
            WNDCLASSEXW wc{};
            wc.cbSize        = sizeof(WNDCLASSEXW);
            wc.style         = CS_HREDRAW | CS_VREDRAW | CS_OWNDC;
            wc.lpfnWndProc   = Win32Window::wndProc;
            wc.hInstance     = hInst;
            // MAKEINTRESOURCEW evita el desajuste LPSTR/LPCWSTR al usar APIs con sufijo W sin el define global UNICODE.
            wc.hCursor       = LoadCursorW(nullptr, MAKEINTRESOURCEW(32512));   // IDC_ARROW
            wc.hbrBackground = reinterpret_cast<HBRUSH>(COLOR_WINDOW + 1);
            wc.lpszClassName = k_class;
            wc.hIcon         = LoadIconW(nullptr, MAKEINTRESOURCEW(32512));     // IDI_APPLICATION
            wc.hIconSm       = LoadIconW(nullptr, MAKEINTRESOURCEW(32512));

            if (!RegisterClassExW(&wc)) {
                LOGF_ERROR(k_category, "RegisterClassExW falló (err=0x{:08X}).", static_cast<unsigned>(GetLastError()));
                return;
            }

            s_class_registered = true;
        }

        // Convertir el título de UTF-8 a UTF-16 -----------------------------------------------------
        const int wLen = MultiByteToWideChar(CP_UTF8, 0, desc.title.c_str(), static_cast<int>(desc.title.size()), nullptr, 0);
        std::wstring wTitle(static_cast<size_t>(wLen), L'\0');
        MultiByteToWideChar(CP_UTF8, 0, desc.title.c_str(), static_cast<int>(desc.title.size()), wTitle.data(), wLen);

        // Calcular el rectángulo de ventana que da el área cliente solicitada ----------------------
        DWORD style = WS_OVERLAPPEDWINDOW;
        if (!desc.resizable) style &= ~(WS_THICKFRAME | WS_MAXIMIZEBOX);

        RECT rc{ 0, 0, static_cast<LONG>(desc.width), static_cast<LONG>(desc.height) };
        AdjustWindowRectEx(&rc, style, FALSE, 0);

        // Crear la ventana ---------------------------------------------------------------------------
        // Se pasa `this` como lpCreateParams para que wndProc pueda guardarlo en GWLP_USERDATA durante WM_NCCREATE antes de que CreateWindowExW retorne.
        m_hwnd = CreateWindowExW(
            0,
            k_class,
            wTitle.c_str(),
            style,
            CW_USEDEFAULT, CW_USEDEFAULT,
            rc.right - rc.left,
            rc.bottom - rc.top,
            nullptr, nullptr, hInst,
            this);

        if (!m_hwnd) {
            LOGF_ERROR(k_category, "CreateWindowExW falló (err=0x{:08X}).", static_cast<unsigned>(GetLastError()));
            return;
        }

        m_open = true;
        ShowWindow(m_hwnd, SW_SHOWDEFAULT);
        UpdateWindow(m_hwnd);

        LOGF_INFO(k_category, "Ventana '{}' creada {}x{} (HWND=0x{:X}).", desc.title, m_width, m_height, reinterpret_cast<uintptr_t>(m_hwnd));
    }

    Win32Window::~Win32Window() {
        if (m_hwnd) {
            DestroyWindow(m_hwnd);
            m_hwnd = nullptr;
        }
    }

    // IWindow ------------------------------------------------------------------------------------
    bool Win32Window::poll_events() {
        MSG msg;
        while (PeekMessageW(&msg, nullptr, 0, 0, PM_REMOVE)) {
            if (msg.message == WM_QUIT) {
                m_open = false;
                return false;
            }
            TranslateMessage(&msg);
            DispatchMessageW(&msg);
        }
        return m_open;
    }

    void Win32Window::close() { if (m_hwnd) PostMessageW(m_hwnd, WM_CLOSE, 0, 0); }

    bool     Win32Window::is_open()       const noexcept { return m_open; }
    uint32_t Win32Window::width()         const noexcept { return m_width; }
    uint32_t Win32Window::height()        const noexcept { return m_height; }
    void*    Win32Window::native_handle() const noexcept { return m_hwnd; }

    // Procedimiento de ventana ---------------------------------------------------------------------
    LRESULT CALLBACK Win32Window::wndProc(HWND hwnd, UINT msg,
        WPARAM wParam, LPARAM lParam) {
        // En el primerísimo mensaje (WM_NCCREATE), vincula el Win32Window* almacenado en
        // lpCreateParams a GWLP_USERDATA para que los mensajes siguientes puedan encontrarlo.
        if (msg == WM_NCCREATE) {
            const auto* cs = reinterpret_cast<const CREATESTRUCTW*>(lParam);
            auto* self     = static_cast<Win32Window*>(cs->lpCreateParams);
            SetWindowLongPtrW(hwnd, GWLP_USERDATA, reinterpret_cast<LONG_PTR>(self));
            self->m_hwnd = hwnd;                        // disponible para cualquier mensaje disparado durante la creación
            return DefWindowProcW(hwnd, msg, wParam, lParam);
        }

        auto* self = reinterpret_cast<Win32Window*>(GetWindowLongPtrW(hwnd, GWLP_USERDATA));
        if (!self) return DefWindowProcW(hwnd, msg, wParam, lParam);

        switch (msg) {
        case WM_CLOSE:
            self->m_open = false;
            DestroyWindow(hwnd);
            return 0;

        case WM_DESTROY:
            PostQuitMessage(0);
            return 0;

        case WM_SIZE:
            if (wParam != SIZE_MINIMIZED) {
                self->m_width  = LOWORD(lParam);
                self->m_height = HIWORD(lParam);
            }
            return 0;

        default:
            return DefWindowProcW(hwnd, msg, wParam, lParam);
        }
    }
} // namespace anxiety::platform
