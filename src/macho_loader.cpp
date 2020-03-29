#include <std_include.hpp>
#include "utils/io.hpp"
#include "macho_loader.hpp"

macho_loader::macho_loader(const std::string& file)
{
	if (!utils::io::read_file(file, &this->binary_data_))
	{
		throw std::runtime_error("Oof.");
	}

	this->binary_ = macho(this->binary_data_.data());
	this->map_binary();
}

void macho_loader::allocate_segments()
{
	SYSTEM_INFO info;
	GetSystemInfo(&info);

	auto start = static_cast<uint64_t>(-1);
	auto end = 0ull;

	auto segments = this->binary_.get_load_commands<segment_command_64>({LC_SEGMENT_64});
	for (auto& segment : segments)
	{
		if (!strcmp(segment->segname, SEG_PAGEZERO))
		{
			continue;
		}

		const auto seg_start = segment->vmaddr;
		const auto seg_end = segment->vmaddr + segment->vmsize;

		if (seg_start < start)
		{
			start = seg_start;
		}

		if (end < seg_end)
		{
			end = seg_end;
		}
	}

	const uint64_t page_align = info.dwPageSize - 1;
	const auto size = (end + page_align) & ~page_align - start;
	auto* mem = VirtualAlloc(reinterpret_cast<void*>(start), size, MEM_RESERVE | MEM_COMMIT,
	                         PAGE_EXECUTE_READWRITE);
	if (!mem)
	{
		throw std::runtime_error("Unable to allocate segments");
	}

	this->mapped_binary_ = macho(mem);
}

void macho_loader::map_segments() const
{
	auto segments = this->binary_.get_load_commands<segment_command_64>({LC_SEGMENT_64});
	for (auto& segment : segments)
	{
		if (!strcmp(segment->segname, SEG_PAGEZERO))
		{
			continue;
		}

		auto* seg_start = reinterpret_cast<void*>(segment->vmaddr);
		std::memmove(seg_start, this->binary_.get_rva<void*>(segment->fileoff), segment->filesize);

		// TODO: Apply protection
	}

	if (!this->mapped_binary_)
	{
		throw std::runtime_error("Failed to map binary");
	}
}

void macho_loader::map_imports() const
{
	struct import
	{
		std::string name;
	};

	struct import_lib
	{
		import_lib(dylib_command* c, std::string n)
			: command(c), name(std::move(n))
		{
		}

		dylib_command* command;
		std::string name;
		std::vector<import> imports;
	};

	std::vector<import_lib> import_list;

	const auto imports = this->binary_.get_load_commands<dylib_command>({LC_LOAD_DYLIB, LC_LOAD_WEAK_DYLIB});
	for (const auto& lib : imports)
	{
		const std::string name = reinterpret_cast<char*>(lib) + lib->dylib.name.offset;
		import_list.emplace_back(lib, name);
	}

	const auto binds = this->binary_.get_binds();
	for (const auto& bind : binds)
	{
		import i;
		i.name = bind.name;

		if (bind.ordinal == 0)
		{
			continue;
		}

		auto& i_list = import_list[bind.ordinal - 1].imports;

		if (std::find_if(i_list.begin(), i_list.end(), [&bind](const import& i)
		{
			return i.name == bind.name;
		}) == i_list.end())
		{
			i_list.push_back(i);
		}
	}

	for (const auto& i : import_list)
	{
		printf("Importing symbols from: %s\n", i.name.data());

		for (const auto& symbol : i.imports)
		{
			printf("\t\t%s\n", symbol.name.data());
		}
	}
}

void macho_loader::map_binary()
{
	this->allocate_segments();
	this->map_segments();
	this->map_imports();
}
