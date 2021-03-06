#include "core/log.h"
#include "client_file/api.h"
#include "network/api.h"
#include "main_context/api.h"
#include "replicate/api.h"
#include "voxelworld/api.h"
#include "ground_plane_lighting/api.h"
#include "worldgen/api.h"
#include "interface/module.h"
#include "interface/server.h"
#include "interface/event.h"
#include "interface/mesh.h"
#include "interface/voxel.h"
#include "interface/noise.h"
#include "interface/voxel_volume.h"
#include <Scene.h>
#include <RigidBody.h>
#include <CollisionShape.h>
#include <ResourceCache.h>
#include <Context.h>
#include <StaticModel.h>
#include <Model.h>
#include <Material.h>
#include <Texture2D.h>
#include <Technique.h>
#include <cereal/archives/portable_binary.hpp>
#include <cereal/types/unordered_map.hpp>
#include <cereal/types/vector.hpp>
#define MODULE "main"

namespace magic = Urho3D;
namespace pv = PolyVox;

using interface::Event;
using interface::VoxelInstance;
using main_context::SceneReference;

// TODO: Move to a header (core/types_polyvox.h or something)
#define PV3I_FORMAT "(%i, %i, %i)"
#define PV3I_PARAMS(p) p.getX(), p.getY(), p.getZ()

// TODO: Move to a header (core/cereal_polyvox.h or something)
namespace cereal {

template<class Archive>
void save(Archive &archive, const pv::Vector3DInt32 &v){
	archive((int32_t)v.getX(), (int32_t)v.getY(), (int32_t)v.getZ());
}
template<class Archive>
void load(Archive &archive, pv::Vector3DInt32 &v){
	int32_t x, y, z;
	archive(x, y, z);
	v.setX(x); v.setY(y); v.setZ(z);
}

}

namespace main {

using namespace Urho3D;

struct Worldgen: public worldgen::GeneratorInterface
{
	void generate_section(interface::Server *server,
			SceneReference scene_ref,
			const pv::Vector3DInt16 &section_p)
	{
		voxelworld::access(server, [&](voxelworld::Interface *ivoxelworld)
		{
			voxelworld::Instance *world =
					ivoxelworld->get_instance(scene_ref);

			pv::Region region = world->get_section_region_voxels(
					section_p);

			auto lc = region.getLowerCorner();
			auto uc = region.getUpperCorner();

			log_t(MODULE, "on_generation_request(): lc: (%i, %i, %i)",
					lc.getX(), lc.getY(), lc.getZ());
			log_t(MODULE, "on_generation_request(): uc: (%i, %i, %i)",
					uc.getX(), uc.getY(), uc.getZ());

			interface::v3f spread(160, 160, 160);
			interface::NoiseParams np(0, 40, spread, 0, 7, 0.55);

			int w = uc.getX() - lc.getX() + 1;
			int d = uc.getZ() - lc.getZ() + 1;

			interface::Noise noise(&np, 3, w, d);
			noise.perlinMap2D(lc.getX() + spread.X/2, lc.getZ() + spread.Z/2);
			noise.transformNoiseMap(); // ?

			size_t noise_i = 0;
			for(int z = lc.getZ(); z <= uc.getZ(); z++){
				for(int x = lc.getX(); x <= uc.getX(); x++){
					double a = noise.result[noise_i];
					noise_i++;
					for(int y = lc.getY(); y <= uc.getY(); y++){
						pv::Vector3DInt32 p(x, y, z);
						pv::Vector3DInt32 cp(-112, 20, 253);
						if((p - cp).lengthSquared() < 30*30){
							world->set_voxel(p, VoxelInstance(1));
							continue;
						}
						if(y >= 2 && y <= 3 && z >= 256 && z <= 258 &&
								x >= -112 && x <= -5){
							world->set_voxel(p, VoxelInstance(1));
							continue;
						}
						if(z > 37 && z < 50 && y > 20){
							world->set_voxel(p, VoxelInstance(1));
							continue;
						}
						if(x > 27 && x < 40 && y > 20){
							world->set_voxel(p, VoxelInstance(1));
							continue;
						}
						if(x > 18 && x < 25 && z >= 32 && z <= 37 &&
								y > 20 && y < 25){
							world->set_voxel(p, VoxelInstance(1));
							continue;
						}
						if(y < a+5){
							world->set_voxel(p, VoxelInstance(2));
						} else if(y < a+10){
							world->set_voxel(p, VoxelInstance(3));
						} else if(y < a+11){
							world->set_voxel(p, VoxelInstance(4));
						} else {
							world->set_voxel(p, VoxelInstance(1));
						}
					}
				}
			}

			// Add random trees
			auto extent = uc - lc + pv::Vector3DInt32(1, 1, 1);
			int area = extent.getX() * extent.getZ();
			auto pr = interface::PseudoRandom(13241);
			for(int i = 0; i < area / 100; i++){
				int x = pr.range(lc.getX(), uc.getX());
				int z = pr.range(lc.getZ(), uc.getZ());

				/*int y = 50;
				for(; y>-50; y--){
					pv::Vector3DInt32 p(x, y, z);
					VoxelInstance v = world->get_voxel(p);
					if(v.get_id() != 1)
						break;
				}
				y++;*/
				size_t noise_i = (z-lc.getZ())*d + (x-lc.getX());
				double a = noise.result[noise_i];
				int y = a + 11.0;
				if(y < lc.getY() - 5 || y > uc.getY() - 5)
					continue;

				for(int y1 = y; y1<y+4; y1++){
					pv::Vector3DInt32 p(x, y1, z);
					world->set_voxel(p, VoxelInstance(6), true);
				}

				for(int x1 = x-2; x1 <= x+2; x1++){
					for(int y1 = y+3; y1 <= y+7; y1++){
						for(int z1 = z-2; z1 <= z+2; z1++){
							pv::Vector3DInt32 p(x1, y1, z1);
							world->set_voxel(p, VoxelInstance(5), true);
						}
					}
				}
			}
		});
	}
};

struct Module: public interface::Module
{
	interface::Server *m_server;

	SceneReference m_main_scene;

	Module(interface::Server *server):
		interface::Module(MODULE),
		m_server(server)
	{
	}

	~Module()
	{}

	void init()
	{
		m_server->sub_event(this, Event::t("core:start"));
		m_server->sub_event(this, Event::t("core:unload"));
		m_server->sub_event(this, Event::t("core:continue"));
		m_server->sub_event(this, Event::t("core:tick"));
		m_server->sub_event(this, Event::t("client_file:files_transmitted"));
		m_server->sub_event(this, Event::t(
				"network:packet_received/main:place_voxel"));
		m_server->sub_event(this, Event::t(
				"network:packet_received/main:dig_voxel"));
		m_server->sub_event(this, Event::t("worldgen:queue_modified"));
	}

	void event(const Event::Type &type, const Event::Private *p)
	{
		EVENT_VOIDN("core:start", on_start)
		EVENT_VOIDN("core:unload", on_unload)
		EVENT_VOIDN("core:continue", on_continue)
		EVENT_TYPEN("core:tick", on_tick, interface::TickEvent)
		EVENT_TYPEN("client_file:files_transmitted",
				on_files_transmitted, client_file::FilesTransmitted)
		EVENT_TYPEN("network:packet_received/main:place_voxel",
				on_place_voxel, network::Packet)
		EVENT_TYPEN("network:packet_received/main:dig_voxel",
				on_dig_voxel, network::Packet)
		EVENT_TYPEN("worldgen:queue_modified",
				on_worldgen_queue_modified, worldgen::QueueModifiedEvent);
	}

	void on_start()
	{
		main_context::access(m_server, [&](main_context::Interface *imc){
			m_main_scene = imc->create_scene();
		});

		// Worldgen must be created before the voxelworld; otherwise initial
		// generation request events are lost
		worldgen::access(m_server, [&](worldgen::Interface *iworldgen)
		{
			iworldgen->create_instance(m_main_scene);

			auto instance = iworldgen->get_instance(m_main_scene);
			instance->set_generator(new Worldgen());
		});

		voxelworld::access(m_server, [&](voxelworld::Interface *ivoxelworld)
		{
			//pv::Region region(0, 0, 0, 0, 0, 0); // Use this for valgrind
			//pv::Region region(-1, 0, -1, 1, 0, 1);
			//pv::Region region(-1, -1, -1, 1, 1, 1);
			//pv::Region region(-2, -1, -2, 2, 1, 2);
			//pv::Region region(-3, -1, -3, 3, 1, 3);
			pv::Region region(-5, -1, 0, 0, 1, 5);
			//pv::Region region(-5, -1, -5, 5, 1, 5);
			//pv::Region region(-6, -1, -6, 6, 1, 6);
			//pv::Region region(-8, -1, -8, 8, 1, 8);
			ivoxelworld->create_instance(m_main_scene, region);
		});

		ground_plane_lighting::access(m_server,
				[&](ground_plane_lighting::Interface *igpl)
		{
			igpl->create_instance(m_main_scene);
		});

		// Define voxels on core:start (woxelworld will restore them on reload)
		voxelworld::access(m_server, [&](voxelworld::Interface *ivoxelworld)
		{
			voxelworld::Instance *world =
					ivoxelworld->get_instance(m_main_scene);
			interface::VoxelRegistry *voxel_reg = world->get_voxel_reg();
			{
				interface::VoxelDefinition vdef;
				vdef.name.block_name = "air";
				vdef.name.segment_x = 0;
				vdef.name.segment_y = 0;
				vdef.name.segment_z = 0;
				vdef.name.rotation_primary = 0;
				vdef.name.rotation_secondary = 0;
				vdef.handler_module = "";
				for(size_t i = 0; i < 6; i++){
					interface::AtlasSegmentDefinition &seg = vdef.textures[i];
					seg.resource_name = "";
					seg.total_segments = magic::IntVector2(0, 0);
					seg.select_segment = magic::IntVector2(0, 0);
				}
				vdef.edge_material_id = interface::EDGEMATERIALID_EMPTY;
				voxel_reg->add_voxel(vdef); // id 1
			}
			{
				interface::VoxelDefinition vdef;
				vdef.name.block_name = "rock";
				vdef.name.segment_x = 0;
				vdef.name.segment_y = 0;
				vdef.name.segment_z = 0;
				vdef.name.rotation_primary = 0;
				vdef.name.rotation_secondary = 0;
				vdef.handler_module = "";
				for(size_t i = 0; i < 6; i++){
					interface::AtlasSegmentDefinition &seg = vdef.textures[i];
					seg.resource_name = "main/rock.png";
					seg.total_segments = magic::IntVector2(1, 1);
					seg.select_segment = magic::IntVector2(0, 0);
				}
				vdef.edge_material_id = interface::EDGEMATERIALID_GROUND;
				vdef.physically_solid = true;
				voxel_reg->add_voxel(vdef); // id 2
			}
			{
				interface::VoxelDefinition vdef;
				vdef.name.block_name = "dirt";
				vdef.name.segment_x = 0;
				vdef.name.segment_y = 0;
				vdef.name.segment_z = 0;
				vdef.name.rotation_primary = 0;
				vdef.name.rotation_secondary = 0;
				vdef.handler_module = "";
				for(size_t i = 0; i < 6; i++){
					interface::AtlasSegmentDefinition &seg = vdef.textures[i];
					seg.resource_name = "main/dirt.png";
					seg.total_segments = magic::IntVector2(1, 1);
					seg.select_segment = magic::IntVector2(0, 0);
				}
				vdef.edge_material_id = interface::EDGEMATERIALID_GROUND;
				vdef.physically_solid = true;
				voxel_reg->add_voxel(vdef); // id 3
			}
			{
				interface::VoxelDefinition vdef;
				vdef.name.block_name = "grass";
				vdef.name.segment_x = 0;
				vdef.name.segment_y = 0;
				vdef.name.segment_z = 0;
				vdef.name.rotation_primary = 0;
				vdef.name.rotation_secondary = 0;
				vdef.handler_module = "";
				for(size_t i = 0; i < 6; i++){
					interface::AtlasSegmentDefinition &seg = vdef.textures[i];
					seg.resource_name = "main/grass.png";
					seg.total_segments = magic::IntVector2(1, 1);
					seg.select_segment = magic::IntVector2(0, 0);
				}
				vdef.edge_material_id = interface::EDGEMATERIALID_GROUND;
				vdef.physically_solid = true;
				voxel_reg->add_voxel(vdef); // id 4
			}
			{
				interface::VoxelDefinition vdef;
				vdef.name.block_name = "leaves";
				vdef.name.segment_x = 0;
				vdef.name.segment_y = 0;
				vdef.name.segment_z = 0;
				vdef.name.rotation_primary = 0;
				vdef.name.rotation_secondary = 0;
				vdef.handler_module = "";
				for(size_t i = 0; i < 6; i++){
					interface::AtlasSegmentDefinition &seg = vdef.textures[i];
					seg.resource_name = "main/leaves.png";
					seg.total_segments = magic::IntVector2(1, 1);
					seg.select_segment = magic::IntVector2(0, 0);
				}
				vdef.edge_material_id = interface::EDGEMATERIALID_GROUND;
				vdef.physically_solid = true;
				voxel_reg->add_voxel(vdef); // id 5
			}
			{
				interface::VoxelDefinition vdef;
				vdef.name.block_name = "tree";
				vdef.name.segment_x = 0;
				vdef.name.segment_y = 0;
				vdef.name.segment_z = 0;
				vdef.name.rotation_primary = 0;
				vdef.name.rotation_secondary = 0;
				vdef.handler_module = "";
				for(size_t i = 0; i < 6; i++){
					interface::AtlasSegmentDefinition &seg = vdef.textures[i];
					seg.resource_name = "main/tree.png";
					seg.total_segments = magic::IntVector2(1, 1);
					seg.select_segment = magic::IntVector2(0, 0);
				}
				vdef.textures[0].resource_name = "main/tree_top.png";
				vdef.textures[1].resource_name = "main/tree_top.png";
				vdef.edge_material_id = interface::EDGEMATERIALID_GROUND;
				vdef.physically_solid = true;
				voxel_reg->add_voxel(vdef); // id 6
			}
		});

		// Enable world generation now that the voxels are defined
		worldgen::access(m_server, m_main_scene, [&](worldgen::Instance *instance)
		{
			instance->enable();
		});

		voxelworld::access(m_server, m_main_scene,
				[&](voxelworld::Instance *instance)
		{
			main_context::access(m_server, [&](main_context::Interface *imc)
			{
				Scene *scene = imc->check_scene(m_main_scene);
				Context *context = imc->get_context();
				ResourceCache *cache = context->GetSubsystem<ResourceCache>();

				interface::VoxelRegistry *voxel_reg =
						instance->get_voxel_reg();

				Node *n = scene->CreateChild("Testbox");
				n->SetPosition(Vector3(30.0f, 30.0f, 40.0f));
				n->SetScale(Vector3(1.0f, 1.0f, 1.0f));

				/*int w = 1, h = 1, d = 1;
				ss_ data = "1";*/
				int w = 2, h = 2, d = 1;
				ss_ data = "1333";

				// Convert data to the actually usable voxel type id namespace
				// starting from VOXELTYPEID_UNDEFINED=0
				for(size_t i = 0; i < data.size(); i++){
					data[i] = data[i] - '0';
				}

				n->SetVar(StringHash("simple_voxel_data"), Variant(
						PODVector<uint8_t>((const uint8_t*)data.c_str(),
						data.size())));
				n->SetVar(StringHash("simple_voxel_w"), Variant(w));
				n->SetVar(StringHash("simple_voxel_h"), Variant(h));
				n->SetVar(StringHash("simple_voxel_d"), Variant(d));


				// Load the same model in here and give it to the physics
				// subsystem so that it can be collided to
				SharedPtr<Model> model(interface::mesh::
						create_8bit_voxel_physics_model(context, w, h, d, data,
						voxel_reg));

				RigidBody *body = n->CreateComponent<RigidBody>(LOCAL);
				body->SetFriction(0.75f);
				body->SetMass(1.0);
				CollisionShape *shape =
						n->CreateComponent<CollisionShape>(LOCAL);
				shape->SetConvexHull(model, 0, Vector3::ONE);
				//shape->SetTriangleMesh(model, 0, Vector3::ONE);
				//shape->SetBox(Vector3::ONE);
			});
		});
	}

	void on_unload()
	{
		// TODO: Store main scene reference
		// Just do this for now
		main_context::access(m_server, [&](main_context::Interface *imc){
			imc->delete_scene(m_main_scene);
		});
	}

	void on_continue()
	{
		// TODO: Restore main scene reference
		// Just do this for now
		on_start();
	}

	void on_tick(const interface::TickEvent &event)
	{
		/*main_context::access(m_server, [&](main_context::Interface *imc)
		{
			Scene *scene = imc->check_scene(m_main_scene);
			Node *n = scene->GetChild("Testbox");
			auto p = n->GetPosition();
			log_v(MODULE, "Testbox: (%f, %f, %f)", p.x_, p.y_, p.z_);
		});*/
		static uint a = 0;
		if(((a++) % 150) == 0){
			main_context::access(m_server, [&](main_context::Interface *imc)
			{
				Scene *scene = imc->check_scene(m_main_scene);
				Node *n = scene->GetChild("Testbox");
				if(n){
					n->SetRotation(Quaternion(30, 60, 90));
					n->SetPosition(Vector3(30.0f, 30.0f, 40.0f));
				}
			});
		}
	}

	void on_files_transmitted(const client_file::FilesTransmitted &event)
	{
		replicate::access(m_server, [&](replicate::Interface *ireplicate){
			ireplicate->assign_scene_to_peer(m_main_scene, event.recipient);
		});
		network::access(m_server, [&](network::Interface *inetwork){
			inetwork->send(event.recipient, "core:run_script",
					"buildat.run_script_file(\"main/init.lua\")");
		});
	}

	void on_place_voxel(const network::Packet &packet)
	{
		pv::Vector3DInt32 voxel_p;
		{
			std::istringstream is(packet.data, std::ios::binary);
			cereal::PortableBinaryInputArchive ar(is);
			ar(voxel_p);
		}
		log_v(MODULE, "C%i: on_place_voxel(): p=" PV3I_FORMAT,
				packet.sender, PV3I_PARAMS(voxel_p));

		voxelworld::access(m_server, m_main_scene,
				[&](voxelworld::Instance *instance)
		{
			instance->set_voxel(voxel_p, VoxelInstance(2));
		});
	}

	void on_dig_voxel(const network::Packet &packet)
	{
		pv::Vector3DInt32 voxel_p;
		{
			std::istringstream is(packet.data, std::ios::binary);
			cereal::PortableBinaryInputArchive ar(is);
			ar(voxel_p);
		}
		log_v(MODULE, "C%i: on_dig_voxel(): p=" PV3I_FORMAT,
				packet.sender, PV3I_PARAMS(voxel_p));

		voxelworld::access(m_server, m_main_scene,
				[&](voxelworld::Instance *instance)
		{
			instance->set_voxel(voxel_p, VoxelInstance(1));
		});
	}

	void on_worldgen_queue_modified(const worldgen::QueueModifiedEvent &event)
	{
		log_t(MODULE, "on_worldgen_queue_modified()");
		network::access(m_server, [&](network::Interface *inetwork){
			sv_<network::PeerInfo::Id> peers = inetwork->list_peers();
			for(auto &peer: peers){
				inetwork->send(peer, "main:worldgen_queue_size",
						itos(event.queue_size));
			}
		});
	}
};

extern "C" {
	BUILDAT_EXPORT void* createModule_main(interface::Server *server){
		return (void*)(new Module(server));
	}
}
}
// vim: set noet ts=4 sw=4:
