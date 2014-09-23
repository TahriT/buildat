-- Buildat: extension/urho3d
-- http://www.apache.org/licenses/LICENSE-2.0
-- Copyright 2014 Perttu Ahola <celeron55@gmail.com>
local log = buildat.Logger("extension/urho3d")
local dump = buildat.dump
local M = {safe = {}}

-- Set every plain value in global environment to the sandbox
-- ...it's maybe safe enough... TODO: Not safe
for k, v in pairs(_G) do
	if type(v) == 'number' or type(v) == 'string' then
		--log:info("Setting sandbox["..k.."] = "..buildat.dump(v))
		M.safe[k] = _G[k]
	end
end

-- TODO: Require explicit whitelisting of classes, method/function argument and
--       property types

local safe_globals = {
	-- Instances
	"cache",
	"ui",
	"renderer",
	"input",
	-- Types
	"Scene",
	"Text",
	"Color",
	"Vector3",
	"Quaternion",
	"Viewport",
	"CustomGeometry",
	"Texture",
	"Material",
	-- Functions
	"Random",
	"Clamp",
	-- WTF properties
	"KEY_W",
	"KEY_S",
	"KEY_A",
	"KEY_D",
}

for _, v in ipairs(safe_globals) do
	M.safe[v] = _G[v]
end

-- ResourceCache

-- Checks that this is not an absolute file path or anything funny
local allowed_name_pattern = '^[a-zA-Z0-9][a-zA-Z0-9/._ ]*$'
function M.check_safe_resource_name(orig_name)
	local name = orig_name
	if type(name) ~= "string" then
		error("Unsafe resource name: "..dump(orig_name).." (not string)")
	end
	if string.match(name, '^/.*$') then
		error("Unsafe resource name: "..dump(orig_name).." (absolute path)")
	end
	if not string.match(name, allowed_name_pattern) then
		error("Unsafe resource name: "..dump(orig_name).." (unneeded chars)")
	end
	if string.match(name, '[.][.]') then
		error("Unsafe resource name: "..dump(orig_name).." (contains ..)")
	end
	log:verbose("Safe resource name: "..orig_name.." -> "..name)
	return name
end

-- Basic tests
assert(pcall(function()
	M.check_safe_resource_name("/etc/passwd")
end) == false)
assert(pcall(function()
	M.check_safe_resource_name(" /etc/passwd")
end) == false)
assert(pcall(function()
	M.check_safe_resource_name("\t /etc/passwd")
end) == false)
assert(pcall(function()
	M.check_safe_resource_name("Models/Box.mdl")
end) == true)
assert(pcall(function()
	M.check_safe_resource_name("Fonts/Anonymous Pro.ttf")
end) == true)
assert(pcall(function()
	M.check_safe_resource_name("test1/pink_texture.png")
end) == true)
assert(pcall(function()
	M.check_safe_resource_name(" Box.mdl ")
end) == false)
assert(pcall(function()
	M.check_safe_resource_name("../../foo")
end) == false)
assert(pcall(function()
	M.check_safe_resource_name("abc$de")
end) == false)

local hack_resaved_files = {}

-- Create temporary file with wanted file name to make Urho3D load it correctly
function M.resave_file(resource_name)
	M.check_safe_resource_name(resource_name)
	local path = __buildat_get_file_path(resource_name)
	if path == nil then
		return nil
	end
	local path2 = hack_resaved_files[path]
	if path2 == nil then
		path2 = __buildat_get_path("tmp").."/"..resource_name
		dir2 = string.match(path2, '^(.*)/.+$')
		if dir2 then
			if not __buildat_mkdir(dir2) then
				error("Failed to create directory: \""..dir2.."\"")
			end
		end
		log:info("Temporary path: "..path2)
		local src = io.open(path, "rb")
		local dst = io.open(path2, "wb")
		while true do
			local buf = src:read(100000)
			if buf == nil then break end
			dst:write(buf)
		end
		src:close()
		dst:close()
		hack_resaved_files[path] = path2
	end
	return path2
end

M.safe.cache = {
	GetResource = function(self, resource_type, resource_name)
		local path = M.resave_file(resource_name)
		-- Note: path is unused
		resource_name = M.check_safe_resource_name(resource_name)
		return cache:GetResource(resource_type, resource_name)
	end,
}

-- SubscribeToEvent

local sandbox_function_name_to_global_function_name = {}
local next_global_function_i = 1

function M.safe.SubscribeToEvent(event_name, function_name)
	local caller_environment = getfenv(2)
	local callback = caller_environment[function_name]
	if type(callback) ~= 'function' then
		error("SubscribeToEvent(): '"..function_name..
				"' is not a global function in current sandbox environment")
	end
	local global_function_i = next_global_function_i
	next_global_function_i = next_global_function_i + 1
	local global_function_name = "__buildat_sandbox_callback_"..global_function_i
	sandbox_function_name_to_global_function_name[function_name] = global_function_name
	_G[global_function_name] = function(eventType, eventData)
		local f = function()
			callback(eventType, eventData)
		end
		__buildat_run_function_in_sandbox(f)
	end
	SubscribeToEvent(event_name, global_function_name)
end

return M
