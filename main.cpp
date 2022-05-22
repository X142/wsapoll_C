#include "header.h"

extern int main_2();

int main()
{
#ifdef _DEBUG
	// メモリリーク検出用
	::_CrtSetDbgFlag(_CRTDBG_LEAK_CHECK_DF | _CRTDBG_ALLOC_MEM_DF);
	// レポートの出力先の指定
	::_CrtSetReportMode(_CRT_WARN, _CRTDBG_MODE_DEBUG);
#endif

	{
		WSADATA d;
		if (WSAStartup(MAKEWORD(2, 2), &d) != 0)
		{
			std::cerr << "!!! WSAStartup() に失敗しました。";
			return -1;
		}
	}

	int ret_val;
	try
	{
		ret_val = main_2();
	}
	catch (const char* const perr_msg)
	{
		WSACleanup();

		std::cout << perr_msg << std::endl;
		return -1;
	}

	WSACleanup();
	return ret_val;
}
