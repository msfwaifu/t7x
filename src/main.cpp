#include <std_include.hpp>
#include "macho_loader.hpp"

int __stdcall WinMain(HINSTANCE, HINSTANCE, PSTR, int)
{
#if _DEBUG
	AllocConsole();
	AttachConsole(GetCurrentProcessId());

	freopen("CONIN$", "r", stdin);
	freopen("CONOUT$", "w", stdout);
	freopen("CONOUT$", "w", stderr);
#endif

	macho_loader loader("./CoDBlkOps3_Exe");
	return 0;
}
