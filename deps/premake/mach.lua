mach = {
	source = path.join(dependencies.basePath, "mach"),
}

function mach.import()
	mach.includes()
end

function mach.includes()
	includedirs {
		mach.source
	}
end

function mach.project()

end

table.insert(dependencies, mach)
