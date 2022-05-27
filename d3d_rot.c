
/*
 * Windows 10:

 gcc -g -O2 -Wall -Wextra -o d3d_rot d3d_rot.c -ld3d11 -ld3dcompiler -ldxgi -luuid -D_WIN32_WINNT=0x0A00

 * Windows 7:

 gcc -g -O2 -Wall -Wextra -o d3d_rot d3d_rot.c -ld3d11 -ld3dcompiler -ldxgi -luuid -D_WIN32_WINNT=0x0601

 */

#include <stdlib.h>
#include <stdio.h>

#ifndef WIN32_LEAN_AND_MEAN
# define WIN32_LEAN_AND_MEAN
#endif

#include <windows.h>

#if defined _WIN32_WINNT && _WIN32_WINNT >= 0x0A00
# define HAVE_WIN10
#endif

#define _DEBUG

/* C API for d3d11 */
#define COBJMACROS

#ifdef HAVE_WIN10
# include <dxgi1_3.h>
#else
# include <dxgi.h>
#endif
#include <d3d11.h>
#include <d3dcompiler.h>

/* comment for no debug informations */
#define _DEBUG

#ifdef _DEBUG
# define FCT \
do { printf(" * %s\n", __FUNCTION__); fflush(stdout); } while (0)
#else
# define FCT \
do { } while (0)
#endif

#define XF(w,x) ((float)(2 * (x) - (w)) / (float)(w))
#define YF(h,y) ((float)((h) - 2 * (y)) / (float)(h))

typedef struct Window Window;
typedef struct D3d D3d;

struct Window
{
    HINSTANCE instance;
    RECT rect;
    HWND win;
    D3d *d3d;
    int rotation; /* rotation (clockwise): 0, 1, 2 3 */
    unsigned int fullscreen: 1;
};

void window_fullscreen_set(Window *win, unsigned int fullscreen);

void window_rotation_set(Window *win, int rotation);

struct D3d
{
    /* DXGI */
#ifdef HAVE_WIN10
    IDXGIFactory2 *dxgi_factory;
    IDXGISwapChain1 *dxgi_swapchain;
#else
    IDXGIFactory *dxgi_factory;
    IDXGISwapChain *dxgi_swapchain;
#endif
    /* D3D11 */
    ID3D11Device *d3d_device;
    ID3D11DeviceContext *d3d_device_ctx;
    ID3D11RenderTargetView *d3d_render_target_view;
    ID3D11InputLayout *d3d_input_layout;
    ID3D11VertexShader *d3d_vertex_shader;
    ID3D11Buffer *d3d_const_buffer;
    ID3D11RasterizerState *d3d_rasterizer_state;
    ID3D11PixelShader *d3d_pixel_shader;
    D3D11_VIEWPORT viewport;
    unsigned int vsync : 1;
};

typedef struct
{
    FLOAT x;
    FLOAT y;
    BYTE r;
    BYTE g;
    BYTE b;
    BYTE a;
} Vertex;

typedef struct
{
    float rotation[2][4];
} Const_Buffer;

D3d *d3d_init(Window *win, int vsync);

void d3d_shutdown(D3d *d3d);

void d3d_resize(D3d *d3d, int rot, UINT width, UINT height);

void d3d_render(D3d *d3d);

/************************* Window *************************/

LRESULT CALLBACK
_window_procedure(HWND   window,
                  UINT   message,
                  WPARAM window_param,
                  LPARAM data_param)
{
  switch (message)
    {
    case WM_CLOSE:
        PostQuitMessage(0);
        return 0;
    case WM_KEYUP:
        if (window_param == 'Q')
        {
            PostQuitMessage(0);
        }
        if (window_param == 'F')
        {
            Window *win;

#ifdef _DEBUG
            printf("fullscreen\n");
            fflush(stdout);
#endif
            win = (Window *)GetWindowLongPtr(window, GWLP_USERDATA);
            window_fullscreen_set(win, !win->fullscreen);
        }
        if (window_param == 'R')
        {
            RECT r;
            Window *win;

#ifdef _DEBUG
            printf("rotation\n");
            fflush(stdout);
#endif
            win = (Window *)GetWindowLongPtr(window, GWLP_USERDATA);
            window_rotation_set(win, (win->rotation + 1) % 4);
        }
        if (window_param == 'D')
        {
            RECT r;
            Window* win;

#ifdef _DEBUG
            printf("draw texture\n");
            fflush(stdout);
#endif
            win = (Window*)GetWindowLongPtr(window, GWLP_USERDATA);
        }
        if (window_param == 'U')
        {
            RECT r;
            Window* win;

#ifdef _DEBUG
            printf("update d3d\n");
            fflush(stdout);
#endif
            win = (Window*)GetWindowLongPtr(window, GWLP_USERDATA);
            GetClientRect(window, &r);
            d3d_resize(win->d3d,
                win->rotation,
                r.right - r.left, r.bottom - r.top);
            d3d_render(win->d3d);
        }
        return 0;
    case WM_ERASEBKGND:
        /* no need to erase back */
        return 1;
    /* GDI notifications */
    case WM_CREATE:
#ifdef _DEBUG
        printf(" * WM_CREATE\n");
        fflush(stdout);
#endif
        return 0;
    case WM_SIZE:
    {
        Window * win;

#ifdef _DEBUG
        printf(" * WM_SIZE : %u %u\n", (UINT)LOWORD(data_param), (UINT)HIWORD(data_param));
        fflush(stdout);
#endif

        win = (Window *)GetWindowLongPtr(window, GWLP_USERDATA);
        d3d_resize(win->d3d,
                   win->rotation,
                   (UINT)LOWORD(data_param), (UINT)HIWORD(data_param));

        return 0;
    }
    case WM_PAINT:
    {
#ifdef _DEBUG
        printf(" * WM_PAINT\n");
        fflush(stdout);
#endif

        if (GetUpdateRect(window, NULL, FALSE))
        {
            PAINTSTRUCT ps;
            Window *win;

            BeginPaint(window, &ps);

            win = (Window *)GetWindowLongPtr(window, GWLP_USERDATA);
            d3d_render(win->d3d);

            EndPaint(window, &ps);
        }

        return 0;
      }
    default:
      return DefWindowProc(window, message, window_param, data_param);
    }
}

Window *window_new(int x, int y, int w, int h)
{
    WNDCLASS wc;
    RECT r;
    Window *win;

    win = (Window *)calloc(1, sizeof(Window));
    if (!win)
        return NULL;

    win->instance = GetModuleHandle(NULL);
    if (!win->instance)
        goto free_win;

    memset (&wc, 0, sizeof (WNDCLASS));
    wc.style = CS_HREDRAW | CS_VREDRAW;
    wc.lpfnWndProc = _window_procedure;
    wc.cbClsExtra = 0;
    wc.cbWndExtra = 0;
    wc.hInstance = win->instance;
    wc.hIcon = LoadIcon (NULL, IDI_APPLICATION);
    wc.hCursor = LoadCursor (NULL, IDC_ARROW);
    wc.hbrBackground = NULL;
    wc.lpszMenuName =  NULL;
    wc.lpszClassName = "D3D";

    if(!RegisterClass(&wc))
        goto free_library;

    r.left = 0;
    r.top = 0;
    r.right = w;
    r.bottom = h;
    if (!AdjustWindowRectEx(&r,
                            WS_OVERLAPPEDWINDOW | WS_SIZEBOX,
                            FALSE,
                            0U))
        goto unregister_class;

    printf("window new : %d %d %ld %ld", w, h, r.right - r.left, r.bottom - r.top);
    fflush(stdout);

    win->win = CreateWindowEx(0U,
                              "D3D", "Test",
                              WS_OVERLAPPEDWINDOW | WS_SIZEBOX,
                              x, y,
                              r.right - r.left,
                              r.bottom - r.top,
                              NULL,
                              NULL, win->instance, NULL);
    if (!win->win)
        goto unregister_class;

    return win;

  unregister_class:
    UnregisterClass("D2D", win->instance);
  free_library:
    FreeLibrary(win->instance);
  free_win:
    free(win);

    return NULL;
}

void window_del(Window *win)
{
    if (!win)
        return;

    DestroyWindow(win->win);
    UnregisterClass("D2D", win->instance);
    FreeLibrary(win->instance);
    free(win);
}

void window_show(Window *win)
{
    ShowWindow(win->win, SW_SHOWNORMAL);
}

void window_fullscreen_set(Window *win, unsigned int on)
{
    HWND prev;
    DWORD style;
    DWORD exstyle;
    UINT flags;
    int x;
    int y;
    int w;
    int h;

    on =  !!on;
    if ((win->fullscreen && on) ||
        (!win->fullscreen && !on))
        return;

    if (on)
    {
        MONITORINFO mi;
        HMONITOR monitor;

        if (!GetWindowRect(win->win, &win->rect))
        {
            printf("GetWindowRect() failed\n");
            return;
        }

        monitor = MonitorFromWindow(win->win, MONITOR_DEFAULTTONEAREST);
        mi.cbSize = sizeof(MONITORINFO);
        if (!GetMonitorInfo(monitor, &mi))
            return;

        style = WS_VISIBLE | WS_POPUP;
        exstyle = WS_EX_TOPMOST;
        prev = HWND_TOPMOST;
        x = 0;
        y = 0;
        w = mi.rcMonitor.right - mi.rcMonitor.left;
        h = mi.rcMonitor.bottom - mi.rcMonitor.top;
        flags = SWP_NOCOPYBITS | SWP_SHOWWINDOW;
    }
    else
    {

        style = WS_OVERLAPPEDWINDOW | WS_SIZEBOX;
        exstyle = 0U;
        prev = HWND_NOTOPMOST;
        x = win->rect.left;
        y = win->rect.top;
        w = win->rect.right - win->rect.left;
        h = win->rect.bottom - win->rect.top;
        flags = SWP_NOCOPYBITS | SWP_SHOWWINDOW;
    }

    SetLastError(0);
    if (!SetWindowLongPtr(win->win, GWL_STYLE, style) &&
        (GetLastError() != 0))
    {
        printf("SetWindowLongPtr() failed\n");
        return;
    }
    SetLastError(0);
    if (!SetWindowLongPtr(win->win, GWL_EXSTYLE, exstyle) &&
        (GetLastError() != 0))
    {
        printf("SetWindowLongPtr() failed\n");
        return;
    }
    if (!SetWindowPos(win->win, prev, x, y, w, h, flags))
    {
        printf("SetWindowPos() failed\n");
        return;
    }

    win->fullscreen = on;
}

void window_rotation_set(Window *win, int rotation)
{
    int rdiff;

    if (win->rotation == rotation)
        return;

    rdiff = win->rotation - rotation;
    if (rdiff < 0) rdiff = -rdiff;

    if (rdiff != 2)
    {
        RECT r;
        RECT r2;
        int x;
        int y;

        win->rotation = rotation;

        if (!GetWindowRect(win->win, &r))
        {
            printf("GetClient failed\n");
            return;
        }

        x = r.left;
        y = r.top;

        if (!GetClientRect(win->win, &r))
        {
            printf("GetClient failed\n");
            return;
        }

        printf(" * win rot : %ld %ld\n", r.bottom - r.top, r.right - r.left);
        fflush(stdout);

        r2.left = 0;
        r2.top = 0;
        r2.right = r.bottom - r.top;
        r2.bottom = r.right - r.left;
        if (!AdjustWindowRectEx(&r2,
                                WS_OVERLAPPEDWINDOW | WS_SIZEBOX,
                                FALSE,
                                0U))
        {
            printf("AdjustWindowRectEx failed\n");
            return;
        }

        printf(" * win rot 2 : %ld %ld\n", r2.bottom - r2.top, r2.right - r2.left);
        fflush(stdout);

        if (!MoveWindow(win->win,
                        x, y,
                        r2.right - r2.left, r2.bottom - r2.top,
                        TRUE))
        {
            printf("MoveWindow() failed\n");
            return;
        }
    }
}



/************************** D3D11 **************************/

static void d3d_refresh_rate_get(D3d *d3d, UINT *num, UINT *den)
{
    DXGI_MODE_DESC *display_mode_list = NULL; /* 28 bytes */
    IDXGIAdapter *dxgi_adapter;
    IDXGIOutput *dxgi_output;
    UINT nbr_modes;
    UINT i;
    HRESULT res;

    *num = 0U;
    *den = 1U;

    if (!d3d->vsync)
        return;

    /* adapter of primary desktop : pass 0U */
    res = IDXGIFactory_EnumAdapters(d3d->dxgi_factory, 0U, &dxgi_adapter);
    if (FAILED(res))
        return;

    /* output of primary desktop : pass 0U */
    res = IDXGIAdapter_EnumOutputs(dxgi_adapter, 0U, &dxgi_output);
    if (FAILED(res))
        goto release_dxgi_adapter;

    /* number of mode that fit the format */
     res = IDXGIOutput_GetDisplayModeList(dxgi_output,
                                         DXGI_FORMAT_B8G8R8A8_UNORM,
                                         DXGI_ENUM_MODES_INTERLACED,
                                         &nbr_modes, NULL);
    if (FAILED(res))
        goto release_dxgi_output;

    printf("display mode list : %d\n", nbr_modes);
    fflush(stdout);
    display_mode_list = (DXGI_MODE_DESC *)malloc(nbr_modes * sizeof(DXGI_MODE_DESC));
    if (!display_mode_list)
        goto release_dxgi_output;

    /* fill the mode list */
    res = IDXGIOutput_GetDisplayModeList(dxgi_output,
                                         DXGI_FORMAT_B8G8R8A8_UNORM,
                                         DXGI_ENUM_MODES_INTERLACED,
                                         &nbr_modes, display_mode_list);
    if (FAILED(res))
        goto free_mode_list;

    for (i = 0; i < nbr_modes; i++)
    {
        if ((display_mode_list[i].Width == (UINT)GetSystemMetrics(SM_CXSCREEN)) &&
            (display_mode_list[i].Height == (UINT)GetSystemMetrics(SM_CYSCREEN)))
        {
            *num = display_mode_list[i].RefreshRate.Numerator;
            *den = display_mode_list[i].RefreshRate.Denominator;
            break;
        }
    }

#ifdef _DEBUG
    {
        DXGI_ADAPTER_DESC adapter_desc;

        IDXGIAdapter_GetDesc(dxgi_adapter, &adapter_desc);
        printf(" * video mem: %llu B, %llu MB\n",
               adapter_desc.DedicatedVideoMemory,
               adapter_desc.DedicatedVideoMemory / 1024 / 1024);
        fflush(stdout);
        wprintf(L" * description: %ls\n", adapter_desc.Description);
        fflush(stdout);
    }
#endif

  free_mode_list:
    free(display_mode_list);
  release_dxgi_output:
    IDXGIOutput_Release(dxgi_output);
  release_dxgi_adapter:
    IDXGIFactory_Release(dxgi_adapter);
}

D3d *d3d_init(Window *win, int vsync)
{
    D3D11_INPUT_ELEMENT_DESC desc_ie[] =
    {
        { "POSITION", 0, DXGI_FORMAT_R32G32_FLOAT, 0, 0, D3D11_INPUT_PER_VERTEX_DATA, 0 },
        { "COLOR", 0, DXGI_FORMAT_R8G8B8A8_UNORM, 0, 2 * sizeof(FLOAT), D3D11_INPUT_PER_VERTEX_DATA, 0 }
    };
#ifdef HAVE_WIN10
    DXGI_SWAP_CHAIN_DESC1 desc_sw;
    DXGI_SWAP_CHAIN_FULLSCREEN_DESC desc_fs;
#else
    DXGI_SWAP_CHAIN_DESC desc_sw;
#endif
    D3D11_BUFFER_DESC desc_buf;
    D3D11_RASTERIZER_DESC desc_rs;
    D3d *d3d;
    RECT r;
    HRESULT res;
    UINT flags;
    UINT num;
    UINT den;
    D3D_FEATURE_LEVEL feature_level[4];
    ID3DBlob *vs_blob; /* vertex shader blob ptr */
    ID3DBlob *ps_blob; /* pixel shader blob ptr */
    ID3DBlob *err_blob; /* error blob ptr */

    d3d = (D3d *)calloc(1, sizeof(D3d));
    if (!d3d)
        return NULL;

    d3d->vsync = vsync;
    win->d3d = d3d;

    /* create the DXGI factory */
    flags = 0;
#ifdef HAVE_WIN10
# ifdef _DEBUG
    flags = DXGI_CREATE_FACTORY_DEBUG;
# endif
    res = CreateDXGIFactory2(flags, &IID_IDXGIFactory2, (void **)&d3d->dxgi_factory);
#else
    res = CreateDXGIFactory(&IID_IDXGIFactory, (void **)&d3d->dxgi_factory);
#endif
    if (FAILED(res))
        goto free_d3d;

    /* software engine functions are called from the main loop */
    flags = D3D11_CREATE_DEVICE_SINGLETHREADED |
            D3D11_CREATE_DEVICE_BGRA_SUPPORT;
#ifdef HAVE_WIN10
# ifdef _DEBUG
    flags |= D3D11_CREATE_DEVICE_DEBUG;
# endif
#endif

    feature_level[0] = D3D_FEATURE_LEVEL_11_1;
    feature_level[1] = D3D_FEATURE_LEVEL_11_0;
    feature_level[2] = D3D_FEATURE_LEVEL_10_1;
    feature_level[3] = D3D_FEATURE_LEVEL_10_0;

    /* create device and device context with hardware support */
    res = D3D11CreateDevice(NULL,
                            D3D_DRIVER_TYPE_HARDWARE,
                            NULL,
                            flags,
                            feature_level,
                            3U,
                            D3D11_SDK_VERSION,
                            &d3d->d3d_device,
                            NULL,
                            &d3d->d3d_device_ctx);
    if (FAILED(res))
        goto release_dxgi_factory2;

    if (!GetClientRect(win->win, &r))
        goto release_d3d_device;

    /*
     * create the swap chain. It needs some settings...
     * the size of the internal buffers
     * the image format
     * the number of back buffers (>= 2 for flip model, see SwapEffect field)
     * vsync enabled: need refresh rate
     *
     * Settings are different in win 7 and win10
     */

    d3d_refresh_rate_get(d3d, &num, &den);

#ifdef HAVE_WIN10
    desc_sw.Width = r.right - r.left;
    desc_sw.Height = r.bottom - r.top;
    desc_sw.Format = DXGI_FORMAT_B8G8R8A8_UNORM;
    desc_sw.Stereo = FALSE;
#else
    desc_sw.BufferDesc.Width= r.right - r.left;
    desc_sw.BufferDesc.Height = r.bottom - r.top;
    desc_sw.BufferDesc.RefreshRate.Numerator = num;
    desc_sw.BufferDesc.RefreshRate.Denominator = den;
    desc_sw.BufferDesc.Format = DXGI_FORMAT_B8G8R8A8_UNORM;;
    desc_sw.BufferDesc.ScanlineOrdering = DXGI_MODE_SCANLINE_ORDER_UNSPECIFIED;
    desc_sw.BufferDesc.Scaling = DXGI_MODE_SCALING_UNSPECIFIED;
#endif
    desc_sw.SampleDesc.Count = 1U;
    desc_sw.SampleDesc.Quality = 0U;
    desc_sw.BufferUsage = DXGI_USAGE_RENDER_TARGET_OUTPUT;
    desc_sw.BufferCount = 2U;
#ifdef HAVE_WIN10
    desc_sw.Scaling = DXGI_SCALING_NONE;
#else
    desc_sw.OutputWindow = win->win;
    desc_sw.Windowed = TRUE;
#endif
    desc_sw.SwapEffect = DXGI_SWAP_EFFECT_FLIP_SEQUENTIAL;
#ifdef HAVE_WIN10
    desc_sw.AlphaMode = DXGI_ALPHA_MODE_UNSPECIFIED;
#endif
    desc_sw.Flags = DXGI_SWAP_CHAIN_FLAG_ALLOW_MODE_SWITCH;

#ifdef HAVE_WIN10
    desc_fs.RefreshRate.Numerator = num;
    desc_fs.RefreshRate.Denominator = den;
    desc_fs.ScanlineOrdering = DXGI_MODE_SCANLINE_ORDER_UNSPECIFIED;
    desc_fs.Scaling = DXGI_MODE_SCALING_UNSPECIFIED;
    desc_fs.Windowed = TRUE;
#endif

#ifdef HAVE_WIN10
    res = IDXGIFactory2_CreateSwapChainForHwnd(d3d->dxgi_factory,
                                               (IUnknown *)d3d->d3d_device,
                                               win->win,
                                               &desc_sw,
                                               &desc_fs,
                                               NULL,
                                               &d3d->dxgi_swapchain);
#else
    res = IDXGIFactory_CreateSwapChain(d3d->dxgi_factory,
                                       (IUnknown *)d3d->d3d_device,
                                       &desc_sw,
                                       &d3d->dxgi_swapchain);
#endif
    if (FAILED(res))
        goto release_d3d_device;

    /* rasterizer */
    desc_rs.FillMode = D3D11_FILL_SOLID;
    desc_rs.CullMode = D3D11_CULL_NONE;
    desc_rs.FrontCounterClockwise = FALSE;
    desc_rs.DepthBias = 0;
    desc_rs.DepthBiasClamp = 0.0f;
    desc_rs.SlopeScaledDepthBias = 0.0f;
    desc_rs.DepthClipEnable = TRUE;
    desc_rs.ScissorEnable = FALSE;
    desc_rs.MultisampleEnable = FALSE;
    desc_rs.AntialiasedLineEnable = FALSE;

    res = ID3D11Device_CreateRasterizerState(d3d->d3d_device,
                                             &desc_rs,
                                             &d3d->d3d_rasterizer_state);
    if (FAILED(res))
        goto release_dxgi_swapchain;

    /* Vertex shader */
    flags = D3DCOMPILE_ENABLE_STRICTNESS;
#ifdef _DEBUG
    flags |= D3DCOMPILE_DEBUG;
#endif
    vs_blob = NULL;
    res = D3DCompileFromFile(L"shader_3.hlsl",
                             NULL,
                             D3D_COMPILE_STANDARD_FILE_INCLUDE,
                             "main_vs",
                             "vs_5_0",
                             flags,
                             0U,
                             &vs_blob,
                             &err_blob );

    if (FAILED(res))
    {
        printf(" * vs error : %s\n", (char *)ID3D10Blob_GetBufferPointer(err_blob));
        goto release_d3D_rasterizer;
    }

    res = ID3D11Device_CreateVertexShader(d3d->d3d_device,
                                          ID3D10Blob_GetBufferPointer(vs_blob),
                                          ID3D10Blob_GetBufferSize(vs_blob),
                                          NULL,
                                          &d3d->d3d_vertex_shader);

    if (FAILED(res))
    {
        printf(" * vs error : %s\n", (char *)ID3D10Blob_GetBufferPointer(err_blob));
        ID3D10Blob_Release(vs_blob);
        goto release_d3D_rasterizer;
    }

    /* create the input layout */
    res = ID3D11Device_CreateInputLayout(d3d->d3d_device,
                                         desc_ie,
                                         sizeof(desc_ie) / sizeof(D3D11_INPUT_ELEMENT_DESC),
                                         ID3D10Blob_GetBufferPointer(vs_blob),
                                         ID3D10Blob_GetBufferSize(vs_blob),
                                         &d3d->d3d_input_layout);
    ID3D10Blob_Release(vs_blob);
    if (FAILED(res))
    {
        printf(" * CreateInputLayout() failed\n");
        goto release_vertex_shader;
    }

    /* Pixel shader */
    ps_blob = NULL;
    res = D3DCompileFromFile(L"shader_3.hlsl",
                             NULL,
                             D3D_COMPILE_STANDARD_FILE_INCLUDE,
                             "main_ps",
                             "ps_5_0",
                             flags,
                             0U,
                             &ps_blob,
                             &err_blob );

    if (FAILED(res))
    {
        printf(" * ps blob error : %s\n", (char *)ID3D10Blob_GetBufferPointer(err_blob));
        goto release_input_layout;
    }

    res = ID3D11Device_CreatePixelShader(d3d->d3d_device,
                                         ID3D10Blob_GetBufferPointer(ps_blob),
                                         ID3D10Blob_GetBufferSize(ps_blob),
                                         NULL,
                                         &d3d->d3d_pixel_shader);
    ID3D10Blob_Release(ps_blob);
    if (FAILED(res))
    {
        printf(" * CreatePixelShader() failed\n");
        goto release_input_layout;
    }

    desc_buf.ByteWidth = sizeof(Const_Buffer);
    desc_buf.Usage = D3D11_USAGE_DYNAMIC; /* because buffer is updated when the window has resized */
    desc_buf.BindFlags = D3D11_BIND_CONSTANT_BUFFER;
    desc_buf.CPUAccessFlags = D3D11_CPU_ACCESS_WRITE;
    desc_buf.MiscFlags = 0;
    desc_buf.StructureByteStride = 0;

    res = ID3D11Device_CreateBuffer(d3d->d3d_device,
                                    &desc_buf,
                                    NULL,
                                    &d3d->d3d_const_buffer);
    if (FAILED(res))
    {
        printf(" * CreateBuffer() failed 0x%lx\n", res);
        goto release_pixel_shader;
    }

    return d3d;

  release_pixel_shader:
    ID3D11PixelShader_Release(d3d->d3d_pixel_shader);
  release_input_layout:
    ID3D11InputLayout_Release(d3d->d3d_input_layout);
  release_vertex_shader:
    ID3D11VertexShader_Release(d3d->d3d_vertex_shader);
  release_d3D_rasterizer:
    ID3D11RasterizerState_Release(d3d->d3d_rasterizer_state);
  release_dxgi_swapchain:
#ifdef HAVE_WIN10
    IDXGISwapChain1_SetFullscreenState(d3d->dxgi_swapchain, FALSE, NULL);
    IDXGISwapChain1_Release(d3d->dxgi_swapchain);
#else
    IDXGISwapChain_SetFullscreenState(d3d->dxgi_swapchain, FALSE, NULL);
    IDXGISwapChain_Release(d3d->dxgi_swapchain);
#endif
  release_d3d_device:
    ID3D11DeviceContext_Release(d3d->d3d_device_ctx);
    ID3D11Device_Release(d3d->d3d_device);
  release_dxgi_factory2:
#ifdef HAVE_WIN10
    IDXGIFactory2_Release(d3d->dxgi_factory);
#else
    IDXGIFactory_Release(d3d->dxgi_factory);
#endif
  free_d3d:
    free(d3d);

    return NULL;
}

void d3d_shutdown(D3d *d3d)
{
#ifdef _DEBUG
    ID3D11Debug *d3d_debug;
    HRESULT res;
#endif

    if (!d3d)
        return;

#ifdef _DEBUG
    res = ID3D11Debug_QueryInterface(d3d->d3d_device, &IID_ID3D11Debug,
                                     (void **)&d3d_debug);
#endif

    ID3D11Buffer_Release(d3d->d3d_const_buffer);
    ID3D11PixelShader_Release(d3d->d3d_pixel_shader);
    ID3D11InputLayout_Release(d3d->d3d_input_layout);
    ID3D11VertexShader_Release(d3d->d3d_vertex_shader);
    ID3D11RasterizerState_Release(d3d->d3d_rasterizer_state);
    ID3D11RenderTargetView_Release(d3d->d3d_render_target_view);
#ifdef HAVE_WIN10
    IDXGISwapChain1_SetFullscreenState(d3d->dxgi_swapchain, FALSE, NULL);
    IDXGISwapChain1_Release(d3d->dxgi_swapchain);
#else
    IDXGISwapChain_SetFullscreenState(d3d->dxgi_swapchain, FALSE, NULL);
    IDXGISwapChain_Release(d3d->dxgi_swapchain);
#endif
    ID3D11DeviceContext_Release(d3d->d3d_device_ctx);
    ID3D11Device_Release(d3d->d3d_device);
#ifdef HAVE_WIN10
    IDXGIFactory2_Release(d3d->dxgi_factory);
#else
    IDXGIFactory_Release(d3d->dxgi_factory);
#endif
    free(d3d);

#ifdef _DEBUG
    if (SUCCEEDED(res))
    {
        ID3D11Debug_ReportLiveDeviceObjects(d3d_debug, D3D11_RLDO_DETAIL);
        ID3D11Debug_Release(d3d_debug);
    }
#endif
}

void d3d_resize(D3d *d3d, int rot, UINT width, UINT height)
{
    D3D11_RENDER_TARGET_VIEW_DESC desc_rtv;
    D3D11_MAPPED_SUBRESOURCE mapped;
    ID3D11Texture2D *back_buffer;
    HRESULT res;

    FCT;

    res = ID3D11DeviceContext_Map(d3d->d3d_device_ctx,
                                  (ID3D11Resource *)d3d->d3d_const_buffer,
                                  0U, D3D11_MAP_WRITE_DISCARD, 0, &mapped);

    if (FAILED(res))
    {
        printf("Map() failed\n");
        fflush(stdout);
        return;
    }

    printf(" * d3d_resize: %d\n", rot);
    fflush(stdout);

    switch (rot)
    {
        case 0:
            ((Const_Buffer *)mapped.pData)->rotation[0][0] = 1.0f;
            ((Const_Buffer *)mapped.pData)->rotation[0][1] = 0.0f;
            ((Const_Buffer *)mapped.pData)->rotation[0][2] = 0.0f;
            ((Const_Buffer *)mapped.pData)->rotation[1][0] = 0.0f;
            ((Const_Buffer *)mapped.pData)->rotation[1][1] = 1.0f;
            ((Const_Buffer *)mapped.pData)->rotation[1][2] = 0.0f;
            break;
        case 1:
            ((Const_Buffer *)mapped.pData)->rotation[0][0] = 0.0f;
            ((Const_Buffer *)mapped.pData)->rotation[0][1] = -1.0f;
            ((Const_Buffer *)mapped.pData)->rotation[0][2] = 2.0f;
            ((Const_Buffer *)mapped.pData)->rotation[1][0] = 1.0f;
            ((Const_Buffer *)mapped.pData)->rotation[1][1] = 0.0f;
            ((Const_Buffer *)mapped.pData)->rotation[1][2] = 0.0f;
            break;
        case 2:
            ((Const_Buffer *)mapped.pData)->rotation[0][0] = -1.0f;
            ((Const_Buffer *)mapped.pData)->rotation[0][1] = 0.0f;
            ((Const_Buffer *)mapped.pData)->rotation[0][2] = 0.0f;
            ((Const_Buffer *)mapped.pData)->rotation[1][0] = 0.0f;
            ((Const_Buffer *)mapped.pData)->rotation[1][1] = -1.0f;
            ((Const_Buffer *)mapped.pData)->rotation[1][2] = 0.0f;
            break;
        case 3:
            ((Const_Buffer *)mapped.pData)->rotation[0][0] = 0.0f;
            ((Const_Buffer *)mapped.pData)->rotation[0][1] = 1.0f;
            ((Const_Buffer *)mapped.pData)->rotation[0][2] = 0.0f;
            ((Const_Buffer *)mapped.pData)->rotation[1][0] = -1.0f;
            ((Const_Buffer *)mapped.pData)->rotation[1][1] = 0.0f;
            ((Const_Buffer *)mapped.pData)->rotation[1][2] = 2.0f;
            break;
    }

    ID3D11DeviceContext_Unmap(d3d->d3d_device_ctx,
                              (ID3D11Resource *)d3d->d3d_const_buffer,
                              0U);

    /* unset the render target view in the output merger */
    ID3D11DeviceContext_OMSetRenderTargets(d3d->d3d_device_ctx,
                                           0U, NULL, NULL);

    /* release the render target view */
    if (d3d->d3d_render_target_view)
        ID3D11RenderTargetView_Release(d3d->d3d_render_target_view);

    /* resize the internal nuffers of the swapt chain to the new size */
#ifdef HAVE_WIN10
    res = IDXGISwapChain1_ResizeBuffers(d3d->dxgi_swapchain,
                                        0U, /* preserve buffer count */
                                        width, height,
                                        DXGI_FORMAT_UNKNOWN, /* preserve format */
                                        0U);
#else
    res = IDXGISwapChain_ResizeBuffers(d3d->dxgi_swapchain,
                                       0U, /* preserve buffer count */
                                       width, height,
                                       DXGI_FORMAT_UNKNOWN, /* preserve format */
                                       0U);
#endif
    if ((res == DXGI_ERROR_DEVICE_REMOVED) ||
        (res == DXGI_ERROR_DEVICE_RESET) ||
        (res == DXGI_ERROR_DRIVER_INTERNAL_ERROR))
    {
        return;
    }

    if (FAILED(res))
    {
        printf("ResizeBuffers() failed\n");
        fflush(stdout);
        return;
    }

    /* get the internal buffer of the swap chain */
#ifdef HAVE_WIN10
    res = IDXGISwapChain1_GetBuffer(d3d->dxgi_swapchain, 0,
                                    &IID_ID3D11Texture2D,
                                    (void **)&back_buffer);
#else
    res = IDXGISwapChain_GetBuffer(d3d->dxgi_swapchain, 0,
                                   &IID_ID3D11Texture2D,
                                   (void **)&back_buffer);
#endif
    if (FAILED(res))
    {
        printf("swapchain GetBuffer() failed\n");
        fflush(stdout);
        return;
    }

    ZeroMemory(&desc_rtv, sizeof(D3D11_RENDER_TARGET_VIEW_DESC));
    desc_rtv.Format = DXGI_FORMAT_B8G8R8A8_UNORM;
    desc_rtv.ViewDimension = D3D11_RTV_DIMENSION_TEXTURE2D;

    /* create the new render target view from this internal buffer */
    res = ID3D11Device_CreateRenderTargetView(d3d->d3d_device,
                                              (ID3D11Resource *)back_buffer,
                                              &desc_rtv,
                                              &d3d->d3d_render_target_view);

    ID3D11Texture2D_Release(back_buffer);

    /* update the pipeline with the new render target view */
    ID3D11DeviceContext_OMSetRenderTargets(d3d->d3d_device_ctx,
                                           1U, &d3d->d3d_render_target_view,
                                           NULL);

    /* set viewport, depends on size of the window */
    d3d->viewport.TopLeftX = 0.0f;
    d3d->viewport.TopLeftY = 0.0f;
    d3d->viewport.Width = (float)width;
    d3d->viewport.Height = (float)height;
    d3d->viewport.MinDepth = 0.0f;
    d3d->viewport.MaxDepth = 1.0f;

    /* update the pipeline with the new viewport */
    ID3D11DeviceContext_RSSetViewports(d3d->d3d_device_ctx,
                                       1U, &d3d->viewport);
}

/*** triangle ***/

typedef struct
{
    ID3D11Buffer* vertex_buffer;
    ID3D11Buffer* index_buffer; /* not useful for a single triangle */
    UINT stride;
    UINT offset;
    UINT count;
    UINT index_count;
} Triangle;

Triangle *triangle_new(D3d *d3d,
                       int w, int h,
                       int x1, int y1,
                       int x2, int y2,
                       int x3, int y3,
                       unsigned char r,
                       unsigned char g,
                       unsigned char b,
                       unsigned char a)
{
    Vertex vertices[3];
    unsigned int indices[3];
    D3D11_BUFFER_DESC desc;
    D3D11_SUBRESOURCE_DATA sr_data;
    Triangle *t;
    HRESULT res;

    t = (Triangle *)malloc(sizeof(Triangle));
    if (!t)
        return NULL;

    vertices[0].x = XF(w, x1);
    vertices[0].y = YF(h, y1);
    vertices[0].r = r;
    vertices[0].g = g;
    vertices[0].b = b;
    vertices[0].a = a;
    vertices[1].x = XF(w, x2);
    vertices[1].y = YF(h, y2);
    vertices[1].r = r;
    vertices[1].g = g;
    vertices[1].b = b;
    vertices[1].a = a;
    vertices[2].x = XF(w, x3);
    vertices[2].y = YF(h, y3);
    vertices[2].r = r;
    vertices[2].g = g;
    vertices[2].b = b;
    vertices[2].a = a;

    indices[0] = 0;
    indices[1] = 1;
    indices[2] = 2;

    t->stride = sizeof(Vertex);
    t->offset = 0U;
    t->index_count = 3U;

    desc.ByteWidth = sizeof(vertices);
    desc.Usage = D3D11_USAGE_DYNAMIC;
    desc.BindFlags = D3D11_BIND_VERTEX_BUFFER;
    desc.CPUAccessFlags = D3D11_CPU_ACCESS_WRITE;
    desc.MiscFlags = 0U;
    desc.StructureByteStride = 0U;

    sr_data.pSysMem = vertices;
    sr_data.SysMemPitch = 0U;
    sr_data.SysMemSlicePitch = 0U;

    res =ID3D11Device_CreateBuffer(d3d->d3d_device,
                                   &desc,
                                   &sr_data,
                                   &t->vertex_buffer);
    if (FAILED(res))
    {
        free(t);
        return NULL;
    }

    desc.ByteWidth = sizeof(indices);
    desc.Usage = D3D11_USAGE_DYNAMIC;
    desc.BindFlags = D3D11_BIND_INDEX_BUFFER;
    desc.CPUAccessFlags = D3D11_CPU_ACCESS_WRITE;
    desc.MiscFlags = 0U;
    desc.StructureByteStride = 0U;

    sr_data.pSysMem = indices;
    sr_data.SysMemPitch = 0U;
    sr_data.SysMemSlicePitch = 0U;

    res =ID3D11Device_CreateBuffer(d3d->d3d_device,
                                   &desc,
                                   &sr_data,
                                   &t->index_buffer);
    if (FAILED(res))
    {
        ID3D11Buffer_Release(t->vertex_buffer);
        free(t);
        return NULL;
    }

    return t;
}

void triangle_free(Triangle *t)
{
    if (!t)
        return ;

    ID3D11Buffer_Release(t->index_buffer);
    ID3D11Buffer_Release(t->vertex_buffer);
    free(t);
}

/*** rectangle ***/

typedef struct
{
    ID3D11Buffer* vertex_buffer;
    ID3D11Buffer* index_buffer;
    UINT stride;
    UINT offset;
    UINT count;
    UINT index_count;
} Rect;

Rect *rectangle_new(D3d *d3d,
                    int w, int h,
                    int x, int y,
                    int rw, int rh, /* width and height of the rectangle */
                    unsigned char r,
                    unsigned char g,
                    unsigned char b,
                    unsigned char a)
{
    Vertex vertices[4];
    unsigned int indices[6];
    D3D11_BUFFER_DESC desc;
    D3D11_SUBRESOURCE_DATA sr_data;
    Rect *rc;
    HRESULT res;

    rc = (Rect *)malloc(sizeof(Rect));
    if (!rc)
        return NULL;

    /* vertex upper left */
    vertices[0].x = XF(w, x);
    vertices[0].y = YF(h, y);
    vertices[0].r = r;
    vertices[0].g = g;
    vertices[0].b = b;
    vertices[0].a = a;
    /* vertex upper right*/
    vertices[1].x = XF(w, x + rw);
    vertices[1].y = YF(h, y);
    vertices[1].r = r;
    vertices[1].g = g;
    vertices[1].b = b;
    vertices[1].a = a;
    /* vertex bottom right*/
    vertices[2].x = XF(w, x + rw);
    vertices[2].y = YF(h, y + rh);
    vertices[2].r = r;
    vertices[2].g = g;
    vertices[2].b = b;
    vertices[2].a = a;
    /* vertex bottom left*/
    vertices[3].x = XF(w, x);
    vertices[3].y = YF(h, y + rh);
    vertices[3].r = r;
    vertices[3].g = g;
    vertices[3].b = b;
    vertices[3].a = a;

    /* triangle upper left */
    indices[0] = 0;
    indices[1] = 1;
    indices[2] = 3;
    /* triangle bottom right */
    indices[3] = 1;
    indices[4] = 2;
    indices[5] = 3;

    rc->stride = sizeof(Vertex);
    rc->offset = 0U;
    rc->index_count = 6U;

    desc.ByteWidth = sizeof(vertices);
    desc.Usage = D3D11_USAGE_DYNAMIC;
    desc.BindFlags = D3D11_BIND_VERTEX_BUFFER;
    desc.CPUAccessFlags = D3D11_CPU_ACCESS_WRITE;
    desc.MiscFlags = 0U;
    desc.StructureByteStride = 0U;

    sr_data.pSysMem = vertices;
    sr_data.SysMemPitch = 0U;
    sr_data.SysMemSlicePitch = 0U;

    res =ID3D11Device_CreateBuffer(d3d->d3d_device,
                                   &desc,
                                   &sr_data,
                                   &rc->vertex_buffer);
    if (FAILED(res))
    {
        free(rc);
        return NULL;
    }

    desc.ByteWidth = sizeof(indices);
    desc.Usage = D3D11_USAGE_DYNAMIC;
    desc.BindFlags = D3D11_BIND_INDEX_BUFFER;
    desc.CPUAccessFlags = D3D11_CPU_ACCESS_WRITE;
    desc.MiscFlags = 0U;
    desc.StructureByteStride = 0U;

    sr_data.pSysMem = indices;
    sr_data.SysMemPitch = 0U;
    sr_data.SysMemSlicePitch = 0U;

    res =ID3D11Device_CreateBuffer(d3d->d3d_device,
                                   &desc,
                                   &sr_data,
                                   &rc->index_buffer);
    if (FAILED(res))
    {
        ID3D11Buffer_Release(rc->vertex_buffer);
        free(rc);
        return NULL;
    }

    return rc;
}

void rectangle_free(Rect *r)
{
    if (!r)
        return ;

    ID3D11Buffer_Release(r->index_buffer);
    ID3D11Buffer_Release(r->vertex_buffer);
    free(r);
}

void d3d_render(D3d *d3d)
{
#ifdef HAVE_WIN10
    DXGI_PRESENT_PARAMETERS pp;
    DXGI_SWAP_CHAIN_DESC1 desc;
#else
    DXGI_SWAP_CHAIN_DESC desc;
#endif
    const FLOAT color[4] = { 0.10f, 0.18f, 0.24f, 1.0f };
    HRESULT res;
    int w;
    int h;

    FCT;

#ifdef HAVE_WIN10
    res = IDXGISwapChain1_GetDesc1(d3d->dxgi_swapchain, &desc);
    if (FAILED(res))
        return;

    w = desc.Width;
    h = desc.Height;
#else
    res = IDXGISwapChain_GetDesc(d3d->dxgi_swapchain, &desc);
    if (FAILED(res))
        return;

    w = desc.BufferDesc.Width;
    h = desc.BufferDesc.Height;
#endif

    printf(" * swapchain size : %d %d\n", w, h);
    fflush(stdout);

    /* clear render target */
    ID3D11DeviceContext_ClearRenderTargetView(d3d->d3d_device_ctx,
                                              d3d->d3d_render_target_view,
                                              color);
    /* Input Assembler (IA) stage */
    ID3D11DeviceContext_IASetPrimitiveTopology(d3d->d3d_device_ctx,
                                               D3D11_PRIMITIVE_TOPOLOGY_TRIANGLESTRIP);
    ID3D11DeviceContext_IASetInputLayout(d3d->d3d_device_ctx,
                                         d3d->d3d_input_layout);

    /* vertex shader stage */
    ID3D11DeviceContext_VSSetShader(d3d->d3d_device_ctx,
                                    d3d->d3d_vertex_shader,
                                    NULL,
                                    0);
    ID3D11DeviceContext_VSSetConstantBuffers(d3d->d3d_device_ctx,
                                             0,
                                             1,
                                             &d3d->d3d_const_buffer);

    /*
     * Rasterizer Stage
     *
     * RSSetViewports() called in the resize() callback
     */
    ID3D11DeviceContext_RSSetState(d3d->d3d_device_ctx,
                                   d3d->d3d_rasterizer_state);
    /* pixel shader stage */
    ID3D11DeviceContext_PSSetShader(d3d->d3d_device_ctx,
                                    d3d->d3d_pixel_shader,
                                    NULL,
                                    0);

    /*
     * Output Merger stage
     *
     * OMSetRenderTargets() called in the resize() calback
     */

    /* scene */
    Triangle *t;
    Rect *r;

    t = triangle_new(d3d,
                     w, h,
                     320, 120,
                     480, 360,
                     160, 360,
                     255, 255, 0, 255);

    r = rectangle_new(d3d,
                      w, h,
                      520, 120,
                      200, 100,
                      0, 0, 255, 255);

    /* Input Assembler (IA) stage */
    ID3D11DeviceContext_IASetVertexBuffers(d3d->d3d_device_ctx,
                                           0,
                                           1,
                                           &t->vertex_buffer,
                                           &t->stride,
                                           &t->offset);
    ID3D11DeviceContext_IASetIndexBuffer(d3d->d3d_device_ctx,
                                         t->index_buffer,
                                         DXGI_FORMAT_R32_UINT,
                                         0);
    /* draw */
    ID3D11DeviceContext_DrawIndexed(d3d->d3d_device_ctx,
                                    t->index_count,
                                    0, 0);

    /* Input Assembler (IA) stage */
    ID3D11DeviceContext_IASetVertexBuffers(d3d->d3d_device_ctx,
                                           0,
                                           1,
                                           &r->vertex_buffer,
                                           &r->stride,
                                           &r->offset);
    ID3D11DeviceContext_IASetIndexBuffer(d3d->d3d_device_ctx,
                                         r->index_buffer,
                                         DXGI_FORMAT_R32_UINT,
                                         0);
    /* draw */
    ID3D11DeviceContext_DrawIndexed(d3d->d3d_device_ctx,
                                    r->index_count,
                                    0, 0);

    rectangle_free(r);
    triangle_free(t);

    /*
     * present frame, that is, flip the back buffer and the front buffer
     * if no vsync, we present immediatly
     */
#ifdef HAVE_WIN10
    pp.DirtyRectsCount = 0;
    pp.pDirtyRects = NULL;
    pp.pScrollRect = NULL;
    pp.pScrollOffset = NULL;
    res = IDXGISwapChain1_Present1(d3d->dxgi_swapchain,
                                   d3d->vsync ? 1 : 0, 0, &pp);
#else
    res = IDXGISwapChain_Present(d3d->dxgi_swapchain,
                                 d3d->vsync ? 1 : 0, 0);
#endif
    if (res == DXGI_ERROR_DEVICE_RESET || res == DXGI_ERROR_DEVICE_REMOVED)
    {
        printf("device removed or lost, need to recreate everything\n");
        fflush(stdout);
    }
    else if (res == DXGI_STATUS_OCCLUDED)
    {
        printf("window is not visible, so vsync won't work. Let's sleep a bit to reduce CPU usage\n");
        fflush(stdout);
    }
}


int main()
{
    Window *win;
    D3d *d3d;
    int ret = 1;

    /* remove scaling on HiDPI */
#if _WIN32_WINNT >= 0x0A00
    SetProcessDpiAwarenessContext(DPI_AWARENESS_CONTEXT_SYSTEM_AWARE);
#endif

    win = window_new(100, 100, 800, 480);
    if (!win)
        return ret;

    d3d = d3d_init(win, 0);
    if (!d3d)
    {
        printf(" * d3d_init() failed\n");
        fflush(stdout);
        goto del_window;
    }

    ret = 0;

    SetWindowLongPtr(win->win, GWLP_USERDATA, (LONG_PTR)win);

    window_show(win);

    /* mesage loop */
    while(1)
    {
        MSG msg;
        BOOL ret;

        ret = PeekMessage(&msg, NULL, 0, 0, PM_REMOVE);
        if (ret)
        {
            do
            {
                if (msg.message == WM_QUIT)
                  goto beach;
                TranslateMessage(&msg);
                DispatchMessageW(&msg);
            } while (PeekMessage(&msg, NULL, 0, 0, PM_REMOVE));
        }
    }

  beach:
    d3d_shutdown(d3d);
  del_window:
    window_del(win);

    return ret;
}

