#include <std_include.hpp>
#include "utils/io.hpp"
#include "macho_loader.hpp"

macho_loader::macho_loader(const std::string& file)
{
	if (!utils::io::read_file(file, &this->binary_data_))
	{
		throw std::runtime_error("Oof.");
	}

	const auto* header = this->get_mach_header();
	if (header->magic != MH_MAGIC_64 ||
		header->cputype != CPU_TYPE_X86_64 ||
		header->filetype != MH_EXECUTE)
	{
		throw std::runtime_error("Invalid binary");
	}

	const auto imports = this->get_load_commands<dylib_command>(LC_LOAD_DYLIB);
	for(const auto& import : imports)
	{
		std::string name = reinterpret_cast<char*>(import) + import->dylib.name.offset;
		printf("Import: %s\n", name.data());
	}

	const auto entry_point = this->get_load_command<entry_point_command>(LC_MAIN);
	void* main_func = reinterpret_cast<char*>(this->binary_data_.data()) + entry_point->entryoff;

	DWORD old_protect;
	VirtualProtect(main_func, 0x1000, PAGE_EXECUTE_READWRITE, &old_protect);

	static_cast<void(*)()>(main_func)();
}

mach_header_64* macho_loader::get_mach_header()
{
	return reinterpret_cast<mach_header_64*>(this->binary_data_.data());
}

std::vector<load_command*> macho_loader::get_load_commands()
{
	const auto* header = this->get_mach_header();

	std::vector<load_command*> commands;
	commands.reserve(header->ncmds);

	auto offset = sizeof(*header);

	for(size_t i = 0; i < header->ncmds; ++i)
	{
		auto* command = reinterpret_cast<load_command*>(this->binary_data_.data() + offset);
		commands.push_back(command);
		offset += command->cmdsize;
	}

	return commands;
}
