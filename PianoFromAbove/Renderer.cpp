/*************************************************************************************************
*
* File: Renderer.cpp
*
* Description: Implements the rendering objects. Just a wrapper to Direct3D.
*
* Copyright (c) 2010 Brian Pantano. All rights reserved.
*
*************************************************************************************************/
#include "Renderer.h"

#include <string>
#include <vector>

extern void PFD_StartupLogA( const char *text );

namespace
{
    typedef IDirect3D9* ( WINAPI *PFD_Direct3DCreate9Fn )( UINT );
    typedef HRESULT ( WINAPI *PFD_D3DXCreateSpriteFn )( LPDIRECT3DDEVICE9, LPD3DXSPRITE* );
    typedef HRESULT ( WINAPI *PFD_D3DXCreateFontAFn )( LPDIRECT3DDEVICE9, INT, UINT, UINT, UINT, BOOL, DWORD, DWORD, DWORD, DWORD, LPCSTR, LPD3DXFONT* );

    static HMODULE g_hD3D9 = NULL;
    static HMODULE g_hD3DX9 = NULL;
    static PFD_Direct3DCreate9Fn g_pDirect3DCreate9 = NULL;
    static PFD_D3DXCreateSpriteFn g_pD3DXCreateSprite = NULL;
    static PFD_D3DXCreateFontAFn g_pD3DXCreateFontA = NULL;

    static HWND g_hPFDRenderWindow = NULL;

    struct PFD_GDITextCommand
    {
        std::string text;
        Renderer::FontSize font;
        RECT rect;
        DWORD format;
        DWORD color;
    };

    static std::vector< PFD_GDITextCommand > g_vPFDGDIText;
    static HFONT g_hPFDGDIFonts[5] = { NULL, NULL, NULL, NULL, NULL };

    static bool PFD_IsWin9xRenderer()
    {
        return ( GetVersion() & 0x80000000UL ) != 0;
    }

    static HFONT PFD_GetGDIFont( Renderer::FontSize fsFont )
    {
        int i = static_cast< int >( fsFont );
        if ( i < 0 || i >= 5 ) i = static_cast< int >( Renderer::Medium );
        if ( g_hPFDGDIFonts[i] ) return g_hPFDGDIFonts[i];

        int height = 15;
        int weight = FW_NORMAL;
        const char *face = "Tahoma";
        if ( fsFont == Renderer::SmallBold )
            weight = FW_BOLD;
        else if ( fsFont == Renderer::SmallComic )
        {
            height = 20;
            weight = FW_BOLD;
            face = "Comic Sans MS";
        }
        else if ( fsFont == Renderer::Medium )
            height = 25;
        else if ( fsFont == Renderer::Large )
            height = 35;

        g_hPFDGDIFonts[i] = CreateFontA( height, 0, 0, 0, weight, FALSE, FALSE, FALSE,
                                         DEFAULT_CHARSET, OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS,
                                         PROOF_QUALITY, DEFAULT_PITCH | FF_DONTCARE, face );
        if ( !g_hPFDGDIFonts[i] )
            g_hPFDGDIFonts[i] = ( HFONT )GetStockObject( DEFAULT_GUI_FONT );
        return g_hPFDGDIFonts[i];
    }

    static bool PFD_WideToAnsiText( const WCHAR *sText, INT iChars, std::string &out )
    {
        out.clear();
        if ( !sText ) return false;

        int sourceChars = iChars;
        if ( sourceChars < 0 )
        {
            sourceChars = 0;
            while ( sText[sourceChars] ) ++sourceChars;
        }
        if ( sourceChars <= 0 ) return true;

        int bytes = WideCharToMultiByte( CP_ACP, 0, sText, sourceChars, NULL, 0, NULL, NULL );
        if ( bytes > 0 )
        {
            std::vector< char > tmp( bytes );
            int converted = WideCharToMultiByte( CP_ACP, 0, sText, sourceChars, &tmp[0], bytes, NULL, NULL );
            if ( converted > 0 )
            {
                out.assign( &tmp[0], converted );
                return true;
            }
        }

        // Last-resort Win9x conversion.  MIDI/UI strings are overwhelmingly
        // ASCII, so keep the renderer alive even if the system conversion API
        // rejects a character.
        out.reserve( sourceChars );
        for ( int i = 0; i < sourceChars; ++i )
        {
            WCHAR wc = sText[i];
            out.push_back( wc <= 0xFF ? static_cast< char >( wc ) : '?' );
        }
        return true;
    }

    static HRESULT PFD_QueueGDITextA( const CHAR *sText, Renderer::FontSize fsFont,
                                       LPRECT rcPos, DWORD dwFormat, DWORD dwColor, INT iChars )
    {
        if ( !sText || !rcPos ) return E_INVALIDARG;

        int chars = iChars;
        if ( chars < 0 ) chars = lstrlenA( sText );
        if ( chars < 0 ) chars = 0;

        HFONT hFont = PFD_GetGDIFont( fsFont );
        if ( dwFormat & DT_CALCRECT )
        {
            HDC hDC = g_hPFDRenderWindow ? GetDC( g_hPFDRenderWindow ) : GetDC( NULL );
            if ( !hDC ) return E_FAIL;
            int saved = SaveDC( hDC );
            SelectObject( hDC, hFont );
            RECT rc = *rcPos;
            ::DrawTextA( hDC, sText, chars, &rc, dwFormat );
            *rcPos = rc;
            if ( saved ) RestoreDC( hDC, saved );
            if ( g_hPFDRenderWindow ) ReleaseDC( g_hPFDRenderWindow, hDC );
            else ReleaseDC( NULL, hDC );
            return S_OK;
        }

        // Fully transparent D3DX text should remain invisible.
        if ( ( dwColor & 0xFF000000UL ) == 0 ) return S_OK;

        PFD_GDITextCommand cmd;
        cmd.text.assign( sText, chars );
        cmd.font = fsFont;
        cmd.rect = *rcPos;
        cmd.format = dwFormat;
        cmd.color = dwColor;
        g_vPFDGDIText.push_back( cmd );
        return S_OK;
    }

    static void PFD_DrawQueuedGDIText()
    {
        if ( !g_hPFDRenderWindow || g_vPFDGDIText.empty() ) return;

        HDC hDC = GetDC( g_hPFDRenderWindow );
        if ( !hDC ) return;
        int saved = SaveDC( hDC );
        SetBkMode( hDC, TRANSPARENT );

        for ( size_t i = 0; i < g_vPFDGDIText.size(); ++i )
        {
            const PFD_GDITextCommand &cmd = g_vPFDGDIText[i];
            SelectObject( hDC, PFD_GetGDIFont( cmd.font ) );
            SetTextColor( hDC, RGB( ( cmd.color >> 16 ) & 0xFF,
                                    ( cmd.color >> 8 ) & 0xFF,
                                    cmd.color & 0xFF ) );
            RECT rc = cmd.rect;
            ::DrawTextA( hDC, cmd.text.c_str(), static_cast< int >( cmd.text.size() ), &rc, cmd.format );
        }

        if ( saved ) RestoreDC( hDC, saved );
        ReleaseDC( g_hPFDRenderWindow, hDC );
    }

    static HRESULT PFD_LoadDirectXRuntimes( HWND hWnd )
    {
        if ( !g_hD3D9 )
        {
            g_hD3D9 = LoadLibraryA( "d3d9.dll" );
            if ( !g_hD3D9 )
            {
                MessageBoxA( hWnd,
                             "PianoFromDOS could not load D3D9.DLL.\r\n\r\nInstall DirectX 9.0c for Windows 98/ME.",
                             "PianoFromDOS - DirectX 9 required", MB_OK | MB_ICONERROR );
                return HRESULT_FROM_WIN32( ERROR_MOD_NOT_FOUND );
            }
            g_pDirect3DCreate9 = reinterpret_cast< PFD_Direct3DCreate9Fn >( GetProcAddress( g_hD3D9, "Direct3DCreate9" ) );
            if ( !g_pDirect3DCreate9 )
            {
                MessageBoxA( hWnd, "D3D9.DLL does not export Direct3DCreate9.",
                             "PianoFromDOS - incompatible DirectX", MB_OK | MB_ICONERROR );
                return HRESULT_FROM_WIN32( ERROR_PROC_NOT_FOUND );
            }
        }

        // Win95/98/ME use the GDI text path below.  Avoid D3DXFont entirely:
        // ID3DXFont::DrawTextA/W is the call that faults on the tested WinME
        // system (0x80000003/EIP=1).  D3D9 itself remains the renderer.
        if ( PFD_IsWin9xRenderer() )
            return S_OK;

        if ( !g_hD3DX9 )
        {
            g_hD3DX9 = LoadLibraryA( "d3dx9_30.dll" );
            if ( !g_hD3DX9 )
            {
                MessageBoxA( hWnd,
                             "PianoFromDOS could not load D3DX9_30.DLL.\r\n\r\n"
                             "Install the April 2006 DirectX 9.0c D3DX runtime, or place D3DX9_30.DLL next to PianoFromDOS.exe.",
                             "PianoFromDOS - D3DX9_30.DLL required", MB_OK | MB_ICONERROR );
                return HRESULT_FROM_WIN32( ERROR_MOD_NOT_FOUND );
            }
            g_pD3DXCreateSprite = reinterpret_cast< PFD_D3DXCreateSpriteFn >( GetProcAddress( g_hD3DX9, "D3DXCreateSprite" ) );
            g_pD3DXCreateFontA = reinterpret_cast< PFD_D3DXCreateFontAFn >( GetProcAddress( g_hD3DX9, "D3DXCreateFontA" ) );
            if ( !g_pD3DXCreateSprite || !g_pD3DXCreateFontA )
            {
                MessageBoxA( hWnd, "D3DX9_30.DLL is present but does not provide the required D3DX entry points.",
                             "PianoFromDOS - incompatible D3DX", MB_OK | MB_ICONERROR );
                return HRESULT_FROM_WIN32( ERROR_PROC_NOT_FOUND );
            }
        }

        return S_OK;
    }
}

HRESULT Renderer::SetLimitFPS( bool bLimitFPS )
{
    if ( bLimitFPS != m_bLimitFPS )
    {
        m_bLimitFPS = bLimitFPS;
        return ResetDevice();
    }
    return S_OK;
}

D3D9Renderer::~D3D9Renderer()
{
    DestroyDeviceObjects();

    if( m_pTextSprite ) m_pTextSprite->Release();
    if( m_pSmallFont ) m_pSmallFont->Release();
    if( m_pSmallBoldFont ) m_pSmallBoldFont->Release();
    if( m_pSmallComicFont ) m_pSmallComicFont->Release();
    if( m_pMediumFont ) m_pMediumFont->Release();
    if( m_pLargeFont ) m_pLargeFont->Release();
    
    if( m_pd3dDevice ) m_pd3dDevice->Release();

    if( m_pD3D ) m_pD3D->Release();
}

void D3D9Renderer::DestroyDeviceObjects()
{
    if ( m_pTextSprite ) m_pTextSprite->OnLostDevice();
    if ( m_pSmallFont ) m_pSmallFont->OnLostDevice();
    if ( m_pSmallBoldFont ) m_pSmallBoldFont->OnLostDevice();
    if ( m_pSmallComicFont ) m_pSmallComicFont->OnLostDevice();
    if ( m_pMediumFont ) m_pMediumFont->OnLostDevice();
    if ( m_pLargeFont ) m_pLargeFont->OnLostDevice();

    if( m_pVertexBuffer ) m_pVertexBuffer->Release();
    if( m_pStaticVertexBuffer ) ReleaseStaticBuffer();

    m_bIsDeviceValid = false;
}

HRESULT D3D9Renderer::Init( HWND hWnd, bool bLimitFPS )
{
    HRESULT hr;

    if ( FAILED( hr = PFD_LoadDirectXRuntimes( hWnd ) ) )
        return hr;

    // Create the D3D object.
    if( NULL == ( m_pD3D = g_pDirect3DCreate9( D3D_SDK_VERSION ) ) )
        return E_FAIL;

    // Set up the structure used to create the D3DDevice
    ZeroMemory( &m_d3dPP, sizeof( D3DPRESENT_PARAMETERS ) );
    m_d3dPP.Windowed = TRUE;
    m_d3dPP.SwapEffect = D3DSWAPEFFECT_DISCARD;
    m_d3dPP.BackBufferFormat = D3DFMT_UNKNOWN;
    m_d3dPP.BackBufferWidth = 0;
    m_d3dPP.BackBufferHeight = 0;
    m_d3dPP.PresentationInterval = ( bLimitFPS ? D3DPRESENT_INTERVAL_ONE : D3DPRESENT_INTERVAL_IMMEDIATE );
    
    // Create the D3DDevice
    if( FAILED( hr = m_pD3D->CreateDevice( D3DADAPTER_DEFAULT, D3DDEVTYPE_HAL, hWnd, //D3DDEVTYPE_REF
                                           D3DCREATE_SOFTWARE_VERTEXPROCESSING,
                                           &m_d3dPP, &m_pd3dDevice ) ) )
        return hr;

    g_hPFDRenderWindow = hWnd;

    if ( !PFD_IsWin9xRenderer() )
    {
        if( FAILED( hr = g_pD3DXCreateSprite( m_pd3dDevice, &m_pTextSprite ) ) )
            return hr;

        if( FAILED( hr = g_pD3DXCreateFontA( m_pd3dDevice, 15, 0, FW_NORMAL, 1, FALSE, DEFAULT_CHARSET,
                                          OUT_DEFAULT_PRECIS, PROOF_QUALITY, DEFAULT_PITCH | FF_DONTCARE,
                                          "Tahoma", &m_pSmallFont ) ) )
            return hr;

        if( FAILED( hr = g_pD3DXCreateFontA( m_pd3dDevice, 15, 0, FW_BOLD, 1, FALSE, DEFAULT_CHARSET,
                                          OUT_DEFAULT_PRECIS, PROOF_QUALITY, DEFAULT_PITCH | FF_DONTCARE,
                                          "Tahoma", &m_pSmallBoldFont ) ) )
            return hr;

        if( FAILED( hr = g_pD3DXCreateFontA( m_pd3dDevice, 20, 0, FW_BOLD, 1, FALSE, DEFAULT_CHARSET,
                                          OUT_DEFAULT_PRECIS, PROOF_QUALITY, DEFAULT_PITCH | FF_DONTCARE,
                                          "Comic Sans MS", &m_pSmallComicFont ) ) )
            return hr;

        if( FAILED( hr = g_pD3DXCreateFontA( m_pd3dDevice, 25, 0, FW_NORMAL, 1, FALSE, DEFAULT_CHARSET,
                                          OUT_DEFAULT_PRECIS, PROOF_QUALITY, DEFAULT_PITCH | FF_DONTCARE,
                                          "Tahoma", &m_pMediumFont ) ) )
            return hr;

        if( FAILED( hr = g_pD3DXCreateFontA( m_pd3dDevice, 35, 0, FW_NORMAL, 1, FALSE, DEFAULT_CHARSET,
                                          OUT_DEFAULT_PRECIS, PROOF_QUALITY, DEFAULT_PITCH | FF_DONTCARE,
                                          "Tahoma", &m_pLargeFont ) ) )
            return hr;
    }
    else
    {
        PFD_StartupLogA( "Renderer: Win9x GDI text overlay enabled; D3DXFont is bypassed.\r\n" );
    }

    if ( FAILED( hr = RestoreDeviceObjects() ) )
        return hr;

    m_iBufferWidth = m_d3dPP.BackBufferWidth;
    m_iBufferHeight = m_d3dPP.BackBufferHeight;
    m_bLimitFPS = bLimitFPS;
    m_bIsDeviceValid = true;

    return S_OK;
}

HRESULT D3D9Renderer::RestoreDeviceObjects()
{
    HRESULT hr;

    if ( FAILED( hr = m_pd3dDevice->CreateVertexBuffer( VertexBufferSize,
                                                        D3DUSAGE_DYNAMIC | D3DUSAGE_WRITEONLY, SCREEN_VERTEX::FVF,
                                                        D3DPOOL_DEFAULT, &m_pVertexBuffer, NULL) ) )
        return hr;
    m_iTriangle = 0;

    if ( m_pTextSprite ) m_pTextSprite->OnResetDevice();
    if ( m_pSmallFont ) m_pSmallFont->OnResetDevice();
    if ( m_pSmallBoldFont ) m_pSmallBoldFont->OnResetDevice();
    if ( m_pSmallComicFont ) m_pSmallComicFont->OnResetDevice();
    if ( m_pMediumFont ) m_pMediumFont->OnResetDevice();
    if ( m_pLargeFont ) m_pLargeFont->OnResetDevice();

    m_pd3dDevice->SetRenderState( D3DRS_ALPHABLENDENABLE, TRUE );
    m_pd3dDevice->SetRenderState( D3DRS_SRCBLEND, D3DBLEND_INVSRCALPHA );
    m_pd3dDevice->SetRenderState( D3DRS_DESTBLEND, D3DBLEND_SRCALPHA );

    m_pd3dDevice->SetFVF( SCREEN_VERTEX::FVF );

    return S_OK;
}

HRESULT D3D9Renderer::ResetDeviceIfNeeded()
{
    if ( !m_bIsDeviceValid )
    {
        HRESULT hr = m_pd3dDevice->TestCooperativeLevel();
        if ( hr == D3DERR_DEVICENOTRESET )
            hr = ResetDevice();
        if ( FAILED( hr ) )
            return hr;
    }
    return S_OK;
}

HRESULT D3D9Renderer::ResetDevice()
{
    HRESULT hr;
    
    // Destroy the objects and reinitialize
    if ( m_bIsDeviceValid )
        DestroyDeviceObjects();

    // Reset the device
    m_d3dPP.BackBufferHeight = 0;
    m_d3dPP.BackBufferWidth = 0;
    m_d3dPP.PresentationInterval = ( m_bLimitFPS ? D3DPRESENT_INTERVAL_ONE : D3DPRESENT_INTERVAL_IMMEDIATE );
    if ( FAILED( hr = m_pd3dDevice->Reset( &m_d3dPP ) ) )
        return hr;

    // Restore the device objects
    if ( FAILED( hr = RestoreDeviceObjects() ) )
        return hr;

    m_iBufferWidth = m_d3dPP.BackBufferWidth;
    m_iBufferHeight = m_d3dPP.BackBufferHeight;
    m_bIsDeviceValid = true;
    return S_OK;
}

HRESULT D3D9Renderer::Clear( DWORD color )
{
    return m_pd3dDevice->Clear( 0, NULL, D3DCLEAR_TARGET, color, 1.0f, 0 );
}

HRESULT D3D9Renderer::BeginScene()
{
    if ( PFD_IsWin9xRenderer() ) g_vPFDGDIText.clear();
    return m_pd3dDevice->BeginScene();
}

HRESULT D3D9Renderer::EndScene()
{
    FlushBuffer();
    return m_pd3dDevice->EndScene();
}

HRESULT D3D9Renderer::BeginText()
{
    FlushBuffer();
    if ( PFD_IsWin9xRenderer() ) return S_OK;
    return m_pTextSprite->Begin( D3DXSPRITE_ALPHABLEND | D3DXSPRITE_SORT_TEXTURE );
}

HRESULT D3D9Renderer::DrawTextW( const WCHAR *sText, FontSize fsFont, LPRECT rcPos, DWORD dwFormat, DWORD dwColor, INT iChars )
{
    if ( PFD_IsWin9xRenderer() )
    {
        std::string ansi;
        if ( !PFD_WideToAnsiText( sText, iChars, ansi ) ) return E_FAIL;
        return PFD_QueueGDITextA( ansi.c_str(), fsFont, rcPos, dwFormat, dwColor,
                                  static_cast< INT >( ansi.size() ) );
    }

    LPD3DXFONT pFont = ( fsFont == Small ? m_pSmallFont :
                         fsFont == SmallBold ? m_pSmallBoldFont :
                         fsFont == SmallComic ? m_pSmallComicFont :
                         fsFont == Medium ? m_pMediumFont :
                         fsFont == Large ? m_pLargeFont : m_pMediumFont );

    if ( !pFont || !pFont->DrawTextW( m_pTextSprite, sText, iChars, rcPos, dwFormat, D3DXCOLOR( dwColor ) ) )
        return E_FAIL;
    return S_OK;
}

HRESULT D3D9Renderer::DrawTextA( const CHAR *sText, FontSize fsFont, LPRECT rcPos, DWORD dwFormat, DWORD dwColor, INT iChars )
{
    if ( PFD_IsWin9xRenderer() )
        return PFD_QueueGDITextA( sText, fsFont, rcPos, dwFormat, dwColor, iChars );

    LPD3DXFONT pFont = ( fsFont == Small ? m_pSmallFont :
                         fsFont == SmallBold ? m_pSmallBoldFont :
                         fsFont == SmallComic ? m_pSmallComicFont :
                         fsFont == Medium ? m_pMediumFont :
                         fsFont == Large ? m_pLargeFont : m_pMediumFont );

    if ( !pFont || !pFont->DrawTextA( m_pTextSprite, sText, iChars, rcPos, dwFormat, D3DXCOLOR( dwColor ) ) )
        return E_FAIL;
    return S_OK;
}

HRESULT D3D9Renderer::EndText()
{
    if ( PFD_IsWin9xRenderer() ) return S_OK;
    return m_pTextSprite->End();
}

HRESULT D3D9Renderer::Present()
{
    HRESULT hr = m_pd3dDevice->Present(NULL, NULL, NULL, NULL);
    if ( hr == D3DERR_DEVICELOST )
        DestroyDeviceObjects();
    else if ( SUCCEEDED( hr ) && PFD_IsWin9xRenderer() )
        PFD_DrawQueuedGDIText();
    return hr;
}

HRESULT D3D9Renderer::DrawRect( float x, float y, float cx, float cy, DWORD color )
{
    return DrawRect( x, y, cx, cy, color, color, color, color );
}

HRESULT D3D9Renderer::DrawRect( float x, float y, float cx, float cy,
                                DWORD c1, DWORD c2, DWORD c3, DWORD c4 )
{
    x -= 0.5f;
    y -= 0.5f;

    SCREEN_VERTEX vertices[6] =
    {
        x,  y,            0.5f, 1.0f, c1,
        x + cx, y,        0.5f, 1.0f, c2,
        x + cx, y + cy,   0.5f, 1.0f, c3,
        x,  y,            0.5f, 1.0f, c1,
        x + cx, y + cy,   0.5f, 1.0f, c3,
        x,  y + cy,       0.5f, 1.0f, c4
    };

    return Blit( vertices, 2 );
}

HRESULT D3D9Renderer::DrawSkew( float x1, float y1, float x2, float y2, float x3, float y3, float x4, float y4, DWORD color )
{
    return DrawSkew( x1, y1, x2, y2, x3, y3, x4, y4, color, color, color, color );
}

HRESULT D3D9Renderer::DrawSkew( float x1, float y1, float x2, float y2, float x3, float y3, float x4, float y4,
                                DWORD c1, DWORD c2, DWORD c3, DWORD c4 )
{
    SCREEN_VERTEX vertices[6] =
    {
        x1 - 0.5f, y1 - 0.5f, 0.5f, 1.0f, c1,
        x2 - 0.5f, y2 - 0.5f, 0.5f, 1.0f, c2,
        x3 - 0.5f, y3 - 0.5f, 0.5f, 1.0f, c3,
        x1 - 0.5f, y1 - 0.5f, 0.5f, 1.0f, c1,
        x3 - 0.5f, y3 - 0.5f, 0.5f, 1.0f, c3,
        x4 - 0.5f, y4 - 0.5f, 0.5f, 1.0f, c4
    };

    return Blit( vertices, 2 );
}

HRESULT D3D9Renderer::Blit( SCREEN_VERTEX *vertices, int iTriangles )
{
    if ( m_bStatic )
    {
        memcpy( m_pStaticVertexData + m_iStaticTriangle * 3 * sizeof( SCREEN_VERTEX ), vertices, iTriangles * 3 * sizeof( SCREEN_VERTEX ) );
        m_iStaticTriangle += 2;
    }
    else
    {
        PrepBuffer( iTriangles );
        memcpy( m_pVertexData + m_iTriangle * 3 * sizeof( SCREEN_VERTEX ), vertices, iTriangles * 3 * sizeof( SCREEN_VERTEX ) );
        m_iTriangle += 2;
    }
    return S_OK;
}

HRESULT D3D9Renderer::PrepBuffer( int iTriangles )
{
    if ( m_iTriangle > MaxTriangles )
        return E_FAIL;
    if ( m_iTriangle == 0 )
        return m_pVertexBuffer->Lock( 0, 0, reinterpret_cast< void** >( &m_pVertexData ), D3DLOCK_DISCARD );
    if ( m_iTriangle + iTriangles <= MaxTriangles )
        return S_OK;

    FlushBuffer();
    return m_pVertexBuffer->Lock( 0, 0, reinterpret_cast< void** >( &m_pVertexData ), D3DLOCK_DISCARD );
}

HRESULT D3D9Renderer::FlushBuffer()
{
    if ( m_iTriangle == 0 )
        return S_OK;

    m_pVertexBuffer->Unlock();
    m_pd3dDevice->SetStreamSource( 0, m_pVertexBuffer, 0, sizeof( SCREEN_VERTEX ) );
    HRESULT hr = m_pd3dDevice->DrawPrimitive( D3DPT_TRIANGLELIST, 0, m_iTriangle );
    m_iTriangle = 0;
    return hr;
}

HRESULT D3D9Renderer::BeginStaticBuffer( int iTriangles )
{
    HRESULT hr;

    if (iTriangles > m_iStaticMaxTriangles)
    {
        ReleaseStaticBuffer();
        if ( FAILED( hr = m_pd3dDevice->CreateVertexBuffer( sizeof( SCREEN_VERTEX ) * 3 * iTriangles,
                                                            D3DUSAGE_WRITEONLY, SCREEN_VERTEX::FVF,
                                                            D3DPOOL_DEFAULT, &m_pStaticVertexBuffer, NULL) ) )
            return hr;
    }

    if ( FAILED( hr = m_pStaticVertexBuffer->Lock( 0, 0, reinterpret_cast< void** >( &m_pStaticVertexData ), 0 ) ) )
        return hr;

    m_bStatic = true;
    m_iStaticTriangle = 0;
    m_iStaticMaxTriangles = iTriangles;
    return S_OK;
}

HRESULT D3D9Renderer::EndStaticBuffer()
{
    m_bStatic = false;
    return m_pStaticVertexBuffer->Unlock();
}

HRESULT D3D9Renderer::DrawStaticBuffer() {
    if ( m_iStaticTriangle == 0 )
        return S_OK;

    FlushBuffer();
    m_pd3dDevice->SetStreamSource( 0, m_pStaticVertexBuffer, 0, sizeof( SCREEN_VERTEX ) );
    return m_pd3dDevice->DrawPrimitive( D3DPT_TRIANGLELIST, 0, m_iStaticTriangle );
}

HRESULT D3D9Renderer::ReleaseStaticBuffer()
{
    m_bStatic = false;
    m_iStaticTriangle = m_iStaticMaxTriangles = 0;
    return m_pStaticVertexBuffer->Release();
}