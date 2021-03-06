// GetQrcode.cpp : 定义 DLL 应用程序的导出函数。
//

#include "stdafx.h"
#include <Windows.h>
#include <atlimage.h>
#include <stdio.h>
#include "resource.h"
#include <stdlib.h>
#define HOOK_LEN 5

BYTE backCode[HOOK_LEN] = {0};
HWND hDlg = 0;
DWORD hookData = 0;
DWORD WinAdd = 0;
DWORD K32Add = 0;
DWORD retCallAdd = 0;
DWORD retAdd = 0;
//获取wechatWin模块
DWORD getWechatWin()
{
	return (DWORD)LoadLibrary("WeChatWin.dll");
}

DWORD getKernel32()
{
	return (DWORD)LoadLibrary("Kernel32.dll");
}


//开始hook
/**
 * 参数一 hookAdd 想要hook的地址
 * 参数二 jmpAdd hook完回去的地址
 * 参数三 oldBye 备份被hook地址的那段二进制数据 用于恢复hook
**/
VOID StartHook(DWORD hookAdd, LPVOID jmpAdd)
{
	BYTE JmpCode[HOOK_LEN] = {0};
	//我们需要组成一段这样的数据
	// E9 11051111(这里是跳转的地方这个地方不是一个代码地址 而是根据hook地址和跳转的代码地址的距离计算出来的)
	JmpCode[0] = 0xE9;
	//计算跳转的距离公式是固定的
	//计算公式为 跳转的地址(也就是我们函数的地址) - hook的地址 - hook的字节长度
	*(DWORD *)&JmpCode[1] = (DWORD)jmpAdd - hookAdd - HOOK_LEN;

	//hook第二步 先备份将要被我们覆盖地址的数据 长度为我们hook的长度 HOOK_LEN 5个字节

	//获取进程句柄
	HANDLE hWHND = OpenProcess(PROCESS_ALL_ACCESS, NULL, GetCurrentProcessId());
	
	//备份数据
	if (ReadProcessMemory(hWHND, (LPVOID)hookAdd, backCode, HOOK_LEN, NULL) == 0) {
		MessageBox(NULL,"hook地址的数据读取失败","读取失败",MB_OK);
		return;
	}
	
	//真正的hook开始了 把我们要替换的函数地址写进去 让他直接跳到我们函数里面去然后我们处理完毕后再放行吧！
	if (WriteProcessMemory(hWHND, (LPVOID)hookAdd, JmpCode, HOOK_LEN, NULL) == 0) {
		MessageBox(NULL,"hook写入失败，函数替换失败","错误",MB_OK);
		return;
	}

}


//卸载hook
VOID UnHook(DWORD HookAdd)
{
	HANDLE hWHND = OpenProcess(PROCESS_ALL_ACCESS, NULL, GetCurrentProcessId());
	if (WriteProcessMemory(hWHND, (LPVOID)HookAdd, backCode, HOOK_LEN, NULL) == 0) {
		MessageBox(NULL, "hook卸载失败","失败",MB_OK);
		return;
	}
}

//实时显示图、
VOID ShowPic(DWORD picAdd)
{

	DWORD picLen = picAdd + 0x4;

	char PicData[0xFFF] = {0};
	size_t cpyLen = (size_t)*((LPVOID *)picLen);

	memcpy(PicData, *((LPVOID *)picAdd), cpyLen);
	FILE * pFile;
	fopen_s(&pFile, "qrcode.png", "wb");
	fwrite(PicData, sizeof(char), sizeof(PicData), pFile); 
	fclose(pFile);

	CImage img;
	CRect rect;
	HWND obj = GetDlgItem(hDlg, CODE_PIC);
	GetClientRect(obj, &rect);
	//载入图片
	HRESULT result = img.Load(_T("qrcode.png"));
	img.Draw(GetDC(obj), rect);
}

char testCall[0x100] = {0};
DWORD cEax = 0;
DWORD cEcx = 0;
DWORD cEdx = 0;
DWORD cEbx = 0;
DWORD cEsp = 0;
DWORD cEbp = 0;
DWORD cEsi = 0;
DWORD cEdi = 0;
//跳转过来的函数 我们自己的
VOID __declspec(naked) HookF()
{
	//pushad: 将所有的32位通用寄存器压入堆栈
	//pushfd:然后将32位标志寄存器EFLAGS压入堆栈
	//popad:将所有的32位通用寄存器取出堆栈
	//popfd:将32位标志寄存器EFLAGS取出堆栈
	//先保存寄存器
	// 使用pushad这些来搞还是不太稳定  还是用变量把寄存器的值保存下来 这样可靠点
	__asm {
		mov cEax, eax
		mov cEcx, ecx
		mov cEdx, edx
		mov cEbx, ebx
		mov cEsp, esp
		mov cEbp, ebp
		mov cEsi, esi
		mov cEdi, edi
	}
	//然后跳转到我们自己的处理函数 想干嘛干嘛
	ShowPic(cEcx);
	retCallAdd = WinAdd + 0x470940;
	retAdd = WinAdd + 0x1E104D;
	//然后在还原他进来之前的所有数据
	/*popad
		popfd  不太可靠恢复不全 所以才有变量的方式保存下来再赋值过去*/
	__asm {
		mov eax, cEax
		mov ecx, cEcx
		mov edx, cEdx
		mov ebx, cEbx
		mov esp, cEsp
		mov ebp, cEbp
		mov esi, cEsi
		mov edi, cEdi
		call retCallAdd
		jmp retAdd
	}
}


CHAR* UnicodeToUTF8(const WCHAR* wideStr)
{
	char* utf8Str = NULL;
	int charLen = -1;
	charLen = WideCharToMultiByte(CP_UTF8, 0, wideStr, -1, NULL, 0, NULL, NULL);
	utf8Str = (char*)malloc(charLen);
	WideCharToMultiByte(CP_UTF8, 0, wideStr, -1, utf8Str, charLen, NULL, NULL);
	return utf8Str;
}


//char log[0x6000] = { 0 };
//修改互斥体名称

VOID updateMutexName(DWORD add, DWORD ECXS)
{
	//wchar_t mutexName[0x100] = { L"sdsdsdfds15sdf51sf51d" };
	//LPVOID wAddress = (LPVOID *)add;
	//GetDlgItemText(hDlg, LOG_TEXT, log,sizeof(log));
	char text[0x100] = { 0 };
	wchar_t mutexName[0x100] = { 0};
	swprintf_s(mutexName,L"HEZONE_WECHAT8888%dRAND%d", rand() % 11, rand()%11);
	if (add > 0xFFF && ECXS == 0x0) {
		//_WeChat_App_Instance_Identity_Mutex_Name

		HANDLE hWHND = OpenProcess(PROCESS_ALL_ACCESS, NULL, GetCurrentProcessId());
		if (WriteProcessMemory(hWHND, (LPVOID)add, mutexName, 40, NULL) == 0) {
			MessageBox(NULL, "hook卸载失败", "失败", MB_OK);
			return;
		}

		sprintf_s(text, "mutex地址=%s\n ", UnicodeToUTF8((WCHAR *)add));
		//MessageBox(NULL, text, "aa", MB_OK);
		SetDlgItemText(hDlg, LOG_TEXT, text);
	}
	
	/*::SetDlgItemText(hDlg, SHOW_PIC, text);*/
	
	
	/*sprintf_s(text,"mutex地址=%p",wAddress);
	MessageBox(NULL,text,"aa",MB_OK);*/
	/*HANDLE hHWMD = OpenProcess(PROCESS_ALL_ACCESS, NULL, GetCurrentProcessId());
	if (!WriteProcessMemory(hHWMD, wAddress, mutexName, wcslen(mutexName), NULL)) {
		MessageBox(NULL,"写入失败","aa",MB_OK);
	}*/
}



HMODULE hModule = GetModuleHandle("Kernel32.dll");
LPVOID address = GetProcAddress(hModule, "CreateMutexW");

VOID __declspec(naked) HookOpens()
{
	//pushad: 将所有的32位通用寄存器压入堆栈
	//pushfd:然后将32位标志寄存器EFLAGS压入堆栈
	//popad:将所有的32位通用寄存器取出堆栈
	//popfd:将32位标志寄存器EFLAGS取出堆栈
	//先保存寄存器
	// 使用pushad这些来搞还是不太稳定  还是用变量把寄存器的值保存下来 这样可靠点
	__asm {
		mov cEax, eax
		mov cEcx, ecx
		mov cEdx, edx
		mov cEbx, ebx
		mov cEsp, esp
		mov cEbp, ebp
		mov cEsi, esi
		mov cEdi, edi
	}
	//然后跳转到我们自己的处理函数 想干嘛干嘛
	updateMutexName(cEax, cEcx);
	//retCallAdd = (DWORD)address;
	retAdd = K32Add + 0x2822645;
	//然后在还原他进来之前的所有数据
	/*popad
		popfd  不太可靠恢复不全 所以才有变量的方式保存下来再赋值过去
		
		770E2640 >  8BFF            mov edi,edi
		770E2642    55              push ebp
		770E2643    8BEC            mov ebp,esp
		*/
	__asm {
		mov eax, cEax
		mov ecx, cEcx
		mov edx, cEdx
		mov ebx, cEbx
		mov esp, cEsp
		mov ebp, cEbp
		mov esi, cEsi
		mov edi, cEdi
		mov edi,edi
		push ebp
		mov ebp,esp
		jmp retAdd
	}
}

//
VOID HookWechatQrcode(HWND hwndDlg, DWORD HookAdd)
{
	WinAdd = getWechatWin();
	hDlg = hwndDlg;
	StartHook(HookAdd, &HookF);
}


//双开限制解除
VOID openApps(HWND hwndDlg, DWORD HookAdd)
{
	K32Add = getKernel32();
	char buff[0x100] = {0};
	/*sprintf_s(buff,"模块基址=%p hook的地址=%p",WinAdd, HookAdd);
	MessageBox(NULL,buff,"aa",0);*/
	hDlg = hwndDlg;
	StartHook(HookAdd, &HookOpens);
}