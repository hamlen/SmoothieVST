#include "Smoothie.h"
#include "Windows.h"

#ifdef LOGGING
constexpr LPCWSTR log_file = L"C:\\TestVST3.log";

void log(const char* format, ...)
{
	char buf[2048];
	va_list args;
	va_start(args, format);
	vsprintf_s(buf, sizeof(buf) / sizeof(*buf), format, args);
	va_end(args);

	HANDLE h = CreateFile(log_file, FILE_APPEND_DATA, FILE_SHARE_READ | FILE_SHARE_WRITE, NULL, OPEN_ALWAYS, FILE_ATTRIBUTE_NORMAL, NULL);
	if (h != INVALID_HANDLE_VALUE)
	{
		WriteFile(h, buf, (DWORD)strlen(buf), NULL, NULL);
		CloseHandle(h);
	}
}
#endif
