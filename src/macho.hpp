#pragma once

#include <mach-o/loader.h>

class macho
{
public:
	struct bind
	{
		int seg_index{};
		uint64_t seg_offset{};
		const char* name{};

		union
		{
			int64_t addend{};
			uint64_t value;
		};

		uint8_t type{};
		uint8_t ordinal{};
		bool is_weak{};
		bool is_classic{};
	};

	macho() = default;
	macho(void* pointer);

	explicit operator bool() const;

	void* get_pointer() const;
	void* get_entry_point() const;

	mach_header_64* get_mach_header() const;
	std::vector<load_command*> get_load_commands() const;

	std::vector<section_64*> get_sections() const;
	section_64* get_section(const std::string& name) const;

	std::vector<void(*)()> get_constructors() const;
	std::vector<void(*)()> get_destructors() const;

	std::vector<bind> get_binds() const;

	template <typename T>
	std::vector<T*> get_load_commands(std::vector<uint32_t> command_ids) const
	{
		std::vector<T*> result;

		const auto commands = this->get_load_commands();
		for (auto& command : commands)
		{
			if (std::find(command_ids.begin(), command_ids.end(), command->cmd) != command_ids.end())
			{
				result.push_back(reinterpret_cast<T*>(command));
			}
		}

		return result;
	}

	template <typename T>
	T* get_load_command(const uint32_t command_id) const
	{
		const auto commands = this->get_load_commands<T>({command_id});
		if (commands.empty())
		{
			throw std::runtime_error("Command not found");
		}

		return commands.front();
	}

	template <typename T>
	T* get_rva(const uint64_t rva) const
	{
		auto* header = static_cast<uint8_t*>(this->pointer_);
		return reinterpret_cast<T*>(header + rva);
	}

private:
	void* pointer_ = nullptr;
};
