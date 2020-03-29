#pragma once

#include "macho.hpp"

class macho_loader
{
public:
	using resolver = std::function<void*(const std::string& module, const std::string & function)>;

	macho_loader(const std::string& file, const resolver& import_resolver = {});

	const macho& get_mapped_binary() const;

private:
	void allocate_segments();
	void map_segments() const;
	void map_imports() const;
	void map_binary();

	std::string binary_data_;
	macho binary_;
	macho mapped_binary_;

	resolver import_resolver_;
};
