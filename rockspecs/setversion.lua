#!/usr/bin/lua

os.setlocale("C")

local format = string.format
local lfs    = require("lfs")

local version = ...
assert(version:match("^%d+%.%d+%.%d+$"), format("invalid version %q", version))

for fileName in lfs.dir(".") do
    local p1, v, p2 = fileName:match("^(lpugl.*%-)(%d+%.%d+%.%d+)(%-%d+%.rockspec)$")
    if p1 then
        local newName = p1..version..p2
        print(format("%-30s -> %s", fileName, newName))
        local out = {}
        local matched = false
        local inFile = io.open(fileName, "r")
        for line in inFile:lines() do
            local l1, l2 = line:match("^(%s*version%s*%=%s*\")%d+%.%d+%.%d+(%-%d+\"%s*)$")
            if l1 then
                assert(not matched)
                matched = true
                out[#out+1] = l1..version..l2
            else
                out[#out+1] = line
            end
        end
        out[#out+1] = ""
        inFile:close()
        assert(matched)
        local newFile, err = io.open(newName, "w")
        assert(newFile, err)
        newFile:write(table.concat(out, "\n"))
        newFile:close()
        if fileName ~= newName then
            os.remove(fileName)
        end
    end
end
