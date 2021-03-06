-- Buildat: extension/urho3d/safe_classes.lua
-- http://www.apache.org/licenses/LICENSE-2.0
-- Copyright 2014 Perttu Ahola <celeron55@gmail.com>
local dump = buildat.dump
local log = buildat.Logger("safe_classes")
local M = {}

function M.define(dst, util)
	util.wc("StringHash", {
		unsafe_constructor = util.wrap_function({{"string"}},
		function(value)
			return util.wrap_instance("StringHash", StringHash(value))
		end),
		instance = {
		}
	})

	util.wc("VectorBuffer", {
		instance = {
			GetSize = util.self_function(
					"GetSize", {"number"}, {"VectorBuffer"}),
			ReadString = util.self_function(
					"ReadString", {"string"}, {"VectorBuffer"}),
			ReadInt = util.self_function(
					"ReadInt", {"number"}, {"VectorBuffer"}),
			ReadFloat = util.self_function(
					"ReadFloat", {"number"}, {"VectorBuffer"}),
			ReadVector3 = util.wrap_function({"VectorBuffer"},
				function(self)
					return util.wrap_instance("Vector3", self:ReadVector3())
				end
			),
		},
		properties = {
			size = util.simple_property("number"),
			eof = util.simple_property("boolean"),
		},
	})

	util.wc("Variant", {
		unsafe_constructor = util.wrap_function({{"Color"}},
		function(value)
			return util.wrap_instance("Variant", Variant(value))
		end),
		instance = {
			IsEmpty = util.self_function(
					"IsEmpty", {"boolean"}, {"Variant"}),
			GetString = util.self_function(
					"GetString", {"string"}, {"Variant"}),
			GetInt = util.self_function(
					"GetInt", {"number"}, {"Variant"}),
			GetBool = util.self_function(
					"GetBool", {"boolean"}, {"Variant"}),
			GetBuffer = util.wrap_function({"Variant"},
				function(self)
					return util.wrap_instance("VectorBuffer", self:GetBuffer())
				end
			),
		}
	})

	util.wc("VariantMap", {
		unsafe_constructor = util.wrap_function({},
		function()
			return util.wrap_instance("VariantMap", VariantMap())
		end),
		instance = {
			SetFloat = util.self_function(
					"SetFloat", {}, {"VariantMap", "string", "number"}),
			GetFloat = util.self_function(
					"GetFloat", {"number"}, {"VariantMap", "string"}),
			SetInt = util.self_function(
					"SetInt", {}, {"VariantMap", "string", "number"}),
			GetInt = util.self_function(
					"GetInt", {"number"}, {"VariantMap", "string"}),
			SetString = util.self_function(
					"SetString", {}, {"VariantMap", "string", "string"}),
			GetString = util.self_function(
					"GetString", {"string"}, {"VariantMap", "string"}),
			SetBuffer = util.self_function(
					"SetBuffer", {}, {"VariantMap", "string", "VectorBuffer"}),
			GetBuffer = util.self_function(
					"GetBuffer", {dst.VectorBuffer}, {"VariantMap", "string"}),

			SetPtr = util.self_function(
					"SetPtr", {}, {"VariantMap", "string",
						{"Node", "Component"}}),
			GetPtr = util.wrap_function({"VariantMap", "string", "string"},
				function(self, type, key)
					return util.wrap_instance(type, self:GetPtr(type, key))
				end
			),
		}
	})

	util.wc("Quaternion", {
		unsafe_constructor = util.wrap_function({
				"number", {"number", "Vector3"}, {"number", "__nil"},
						{"number", "__nil"}},
		function(w, x, y, z)
			if type(w) == "number" and type(x) == "number" and
					type(y) == "number" and type(z) == "number" then
				return util.wrap_instance("Quaternion", Quaternion(w, x, y, z))
			elseif type(w) == "number" and type(x) == "number" and
					type(y) == "number" and z == nil then
				-- y, x, z (euler angles)
				return util.wrap_instance("Quaternion", Quaternion(w, x, y))
			else
				-- angle: float, axis: Vector3
				return util.wrap_instance("Quaternion", Quaternion(w, x))
			end
		end),
		instance = {
			YawAngle = util.self_function(
					"YawAngle", {"number"}, {"Quaternion"}),
			PitchAngle = util.self_function(
					"PitchAngle", {"number"}, {"Quaternion"}),
			RollAngle = util.self_function(
					"RollAngle", {"number"}, {"Quaternion"}),
			EulerAngles = util.wrap_function({"Quaternion"},
				function(self)
					return util.wrap_instance("Vector3", self:EulerAngles())
				end
			),
		},
		instance_meta = {
			__mul = util.wrap_function({"Quaternion", "number"}, function(self, n)
				return util.wrap_instance("Quaternion", self * n)
			end),
			__add = util.wrap_function({"Quaternion", "Quaternion"}, function(self, other)
				return util.wrap_instance("Quaternion", self + other)
			end),
			__sub = util.wrap_function({"Quaternion", "Quaternion"}, function(self, other)
				return util.wrap_instance("Quaternion", self - other)
			end),
			__eq = util.wrap_function({"Quaternion", "Quaternion"}, function(self, other)
				return (self == other)
			end),
		},
		properties = {
			w = util.simple_property("number"),
			x = util.simple_property("number"),
			y = util.simple_property("number"),
			z = util.simple_property("number"),
		},
	})

	util.wc("Vector3", {
		unsafe_constructor = util.wrap_function({"number", "number", "number"},
		function(x, y, z)
			return util.wrap_instance("Vector3", Vector3(x, y, z))
		end),
		class = {
			from_buildat = function(v)
				return util.wrap_instance("Vector3", Vector3(v.x, v.y, v.z))
			end,
		},
		instance = {
			Length = util.self_function(
					"Length", {"number"}, {"Vector3"}),
			CrossProduct = util.wrap_function({"Vector3", "Vector3"},
				function(self, other)
					return util.wrap_instance("Vector3", self:CrossProduct(other))
				end
			),
			Normalized = util.wrap_function({"Vector3"},
				function(self)
					return util.wrap_instance("Vector3", self:Normalized())
				end
			),
		},
		instance_meta = {
			__mul = util.wrap_function({"Vector3", "number"}, function(self, n)
				return util.wrap_instance("Vector3", self * n)
			end),
			__div = util.wrap_function({"Vector3", "number"}, function(self, n)
				return util.wrap_instance("Vector3", self / n)
			end),
			__add = util.wrap_function({"Vector3", "Vector3"}, function(self, other)
				return util.wrap_instance("Vector3", self + other)
			end),
			__sub = util.wrap_function({"Vector3", "Vector3"}, function(self, other)
				return util.wrap_instance("Vector3", self - other)
			end),
			__eq = util.wrap_function({"Vector3", "Vector3"}, function(self, other)
				return (self == other)
			end),
		},
		properties = {
			x = util.simple_property("number"),
			y = util.simple_property("number"),
			z = util.simple_property("number"),
		},
	})

	util.wc("IntVector2", {
		unsafe_constructor = util.wrap_function({"number", "number"},
		function(x, y)
			return util.wrap_instance("IntVector2", IntVector2(x, y))
		end),
		instance = {
		},
		instance_meta = {
			__mul = util.wrap_function({"IntVector2", "number"}, function(self, n)
				return util.wrap_instance("IntVector2", self * n)
			end),
			__add = util.wrap_function({"IntVector2", "IntVector2"}, function(self, other)
				return util.wrap_instance("IntVector2", self + other)
			end),
			__sub = util.wrap_function({"IntVector2", "IntVector2"}, function(self, other)
				return util.wrap_instance("IntVector2", self - other)
			end),
			__eq = util.wrap_function({"IntVector2", "IntVector2"}, function(self, other)
				return (self == other)
			end),
		},
		properties = {
			x = util.simple_property("number"),
			y = util.simple_property("number"),
		},
	})

	util.wc("IntRect", {
		unsafe_constructor = util.wrap_function({"number", "number", "number", "number"},
		function(left, top, right, bottom)
			return util.wrap_instance("IntRect", IntRect(left, top, right, bottom))
		end),
		properties = {
			left = util.simple_property("number"),
			top = util.simple_property("number"),
			right = util.simple_property("number"),
			bottom = util.simple_property("number"),
		},
	})

	util.wc("Color", {
		unsafe_constructor = util.wrap_function({"number", "number", "number",
				{"number", "__nil"}},
		function(r, g, b, a)
			a = a or 1.0
			return util.wrap_instance("Color", Color(r, g, b, a))
		end),
		instance_meta = {
			__mul = util.wrap_function({"Color", "number"}, function(self, n)
				return util.wrap_instance("Color", self * n)
			end),
		},
		properties = {
			r = util.simple_property("number"),
			g = util.simple_property("number"),
			b = util.simple_property("number"),
			a = util.simple_property("number"),
		},
	})

	util.wc("BoundingBox", {
		unsafe_constructor = util.wrap_function({
				{"number", "Vector3"}, {"number", "Vector3"}},
		function(min, max)
			return util.wrap_instance("BoundingBox", BoundingBox(min, max))
		end),
		instance_meta = {
		},
		properties = {
		},
	})

	util.wc("BiasParameters", {
		unsafe_constructor = util.wrap_function({"number", "number"},
		function(constant_bias, slope_scaled_bias)
			return util.wrap_instance("BiasParameters",
					BiasParameters(constant_bias, slope_scaled_bias))
		end),
	})

	util.wc("CascadeParameters", {
		unsafe_constructor = util.wrap_function({"number", "number", "number", "number", "number", {"number", "__nil"}},
		function(split1, split2, split3, split4, fadeStart, biasAutoAdjust)
			biasAutoAdjust = biasAutoAdjust or 1.0
			return util.wrap_instance("CascadeParameters",
					CascadeParameters(split1, split2, split3, split4, fadeStart, biasAutoAdjust))
		end),
	})

	util.wc("Resource", {
	})

	util.wc("Component", {
	})

	util.wc("Octree", {
		inherited_from_by_wrapper = dst.Component,
		instance = {
			GetDrawables = util.wrap_function({"table"}, {"Octree", "BoundingBox"},
				function(self, query)
					local unsafe_result = self:GetDrawables(query)
					--log:info(dump(result))
					-- The result is a list of OctreeQueryResults; we will
					-- convert it to a list of tables that contain the fields of
					-- OctreeQueryResult.
					local result = {}
					for _, v in ipairs(unsafe_result) do
						table.insert(result, {
							drawable = util.wrap_instance("Drawable", v.drawable),
							node = util.wrap_instance("Node", v.node),
						})
					end
					return result
				end
			),
		},
	})

	util.wc("Drawable", {
		inherited_from_by_wrapper = dst.Component,
	})

	util.wc("Light", {
		inherited_from_by_wrapper = dst.Drawable,
		properties = {
			lightType = util.simple_property("number"),
			brightness = util.simple_property("number"),
			castShadows = util.simple_property("boolean"),
			shadowBias = util.simple_property("BiasParameters"),
			shadowCascade = util.simple_property("CascadeParameters"),
			color = util.simple_property("Color"),
			range = util.simple_property("number"),
			fadeDistance = util.simple_property("number"),
		},
	})

	util.wc("CustomGeometry", {
		inherited_from_by_wrapper = dst.Drawable,
		instance = {
			SetNumGeometries = util.self_function(
					"SetNumGeometries", {}, {"CustomGeometry", "number"}),
			BeginGeometry = util.self_function(
					"BeginGeometry", {}, {"CustomGeometry", "number", "number"}),
			DefineVertex = util.self_function(
					"DefineVertex", {}, {"CustomGeometry", "Vector3"}),
			DefineNormal = util.self_function(
					"DefineNormal", {}, {"CustomGeometry", "Vector3"}),
			DefineColor = util.self_function(
					"DefineColor", {}, {"CustomGeometry", "Color"}),
			Commit = util.self_function(
					"Commit", {}, {"CustomGeometry"}),
			GetMaterial = util.self_function(
					"GetMaterial", {{"Material", "nil"}}, {"CustomGeometry", "number"}),
			SetMaterial = util.self_function(
					"SetMaterial", {}, {"CustomGeometry", "number", "Material"}),
		},
		properties = {
			dynamic = util.simple_property("boolean"),
		},
	})

	util.wc("Camera", {
		inherited_from_by_wrapper = dst.Component,
		properties = {
			nearClip = util.simple_property("number"),
			farClip = util.simple_property("number"),
			fov = util.simple_property("number"),
		},
	})

	util.wc("RigidBody", {
		inherited_from_by_wrapper = dst.Component,
		instance = {
			ApplyForce = util.self_function(
					"ApplyForce", {}, {"RigidBody", "Vector3"}),
			ApplyImpulse = util.self_function(
					"ApplyImpulse", {}, {"RigidBody", "Vector3"}),
		},
		properties = {
			mass = util.simple_property("number"),
			gravityOverride = util.simple_property(dst.Vector3),
			friction = util.simple_property("number"),
			angularFactor = util.simple_property(dst.Vector3),
			kinematic = util.simple_property("boolean"),
			linearVelocity = util.simple_property(dst.Vector3),
		},
	})

	util.wc("CollisionShape", {
		inherited_from_by_wrapper = dst.Component,
		instance = {
			SetBox = util.self_function(
					"SetBox", {}, {"CollisionShape", "Vector3"}),
			SetCapsule = util.self_function(
					"SetCapsule", {}, {"CollisionShape", "number", "number"}),
		},
	})

	util.wc("Zone", {
		inherited_from_by_wrapper = dst.Component,
		instance = {
		},
		properties = {
			boundingBox = util.simple_property(dst.BoundingBox),
			ambientColor = util.simple_property(dst.Color),
			fogColor = util.simple_property(dst.Color),
			fogStart = util.simple_property("number"),
			fogEnd = util.simple_property("number"),
			priority = util.simple_property("number"),
			heightFog = util.simple_property("boolean"),
			override = util.simple_property("boolean"),
			ambientGradient = util.simple_property("boolean"),
		},
	})

	util.wc("Model", {
		inherited_from_by_wrapper = dst.Resource,
	})

	util.wc("Material", {
		inherited_from_by_wrapper = dst.Resource,
		class = {
			new = function()
				return util.wrap_instance("Material", Material:new())
			end,
		},
		instance = {
			--SetTexture = util.wrap_function({"Material", "number", "Texture"},
			--function(self, index, texture)
			--	log:info("Material:SetTexture("..dump(index)..", "..dump(texture)..")")
			--	self:SetTexture(index, texture)
			--end),
			SetShaderParameter = util.self_function(
					"SetShaderParameter", {}, {"Material", "string", "Variant"}),
			SetTexture = util.self_function(
					"SetTexture", {}, {"Material", "number", "Texture"}),
			SetTechnique = util.self_function(
					"SetTechnique", {}, {"Material", "number", "Technique",
							{"number", "__nil"}, {"number", "__nil"}}),
		},
	})

	util.wc("Texture", {
		inherited_from_by_wrapper = dst.Resource,
	})

	util.wc("Texture2D", {
		inherited_from_by_wrapper = dst.Texture,
	})

	util.wc("Font", {
		inherited_from_by_wrapper = dst.Resource,
	})

	util.wc("XMLFile", {
		inherited_from_by_wrapper = dst.Resource,
	})

	util.wc("StaticModel", {
		inherited_from_by_wrapper = dst.Octree,
		instance = {
			SetModel = util.self_function(
					"SetModel", {}, {"StaticModel", "Model"}),
		},
		properties = {
			model = util.simple_property(dst.Model),
			material = util.simple_property(dst.Material),
			castShadows = util.simple_property("boolean"),
		},
	})

	util.wc("Technique", {
		inherited_from_by_wrapper = dst.Resource,
	})

	util.wc("Node", {
		class = {
			new = function()
				return util.wrap_instance("Node", Node:new())
			end,
		},
		instance = {
			CreateChild = util.wrap_function({"Node", "string",
					{"number", "__nil"}},
				function(self, name, mode)
					if mode ~= nil then
						return util.wrap_instance("Node", self:CreateChild(name, mode))
					else
						return util.wrap_instance("Node", self:CreateChild(name, LOCAL))
					end
				end
			),
			CreateComponent = util.wrap_function(
					{"Node", "string", {"number", "__nil"}},
				function(self, name, mode)
					local component = nil
					if mode ~= nil then
						component = self:CreateComponent(name, mode)
					else
						component = self:CreateComponent(name, LOCAL)
					end
					assert(component)
					return util.wrap_instance(name, component)
				end
			),
			GetComponent = util.wrap_function({"Node", "string"}, function(self, name)
				local component = self:GetComponent(name)
				if not component then
					return nil
				end
				return util.wrap_instance(name, component)
			end),
			LookAt = util.wrap_function({"Node", "Vector3"}, function(self, p)
				self:LookAt(p)
			end),
			Translate = util.wrap_function({"Node", "Vector3"}, function(self, v)
				self:Translate(v)
			end),
			RemoveChild = util.wrap_function({"Node", "Node"}, function(self, v)
				self:RemoveChild(v)
			end),
			GetID = util.self_function("GetID", {"number"}, {"Node"}),
			GetName = util.self_function("GetName", {"string"}, {"Node"}),
			GetNumChildren = util.self_function(
					"GetNumChildren", {"number"}, {"Node"}),
			GetChild = util.wrap_function({"Node", {"string", "number"}},
				function(self, name_or_index)
					return util.wrap_instance("Node",
							self:GetChild(name_or_index))
				end
			),
			SetScale = util.self_function("SetScale", {}, {"Node", "Vector3"}),
			GetVar = util.wrap_function({"Node", {"string", "StringHash"}},
				function(self, name_or_stringhash)
					if type(name_or_stringhash) == "string" then
						return util.wrap_instance("Variant",
								self:GetVar(StringHash(name_or_stringhash)))
					else
						return util.wrap_instance("Variant",
								self:GetVar(name_or_stringhash))
					end
				end
			),
			SetVar = util.wrap_function({"Node", {"string", "StringHash"}, "Variant"},
				function(self, name_or_stringhash, value)
					if type(name_or_stringhash) == "string" then
						self:SetVar(StringHash(name_or_stringhash), value)
					else
						self:SetVar(name_or_stringhash, value)
					end
				end
			),
			SetVar = util.self_function("SetVar", {},
					{"Node", "StringHash", "Variant"}),
			GetWorldPosition = util.self_function("GetWorldPosition", {dst.Vector3}, {"Node"}),
			GetWorldDirection = util.self_function("GetWorldDirection", {dst.Vector3}, {"Node"}),
			GetRotation = util.self_function("GetRotation", {dst.Quaternion}, {"Node"}),
			Pitch = util.self_function("Pitch", {}, {"Node", "number"}),
			Yaw = util.self_function("Yaw", {}, {"Node", "number"}),
			Roll = util.self_function("Roll", {}, {"Node", "number"}),
		},
		properties = {
			scale = util.simple_property(dst.Vector3),
			direction = util.simple_property(dst.Vector3),
			position = util.simple_property(dst.Vector3),
			worldDirection = util.simple_property(dst.Vector3),
			worldPosition = util.simple_property(dst.Vector3),
			enabled = util.simple_property("boolean"),
			rotation = util.simple_property(dst.Quaternion),
		},
	})

	util.wc("Plane", {
	})

	util.wc("Scene", {
		inherited_from_by_wrapper = dst.Node,
		unsafe_constructor = util.wrap_function({}, function()
			return util.wrap_instance("Scene", Scene())
		end),
		class = {
			new = function()
				return util.wrap_instance("Scene", Scene:new())
			end,
		},
		instance = {
			GetNode = util.wrap_function({"Scene", "number"},
				function(self, id)
					return util.wrap_instance("Node", self:GetNode(id))
				end
			),
		},
	})

	-- Add properties to Node, using types defined later
	getmetatable(dst.Node).def.properties["scene"] =
			util.simple_property(dst.Scene),

	util.wc("ResourceCache", {
		instance = {
			GetResource = util.wrap_function({"ResourceCache", "string", "string"},
			function(self, resource_type, unsafe_resource_name)
				--[[
				-- NOTE: resource_type=XMLFile can refer to other resources even
				-- in absolute and arbitrary relative paths. Make sure file
				-- access (fopen()) is sandboxed appropriately.
				resource_name = util.check_safe_resource_name(unsafe_resource_name)
				log:debug("GetResource: "..dump(unsafe_resource_name)..
						" -> "..dump(resource_name))
				local saved_path = util.resave_file(resource_name)
				-- Note: saved_path is ignored
				--]]
				local res = cache:GetResource(resource_type, unsafe_resource_name)
				return util.wrap_instance(resource_type, res)
			end),
		},
	})

	util.wc("Viewport", {
		class = {
			new = util.wrap_function({"__to_nil", "Scene", "Camera"},
			function(_, scene, camera_component)
				return util.wrap_instance("Viewport", Viewport:new(scene, camera_component))
			end),
		},
		instance = {
			GetScene = util.wrap_function({"Viewport"},
				function(self)
					return util.wrap_instance("Scene", self:GetScene())
				end
			),
			GetCamera = util.wrap_function({"Viewport"},
				function(self)
					return util.wrap_instance("Camera", self:GetCamera())
				end
			),
		},
	})

	util.wc("Renderer", {
		instance = {
			SetViewport = util.wrap_function({"Renderer", "number", "Viewport"},
				function(self, index, viewport)
					self:SetViewport(index, viewport)
				end
			),
			GetViewport = util.wrap_function({"Renderer", "number"},
				function(self, index)
					local ret = self:GetViewport(index)
					if ret == nil then return nil end
					return util.wrap_instance("Viewport", ret)
				end
			),
		},
	})

	util.wc("Animatable", {
	})

	util.wc("UIElement", {
		inherited_from_by_wrapper = dst.Animatable,
		instance = {
			CreateChild = util.wrap_function({"UIElement", "string",
					{"string", "__nil"}},
				function(self, element_type, name)
					return util.wrap_instance(element_type,
							self:CreateChild(element_type, name))
				end
			),
			GetChild = util.wrap_function({"UIElement", {"string", "number"}},
				function(self, name_or_index)
					return util.wrap_instance("UIElement",
							self:GetChild(name_or_index))
				end
			),
			GetNumChildren = util.self_function(
					"GetNumChildren", {"number"}, {"UIElement"}),
			RemoveChild = util.wrap_function({"UIElement", "UIElement"},
				function(self, child)
					self:RemoveChild(child)
				end
			),
			Remove = util.self_function("Remove", {}, {"UIElement"}),
			SetName = util.self_function("SetName", {}, {"UIElement", "string"}),
			SetText = util.self_function("SetText", {}, {"UIElement", "string"}),
			SetFont = util.self_function("SetFont", {}, {"UIElement", "Font"}),
			SetPosition = util.self_function(
					"SetPosition", {}, {"UIElement", "number", "number"}),
			SetStyleAuto = util.self_function("SetStyleAuto", {}, {"UIElement"}),
			SetVisible = util.self_function("SetVisible", {},
					{"UIElement", "boolean"}),
			SetLayout = util.wrap_function({"UIElement", "number",
					{"number", "__nil"}, {"IntRect", "__nil"}},
				function(self, mode, spacing, border)
					spacing = spacing or 0
					border = border or dst.IntRect()
					self:SetLayout(mode, spacing, border)
				end
			),
			SetAlignment = util.self_function(
					"SetAlignment", {}, {"UIElement", "number", "number"}),
			SetFocusMode = util.self_function(
					"SetFocusMode", {}, {"UIElement", "number"}),
			SetFocus = util.self_function(
					"SetFocus", {}, {"UIElement", "boolean"}),
			HasFocus = util.self_function(
					"HasFocus", {"boolean"}, {"UIElement"}),
			GetName = util.self_function("GetName", {"string"}, {"UIElement"}),
			GetText = util.self_function("GetText", {"string"}, {"UIElement"}),
		},
		properties = {
			horizontalAlignment = util.simple_property("number"),
			verticalAlignment = util.simple_property("number"),
			height = util.simple_property("number"),
			width = util.simple_property("number"),
			size = util.simple_property("IntVector2"),
			color = util.simple_property("Color"),
			minHeight = util.simple_property("number"),
			minWidth = util.simple_property("number"),
			minSize = util.simple_property("IntVector2"),
			fixedHeight = util.simple_property("number"),
			fixedWidth = util.simple_property("number"),
			fixedSize = util.simple_property("IntVector2"),
			defaultStyle = util.simple_property("XMLFile"),
		},
	})

	util.wc("Text", {
		inherited_from_by_wrapper = dst.UIElement,
		instance = {
			SetTextAlignment = util.self_function(
					"SetTextAlignment", {}, {"UIElement", "number"}),
		},
		properties = {
			text = util.simple_property("string"),
		},
	})

	util.wc("BorderImage", {
		inherited_from_by_wrapper = dst.UIElement,
		properties = {
			texture = util.simple_property("Texture"),
		},
	})

	util.wc("Window", {
		inherited_from_by_wrapper = dst.BorderImage,
	})

	util.wc("Button", {
		inherited_from_by_wrapper = dst.BorderImage,
	})

	util.wc("LineEdit", {
		inherited_from_by_wrapper = dst.BorderImage,
		properties = {
		},
	})

	util.wc("Sprite", {
		inherited_from_by_wrapper = dst.UIElement,
		instance = {
			SetTexture = util.self_function(
					"SetTexture", {}, {"Sprite", "Texture"}),
			SetFixedSize = util.self_function(
					"SetFixedSize", {}, {"Sprite", "number", "number"}),
		},
	})

	util.wc("UI", {
		instance = {
			SetFocusElement = util.wrap_function({"UI", {"UIElement", "__nil"}},
				function(self, element)
					if element == nil then
						self:SetFocusElement(nil)
					else
						self:SetFocusElement(element)
					end
				end
			),
		},
		properties = {
			root = util.simple_property(dst.UIElement),
			focusElement = util.simple_property({dst.UIElement, "__nil"}),
		},
	})

	util.wc("Input", {
		instance = {
			SetMouseVisible = util.self_function("SetMouseVisible", {}, {"Input", "boolean"}),
			GetKeyDown = util.self_function("GetKeyDown", {"boolean"}, {"Input", "number"}),
			GetKeyPress = util.self_function("GetKeyPress", {"boolean"}, {"Input", "number"}),
			GetMouseMove = util.self_function("GetMouseMove", {dst.IntVector2}, {"Input"}),
		},
	})

	dst.cache = util.wrap_instance("ResourceCache", cache)
	dst.renderer = util.wrap_instance("Renderer", renderer)
	dst.ui = util.wrap_instance("UI", ui)
	dst.input = util.wrap_instance("Input", input)
end

return M
-- vim: set noet ts=4 sw=4:
