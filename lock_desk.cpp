﻿// lock_desk.cpp : 定义应用程序的入口点。
//

#include "framework.h"
#include "lock_desk.h"
#include<opencv2/imgproc/types_c.h>
#include <opencv2/opencv.hpp>
#include <winsock2.h>
#include <vector>
#include <stdio.h>
#include<string>
#include <stdlib.h>
#include <iostream>
#include <k4a/k4a.h>
#include "k4a_grabber.h"
#include<ctime>
#include <fstream>
#include "flask.h"


#define MAX_LOADSTRING 100
#define TIMER_SEC 22
#define BUTTON_START 33
#define BUTTON_STOP 44

// 全局变量:
HINSTANCE hInst;                                // 当前实例
WCHAR szTitle[MAX_LOADSTRING];                  // 标题栏文本
WCHAR szWindowClass[MAX_LOADSTRING];            // 主窗口类名
TEXTMETRIC tm;
//cv::Mat img_global = cv::imread("D:\\Downloads\\cityscapes\\leftImg8bit\\val\\frankfurt\\frankfurt_000000_000294_leftImg8bit.png");
cv::Mat RGB;
cv::Mat depth;
//相机内参畸变系数所在文件
string caliberation_camera_file = "caliberation_camera.xml";
//外参所在文件
string Homo_cam2base_file = "Homo_cam2base.xml";

//初始化kinect相机,里面有各种相机相关的参数和函数
k4a::KinectAPI kinect(caliberation_camera_file, true);
//原始的深度图，深度图的伪彩色图，红外线图，红外线伪彩色图
cv::Mat depthMatOld, colorMatOld, depthcolorMatOld, irMatOld, ircolorMatOld;
//通过畸变系数校正过后的图片
cv::Mat depthMat, colorMat, depthcolorMat, irMat, ircolorMat;
vector<cv::Mat> masks;
vector<int> classes;
int lock_classes = -1;

// 此代码模块中包含的函数的前向声明:
ATOM                MyRegisterClass(HINSTANCE hInstance);
BOOL                InitInstance(HINSTANCE, int);
LRESULT CALLBACK    WndProc(HWND, UINT, WPARAM, LPARAM);
INT_PTR CALLBACK    About(HWND, UINT, WPARAM, LPARAM);
auto ConvertCVMatToBMP(cv::Mat frame)->HBITMAP;
void Thread(PVOID pvoid);

//void WinShowMatImage(const cv::Mat& img, HDC hdc, const RECT& rect);
int APIENTRY wWinMain(_In_ HINSTANCE hInstance,
                     _In_opt_ HINSTANCE hPrevInstance,
                     _In_ LPWSTR    lpCmdLine,
                     _In_ int       nCmdShow)
{
    UNREFERENCED_PARAMETER(hPrevInstance);
    UNREFERENCED_PARAMETER(lpCmdLine);

    // TODO: 在此处放置代码。

    // 初始化全局字符串
    LoadStringW(hInstance, IDS_APP_TITLE, szTitle, MAX_LOADSTRING);
    LoadStringW(hInstance, IDC_LOCKDESK, szWindowClass, MAX_LOADSTRING);
    MyRegisterClass(hInstance);

    // 执行应用程序初始化:
    if (!InitInstance (hInstance, nCmdShow))
    {
        return FALSE;
    }

    HACCEL hAccelTable = LoadAccelerators(hInstance, MAKEINTRESOURCE(IDC_LOCKDESK));

    MSG msg;

    // 主消息循环:
    while (GetMessage(&msg, nullptr, 0, 0))
    {
        //if (!TranslateAccelerator(msg.hwnd, hAccelTable, &msg))
        {
            TranslateMessage(&msg);
            DispatchMessage(&msg);
        }
    }

    return (int) msg.wParam;
}



//
//  函数: MyRegisterClass()
//
//  目标: 注册窗口类。
//
ATOM MyRegisterClass(HINSTANCE hInstance)
{
    WNDCLASSEXW wcex;

    wcex.cbSize = sizeof(WNDCLASSEX);

    wcex.style          = CS_HREDRAW | CS_VREDRAW;
    wcex.lpfnWndProc    = WndProc;
    wcex.cbClsExtra     = 0;
    wcex.cbWndExtra     = 0;
    wcex.hInstance      = hInstance;
    wcex.hIcon          = LoadIcon(hInstance, MAKEINTRESOURCE(IDI_LOCKDESK));
    wcex.hCursor        = LoadCursor(nullptr, IDC_ARROW);
    wcex.hbrBackground  = (HBRUSH)(COLOR_WINDOW+1);
    wcex.lpszMenuName   = MAKEINTRESOURCEW(IDC_LOCKDESK);
    wcex.lpszClassName  = szWindowClass;
    wcex.hIconSm        = LoadIcon(wcex.hInstance, MAKEINTRESOURCE(IDI_SMALL));

    return RegisterClassExW(&wcex);
}

//
//   函数: InitInstance(HINSTANCE, int)
//
//   目标: 保存实例句柄并创建主窗口
//
//   注释:
//
//        在此函数中，我们在全局变量中保存实例句柄并
//        创建和显示主程序窗口。
//
BOOL InitInstance(HINSTANCE hInstance, int nCmdShow)
{
   hInst = hInstance; // 将实例句柄存储在全局变量中

   HWND hWnd = CreateWindowW(szWindowClass, szTitle, WS_OVERLAPPEDWINDOW,
      CW_USEDEFAULT, 0, CW_USEDEFAULT, 0, nullptr, nullptr, hInstance, nullptr);
   SetTimer(hWnd, TIMER_SEC, 1000, NULL);

   if (!hWnd)
   {
      return FALSE;
   }

   ShowWindow(hWnd, SW_SHOWMAXIMIZED);
   UpdateWindow(hWnd);

   return TRUE;
}

//
//  函数: WndProc(HWND, UINT, WPARAM, LPARAM)
//
//  目标: 处理主窗口的消息。
//
//  WM_COMMAND  - 处理应用程序菜单
//  WM_PAINT    - 绘制主窗口
//  WM_DESTROY  - 发送退出消息并返回
//
//
int cnt = 1;
bool start = FALSE;
bool stop = FALSE;
CRITICAL_SECTION cs;
LRESULT CALLBACK WndProc(HWND hWnd, UINT message, WPARAM wParam, LPARAM lParam)
{
	static int cxChar, cyChar, cxCaps;
    static int cxClient, cyClient;
    static HWND hwndButton_start;
    static HWND hwndButton_stop;
    static HDC hdcMem_RGB, hdcMem_depth;  
    static HBITMAP hbmp_RGB, hbmp_depth;   //一张位图的句柄  
    static BITMAP bmp_RGB, bmp_depth; 
	static cv::Mat img_RGB, img_depth;
	static int scale = 2;
    static HDC hdc;
	static PAINTSTRUCT ps;
	static TCHAR szBuffer[40];
    static int yd = 300, xd = 200;
    switch (message)
    {

    case WM_CREATE:
		{
        //InitializeCriticalSection(&cs);
        //_beginthread(Thread, 0, NULL);
		    kinect.GetOpenCVImage(RGB, depthMatOld, depth, irMat, TRUE);
			hdc = GetDC(hWnd);
			GetTextMetrics(hdc, &tm);
            cxChar = tm.tmAveCharWidth;
            cyChar = tm.tmHeight + tm.tmExternalLeading;
            cxCaps = (tm.tmPitchAndFamily & 1 ? 3 : 2)* cxChar / 2;
			ReleaseDC(hWnd, hdc);
            hwndButton_start = CreateWindow(TEXT("button"),
                TEXT("开始"),
                WS_CHILD | WS_VISIBLE | BS_PUSHBUTTON,
                700+xd, 100+yd,
                20 * cxChar, 7 * cyChar / 4,
                hWnd, (HMENU)BUTTON_START,
                ((LPCREATESTRUCT)lParam)->hInstance, NULL);
            hwndButton_stop = CreateWindow(TEXT("button"),
                TEXT("暂停"),
                WS_CHILD | WS_VISIBLE | BS_PUSHBUTTON,
                900+xd, 100+yd,
                20 * cxChar, 7 * cyChar / 4,
                hWnd, (HMENU)BUTTON_STOP,
                ((LPCREATESTRUCT)lParam)->hInstance, NULL);
		}
    case WM_SIZE:
		{
			cxClient = LOWORD(lParam);
			cyClient = HIWORD(lParam);
		}
    case WM_COMMAND:
        {
			hdc = GetDC(hWnd);
            int wmId = LOWORD(wParam);
            // 分析菜单选择:
            switch (wmId)
            {
            case BUTTON_START:
                start = TRUE;
                stop = false;
				//TextOut(hdc, 800, 0, szBuffer, wsprintf(szBuffer, TEXT("按下了start          ")));
                break; 
            case BUTTON_STOP:
                stop = TRUE;

				//TextOut(hdc, 800, 0, szBuffer, wsprintf(szBuffer, TEXT("按下了stop       ")));
                break;
            case IDM_ABOUT:
                DialogBox(hInst, MAKEINTRESOURCE(IDD_ABOUTBOX), hWnd, About);
                break;
            case IDM_EXIT:
                KillTimer(hWnd, 22);
                DestroyWindow(hWnd);
                break;
            default:
                return DefWindowProc(hWnd, message, wParam, lParam);
            }
        }
        break;
    case WM_PAINT:
        {
            hdc = BeginPaint(hWnd, &ps);
            // TODO: 在此处添加使用 hdc 的任何绘图代码...
		  //定义字体的属性
		  LOGFONT fontRect;
		  memset(&fontRect,0,sizeof(LOGFONT));

		  fontRect.lfHeight=-50;  //字体的高度
		  fontRect.lfWeight=FW_HEAVY;//字体的粗细

		  auto hFont=CreateFontIndirect(&fontRect); //创建字体
		  auto  hOld=SelectObject(hdc,hFont);//引用上面的字体 
          if (start == false || stop == true)
		  {
              if(start ==false)
		    kinect.GetOpenCVImage(RGB, depthMatOld, depth, irMat, TRUE);
				TextOut(hdc, 750+xd, 20+yd, szBuffer, wsprintf(szBuffer, TEXT("请点击开始   "), lock_classes));
		  }
          else
          {
		    kinect.GetOpenCVImage(RGB, depthMatOld, depth, irMat, TRUE);
			getMasks(RGB, masks, classes);
            if (classes.size() == 0)
                lock_classes = -1;
            else lock_classes = classes[0];
            if (lock_classes != -1)
            {
                for (int h = 0; h < masks[0].rows; h++)
                    for (int w = 0; w < masks[0].cols; w++)
                        if (masks[0].at<UINT8>(h, w) == 255)
                            RGB.at<cv::Vec4b>(h, w) = cv::Vec4b(0, 0, 255, 0);
                  TextOut(hdc, 750+xd, 20+yd, szBuffer, wsprintf(szBuffer, TEXT("锁扣类别:%i   "), lock_classes));
              }
              else
                  TextOut(hdc, 750+xd, 20+yd, szBuffer, wsprintf(szBuffer, TEXT("      无           ")));
          }


			hdcMem_RGB  = CreateCompatibleDC(hdc); 
			hdcMem_depth  = CreateCompatibleDC(hdc); 

            //EnterCriticalSection(&cs);
            cv::resize(RGB, img_RGB, cv::Size(RGB.cols / scale, RGB.rows / scale));
            cv::resize(depth, img_depth, cv::Size(depth.cols / scale, depth.rows / scale));
            //LeaveCriticalSection(&cs);
            hbmp_RGB = ConvertCVMatToBMP(img_RGB);
            hbmp_depth = ConvertCVMatToBMP(img_depth);
			GetObject(hbmp_RGB, sizeof(BITMAP), &bmp_RGB);  //得到一个位图对象  
			GetObject(hbmp_depth, sizeof(BITMAP), &bmp_depth);  //得到一个位图对象  

			SelectObject(hdcMem_RGB, hbmp_RGB);  
			SelectObject(hdcMem_depth, hbmp_depth);  
			BitBlt(hdc, xd, 50, bmp_RGB.bmWidth, bmp_RGB.bmHeight, hdcMem_RGB, 0, 0, SRCCOPY);        //显示位图  
			BitBlt(hdc, xd, bmp_RGB.bmHeight+70, bmp_depth.bmWidth, bmp_depth.bmHeight, hdcMem_depth, 0, 0, SRCCOPY);        //显示位图  

			DeleteDC(hdcMem_RGB);  
			DeleteObject(hbmp_RGB);  
			DeleteDC(hdcMem_depth);  
			DeleteObject(hbmp_depth);  
            EndPaint(hWnd, &ps);
        }
        break;
    case WM_TIMER:
        switch (wParam)
        {
        case TIMER_SEC:
            if (start == FALSE || stop == TRUE)
                break;
            //EnterCriticalSection(&cs);
			InvalidateRect(hWnd,NULL,FALSE);
           //LeaveCriticalSection(&cs);
            break;
        default:
            break;
        }
        break;
    case WM_DESTROY:
        PostQuitMessage(0);
        break;
    default:
        return DefWindowProc(hWnd, message, wParam, lParam);
    }
    return 0;
}

// “关于”框的消息处理程序。
INT_PTR CALLBACK About(HWND hDlg, UINT message, WPARAM wParam, LPARAM lParam)
{
    UNREFERENCED_PARAMETER(lParam);
    switch (message)
    {
    case WM_INITDIALOG:
        return (INT_PTR)TRUE;

    case WM_COMMAND:
        if (LOWORD(wParam) == IDOK || LOWORD(wParam) == IDCANCEL)
        {
            EndDialog(hDlg, LOWORD(wParam));
            return (INT_PTR)TRUE;
        }
        break;
    }
    return (INT_PTR)FALSE;
}

void Thread(PVOID pvoid)
{
    while (TRUE)
    {
        if (start)
        {
            EnterCriticalSection(&cs);
		    kinect.GetOpenCVImage(RGB, depthMatOld, depth, irMat, TRUE);
			getMasks(RGB, masks, classes);
            if (classes.size() == 0)
                lock_classes = -1;
            else lock_classes = classes[0];
            LeaveCriticalSection(&cs);
        }
		Sleep(1000);
    }

}
auto ConvertCVMatToBMP (cv::Mat frame) -> HBITMAP
{      
    auto convertOpenCVBitDepthToBits = [](const int32_t value)
    {
        auto regular = 0u;
    
        switch (value)
        {
        case CV_8U:
        case CV_8S:
            regular = 8u;
            break;

        case CV_16U:
        case CV_16S:
            regular = 16u;
            break;

        case CV_32S:
        case CV_32F:
            regular = 32u;
            break;

        case CV_64F:
            regular = 64u;
            break;

        default:
            regular = 0u;
            break;
        }

        return regular;
    };

    auto imageSize = frame.size();
    assert(imageSize.width && "invalid size provided by frame");
    assert(imageSize.height && "invalid size provided by frame");
    
    if (imageSize.width && imageSize.height)
    {
        auto headerInfo = BITMAPINFOHEADER{};
        ZeroMemory(&headerInfo, sizeof(headerInfo));
        
        headerInfo.biSize     = sizeof(headerInfo);
        headerInfo.biWidth    = imageSize.width;
        headerInfo.biHeight   = -(imageSize.height); // negative otherwise it will be upsidedown
        headerInfo.biPlanes   = 1;// must be set to 1 as per documentation frame.channels();
        
        const auto bits       = convertOpenCVBitDepthToBits( frame.depth() );
        headerInfo.biBitCount = frame.channels() * bits;

        auto bitmapInfo = BITMAPINFO{};
        ZeroMemory(&bitmapInfo, sizeof(bitmapInfo));
        
        bitmapInfo.bmiHeader              = headerInfo;
        bitmapInfo.bmiColors->rgbBlue     = 0;
        bitmapInfo.bmiColors->rgbGreen    = 0;
        bitmapInfo.bmiColors->rgbRed      = 0;
        bitmapInfo.bmiColors->rgbReserved = 0;
        
        auto dc  = GetDC(nullptr);
        assert(dc != nullptr && "Failure to get DC");
        auto bmp = CreateDIBitmap(dc,
                                  &headerInfo,
                                  CBM_INIT,
                                  frame.data,
                                  &bitmapInfo,
                                  DIB_RGB_COLORS);
        assert(bmp != nullptr && "Failure creating bitmap from captured frame");
        
        return bmp;
    }
    else
    {
    	return nullptr;
    }
}
