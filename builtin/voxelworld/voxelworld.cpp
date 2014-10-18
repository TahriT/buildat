// http://www.apache.org/licenses/LICENSE-2.0
// Copyright 2014 Perttu Ahola <celeron55@gmail.com>
#include "voxelworld/api.h"
#include "network/api.h"
#include "client_file/api.h"
#include "replicate/api.h"
#include "core/log.h"
#include "interface/module.h"
#include "interface/server.h"
#include "interface/event.h"
#include "interface/mesh.h"
#include "interface/voxel.h"
#include "interface/block.h"
#include "interface/voxel_volume.h"
#include <PolyVoxCore/RawVolume.h>
#include <cereal/archives/portable_binary.hpp>
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wstrict-aliasing"
#include <Node.h>
#include <Scene.h>
#include <Model.h>
#include <RigidBody.h>
#include <CollisionShape.h>
#include <Context.h>
#include <ResourceCache.h>
#include <Light.h>
#include <Geometry.h>
#pragma GCC diagnostic pop
#include <deque>
#include <algorithm>

using interface::Event;
namespace magic = Urho3D;
namespace pv = PolyVox;
using namespace Urho3D;
using interface::VoxelInstance;

namespace std {

template<> struct hash<pv::Vector<2u, int16_t>>{
	std::size_t operator()(const pv::Vector<2u, int16_t> &v) const {
		return ((std::hash<int16_t>() (v.getX()) << 0) ^
				   (std::hash<int16_t>() (v.getY()) << 1));
	}
};

template<> struct hash<pv::Vector<3u, int16_t>>{
	std::size_t operator()(const pv::Vector<3u, int16_t> &v) const {
		return ((std::hash<int16_t>() (v.getX()) << 0) ^
				   (std::hash<int16_t>() (v.getY()) << 1) ^
				   (std::hash<int16_t>() (v.getZ()) << 2));
	}
};

}

// TODO: Move to a header (core/cereal_polyvox.h or something)
namespace cereal {

template<class Archive>
void save(Archive &archive, const pv::Vector3DInt16 &v){
	archive((int32_t)v.getX(), (int32_t)v.getY(), (int32_t)v.getZ());
}
template<class Archive>
void load(Archive &archive, pv::Vector3DInt16 &v){
	int32_t x, y, z;
	archive(x, y, z);
	v.setX(x); v.setY(y); v.setZ(z);
}

}

// PolyVox logging helpers
// TODO: Move to a header (core/types_polyvox.h or something)
template<>
ss_ dump(const pv::Vector3DInt16 &v){
	std::ostringstream os(std::ios::binary);
	os<<"("<<v.getX()<<", "<<v.getY()<<", "<<v.getZ()<<")";
	return os.str();
}
template<>
ss_ dump(const pv::Vector3DInt32 &v){
	std::ostringstream os(std::ios::binary);
	os<<"("<<v.getX()<<", "<<v.getY()<<", "<<v.getZ()<<")";
	return os.str();
}
#define PV3I_FORMAT "(%i, %i, %i)"
#define PV3I_PARAMS(p) p.getX(), p.getY(), p.getZ()

// TODO: Move to a header (core/numeric.h or something)
static inline int container_coord(int x, int d)
{
	return (x>=0 ? x : x-d+1) / d;
}
static inline pv::Vector3DInt32 container_coord(
		const pv::Vector3DInt32 &p, const pv::Vector3DInt32 &d)
{
	return pv::Vector3DInt32(
			container_coord(p.getX(), d.getX()),
			container_coord(p.getY(), d.getY()),
			container_coord(p.getZ(), d.getZ()));
}
static inline pv::Vector3DInt32 container_coord(
		const pv::Vector3DInt32 &p, const pv::Vector3DInt16 &d)
{
	return pv::Vector3DInt32(
			container_coord(p.getX(), d.getX()),
			container_coord(p.getY(), d.getY()),
			container_coord(p.getZ(), d.getZ()));
}
static inline pv::Vector3DInt16 container_coord16(
		const pv::Vector3DInt32 &p, const pv::Vector3DInt16 &d)
{
	return pv::Vector3DInt16(
			container_coord(p.getX(), d.getX()),
			container_coord(p.getY(), d.getY()),
			container_coord(p.getZ(), d.getZ()));
}

namespace voxelworld {

struct ChunkBuffer
{
	sp_<pv::RawVolume<VoxelInstance>> volume;
	bool dirty = false; // If false, buffer has only been read from so far
};

struct Section
{
	pv::Vector3DInt16 section_p; // Position in sections
	pv::Vector3DInt16 chunk_size;
	pv::Region contained_chunks; // Position and size in chunks
	// Static voxel nodes (each contains one chunk); Initialized to 0.
	sp_<pv::RawVolume<uint32_t>> node_ids;
	size_t num_chunks = 0;
	// Cache these for speed
	int w_chunks = 0;
	int h_chunks = 0;
	int d_chunks = 0;

	// Chunk buffers (index using get_chunk_i())
	sv_<ChunkBuffer> chunk_buffers;

	// TODO: Specify what exactly do these mean and how they are used
	bool loaded = false;
	bool save_enabled = false;
	bool generated = false;

	Section(): // Needed for containers
		chunk_size(0, 0, 0) // This is used to detect uninitialized instance
	{}
	Section(pv::Vector3DInt16 section_p,
			pv::Vector3DInt16 chunk_size,
			pv::Region contained_chunks):
		section_p(section_p),
		chunk_size(chunk_size),
		contained_chunks(contained_chunks),
		node_ids(new pv::RawVolume<uint32_t>(contained_chunks)),
		num_chunks(contained_chunks.getWidthInVoxels() *
			contained_chunks.getHeightInVoxels() *
			contained_chunks.getDepthInVoxels())
	{
		chunk_buffers.resize(num_chunks);
		// Cache these for speed
		w_chunks = contained_chunks.getWidthInVoxels();
		h_chunks = contained_chunks.getHeightInVoxels();
		d_chunks = contained_chunks.getDepthInVoxels();
	}

	size_t get_chunk_i(const pv::Vector3DInt32 &chunk_p); // global chunk_p
	pv::Vector3DInt32 get_chunk_p(size_t chunk_p);

	ChunkBuffer& get_buffer(const pv::Vector3DInt32 &chunk_p,
			interface::Server *server);
};

size_t Section::get_chunk_i(const pv::Vector3DInt32 &chunk_p) // global chunk_p
{
	auto &lc = contained_chunks.getLowerCorner();
	// NOTE: pv::Vector3DInt32 operators and getters are too slow
	int local_x = chunk_p.getX() - lc.getX();
	int local_y = chunk_p.getY() - lc.getY();
	int local_z = chunk_p.getZ() - lc.getZ();
	const int &w = w_chunks;
	const int &h = h_chunks;
	size_t i = local_z * h * w + local_y * w + local_x;
	if(i >= num_chunks) // NOTE: This is not accurate but it is safe and fast
		throw Exception(ss_()+"get_chunk_i: Section "+cs(section_p)+
					  " does not contain chunk"+cs(chunk_p));
	return i;
}

pv::Vector3DInt32 Section::get_chunk_p(size_t chunk_i)
{
	const int &w = w_chunks;
	const int &h = h_chunks;
	pv::Vector3DInt32 p;
	p.setZ(chunk_i / h / w);
	p.setY(chunk_i / w - p.getZ() * h);
	p.setX(chunk_i - p.getZ() * h * w - p.getY() * w);
	return contained_chunks.getLowerCorner() + p;
}

ChunkBuffer& Section::get_buffer(const pv::Vector3DInt32 &chunk_p,
		interface::Server *server)
{
	size_t chunk_i = get_chunk_i(chunk_p);
	ChunkBuffer &buf = chunk_buffers[chunk_i];
	// If loaded, return right away
	if(buf.volume)
		return buf;
	// Not loaded.
	// Get the static voxel node from the scene and read the volume from it
	int32_t node_id = node_ids->getVoxelAt(chunk_p);
	if(node_id == 0){
		log_w("voxelworld", "Section::get_buffer(): No node found for chunk "
				PV3I_FORMAT " in section " PV3I_FORMAT,
				PV3I_PARAMS(chunk_p), PV3I_PARAMS(section_p));
		return buf;
	}
	server->access_scene([&](Scene *scene)
	{
		Node *n = scene->GetNode(node_id);
		if(!n){
			log_w("voxelworld",
					"Section::get_buffer(): Node %i not found in scene "
					"for chunk " PV3I_FORMAT " in section " PV3I_FORMAT,
					node_id, PV3I_PARAMS(chunk_p), PV3I_PARAMS(section_p));
			return;
		}
		const Variant &var = n->GetVar(StringHash("buildat_voxel_data"));
		const PODVector<unsigned char> &rawbuf = var.GetBuffer();
		ss_ data((const char*)&rawbuf[0], rawbuf.Size());
		up_<pv::RawVolume<VoxelInstance>> volume =
			interface::deserialize_volume(data);
		buf.volume = sp_<pv::RawVolume<VoxelInstance>>(std::move(volume));
		if(!buf.volume){
			log_w("voxelworld",
					"Section::get_buffer(): Voxel volume could not be "
					"loaded from node %i for chunk "
					PV3I_FORMAT " in section " PV3I_FORMAT,
					node_id, PV3I_PARAMS(chunk_p), PV3I_PARAMS(section_p));
			return;
		}
	});
	return buf;
}

struct QueuedNodePhysicsUpdate
{
	uint node_id = 0;
	sp_<pv::RawVolume<VoxelInstance>> volume;

	QueuedNodePhysicsUpdate(const uint &node_id,
			sp_<pv::RawVolume<VoxelInstance>> volume):
		node_id(node_id), volume(volume){}
	bool operator>(const QueuedNodePhysicsUpdate &other) const {
		return node_id > other.node_id;
	}
};

struct Module: public interface::Module, public voxelworld::Interface
{
	interface::Server *m_server;

	// Accessing any of these outside of Server::access_scene is disallowed
	sp_<interface::TextureAtlasRegistry> m_atlas_reg;
	sp_<interface::VoxelRegistry> m_voxel_reg;
	sp_<interface::BlockRegistry> m_block_reg;

	// One node holds one chunk of voxels (eg. 24x24x24)
	pv::Vector3DInt16 m_chunk_size_voxels = pv::Vector3DInt16(32, 32, 32);
	//pv::Vector3DInt16 m_chunk_size_voxels = pv::Vector3DInt16(24, 24, 24);
	//pv::Vector3DInt16 m_chunk_size_voxels = pv::Vector3DInt16(16, 16, 16);
	//pv::Vector3DInt16 m_chunk_size_voxels = pv::Vector3DInt16(8, 8, 8);

	// The world is loaded and unloaded by sections (eg. 2x2x2)
	pv::Vector3DInt16 m_section_size_chunks = pv::Vector3DInt16(2, 2, 2);

	// Sections (this(y,z)=sector, sector(x)=section)
	sm_<pv::Vector<2, int16_t>, sm_<int16_t, Section>> m_sections;
	// Cache of last used sections (add to end, remove from beginning)
	std::deque<Section*> m_last_used_sections;

	// Set of sections that have buffers allocated
	// (as a sorted array in descending order)
	std::vector<Section*> m_sections_with_loaded_buffers;

	// Set of nodes by node_id that need set_voxel_physics_boxes()
	// (as a sorted array in descending node_id order)
	std::vector<QueuedNodePhysicsUpdate> m_nodes_needing_physics_update;

	Module(interface::Server *server):
		interface::Module("voxelworld"),
		m_server(server)
	{
		m_voxel_reg.reset(interface::createVoxelRegistry());
		m_block_reg.reset(interface::createBlockRegistry(m_voxel_reg.get()));
	}

	~Module()
	{
	}

	void init()
	{
		m_server->sub_event(this, Event::t("core:start"));
		m_server->sub_event(this, Event::t("core:unload"));
		m_server->sub_event(this, Event::t("core:continue"));
		m_server->sub_event(this, Event::t("network:client_connected"));
		m_server->sub_event(this, Event::t("core:tick"));
		m_server->sub_event(this, Event::t("client_file:files_transmitted"));
		m_server->sub_event(this, Event::t(
					"network:packet_received/voxelworld:get_section"));
		m_server->sub_event(this, Event::t("voxelworld:node_voxel_data_updated"));

		m_server->access_scene([&](Scene *scene)
		{
			Context *context = scene->GetContext();

			m_atlas_reg.reset(interface::createTextureAtlasRegistry(context));
		});

		// Add some test stuff
		// TODO: Remove
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
			m_voxel_reg->add_voxel(vdef); // id 1
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
			m_voxel_reg->add_voxel(vdef); // id 2
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
			m_voxel_reg->add_voxel(vdef); // id 3
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
			m_voxel_reg->add_voxel(vdef); // id 4
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
			m_voxel_reg->add_voxel(vdef); // id 5
		}
	}

	void event(const Event::Type &type, const Event::Private *p)
	{
		EVENT_VOIDN("core:start", on_start)
		EVENT_VOIDN("core:unload", on_unload)
		EVENT_VOIDN("core:continue", on_continue)
		EVENT_TYPEN("network:client_connected", on_client_connected,
				network::NewClient)
		EVENT_TYPEN("core:tick", on_tick, interface::TickEvent)
		EVENT_TYPEN("client_file:files_transmitted", on_files_transmitted,
				client_file::FilesTransmitted)
		EVENT_TYPEN("network:packet_received/voxelworld:get_section",
				on_get_section, network::Packet)
		EVENT_TYPEN("voxelworld:node_voxel_data_updated",
				on_node_voxel_data_updated, voxelworld::NodeVoxelDataUpdatedEvent)
	}

	void on_start()
	{
		// TODO: Load from disk or something

		//pv::Region region(-1, 0, -1, 1, 0, 1);
		pv::Region region(-1, -1, -1, 1, 1, 1);
		//pv::Region region(-2, -1, -2, 2, 1, 2);
		//pv::Region region(-3, -1, -3, 3, 1, 3);
		//pv::Region region(-5, -1, -5, 5, 1, 5);
		//pv::Region region(-6, -1, -6, 6, 1, 6);
		//pv::Region region(-8, -1, -8, 8, 1, 8);
		auto lc = region.getLowerCorner();
		auto uc = region.getUpperCorner();
		for(int z = lc.getZ(); z <= uc.getZ(); z++){
			for(int y = lc.getY(); y <= uc.getY(); y++){
				for(int x = lc.getX(); x <= uc.getX(); x++){
					load_or_generate_section(pv::Vector3DInt16(x, y, z));
				}
			}
		}
	}

	void unload_node(Scene *scene, uint node_id)
	{
		log_v(MODULE, "Unloading node %i", node_id);
		Node *n = scene->GetNode(node_id);
		if(!n){
			log_w(MODULE, "Cannot unload node %i: Not found in scene", node_id);
			return;
		}
		n->RemoveAllComponents();
		n->Remove();
	}

	void on_unload()
	{
		log_v(MODULE, "on_unload()");

		commit();

		// Remove everything managed by us from the scene
		m_server->access_scene([&](Scene *scene)
		{
			for(auto &sector_pair: m_sections){
				for(auto &section_pair: sector_pair.second){
					Section &section = section_pair.second;

					auto region = section.node_ids->getEnclosingRegion();
					auto lc = region.getLowerCorner();
					auto uc = region.getUpperCorner();
					for(int z = lc.getZ(); z <= uc.getZ(); z++){
						for(int y = lc.getY(); y <= uc.getY(); y++){
							for(int x = lc.getX(); x <= uc.getX(); x++){
								uint id = section.node_ids->getVoxelAt(x, y, z);
								section.node_ids->setVoxelAt(x, y, z, 0);
								unload_node(scene, id);
							}
						}
					}
				}
			}
		});
	}

	void on_continue()
	{
		on_start();
	}

	void on_client_connected(const network::NewClient &client_connected)
	{
	}

	void on_client_disconnected(const network::OldClient &old_client)
	{
	}

	void on_tick(const interface::TickEvent &event)
	{
		m_server->access_scene([&](Scene *scene)
		{
			Context *context = scene->GetContext();

			// Update node collision boxes
			if(!m_nodes_needing_physics_update.empty()){
				log_v(MODULE, "on_tick(): Doing %zu lazy node physics updates",
						m_nodes_needing_physics_update.size());
			}
			for(QueuedNodePhysicsUpdate &update: m_nodes_needing_physics_update){
				uint node_id = update.node_id;
				sp_<pv::RawVolume<VoxelInstance>> volume = update.volume;
				Node *n = scene->GetNode(node_id);
				if(!n){
					log_w(MODULE, "on_tick(): Node physics update: "
							"Node %i not found", node_id);
					return;
				}
				// Update collision shape
				interface::mesh::set_voxel_physics_boxes(n, context, *volume,
						m_voxel_reg.get());
			}
			m_nodes_needing_physics_update.clear();
		});
	}

	void on_files_transmitted(const client_file::FilesTransmitted &event)
	{
		int peer = event.recipient;
		network::access(m_server, [&](network::Interface *inetwork){
			inetwork->send(peer, "core:run_script",
					"require(\"buildat/module/voxelworld\").init()");
		});
		std::ostringstream os(std::ios::binary);
		{
			cereal::PortableBinaryOutputArchive ar(os);
			ar(m_chunk_size_voxels);
			ar(m_section_size_chunks);
		}
		network::access(m_server, [&](network::Interface *inetwork){
			inetwork->send(peer, "voxelworld:init", os.str());
		});
	}

	// TODO: How should nodes be filtered for replication?
	// TODO: Generally the client wants roughly one section, but isn't
	//       positioned at the middle of a section
	void on_get_section(const network::Packet &packet)
	{
		pv::Vector3DInt16 section_p;
		{
			std::istringstream is(packet.data, std::ios::binary);
			cereal::PortableBinaryInputArchive ar(is);
			ar(section_p);
		}
		log_v(MODULE, "C%i: on_get_section(): " PV3I_FORMAT,
				packet.sender, PV3I_PARAMS(section_p));
	}

	void on_node_voxel_data_updated(const NodeVoxelDataUpdatedEvent &event)
	{
		// NOTE: This delayed event is used so that when this is received,
		//       replicate has already sent the data to clients

		// Notify clients that know the node
		sv_<replicate::PeerId> peers;
		replicate::access(m_server, [&](replicate::Interface *ireplicate){
			peers = ireplicate->find_peers_that_know_node(event.node_id);
		});
		std::ostringstream os(std::ios::binary);
		{
			cereal::PortableBinaryOutputArchive ar(os);
			ar((int32_t)event.node_id);
		}
		network::access(m_server, [&](network::Interface *inetwork){
			for(auto &peer_id: peers){
				inetwork->send(peer_id, "voxelworld:node_voxel_data_updated",
						os.str());
			}
		});
	}

	// Get section if exists
	Section* get_section(const pv::Vector3DInt16 &section_p)
	{
		// Check cache
		for(Section *section : m_last_used_sections){
			if(section->section_p == section_p)
				return section;
		}
		// Not in cache
		pv::Vector<2, int16_t> p_yz(section_p.getY(), section_p.getZ());
		auto sector_it = m_sections.find(p_yz);
		if(sector_it == m_sections.end())
			return nullptr;
		sm_<int16_t, Section> &sector = sector_it->second;
		auto section_it = sector.find(section_p.getX());
		if(section_it == sector.end())
			return nullptr;
		Section &section = section_it->second;
		// Add to cache and return
		m_last_used_sections.push_back(&section);
		if(m_last_used_sections.size() > 2) // 2 is maybe optimal-ish
			m_last_used_sections.pop_front();
		return &section;
	}

	// Get a section; allocate it if it doesn't exist yet
	Section& force_get_section(const pv::Vector3DInt16 &section_p)
	{
		pv::Vector<2, int16_t> p_yz(section_p.getY(), section_p.getZ());
		sm_<int16_t, Section> &sector = m_sections[p_yz];
		Section &section = sector[section_p.getX()];
		if(section.chunk_size.getX() == 0){
			// Initialize newly created section properly
			pv::Region contained_chunks(
					section_p.getX() * m_section_size_chunks.getX(),
					section_p.getY() * m_section_size_chunks.getY(),
					section_p.getZ() * m_section_size_chunks.getZ(),
					(section_p.getX()+1) * m_section_size_chunks.getX() - 1,
					(section_p.getY()+1) * m_section_size_chunks.getY() - 1,
					(section_p.getZ()+1) * m_section_size_chunks.getZ() - 1
			);
			section = Section(section_p, m_chunk_size_voxels, contained_chunks);
		}
		return section;
	}

	void create_chunk_node(Scene *scene, Section &section, int x, int y, int z)
	{
		Context *context = scene->GetContext();

		pv::Vector3DInt16 section_p = section.section_p;
		pv::Vector3DInt32 chunk_p(
				section_p.getX() * m_section_size_chunks.getX() + x,
				section_p.getY() * m_section_size_chunks.getY() + y,
				section_p.getZ() * m_section_size_chunks.getZ() + z
		);

		Vector3 node_p(
				chunk_p.getX() * m_chunk_size_voxels.getX() +
				m_chunk_size_voxels.getX() / 2.0f,
				chunk_p.getY() * m_chunk_size_voxels.getY() +
				m_chunk_size_voxels.getY() / 2.0f,
				chunk_p.getZ() * m_chunk_size_voxels.getZ() +
				m_chunk_size_voxels.getZ() / 2.0f
		);
		log_t(MODULE, "create_chunk_node(): node_p=(%f, %f, %f)",
				node_p.x_, node_p.y_, node_p.z_);

		ss_ name = "static_"+dump(section_p)+")"+
				"_("+itos(x)+","+itos(y)+","+itos(x)+")";

		Node *n = scene->CreateChild(name.c_str());
		if(n->GetID() == 0)
			throw Exception("Can't handle static node id=0");
		section.node_ids->setVoxelAt(chunk_p, n->GetID());

		n->SetScale(Vector3(1.0f, 1.0f, 1.0f));
		n->SetPosition(node_p);

		int w = m_chunk_size_voxels.getX();
		int h = m_chunk_size_voxels.getY();
		int d = m_chunk_size_voxels.getZ();

		// NOTE: These volumes have one extra voxel at each edge in order to
		//       make proper meshes without gaps
		pv::Region region(-1, -1, -1, w, h, d);
		pv::RawVolume<VoxelInstance> volume(region);

		auto lc = region.getLowerCorner();
		auto uc = region.getUpperCorner();
		for(int z = lc.getZ(); z <= uc.getZ(); z++){
			for(int y = lc.getY(); y <= uc.getY(); y++){
				for(int x = lc.getX(); x <= uc.getX(); x++){
					volume.setVoxelAt(x, y, z, VoxelInstance(0));
				}
			}
		}

		ss_ data = interface::serialize_volume_compressed(volume);

		n->SetVar(StringHash("buildat_voxel_data"), Variant(
					PODVector<uint8_t>((const uint8_t*)data.c_str(), data.size())));

		// There are no collision shapes initially, but add the rigid body now
		RigidBody *body = n->CreateComponent<RigidBody>(LOCAL);
		body->SetFriction(0.75f);
	}

	void create_section(Section &section)
	{
		m_server->access_scene([&](Scene *scene)
		{
			auto lc = section.contained_chunks.getLowerCorner();
			auto uc = section.contained_chunks.getUpperCorner();
			for(int z = 0; z <= uc.getZ() - lc.getZ(); z++){
				for(int y = 0; y <= uc.getY() - lc.getY(); y++){
					for(int x = 0; x <= uc.getX() - lc.getX(); x++){
						create_chunk_node(scene, section, x, y, z);
					}
				}
			}
		});
	}

	// Somehow get the section's static nodes and possible other nodes, either
	// by loading from disk or by creating new ones
	void load_section(Section &section)
	{
		if(section.loaded)
			return;
		section.loaded = true;
		pv::Vector3DInt16 section_p = section.section_p;
		log_v(MODULE, "Loading section " PV3I_FORMAT, PV3I_PARAMS(section_p));

		// TODO: If found on disk, load nodes from there
		// TODO: If not found on disk, create new static nodes
		// Always create new nodes for now
		create_section(section);
	}

	// Generate the section; requires static nodes to already exist
	void generate_section(Section &section)
	{
		if(section.generated)
			return;
		section.generated = true;
		pv::Vector3DInt16 section_p = section.section_p;
		log_v(MODULE, "Generating section " PV3I_FORMAT, PV3I_PARAMS(section_p));
		m_server->emit_event("voxelworld:generation_request",
				new GenerationRequest(section_p));
	}

	void mark_node_for_physics_update(uint node_id,
			sp_<pv::RawVolume<VoxelInstance>> volume)
	{
		QueuedNodePhysicsUpdate update(node_id, volume);
		auto it = std::lower_bound(m_nodes_needing_physics_update.begin(),
					m_nodes_needing_physics_update.end(), update,
					std::greater<QueuedNodePhysicsUpdate>());
		if(it == m_nodes_needing_physics_update.end()){
			m_nodes_needing_physics_update.insert(it, update);
		} else if(it->node_id != node_id){
			m_nodes_needing_physics_update.insert(it, update);
		} else {
			*it = update;
		}
	}

	// Interface

	void load_or_generate_section(const pv::Vector3DInt16 &section_p)
	{
		Section &section = force_get_section(section_p);
		if(!section.loaded)
			load_section(section);
		if(!section.generated)
			generate_section(section);
	}

	pv::Region get_section_region_voxels(const pv::Vector3DInt16 &section_p)
	{
		pv::Vector3DInt32 p0 = pv::Vector3DInt32(
				section_p.getX() * m_section_size_chunks.getX() *
				m_chunk_size_voxels.getX(),
				section_p.getY() * m_section_size_chunks.getY() *
				m_chunk_size_voxels.getY(),
				section_p.getZ() * m_section_size_chunks.getZ() *
				m_chunk_size_voxels.getZ()
		);
		pv::Vector3DInt32 p1 = p0 + pv::Vector3DInt32(
				m_section_size_chunks.getX() * m_chunk_size_voxels.getX() - 1,
				m_section_size_chunks.getY() * m_chunk_size_voxels.getY() - 1,
				m_section_size_chunks.getZ() * m_chunk_size_voxels.getZ() - 1
		);
		return pv::Region(p0, p1);
	}

	void set_voxel_direct(const pv::Vector3DInt32 &p,
			const interface::VoxelInstance &v)
	{
		log_t(MODULE, "set_voxel_direct() p=" PV3I_FORMAT ", v=%i",
				PV3I_PARAMS(p), v.data);
		pv::Vector3DInt32 chunk_p = container_coord(p, m_chunk_size_voxels);
		pv::Vector3DInt16 section_p =
				container_coord16(chunk_p, m_section_size_chunks);
		Section *section = get_section(section_p);
		if(section == nullptr){
			log_w(MODULE, "set_voxel_direct() p=" PV3I_FORMAT ", v=%i: No section "
					" " PV3I_FORMAT " for chunk " PV3I_FORMAT,
					PV3I_PARAMS(p), v.data, PV3I_PARAMS(section_p),
					PV3I_PARAMS(chunk_p));
			return;
		}
		int32_t node_id = section->node_ids->getVoxelAt(chunk_p);
		if(node_id == 0){
			log_w(MODULE, "set_voxel_direct() p=" PV3I_FORMAT ", v=%i: No node for "
					"chunk " PV3I_FORMAT " in section " PV3I_FORMAT,
					PV3I_PARAMS(p), v.data, PV3I_PARAMS(chunk_p),
					PV3I_PARAMS(section_p));
			return;
		}

		// Have to commit first so that this modification doesn't get
		// overwritten by some older one
		// TODO: Commit only the current chunk
		commit();

		// Volume will be used after access_scene()
		sp_<pv::RawVolume<VoxelInstance>> volume;

		m_server->access_scene([&](Scene *scene)
		{
			Node *n = scene->GetNode(node_id);
			const Variant &var = n->GetVar(StringHash("buildat_voxel_data"));
			const PODVector<unsigned char> &buf = var.GetBuffer();
			ss_ data((const char*)&buf[0], buf.Size());
			volume = sp_<pv::RawVolume<VoxelInstance>>(std::move(
						interface::deserialize_volume(data)
					));

			// NOTE: +1 offset needed for mesh generation
			pv::Vector3DInt32 voxel_p(
						p.getX() - chunk_p.getX() * m_chunk_size_voxels.getX() + 1,
						p.getY() - chunk_p.getY() * m_chunk_size_voxels.getY() + 1,
						p.getZ() - chunk_p.getZ() * m_chunk_size_voxels.getZ() + 1
					);
			log_t(MODULE, "set_voxel_direct() p=" PV3I_FORMAT ", v=%i: "
					"Chunk " PV3I_FORMAT " in section " PV3I_FORMAT
					"; internal position " PV3I_FORMAT,
					PV3I_PARAMS(p), v.data, PV3I_PARAMS(chunk_p),
					PV3I_PARAMS(section_p), PV3I_PARAMS(voxel_p));
			volume->setVoxelAt(voxel_p, v);

			ss_ new_data = interface::serialize_volume_compressed(*volume);

			n->SetVar(StringHash("buildat_voxel_data"), Variant(
						PODVector<uint8_t>((const uint8_t*)new_data.c_str(),
						new_data.size())));
		});

		// Mark node for collision box update
		mark_node_for_physics_update(node_id, volume);
	}

	void set_voxel(const pv::Vector3DInt32 &p, const interface::VoxelInstance &v,
			bool disable_warnings)
	{
		// Don't log here; this is a too busy place for even ignored log calls
		/*log_t(MODULE, "set_voxel() p=" PV3I_FORMAT ", v=%i",
		        PV3I_PARAMS(p), v.data);*/
		pv::Vector3DInt32 chunk_p = container_coord(p, m_chunk_size_voxels);
		pv::Vector3DInt16 section_p =
				container_coord16(chunk_p, m_section_size_chunks);
		Section *section = get_section(section_p);
		if(section == nullptr){
			if(!disable_warnings){
				log_w(MODULE, "set_voxel() p=" PV3I_FORMAT ", v=%i: No section "
						PV3I_FORMAT " for chunk " PV3I_FORMAT,
						PV3I_PARAMS(p), v.data, PV3I_PARAMS(section_p),
						PV3I_PARAMS(chunk_p));
			}
			return;
		}

		// Set in buffer
		ChunkBuffer &buf = section->get_buffer(chunk_p, m_server);
		if(!buf.volume){
			if(!disable_warnings){
				log_w(MODULE, "set_voxel() p=" PV3I_FORMAT ", v=%i: Couldn't get "
						"buffer volume for chunk " PV3I_FORMAT " in section "
						PV3I_FORMAT, PV3I_PARAMS(p), v.data, PV3I_PARAMS(chunk_p),
						PV3I_PARAMS(section_p));
			}
			return;
		}
		// NOTE: +1 offset needed for mesh generation
		pv::Vector3DInt32 voxel_p(
				p.getX() - chunk_p.getX() * m_chunk_size_voxels.getX() + 1,
				p.getY() - chunk_p.getY() * m_chunk_size_voxels.getY() + 1,
				p.getZ() - chunk_p.getZ() * m_chunk_size_voxels.getZ() + 1
		);
		buf.volume->setVoxelAt(voxel_p, v);

		// Set buffer dirty
		buf.dirty = true;

		// Set section buffer loaded flag
		auto it = std::lower_bound(m_sections_with_loaded_buffers.begin(),
					m_sections_with_loaded_buffers.end(), section,
					std::greater<Section*>()); // position in descending order
		if(it == m_sections_with_loaded_buffers.end() || *it != section)
			m_sections_with_loaded_buffers.insert(it, section);
	}

	// Commit and unload chunk buffer
	// TODO: Unload after a timeout instead of always
	void commit_chunk_buffer(Section *section, size_t chunk_i)
	{
		ChunkBuffer &chunk_buffer = section->chunk_buffers[chunk_i];
		if(!chunk_buffer.dirty){
			// No changes made; unload buffer volume and return
			chunk_buffer.volume.reset();
			return;
		}

		pv::Vector3DInt32 chunk_p = section->get_chunk_p(chunk_i);

		uint node_id = section->node_ids->getVoxelAt(chunk_p);
		if(node_id == 0){
			log_w(MODULE, "commit_chunk_buffer() chunk_i=%zu: "
					"No node found for chunk " PV3I_FORMAT
					" in section " PV3I_FORMAT,
					chunk_i, PV3I_PARAMS(chunk_p),
					PV3I_PARAMS(section->section_p));
			return;
		}

		ss_ new_data = interface::serialize_volume_compressed(
				*chunk_buffer.volume);

		m_server->access_scene([&](Scene *scene)
		{
			Context *context = scene->GetContext();

			Node *n = scene->GetNode(node_id);
			if(!n){
				log_w(MODULE, "commit_chunk_buffer(): Node %i not found",
						node_id);
				return;
			}
			const Variant &var = n->GetVar(StringHash("buildat_voxel_data"));
			if(var.GetType() != VAR_BUFFER){
				log_w(MODULE, "commit_chunk_buffer(): Node %i does not contain "
						"an existing buffer; assuming some kind of error",
						node_id);
				return;
			}

			n->SetVar(StringHash("buildat_voxel_data"), Variant(
						PODVector<uint8_t>((const uint8_t*)new_data.c_str(),
						new_data.size())));
		});

		// Tell replicate to emit events once it has done its job
		replicate::access(m_server, [&](replicate::Interface *ireplicate){
			ireplicate->emit_after_next_sync(Event(
						"voxelworld:node_voxel_data_updated",
						new NodeVoxelDataUpdatedEvent(node_id)));
		});

		// Mark node for collision box update
		mark_node_for_physics_update(node_id, chunk_buffer.volume);

		// Reset dirty flag
		chunk_buffer.dirty = false;
		// Unload buffer volume
		chunk_buffer.volume.reset();
	}

	size_t num_buffers_loaded()
	{
		return m_sections_with_loaded_buffers.size();
	}

	void commit()
	{
		if(m_sections_with_loaded_buffers.empty())
			return;
		log_v(MODULE, "commit(): %zu sections have loaded buffers",
				m_sections_with_loaded_buffers.size());
		for(Section *section : m_sections_with_loaded_buffers){
			for(size_t i = 0; i < section->chunk_buffers.size(); i++){
				commit_chunk_buffer(section, i);
			}
		}
		m_sections_with_loaded_buffers.clear();
	}

	VoxelInstance get_voxel(const pv::Vector3DInt32 &p, bool disable_warnings)
	{
		pv::Vector3DInt32 chunk_p = container_coord(p, m_chunk_size_voxels);
		pv::Vector3DInt16 section_p =
				container_coord16(chunk_p, m_section_size_chunks);
		Section *section = get_section(section_p);
		if(section == nullptr){
			if(!disable_warnings){
				log_w(MODULE, "get_voxel() p=" PV3I_FORMAT ": No section "
						PV3I_FORMAT " for chunk " PV3I_FORMAT,
						PV3I_PARAMS(p), PV3I_PARAMS(section_p),
						PV3I_PARAMS(chunk_p));
			}
			return VoxelInstance(interface::VOXELTYPEID_UNDEFINED);
		}

		// Get from buffer
		ChunkBuffer &buf = section->get_buffer(chunk_p, m_server);
		if(!buf.volume){
			if(!disable_warnings){
				log_w(MODULE, "get_voxel() p=" PV3I_FORMAT ": Couldn't get "
						"buffer volume for chunk " PV3I_FORMAT " in section "
						PV3I_FORMAT, PV3I_PARAMS(p), PV3I_PARAMS(chunk_p),
						PV3I_PARAMS(section_p));
			}
			return VoxelInstance(interface::VOXELTYPEID_UNDEFINED);
		}
		// NOTE: +1 offset needed for mesh generation
		pv::Vector3DInt32 voxel_p(
				p.getX() - chunk_p.getX() * m_chunk_size_voxels.getX() + 1,
				p.getY() - chunk_p.getY() * m_chunk_size_voxels.getY() + 1,
				p.getZ() - chunk_p.getZ() * m_chunk_size_voxels.getZ() + 1
		);
		VoxelInstance v = buf.volume->getVoxelAt(voxel_p);

		// Set section buffer loaded flag
		auto it = std::lower_bound(m_sections_with_loaded_buffers.begin(),
					m_sections_with_loaded_buffers.end(), section,
					std::greater<Section*>()); // position in descending order
		if(it == m_sections_with_loaded_buffers.end() || *it != section)
			m_sections_with_loaded_buffers.insert(it, section);

		return v;
	}

	interface::VoxelRegistry* get_voxel_reg()
	{
		return m_voxel_reg.get();
	}

	void* get_interface()
	{
		return dynamic_cast<Interface*>(this);
	}
};

extern "C" {
	BUILDAT_EXPORT void* createModule_voxelworld(interface::Server *server){
		return (void*)(new Module(server));
	}
}
}

// vim: set noet ts=4 sw=4:
