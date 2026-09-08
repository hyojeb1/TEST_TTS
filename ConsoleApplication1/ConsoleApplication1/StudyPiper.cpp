#include <windows.h>

#include <chrono>
#include <cstdint>
#include <cstdio>
#include <string>
#include <vector>

#include <piper.h>

//--
#include <iostream>
#include "HyojeHelper.h"

//--


int wmain(int argc, wchar_t* argv[])
{
	::SetConsoleOutputCP(CP_UTF8);
	string text = (argc> 1) ? ToUtf8(argv[1]) : ToUtf8(L"도네이션 감사합니다.");





	return 0;
}