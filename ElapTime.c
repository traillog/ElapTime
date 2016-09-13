//
//  ElapTime.c
//

#include <windows.h>
#include <stdio.h>
#include <process.h>

#define         ID_ELAPTIMELBL          0
#define         ID_RUNCLOCKWND          1
#define         ID_STRPAUBTN            2
#define         ID_RSTBTN               3

#define         ELAPTIMELBL_H           16
#define         ELAPTIMELBL_W           120
#define         RUNCLOCKWND_H           32
#define         RUNCLOCKWND_W           120
#define         BTNS_H                  40
#define         BTNS_W                  120

#define         STATUS_READY            0
#define         STATUS_INICOUNTING      1
#define         STATUS_PAUSED           2
#define         STATUS_RESUMECOUNTING   3

#define         WM_RST_TXT              ( WM_USER + 0 )

// Global vars shared between
// the working thread and the WndProc of the child window
// where the time count is shown
LONG mins, secs, millis;

typedef struct paramsTag
{
     HWND   hClkWnd;
     HANDLE hEvent;
     BOOL   bContinue;
     INT    iStatus;
} PARAMS, *PPARAMS;

LRESULT CALLBACK WndProc( HWND, UINT, WPARAM, LPARAM );
LRESULT CALLBACK WndProcClock( HWND, UINT, WPARAM, LPARAM );

void Thread( PVOID pvoid );

int WINAPI WinMain( HINSTANCE hInstance, HINSTANCE hPrevInstance,
    PSTR szCmdLine, int iCmdShow )
{
    static TCHAR szAppName[] = TEXT( "ElapTime" );
    HWND         hwnd = 0;
    WNDCLASS     wndclass = { 0 };
    MSG          msg = { 0 };

    wndclass.style         = CS_HREDRAW | CS_VREDRAW;
    wndclass.lpfnWndProc   = WndProc;
    wndclass.cbClsExtra    = 0;
    wndclass.cbWndExtra    = 0;
    wndclass.hInstance     = hInstance;
    wndclass.hIcon         = LoadIcon( NULL, IDI_APPLICATION );
    wndclass.hCursor       = LoadCursor( NULL, IDC_ARROW );
    wndclass.hbrBackground = ( HBRUSH )GetStockObject( WHITE_BRUSH );
    wndclass.lpszMenuName  = NULL;
    wndclass.lpszClassName = szAppName;

    if ( !RegisterClass( &wndclass ) )
    {
        MessageBox( NULL, TEXT( "This program requires Windows NT!" ),
            szAppName, MB_ICONERROR) ;
        return 0;
    }

    hwnd = CreateWindow(
        szAppName,
        TEXT( "Elapsed Time" ),
        WS_OVERLAPPED | WS_CAPTION | WS_SYSMENU | WS_MINIMIZEBOX,
        CW_USEDEFAULT, CW_USEDEFAULT,
        350, 210,
        NULL,
        NULL,
        hInstance,
        NULL );

    ShowWindow( hwnd, iCmdShow );
    UpdateWindow( hwnd );

    while ( GetMessage( &msg, NULL, 0, 0 ) )
    {
        TranslateMessage( &msg );
        DispatchMessage( &msg );
    }

    return msg.wParam;
}

LRESULT CALLBACK WndProc( HWND hwnd, UINT message,
    WPARAM wParam, LPARAM lParam )
{
    static PARAMS params;
    static HWND hElapTimeLbl, hRunClockWnd, hStrPauBtn, hRstBtn;
    static int cxClient, cyClient;
    static HBRUSH hBrushStatic;
    WNDCLASS wndclass;

    switch ( message )
    {
    case WM_CREATE :
        // Config and register class for child win for running clock
        wndclass.style         = CS_HREDRAW | CS_VREDRAW;
        wndclass.cbClsExtra    = 0;
        wndclass.cbWndExtra    = 0;
        wndclass.hInstance     = ( ( LPCREATESTRUCT )lParam )->hInstance;
        wndclass.hIcon         = NULL;
        wndclass.hCursor       = LoadCursor( NULL, IDC_ARROW );
        wndclass.hbrBackground = ( HBRUSH )GetStockObject( WHITE_BRUSH );
        wndclass.lpszMenuName  = NULL;
        wndclass.lpfnWndProc   = WndProcClock;
        wndclass.lpszClassName = TEXT( "RunClock" );

        RegisterClass( &wndclass );

        // Create 'Elapsed Time' label
        hElapTimeLbl = CreateWindow(
            TEXT( "static" ), TEXT( "Elapsed Time :" ),
            WS_CHILD | WS_VISIBLE | SS_LEFT,
            0, 0, 0, 0,
            hwnd, ( HMENU )( ID_ELAPTIMELBL ), 
            ( ( LPCREATESTRUCT )lParam )->hInstance, NULL );

        // Create child win for running clock
        hRunClockWnd = CreateWindow(
            TEXT( "RunClock" ), NULL,
            WS_CHILD | WS_BORDER | WS_VISIBLE,
            0, 0, 0, 0, 
            hwnd, ( HMENU )ID_RUNCLOCKWND,
            ( ( LPCREATESTRUCT )lParam )->hInstance, NULL) ;

        // Create 'Start/Pause' button
        hStrPauBtn = CreateWindow(
            TEXT( "button" ), TEXT( "Start" ),
            WS_CHILD | WS_VISIBLE | BS_PUSHBUTTON,
            0, 0, 0, 0,
            hwnd, ( HMENU )ID_STRPAUBTN,
            ( ( LPCREATESTRUCT )lParam )->hInstance, NULL );
        
        // Create 'Reset' button
        hRstBtn = CreateWindow(
            TEXT( "button" ), TEXT( "Reset" ),
            WS_CHILD | WS_VISIBLE | BS_PUSHBUTTON,
            0, 0, 0, 0,
            hwnd, ( HMENU )ID_RSTBTN,
            ( ( LPCREATESTRUCT )lParam )->hInstance, NULL );

        // Create the brush for the label's background
        hBrushStatic = CreateSolidBrush( GetSysColor( COLOR_BTNHIGHLIGHT ) );

        // Set up worker thread's params
        params.hClkWnd = hRunClockWnd;
        params.hEvent = CreateEvent( NULL, FALSE, FALSE, NULL );
        params.bContinue = FALSE;
        params.iStatus = STATUS_READY;

        // Start worker thread (paused)
        _beginthread( Thread, 0, &params );
        return 0;

    case WM_SIZE :
        // Get the size of the client area
        cxClient = LOWORD( lParam );
        cyClient = HIWORD( lParam );

        // Postion 'Elapsed Time' label
        MoveWindow(
            hElapTimeLbl,
            cxClient / 2 - ELAPTIMELBL_W / 2, 45,
            ELAPTIMELBL_W, ELAPTIMELBL_H,
            TRUE );

        // Position 'Running Clock' window
        MoveWindow(
            hRunClockWnd,
            cxClient / 2 - ELAPTIMELBL_W / 2, 65,
            RUNCLOCKWND_W, RUNCLOCKWND_H,
            TRUE );

        // Position 'Start/Pause' button
        MoveWindow(
            hStrPauBtn,
            cxClient / 2 - BTNS_W - 10, cyClient - 60,
            BTNS_W, BTNS_H,
            TRUE );

        // Position 'Reset' button
        MoveWindow(
            hRstBtn,
            cxClient / 2 + 10, cyClient - 60,
            BTNS_W, BTNS_H,
            TRUE );
        return 0;

    case WM_COMMAND :
        if ( LOWORD( wParam ) == ID_STRPAUBTN &&
             HIWORD( wParam ) == BN_CLICKED )
        {
            // Start/Pause button pressed
            if ( params.iStatus == STATUS_READY )
            {
                // From READY to INICOUNTING
                params.iStatus = STATUS_INICOUNTING;
                params.bContinue = TRUE;

                SetEvent( params.hEvent );

                EnableWindow( hRstBtn, FALSE );
                SetWindowText( hStrPauBtn, TEXT( "Pause" ) );
            }
            else if ( params.iStatus == STATUS_INICOUNTING )
            {
                // From INICOUNTING to PAUSED
                params.iStatus = STATUS_PAUSED;
                params.bContinue = FALSE;

                EnableWindow( hRstBtn, TRUE );
                SetWindowText( hStrPauBtn, TEXT( "Resume" ) );
            }
            else if ( params.iStatus == STATUS_PAUSED )
            {
                // From PAUSED to RESUMECOUNTING
                params.iStatus = STATUS_RESUMECOUNTING;
                params.bContinue = TRUE;

                SetEvent( params.hEvent );

                EnableWindow( hRstBtn, FALSE );
                SetWindowText( hStrPauBtn, TEXT( "Pause" ) );
            }
            else if ( params.iStatus == STATUS_RESUMECOUNTING )
            {
                // From RESUMECOUNTING to PAUSE
                params.iStatus = STATUS_PAUSED;
                params.bContinue = FALSE;

                EnableWindow( hRstBtn, TRUE );
                SetWindowText( hStrPauBtn, TEXT( "Resume" ) );
            }
        }

        if ( LOWORD( wParam ) == ID_RSTBTN &&      
             HIWORD( wParam ) == BN_CLICKED )
        {
            // Reset button pressed
            if ( params.iStatus == STATUS_PAUSED )
            {
                // From PAUSED to READY
                params.iStatus = STATUS_READY;
                SetWindowText( hStrPauBtn, TEXT( "Start" ) );

                SendMessage( params.hClkWnd, WM_RST_TXT, 0, 0 );
            }
        }
        return 0;

    case WM_CTLCOLORSTATIC :
        // Set the color of the label's background
        return ( LRESULT )hBrushStatic;

    case WM_SYSCOLORCHANGE :
        // Update hBrushStatic in case the system colors are redefined
        DeleteObject( hBrushStatic );
        hBrushStatic = CreateSolidBrush( GetSysColor( COLOR_BTNHIGHLIGHT ) );
        return 0;

    case WM_DESTROY :
        // Clean up
        params.bContinue = FALSE;
        DeleteObject( hBrushStatic );

        // Close the application
        PostQuitMessage( 0 );
        return 0;
    }

    return DefWindowProc( hwnd, message, wParam, lParam );
}

LRESULT CALLBACK WndProcClock( HWND hwnd, UINT message,
    WPARAM wParam, LPARAM lParam )
{
    HDC hdc;
    RECT rect;
    PAINTSTRUCT ps;
    static TCHAR runClkTxt[ 64 ] = { 0 };

    switch ( message )
    {
    case WM_RST_TXT :
        mins = 0;
        secs = 0;
        millis = 0;

        InvalidateRect( hwnd, NULL, TRUE );
        return 0;

    case WM_PAINT :
        swprintf_s( runClkTxt, _countof( runClkTxt ),
            TEXT( "%03ld:%02ld.%03ld" ), mins, secs, millis );

        hdc = BeginPaint( hwnd, &ps );

        GetClientRect( hwnd, &rect );

        DrawText( hdc, runClkTxt, -1, &rect,
            DT_SINGLELINE | DT_CENTER | DT_VCENTER );

        EndPaint( hwnd, &ps );
        return 0;
    }

    return DefWindowProc( hwnd, message, wParam, lParam );
}

void Thread( PVOID pvoid )
{
    volatile PPARAMS pparams;
    static LONG lIniTime, lLastRead, lTimeDiff;
    static TCHAR runClkTxtThread[ 64 ] = { 0 };
    static LONG resMins, resSecs, resMillis;        // local result vars

    pparams = ( PPARAMS )pvoid;

    while ( TRUE )
    {
        WaitForSingleObject( pparams->hEvent, INFINITE );

        if ( pparams->iStatus == STATUS_INICOUNTING )
            lIniTime = GetCurrentTime();
        else if ( pparams->iStatus == STATUS_RESUMECOUNTING )
            lIniTime += GetCurrentTime() - lLastRead;

        while ( pparams->bContinue )
        {
            // Calculate time difference
            lLastRead = GetCurrentTime();
            lTimeDiff = lLastRead - lIniTime;

            // Calculate results mm:ss.mmm
            resMins = lTimeDiff / 60000;
            resSecs = ( lTimeDiff - ( resMins * 60000 ) ) / 1000;
            resMillis = lTimeDiff % 1000;

            // Assign local result to global vars
            // (intended to make the access to global
            // vars as quick as possible)
            mins = resMins;
            secs = resSecs;
            millis = resMillis;

            // Repaint child window
            InvalidateRect( pparams->hClkWnd, NULL, TRUE );
        }
    }
}