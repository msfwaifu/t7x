#include <std_include.hpp>
#include "macho_loader.hpp"
#include "utils/hook.hpp"

#pragma region runtime

int NSGetExecutablePath(char* buf, uint32_t* bufsize)
{
	*bufsize = GetModuleFileNameA(GetModuleHandleA(nullptr), buf, *bufsize);
	return *bufsize == 0;
}

int onesub()
{
	return 1;
}

int nullsub()
{
	return 0;
}

void* resolve_symbol(const std::string& module, const std::string& function);

void* dlsym(void* handle, const char* name)
{
	return resolve_symbol({}, name);
}

char* realpath(const char* path, char* resolved_path)
{
	return _fullpath(resolved_path, path, 0x7FFFFFFF);
}

std::vector<std::function<void()>> exit_handlers;

int cxa_atexit(void (*func) (void *), void * arg, void * dso_handle)
{
	exit_handlers.emplace_back([arg, func]() {
		func(arg);
	});
	
	return 0;
}

#pragma endregion

void* create_calling_convention_wrapper(void* func)
{
	return utils::hook::assemble([func](utils::hook::assembler& a)
	{
		a.push(rax);
		a.pushad64();

		a.push(rdi);
		a.push(rsi);
		a.push(rdx);
		a.push(rcx);

		// More than 4 arguments not supported yet

		a.pop(r9);
		a.pop(r8);
		a.pop(rdx);
		a.pop(rcx);

		a.call(func);

		a.mov(ptr(rsp, 0x80, 8), rax);

		a.popad64();
		a.pop(rax);
		a.ret();
	});
}

void wrap_calling_convention(std::unordered_map<std::string, void*>& symbols)
{
	auto copy = symbols;
	symbols.clear();

	std::unordered_map<void*, void*> function_mapping;

	for (auto i = copy.begin(); i != copy.end(); ++i)
	{
		if (function_mapping.find(i->second) == function_mapping.end())
		{
			function_mapping[i->second] = create_calling_convention_wrapper(i->second);
		}
	}

	for (auto i = copy.begin(); i != copy.end(); ++i)
	{
		symbols[i->first] = function_mapping[i->second];
	}
}

std::unordered_map<std::string, void*> build_symbol_map()
{
	std::unordered_map<std::string, void*> symbols;

	// Symbols that need calling convention wrapping

	symbols["__Znwm"] = malloc;
	symbols["__Znam"] = malloc;
	symbols["__ZdlPv"] = free;
	symbols["__ZdaPv"] = free;

	symbols["__NSGetExecutablePath"] = NSGetExecutablePath;

	symbols["_strlen"] = strlen;
	symbols["_memcpy"] = memcpy;

	symbols["___cxa_guard_acquire"] = onesub;
	symbols["___cxa_guard_release"] = onesub;

	symbols["_dlsym"] = dlsym;
	symbols["realpath"] = realpath;
	symbols["___cxa_atexit"] = cxa_atexit;

	wrap_calling_convention(symbols);

	// Symbols without calling convention wrapping

	static uint64_t guard = 0x1337;
	symbols["___stack_chk_guard"] = &guard;

	return symbols;
}

void* resolve_symbol(const std::string& module, const std::string& function)
{
	static const auto symbol_map = build_symbol_map();

	const auto symbol = symbol_map.find(function);
	if (symbol != symbol_map.end())
	{
		return symbol->second;
	}

	return nullptr;
}

int __stdcall WinMain(HINSTANCE, HINSTANCE, PSTR, int)
{
#if _DEBUG
	AllocConsole();
	AttachConsole(GetCurrentProcessId());

	freopen("CONIN$", "r", stdin);
	freopen("CONOUT$", "w", stdout);
	freopen("CONOUT$", "w", stderr);
#endif

	macho_loader loader("./CoDBlkOps3_Exe", resolve_symbol);

	for (const auto& constructor : loader.get_mapped_binary().get_constructors())
	{
		constructor();
	}

	const auto entry_point = utils::hook::assemble([&loader](utils::hook::assembler& a)
	{
		a.sub(rsp, 8);
		a.pushad64();

		a.xor_(rdi, rdi);
		a.xor_(rsi, rsi);

		a.call(loader.get_mapped_binary().get_entry_point());

		a.popad64();
		a.add(rsp, 8);
		a.ret();
	});

	static_cast<void(*)()>(entry_point)();

	for (const auto& exit_handler : exit_handlers)
	{
		exit_handler();
	}

	for (const auto& destructor : loader.get_mapped_binary().get_destructors())
	{
		destructor();
	}

	return 0;
}
