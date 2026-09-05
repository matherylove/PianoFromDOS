/*************************************************************************************************
*
* File: PianoFromAbove.cpp
*
* Description: Main entry point for Piano From Above.
*              Creates windows and enters the GUI and game loops
*
* Copyright (c) 2010 Brian Pantano. All rights reserved.
*
*************************************************************************************************/
#include <windows.h>
#include <commctrl.h>
#include <ctime>

#include "MainProcs.h"
#include "resource.h"

#include "Config.h"
#include "GameState.h"
#include "Renderer.h"
#include "Misc.h"

INT WINAPI WinMain( HINSTANCE hInstance, HINSTANCE hPrevInstance, LPSTR lpszCmdLine, INT nCmdShow );
DWORD WINAPI GameThread( LPVOID lpParameter );

//-----------------------------------------------------------------------------
// Global variables
//-----------------------------------------------------------------------------
HINSTANCE g_hInstance = NULL;
HWND g_hWnd = NULL;
HWND g_hWndBar = NULL;
HWND g_hWndLibDlg = NULL;
HWND g_hWndGfx = NULL;
TSQueue< MSG > g_MsgQueue; // Producer/consumer to hold events for our game thread

static HANDLE g_hStartupLog = INVALID_HANDLE_VALUE;
static HMODULE g_hUnicows = NULL;

void PFD_StartupLogA( const char *text )
{
    if ( g_hStartupLog == INVALID_HANDLE_VALUE || !text ) return;
    DWORD written = 0;
    WriteFile( g_hStartupLog, text, ( DWORD )lstrlenA( text ), &written, NULL );
    FlushFileBuffers( g_hStartupLog );
}

static LONG WINAPI PFD_UnhandledExceptionFilter( EXCEPTION_POINTERS *pExceptionInfo )
{
    char line[512];
    DWORD code = 0;
    DWORD address = 0;

    if ( pExceptionInfo && pExceptionInfo->ExceptionRecord )
    {
        code = pExceptionInfo->ExceptionRecord->ExceptionCode;
        address = ( DWORD )( ULONG_PTR )pExceptionInfo->ExceptionRecord->ExceptionAddress;
    }

    wsprintfA( line,
               "UNHANDLED EXCEPTION: code=0x%08lX address=0x%08lX\r\n",
               ( unsigned long )code, ( unsigned long )address );
    PFD_StartupLogA( line );

#if defined(_M_IX86) || defined(__i386__)
    if ( pExceptionInfo && pExceptionInfo->ContextRecord )
    {
        CONTEXT *ctx = pExceptionInfo->ContextRecord;
        wsprintfA( line,
                   "REGISTERS: EIP=%08lX ESP=%08lX EBP=%08lX EAX=%08lX EBX=%08lX ECX=%08lX EDX=%08lX ESI=%08lX EDI=%08lX\r\n",
                   ( unsigned long )ctx->Eip, ( unsigned long )ctx->Esp,
                   ( unsigned long )ctx->Ebp, ( unsigned long )ctx->Eax,
                   ( unsigned long )ctx->Ebx, ( unsigned long )ctx->Ecx,
                   ( unsigned long )ctx->Edx, ( unsigned long )ctx->Esi,
                   ( unsigned long )ctx->Edi );
        PFD_StartupLogA( line );
    }
#endif

    MessageBoxA( NULL,
                 "PianoFromDOS encountered an unhandled exception.\r\n\r\nDetails were written to PianoFromDOS-startup.log.",
                 "PianoFromDOS crash", MB_OK | MB_ICONERROR );
    return EXCEPTION_EXECUTE_HANDLER;
}

static int PFD_StartupFailA( const char *stage, DWORD errorCode )
{
    char buffer[512];
    wsprintfA( buffer,
               "PianoFromDOS could not start.\r\n\r\nStage: %s\r\nWin32 error: %lu\r\n\r\nSee PianoFromDOS-startup.log next to the executable.",
               stage ? stage : "unknown", ( unsigned long )errorCode );
    char logLine[256];
    wsprintfA( logLine, "FAILED: %s (Win32 error %lu)\r\n",
               stage ? stage : "unknown", ( unsigned long )errorCode );
    PFD_StartupLogA( logLine );
    MessageBoxA( NULL, buffer, "PianoFromDOS startup error", MB_OK | MB_ICONERROR );
    return 1;
}

static bool PFD_IsWin9x()
{
    return ( GetVersion() & 0x80000000UL ) != 0;
}

static HANDLE PFD_CreateGameThread( LPVOID lpParameter )
{
    // Windows 95/98/ME require lpThreadId to be non-NULL.  The previous
    // compatibility workaround also forced a 256 KiB stack, but that was not
    // the cause of ERROR_INVALID_PARAMETER and is too small for the full game
    // state/rendering path.  Use the PE/default stack on every platform.
    DWORD threadId = 0;
    SetLastError( ERROR_SUCCESS );
    HANDLE hThread = CreateThread( NULL, 0, GameThread, lpParameter, 0,
                                   PFD_IsWin9x() ? &threadId : NULL );
    if ( hThread )
    {
        char line[192];
        if ( PFD_IsWin9x() )
            wsprintfA( line, "Game thread created with default stack (thread id %lu).\r\n",
                       ( unsigned long )threadId );
        else
            lstrcpyA( line, "Game thread created with default stack.\r\n" );
        PFD_StartupLogA( line );
        return hThread;
    }

    DWORD lastThreadError = GetLastError();
    char line[192];
    wsprintfA( line, "CreateThread(default stack) failed (Win32 error %lu).\r\n",
               ( unsigned long )lastThreadError );
    PFD_StartupLogA( line );

    // Last-resort Win9x fallback only.  Keep it much larger than the old
    // 64/128/256 KiB values to avoid stack exhaustion during song loading.
    if ( PFD_IsWin9x() )
    {
        const DWORD fallbackStack = 1024UL * 1024UL;
        threadId = 0;
        SetLastError( ERROR_SUCCESS );
        hThread = CreateThread( NULL, fallbackStack, GameThread, lpParameter, 0, &threadId );
        if ( hThread )
        {
            wsprintfA( line, "Game thread created with 1024 KiB fallback stack (thread id %lu).\r\n",
                       ( unsigned long )threadId );
            PFD_StartupLogA( line );
            return hThread;
        }
        lastThreadError = GetLastError();
        wsprintfA( line, "CreateThread(1024 KiB fallback stack) failed (Win32 error %lu).\r\n",
                   ( unsigned long )lastThreadError );
        PFD_StartupLogA( line );
    }

    SetLastError( lastThreadError );
    return NULL;
}

//-----------------------------------------------------------------------------
// Name: wWinMain()
// Desc: The application's entry point
//-----------------------------------------------------------------------------
INT WINAPI WinMain( HINSTANCE hInstance, HINSTANCE hPrevInstance, LPSTR lpszCmdLine, INT nCmdShow )
{
    g_hInstance = hInstance;

    // Keep diagnostics strictly ANSI until MSLU is confirmed available.
    g_hStartupLog = CreateFileA( "PianoFromDOS-startup.log", GENERIC_WRITE, FILE_SHARE_READ | FILE_SHARE_WRITE, NULL,
                                 CREATE_ALWAYS, FILE_ATTRIBUTE_NORMAL, NULL );
    PFD_StartupLogA( "PianoFromDOS entered WinMain.\r\n" );
    SetUnhandledExceptionFilter( PFD_UnhandledExceptionFilter );
    PFD_StartupLogA( "Unhandled exception filter installed.\r\n" );

    if ( PFD_IsWin9x() )
    {
        PFD_StartupLogA( "Windows 9x detected; loading UNICOWS.DLL.\r\n" );
        g_hUnicows = LoadLibraryA( "unicows.dll" );
        if ( !g_hUnicows )
        {
            DWORD err = GetLastError();
            MessageBoxA( NULL,
                         "PianoFromDOS requires Microsoft UNICOWS.DLL on Windows 98/ME.\r\n\r\n"
                         "Place UNICOWS.DLL in the same directory as PianoFromDOS.exe and try again.",
                         "PianoFromDOS - missing UNICOWS.DLL", MB_OK | MB_ICONERROR );
            PFD_StartupLogA( "FAILED: UNICOWS.DLL could not be loaded.\r\n" );
            return ( int )( err ? err : 1 );
        }
        PFD_StartupLogA( "UNICOWS.DLL loaded successfully.\r\n" );
    }
    else
    {
        PFD_StartupLogA( "NT-family Windows detected; UNICOWS.DLL is not required.\r\n" );
    }

    srand( ( unsigned )time( NULL ) );

    // Initialize the classic common controls first.  This entry point exists on
    // the original Win9x common-controls DLLs and is sufficient as a safe
    // baseline.  Then request the extra rebar/cool classes when supported.
    InitCommonControls();
    PFD_StartupLogA( "InitCommonControls completed.\r\n" );

    INITCOMMONCONTROLSEX icex;
    icex.dwSize = sizeof( INITCOMMONCONTROLSEX );
    icex.dwICC  = ICC_WIN95_CLASSES | ICC_COOL_CLASSES;
    SetLastError( ERROR_SUCCESS );
    if ( !InitCommonControlsEx( &icex ) )
    {
        DWORD err = GetLastError();
        char ccLog[160];
        wsprintfA( ccLog,
                   "WARNING: InitCommonControlsEx returned FALSE (Win32 error %lu); continuing with classic controls.\r\n",
                   ( unsigned long )err );
        PFD_StartupLogA( ccLog );
    }
    else
    {
        PFD_StartupLogA( "InitCommonControlsEx completed.\r\n" );
    }

    // Initialize COM. For the SH* functions
    HRESULT hr = CoInitialize( NULL );
    if ( FAILED( hr ) ) return PFD_StartupFailA( "CoInitialize", ( DWORD )hr );
    PFD_StartupLogA( "COM initialized.\r\n" );

    // Register the window class
    WNDCLASSEX wc;
    wc.cbSize = sizeof( WNDCLASSEX );
    wc.style = 0;
    wc.lpfnWndProc = WndProc;
    wc.cbClsExtra = 0L;
    wc.cbWndExtra = 0L;
    wc.hInstance = hInstance;
    wc.hIcon = LoadIcon( hInstance, MAKEINTRESOURCE( IDI_PFAICON ) );
    wc.hCursor = LoadCursor( NULL, IDC_ARROW );
    // Window is only a container... never seen, thus null brush
    wc.hbrBackground = NULL; //( HBRUSH )GetStockObject( NULL_BRUSH );
    wc.lpszMenuName = MAKEINTRESOURCE( IDM_MAINMENU );
    wc.lpszClassName = CLASSNAME;
    wc.hIconSm = NULL;
    if ( !RegisterClassEx( &wc ) )
        return PFD_StartupFailA( "RegisterClassEx(main)", GetLastError() );

    // Register the graphics window class
    wc.style = CS_OWNDC;
    wc.lpfnWndProc = GfxProc;
    wc.lpszMenuName = NULL;
    wc.lpszClassName = GFXCLASSNAME;
    if ( !RegisterClassEx( &wc ) )
        return PFD_StartupFailA( "RegisterClassEx(graphics)", GetLastError() );

    // Register the position control window class
    wc.style = 0;
    wc.lpfnWndProc = PosnProc;
    wc.lpszClassName = POSNCLASSNAME;
    if ( !RegisterClassEx( &wc ) )
        return PFD_StartupFailA( "RegisterClassEx(position)", GetLastError() );

    // In addition to getting settings, triggers loading of saved config
    PFD_StartupLogA( "Loading configuration.\r\n" );
    Config &config = Config::GetConfig();
    ViewSettings &cView = config.GetViewSettings();
    PlaybackSettings &cPlayback = config.GetPlaybackSettings();
    PFD_StartupLogA( "Configuration loaded.\r\n" );

    // Create the application window
    g_hWnd = CreateWindowEx( 0, CLASSNAME, CLASSNAME, WS_OVERLAPPEDWINDOW | WS_CLIPCHILDREN, cView.GetMainLeft(), cView.GetMainTop(),
                             cView.GetMainWidth(), cView.GetMainHeight(), NULL, NULL, wc.hInstance, NULL );
    if ( !g_hWnd ) return PFD_StartupFailA( "CreateWindowEx(main)", GetLastError() );
    PFD_StartupLogA( "Main window created.\r\n" );

    // Creation order (z-order) matters big time for full screen

    // Create the controls rebar
    g_hWndBar = CreateRebar( g_hWnd );
    if ( !g_hWndBar ) return PFD_StartupFailA( "CreateRebar", GetLastError() );
    PFD_StartupLogA( "Playback rebar created.\r\n" );

    // Create the library window
    g_hWndLibDlg = CreateDialog( hInstance, MAKEINTRESOURCE( IDD_LIBDLG ), g_hWnd, LibDlgProc );
    if ( !g_hWndLibDlg ) return PFD_StartupFailA( "CreateDialog(library)", GetLastError() );
    PFD_StartupLogA( "Library dialog created.\r\n" );
    SetWindowLongPtr( g_hWndLibDlg, GWL_EXSTYLE,
                      GetWindowLongPtr( g_hWndLibDlg, GWL_EXSTYLE ) | WS_EX_CONTROLPARENT );

    // Create the graphics window
    g_hWndGfx = CreateWindowEx( 0, GFXCLASSNAME, NULL, WS_CHILD | WS_TABSTOP | WS_CLIPSIBLINGS,
                                0, 0, 800, 600, g_hWnd, NULL, wc.hInstance, NULL );
    if ( !g_hWndGfx ) return PFD_StartupFailA( "CreateWindowEx(graphics)", GetLastError() );
    PFD_StartupLogA( "Graphics window created.\r\n" );

    HACCEL hAccel = LoadAccelerators( hInstance, MAKEINTRESOURCE( IDA_MAINMENU ) );
    if ( !hAccel ) return PFD_StartupFailA( "LoadAccelerators", GetLastError() );

    // Get the game going. Keep the startup state alive across Win9x stack-size retries.
    SplashScreen *pInitialState = new SplashScreen( NULL, NULL );
    HANDLE hThread = PFD_CreateGameThread( pInitialState );
    if ( !hThread )
    {
        DWORD threadError = GetLastError();
        delete pInitialState;
        return PFD_StartupFailA( "CreateThread", threadError );
    }
    PFD_StartupLogA( "Game thread is running.\r\n" );

    // Set up GUI and show
    SetPlayMode( GameState::Splash );
    SetOnTop( cView.GetOnTop() );
    ShowControls( cView.GetControls() );
    ShowLibrary( cView.GetLibrary() );
    ShowWindow( g_hWndGfx, SW_SHOW );
    ShowWindow( g_hWnd, nCmdShow );
    UpdateWindow( g_hWnd );
    PFD_StartupLogA( "Main window shown; startup completed.\r\n" );
    SetFocus( g_hWndGfx );
    cPlayback.SetPaused( false, false );

    // Enter the message loop
    MSG msg = { 0 };
    while( GetMessage( &msg, NULL, 0, 0 ) )
    {
        if( !TranslateAccelerator( g_hWnd, hAccel, &msg ) &&
            !IsDialogMessage( g_hWnd, &msg ) )
        {
            TranslateMessage( &msg );
            DispatchMessage( &msg );
        }
    }

    // Signal the game thread to exit and wait for it
    g_MsgQueue.ForcePush( msg );
    WaitForSingleObject( hThread, INFINITE );

    // Save settings
    config.SaveConfigValues();

    // Clean up
    UnregisterClass( CLASSNAME, wc.hInstance );
    CoUninitialize();
    return 0;
}

DWORD WINAPI GameThread( LPVOID lpParameter )
{
    PFD_StartupLogA( "GameThread entered.\r\n" );
    if ( !g_hWndGfx ) return 0;

    // Initialize Direct3D
    Renderer *pRenderer = new D3D9Renderer();
    HRESULT rendererHr = pRenderer->Init( g_hWndGfx, Config::GetConfig().GetVideoSettings().bLimitFPS );
    if( FAILED( rendererHr ) )
    {
        if ( rendererHr != HRESULT_FROM_WIN32( ERROR_MOD_NOT_FOUND ) &&
             rendererHr != HRESULT_FROM_WIN32( ERROR_PROC_NOT_FOUND ) )
        {
            MessageBoxA( g_hWnd,
                         "Fatal error initializing Direct3D. DirectX 9.0c may be missing, incompatible, or unsupported by the current display driver.",
                         "PianoFromDOS - Direct3D initialization error", MB_OK | MB_ICONEXCLAMATION );
        }
        PostMessage( g_hWnd, WM_QUIT, 1, 0 );
        return 1;
    }

    PFD_StartupLogA( "GameThread renderer initialized.\r\n" );

    // Create the game object
    GameState *pGameState = reinterpret_cast< GameState* >( lpParameter );
    pGameState->SetHWnd( g_hWndGfx );
    pGameState->SetRenderer( pRenderer );
    pGameState->Init();
    PFD_StartupLogA( "GameThread initial state initialized.\r\n" );
    GameState::GameError ge;

    // Event, logic, render...
    MSG msg = { 0 };
    while( msg.message != WM_QUIT )
    {
        while ( g_MsgQueue.Pop( msg ) )
        {
            if ( msg.message == WM_COMMAND && LOWORD( msg.wParam ) == ID_CHANGESTATE )
                PFD_StartupLogA( "GameThread received ID_CHANGESTATE.\r\n" );
            pGameState->MsgProc( msg.hwnd, msg.message, msg.wParam, msg.lParam );
        }

        const bool bChangingState = ( pGameState->NextState() != NULL );
        if ( bChangingState )
            PFD_StartupLogA( "GameThread beginning state change.\r\n" );
        if ( ( ge = GameState::ChangeState( pGameState->NextState(), &pGameState ) ) != GameState::Success )
            PostMessage( g_hWnd, WM_COMMAND, ID_GAMEERROR, ge );
        else if ( bChangingState )
            PFD_StartupLogA( "GameThread state change completed.\r\n" );
        pGameState->Logic();
        pGameState->Render();
    }

    delete pGameState;
    delete pRenderer;

    return 0;
}