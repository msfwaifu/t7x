#include <std_include.hpp>
#include "macho_loader.hpp"

int __stdcall WinMain(HINSTANCE, HINSTANCE, PSTR, int)
{
	macho_loader loader("./CoDBlkOps3_Exe");
	return 0;
}
