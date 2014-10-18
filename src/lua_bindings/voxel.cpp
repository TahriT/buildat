// http://www.apache.org/licenses/LICENSE-2.0
// Copyright 2014 Perttu Ahola <celeron55@gmail.com>
#include "lua_bindings/util.h"
#include "core/log.h"
#include "client/app.h"
#include "interface/mesh.h"
#include "interface/voxel_volume.h"
#include "interface/worker_thread.h"
#include <tolua++.h>
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wstrict-aliasing"
#include <Scene.h>
#include <StaticModel.h>
#include <Model.h>
#include <CustomGeometry.h>
#include <CollisionShape.h>
#include <RigidBody.h>
#pragma GCC diagnostic pop
#define MODULE "lua_bindings"

namespace magic = Urho3D;
namespace pv = PolyVox;

using interface::VoxelInstance;

// Just do this; Urho3D's stuff doesn't really clash with anything in buildat
using namespace Urho3D;

namespace lua_bindings {

#define GET_TOLUA_STUFF(result_name, index, type) \
	if(!tolua_isusertype(L, index, #type, 0, &tolua_err)){ \
		tolua_error(L, __PRETTY_FUNCTION__, &tolua_err); \
		return 0; \
	} \
	type *result_name = (type*)tolua_tousertype(L, index, 0);
#define TRY_GET_TOLUA_STUFF(result_name, index, type) \
	type *result_name = nullptr; \
	if(tolua_isusertype(L, index, #type, 0, &tolua_err)){ \
		result_name = (type*)tolua_tousertype(L, index, 0); \
	}

// NOTE: This API is designed this way because otherwise ownership management of
//       objects sucks
// set_simple_voxel_model(node, w, h, d, buffer: VectorBuffer)
static int l_set_simple_voxel_model(lua_State *L)
{
	tolua_Error tolua_err;

	GET_TOLUA_STUFF(node, 1, Node);
	int w = lua_tointeger(L, 2);
	int h = lua_tointeger(L, 3);
	int d = lua_tointeger(L, 4);
	TRY_GET_TOLUA_STUFF(buf, 5, const VectorBuffer);

	log_d(MODULE, "set_simple_voxel_model(): node=%p", node);
	log_d(MODULE, "set_simple_voxel_model(): buf=%p", buf);

	ss_ data;
	if(buf == nullptr)
		data = lua_tocppstring(L, 5);
	else
		data.assign((const char*)&buf->GetBuffer()[0], buf->GetBuffer().Size());

	if((int)data.size() != w * h * d){
		log_e(MODULE, "set_simple_voxel_model(): Data size does not match "
				"with dimensions (%zu vs. %i)", data.size(), w*h*d);
		return 0;
	}

	lua_getfield(L, LUA_REGISTRYINDEX, "__buildat_app");
	app::App *buildat_app = (app::App*)lua_touserdata(L, -1);
	lua_pop(L, 1);
	Context *context = buildat_app->get_scene()->GetContext();

	SharedPtr<Model> fromScratchModel(
			interface::mesh::create_simple_voxel_model(context, w, h, d, data));

	StaticModel *object = node->GetOrCreateComponent<StaticModel>();
	object->SetModel(fromScratchModel);

	return 0;
}

// set_8bit_voxel_geometry(node, w, h, d, buffer: VectorBuffer)
static int l_set_8bit_voxel_geometry(lua_State *L)
{
	tolua_Error tolua_err;

	GET_TOLUA_STUFF(node, 1, Node);
	int w = lua_tointeger(L, 2);
	int h = lua_tointeger(L, 3);
	int d = lua_tointeger(L, 4);
	TRY_GET_TOLUA_STUFF(buf, 5, const VectorBuffer);

	log_d(MODULE, "set_8bit_voxel_geometry(): node=%p", node);
	log_d(MODULE, "set_8bit_voxel_geometry(): buf=%p", buf);

	ss_ data;
	if(buf == nullptr)
		data = lua_tocppstring(L, 5);
	else
		data.assign((const char*)&buf->GetBuffer()[0], buf->GetBuffer().Size());

	if((int)data.size() != w * h * d){
		log_e(MODULE, "set_8bit_voxel_geometry(): Data size does not match "
				"with dimensions (%zu vs. %i)", data.size(), w*h*d);
		return 0;
	}

	lua_getfield(L, LUA_REGISTRYINDEX, "__buildat_app");
	app::App *buildat_app = (app::App*)lua_touserdata(L, -1);
	lua_pop(L, 1);
	Context *context = buildat_app->get_scene()->GetContext();
	auto *voxel_reg = buildat_app->get_voxel_registry();
	auto *atlas_reg = buildat_app->get_atlas_registry();

	CustomGeometry *cg = node->GetOrCreateComponent<CustomGeometry>();

	interface::mesh::set_8bit_voxel_geometry(cg, context, w, h, d, data,
			voxel_reg, atlas_reg);

	// Maybe appropriate
	cg->SetOccluder(true);

	// TODO: Don't do this here; allow the caller to do this
	cg->SetCastShadows(true);

	return 0;
}

struct SetVoxelGeometryTask: public interface::worker_thread::Task
{
	Node *node;
	ss_ data;
	interface::VoxelRegistry *voxel_reg;
	interface::TextureAtlasRegistry *atlas_reg;

	up_<pv::RawVolume<VoxelInstance>> volume;
	sm_<uint, interface::mesh::TemporaryGeometry> temp_geoms;

	SetVoxelGeometryTask(Node *node, const ss_ &data,
			interface::VoxelRegistry *voxel_reg,
			interface::TextureAtlasRegistry *atlas_reg):
		node(node), data(data), voxel_reg(voxel_reg), atlas_reg(atlas_reg)
	{
		// NOTE: Do the pre-processing here so that the calling code can
		//       meaasure how long its execution takes
		// NOTE: Could be split in two calls
		volume = interface::deserialize_volume(data);
		interface::mesh::preload_textures(*volume, voxel_reg, atlas_reg);
	}
	// Called repeatedly from main thread until returns true
	bool pre()
	{
		return true;
	}
	// Called repeatedly from worker thread until returns true
	bool thread()
	{
		generate_voxel_geometry(temp_geoms, *volume, voxel_reg, atlas_reg);
		return true;
	}
	// Called repeatedly from main thread until returns true
	bool post()
	{
		Context *context = node->GetContext();
		CustomGeometry *cg = node->GetOrCreateComponent<CustomGeometry>();
		interface::mesh::set_voxel_geometry(cg, context, temp_geoms, atlas_reg);
		cg->SetOccluder(true);
		cg->SetCastShadows(true);
		return true;
	}
};

// set_voxel_geometry(node, buffer: VectorBuffer)
static int l_set_voxel_geometry(lua_State *L)
{
	tolua_Error tolua_err;

	GET_TOLUA_STUFF(node, 1, Node);
	TRY_GET_TOLUA_STUFF(buf, 2, const VectorBuffer);

	log_d(MODULE, "set_voxel_geometry(): node=%p", node);
	log_d(MODULE, "set_voxel_geometry(): buf=%p", buf);

	ss_ data;
	if(buf == nullptr)
		data = lua_tocppstring(L, 2);
	else
		data.assign((const char*)&buf->GetBuffer()[0], buf->GetBuffer().Size());

	lua_getfield(L, LUA_REGISTRYINDEX, "__buildat_app");
	app::App *buildat_app = (app::App*)lua_touserdata(L, -1);
	lua_pop(L, 1);
	auto *voxel_reg = buildat_app->get_voxel_registry();
	auto *atlas_reg = buildat_app->get_atlas_registry();

	up_<SetVoxelGeometryTask> task(new SetVoxelGeometryTask(
			node, data, voxel_reg, atlas_reg
	));

	auto *thread_pool = buildat_app->get_thread_pool();

	thread_pool->add_task(std::move(task));

	return 0;
}

struct SetVoxelLodGeometryTask: public interface::worker_thread::Task
{
	int lod;
	Node *node;
	ss_ data;
	interface::VoxelRegistry *voxel_reg;
	interface::TextureAtlasRegistry *atlas_reg;

	up_<pv::RawVolume<VoxelInstance>> lod_volume;
	sm_<uint, interface::mesh::TemporaryGeometry> temp_geoms;

	SetVoxelLodGeometryTask(int lod, Node *node, const ss_ &data,
			interface::VoxelRegistry *voxel_reg,
			interface::TextureAtlasRegistry *atlas_reg):
		lod(lod), node(node), data(data),
		voxel_reg(voxel_reg), atlas_reg(atlas_reg)
	{
		// NOTE: Do the pre-processing here so that the calling code can
		//       meaasure how long its execution takes
		// NOTE: Could be split in three calls
		up_<pv::RawVolume<VoxelInstance>> volume_orig =
				interface::deserialize_volume(data);
		lod_volume = interface::mesh::generate_voxel_lod_volume(
				lod, *volume_orig);
		interface::mesh::preload_textures(*lod_volume, voxel_reg, atlas_reg);
	}
	// Called repeatedly from main thread until returns true
	bool pre()
	{
		return true;
	}
	// Called repeatedly from worker thread until returns true
	bool thread()
	{
		generate_voxel_lod_geometry(
				lod, temp_geoms, *lod_volume, voxel_reg, atlas_reg);
		return true;
	}
	// Called repeatedly from main thread until returns true
	bool post()
	{
		Context *context = node->GetContext();
		CustomGeometry *cg = node->GetOrCreateComponent<CustomGeometry>();
		interface::mesh::set_voxel_lod_geometry(
				lod, cg, context, temp_geoms, atlas_reg);
		cg->SetOccluder(true);
		if(lod <= interface::MAX_LOD_WITH_SHADOWS)
			cg->SetCastShadows(true);
		else
			cg->SetCastShadows(false);
		return true;
	}
};

// set_voxel_lod_geometry(lod: number, node: Node, buffer: VectorBuffer)
static int l_set_voxel_lod_geometry(lua_State *L)
{
	tolua_Error tolua_err;

	int lod = lua_tointeger(L, 1);
	GET_TOLUA_STUFF(node, 2, Node);
	TRY_GET_TOLUA_STUFF(buf, 3, const VectorBuffer);

	log_d(MODULE, "set_voxel_lod_geometry(): lod=%i", lod);
	log_d(MODULE, "set_voxel_lod_geometry(): node=%p", node);
	log_d(MODULE, "set_voxel_lod_geometry(): buf=%p", buf);

	ss_ data;
	if(buf == nullptr)
		data = lua_tocppstring(L, 2);
	else
		data.assign((const char*)&buf->GetBuffer()[0], buf->GetBuffer().Size());

	lua_getfield(L, LUA_REGISTRYINDEX, "__buildat_app");
	app::App *buildat_app = (app::App*)lua_touserdata(L, -1);
	lua_pop(L, 1);
	auto *voxel_reg = buildat_app->get_voxel_registry();
	auto *atlas_reg = buildat_app->get_atlas_registry();

	up_<SetVoxelLodGeometryTask> task(new SetVoxelLodGeometryTask(
			lod, node, data, voxel_reg, atlas_reg
	));

	auto *thread_pool = buildat_app->get_thread_pool();

	thread_pool->add_task(std::move(task));

	return 0;
}

// clear_voxel_geometry(node: Node)
static int l_clear_voxel_geometry(lua_State *L)
{
	tolua_Error tolua_err;

	GET_TOLUA_STUFF(node, 1, Node);

	log_d(MODULE, "clear_voxel_geometry(): node=%p", node);

	CustomGeometry *cg = node->GetComponent<CustomGeometry>();
	if(cg)
		node->RemoveComponent(cg);
	//cg->Clear();
	//cg->Commit();

	return 0;
}

struct SetPhysicsBoxesTask: public interface::worker_thread::Task
{
	Node *node;
	ss_ data;
	interface::VoxelRegistry *voxel_reg;

	up_<pv::RawVolume<VoxelInstance>> volume;
	sv_<interface::mesh::TemporaryBox> result_boxes;

	SetPhysicsBoxesTask(Node *node, const ss_ &data,
			interface::VoxelRegistry *voxel_reg):
		node(node), data(data), voxel_reg(voxel_reg)
	{
		// NOTE: Do the pre-processing here so that the calling code can
		//       meaasure how long its execution takes
		// NOTE: Could be split in two calls
		volume = interface::deserialize_volume(data);
	}
	// Called repeatedly from main thread until returns true
	bool pre()
	{
		return true;
	}
	// Called repeatedly from worker thread until returns true
	bool thread()
	{
		interface::mesh::generate_voxel_physics_boxes(
				result_boxes, *volume, voxel_reg);
		return true;
	}
	// Called repeatedly from main thread until returns true
	int post_step = 1;
	bool post()
	{
		Context *context = node->GetContext();
		switch(post_step){
		case 1:
			node->GetOrCreateComponent<RigidBody>(LOCAL);
			break;
		case 2:
			interface::mesh::set_voxel_physics_boxes(
					node, context, result_boxes, false);
			break;
		case 3: {
			RigidBody *body = node->GetComponent<RigidBody>();
			if(body)
				body->OnSetEnabled();
			return true; }
		}
		post_step++;
		return false;
	}
};

// set_voxel_physics_boxes(node, buffer: VectorBuffer)
static int l_set_voxel_physics_boxes(lua_State *L)
{
	tolua_Error tolua_err;

	GET_TOLUA_STUFF(node, 1, Node);
	TRY_GET_TOLUA_STUFF(buf, 2, const VectorBuffer);

	log_d(MODULE, "set_voxel_physics_boxes(): node=%p", node);
	log_d(MODULE, "set_voxel_physics_boxes(): buf=%p", buf);

	ss_ data;
	if(buf == nullptr)
		data = lua_tocppstring(L, 2);
	else
		data.assign((const char*)&buf->GetBuffer()[0], buf->GetBuffer().Size());

	lua_getfield(L, LUA_REGISTRYINDEX, "__buildat_app");
	app::App *buildat_app = (app::App*)lua_touserdata(L, -1);
	lua_pop(L, 1);
	auto *voxel_reg = buildat_app->get_voxel_registry();

	up_<SetPhysicsBoxesTask> task(new SetPhysicsBoxesTask(
			node, data, voxel_reg
	));

	auto *thread_pool = buildat_app->get_thread_pool();

	thread_pool->add_task(std::move(task));

	return 0;
}

// clear_voxel_physics_boxes(node)
static int l_clear_voxel_physics_boxes(lua_State *L)
{
	tolua_Error tolua_err;

	GET_TOLUA_STUFF(node, 1, Node);

	log_d(MODULE, "clear_voxel_physics_boxes(): node=%p", node);

	RigidBody *body = node->GetComponent<RigidBody>();
	if(body)
		node->RemoveComponent(body);

	PODVector<CollisionShape*> previous_shapes;
	node->GetComponents<CollisionShape>(previous_shapes);
	for(size_t i = 0; i < previous_shapes.Size(); i++)
		node->RemoveComponent(previous_shapes[i]);

	return 0;
}

void init_voxel(lua_State *L)
{
#define DEF_BUILDAT_FUNC(name){ \
		lua_pushcfunction(L, l_##name); \
		lua_setglobal(L, "__buildat_" #name); \
}
	DEF_BUILDAT_FUNC(set_simple_voxel_model);
	DEF_BUILDAT_FUNC(set_8bit_voxel_geometry);
	DEF_BUILDAT_FUNC(set_voxel_geometry);
	DEF_BUILDAT_FUNC(set_voxel_lod_geometry);
	DEF_BUILDAT_FUNC(clear_voxel_geometry);
	DEF_BUILDAT_FUNC(set_voxel_physics_boxes);
	DEF_BUILDAT_FUNC(clear_voxel_physics_boxes);
}

} // namespace lua_bindingss

// vim: set noet ts=4 sw=4:
