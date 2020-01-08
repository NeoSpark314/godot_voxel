#include "voxel.h"
#include "meshers/blocky/voxel_mesher_blocky.h" // TODO Only required because of MAX_MATERIALS... could be enough inverting that dependency
#include "voxel_library.h"

#define STRLEN(x) (sizeof(x) / sizeof(x[0]))

const float uv_epsilon = 0.001;

Voxel::Voxel() :
		_library(0),
		_id(-1),
		_material_id(0),
		_is_transparent(false),
		_color(1.f, 1.f, 1.f),
		_geometry_type(GEOMETRY_NONE),
		_cube_geometry_padding_y(0), 
		_plant_height(1.0),
		_plant_tile(0.0, 0.0)
		{}

static Cube::Side name_to_side(const String &s) {
	if (s == "left")
		return Cube::SIDE_LEFT;
	if (s == "right")
		return Cube::SIDE_RIGHT;
	if (s == "top")
		return Cube::SIDE_TOP;
	if (s == "bottom")
		return Cube::SIDE_BOTTOM;
	if (s == "front")
		return Cube::SIDE_FRONT;
	if (s == "back")
		return Cube::SIDE_BACK;
	return Cube::SIDE_COUNT; // Invalid
}

bool Voxel::_set(const StringName &p_name, const Variant &p_value) {

	String name = p_name;

	// TODO Eventualy these could be Rect2 for maximum flexibility?
	if (name.begins_with("cube_tiles/")) {

		String s = name.substr(STRLEN("cube_tiles/") - 1, name.length());
		Cube::Side side = name_to_side(s);
		if (side != Cube::SIDE_COUNT) {
			Vector2 v = p_value;
			set_cube_uv_side(side, v);
			return true;
		}

	} else if (name == "cube_geometry/padding_y") {

		_cube_geometry_padding_y = p_value;
		set_cube_geometry(_cube_geometry_padding_y);
		return true;
	} else if (name == "plant_height") {
		_plant_height = p_value;
		set_plant_geometry();
		return true;
	} else if (name == "plant_tile") {
		_plant_tile = p_value;
		set_plant_geometry();
		return true;
	}

	return false;
}

bool Voxel::_get(const StringName &p_name, Variant &r_ret) const {

	String name = p_name;

	if (name.begins_with("cube_tiles/")) {

		String s = name.substr(STRLEN("cube_tiles/") - 1, name.length());
		Cube::Side side = name_to_side(s);
		if (side != Cube::SIDE_COUNT) {
			r_ret = _cube_tiles[side];
			return true;
		}

	} else if (name == "cube_geometry/padding_y") {

		r_ret = _cube_geometry_padding_y;
		return true;
	} else if (name == "plant_height") {
		r_ret = _plant_height;
		return true;
	} else if (name == "plant_tile") {
		r_ret = _plant_tile;
		return true;
	}

	return false;
}

void Voxel::_get_property_list(List<PropertyInfo> *p_list) const {

	if (_geometry_type == GEOMETRY_CUBE) {

		p_list->push_back(PropertyInfo(Variant::REAL, "cube_geometry/padding_y"));

		p_list->push_back(PropertyInfo(Variant::VECTOR2, "cube_tiles/left"));
		p_list->push_back(PropertyInfo(Variant::VECTOR2, "cube_tiles/right"));
		p_list->push_back(PropertyInfo(Variant::VECTOR2, "cube_tiles/bottom"));
		p_list->push_back(PropertyInfo(Variant::VECTOR2, "cube_tiles/top"));
		p_list->push_back(PropertyInfo(Variant::VECTOR2, "cube_tiles/back"));
		p_list->push_back(PropertyInfo(Variant::VECTOR2, "cube_tiles/front"));
	}

	if (_geometry_type == GEOMETRY_PLANT) {
		p_list->push_back(PropertyInfo(Variant::REAL, "plant_height"));
		p_list->push_back(PropertyInfo(Variant::REAL, "plant_tile"));
	}
}

Ref<Voxel> Voxel::set_voxel_name(String name) {
	_name = name;
	return Ref<Voxel>(this);
}

Ref<Voxel> Voxel::set_id(int id) {
	ERR_FAIL_COND_V(id < 0 || id >= 256, Ref<Voxel>(this));
	// Cannot modify ID after creation
	ERR_FAIL_COND_V(_id != -1, Ref<Voxel>(this));
	_id = id;
	return Ref<Voxel>(this);
}

Ref<Voxel> Voxel::set_color(Color color) {
	_color = color;
	return Ref<Voxel>(this);
}

Ref<Voxel> Voxel::set_material_id(unsigned int id) {
	ERR_FAIL_COND_V(id >= VoxelMesherBlocky::MAX_MATERIALS, Ref<Voxel>(this));
	_material_id = id;
	return Ref<Voxel>(this);
}

Ref<Voxel> Voxel::set_transparent(bool t) {
	_is_transparent = t;
	return Ref<Voxel>(this);
}

void Voxel::set_geometry_type(GeometryType type) {

	_geometry_type = type;

	switch (_geometry_type) {

		case GEOMETRY_NONE: {
			// Clear all geometry
			_model_positions.resize(0);
			_model_normals.resize(0);
			_model_uvs.resize(0);
			_model_indices.resize(0);

			for (int side = 0; side < Cube::SIDE_COUNT; ++side) {
				_model_side_positions[side].resize(0);
				_model_side_uvs[side].resize(0);
				_model_side_indices[side].resize(0);
			}

		} break;

		case GEOMETRY_CUBE:
			set_cube_geometry(_cube_geometry_padding_y);
			update_cube_uv_sides();
			break;

		case GEOMETRY_PLANT:
			set_plant_geometry();
			break;

		default:
			print_line("Wtf? Unknown geometry type");
			break;
	}
}

Voxel::GeometryType Voxel::get_geometry_type() const {
	return _geometry_type;
}

void Voxel::set_library(Ref<VoxelLibrary> lib) {
	if (lib.is_null())
		_library = 0;
	else
		_library = lib->get_instance_id();
	
	set_geometry_type(_geometry_type); // update everything
}

VoxelLibrary *Voxel::get_library() const {
	if (_library == 0)
		return NULL;
	Object *v = ObjectDB::get_instance(_library);
	if (v)
		return Object::cast_to<VoxelLibrary>(v);
	return NULL;
}


Ref<Voxel> Voxel::set_plant_geometry() {
	for (int side = 0; side < Cube::SIDE_COUNT; ++side) {
		_model_side_positions[side].resize(0);
		_model_side_uvs[side].resize(0);
		_model_side_indices[side].resize(0);
	}

	const Vector2 uv[4] = {
		Vector2(uv_epsilon, 1.f - uv_epsilon),
		Vector2(1.f - uv_epsilon, 1.f - uv_epsilon),
		Vector2(uv_epsilon, uv_epsilon),
		Vector2(1.f - uv_epsilon, uv_epsilon),
	};

	// creates a cross out of the cube positions
	_model_positions.resize(8);
	_model_normals.resize(8);
	_model_uvs.resize(8);
	_model_indices.resize(12);

	PoolVector3Array::Write pos = _model_positions.write();
	PoolVector3Array::Write nrm = _model_normals.write();
	PoolVector2Array::Write uvs = _model_uvs.write();
	for (int i = 0; i < 8; i++) {
		pos[i] = Cube::g_corner_position[i];
		pos[i].y *= _plant_height;
		nrm[i] = Vector3(0.0, 1.0, 0.0); // up for now to be more consistent in brightness with ground
		uvs[i] = (_plant_tile + uv[i/2]); // default uv's will be set via atlas size later
	}

	PoolIntArray::Write indices = _model_indices.write();

	for (int i = 0; i < 2; i++) {
		indices[i*6+0] = 0+i; indices[i*6+1] = 4+i; indices[i*6+2] = 2+i;
		indices[i*6+3] = 2+i; indices[i*6+4] = 4+i; indices[i*6+5] = 6+i;
	}

	VoxelLibrary *library = get_library();
	if (library == NULL) {
		print_line("set_plant_geometry(): VoxelLibrary not set yet");
	} else { // now we can update the uv's
		float atlas_size = (float)library->get_atlas_size();
		CRASH_COND(atlas_size <= 0);
		float s = 1.0 / atlas_size;

		for (int i = 0; i < 8; i++) {
			uvs[i] = (_plant_tile + uv[i/2]);
			uvs[i].y *= _plant_height; // account for lower height
			uvs[i] *= s;
		}
	}

	return Ref<Voxel>(this);
}


Ref<Voxel> Voxel::set_cube_geometry(float sy) {
	sy = 1.0 + sy;

	for (unsigned int side = 0; side < Cube::SIDE_COUNT; ++side) {

		{
			_model_side_positions[side].resize(4);
			PoolVector<Vector3>::Write w = _model_side_positions[side].write();
			for (unsigned int i = 0; i < 4; ++i) {
				int corner = Cube::g_side_corners[side][i];
				Vector3 p = Cube::g_corner_position[corner];
				if (p.y > 0.9)
					p.y = sy;
				w[i] = p;
			}
		}

		{
			_model_side_indices[side].resize(6);
			PoolVector<int>::Write w = _model_side_indices[side].write();
			for (unsigned int i = 0; i < 6; ++i) {
				w[i] = Cube::g_side_quad_triangles[side][i];
			}
		}
	}

	return Ref<Voxel>(this);
}

void Voxel::set_cube_uv_side(int side, Vector2 tile_pos) {
	_cube_tiles[side] = tile_pos;
	// TODO Better have a dirty flag, otherwise UVs will be needlessly updated at least 6 times everytime a Voxel resource is loaded!
	update_cube_uv_sides();
}

void Voxel::update_cube_uv_sides() {
	VoxelLibrary *library = get_library();
	//ERR_FAIL_COND(library == NULL);
	if (library == NULL) {
		// Not an error, the Voxel might have been created before the library, and can't be used without anyways
		print_line("VoxelLibrary not set yet");
		return;
	}

	// Winding is the same as the one chosen in Cube:: vertices
	// I am confused. I read in at least 3 OpenGL tutorials that texture coordinates start at bottom-left (0,0).
	// But even though Godot is said to follow OpenGL's convention, the engine starts at top-left!
	const Vector2 uv[4] = {
		Vector2(uv_epsilon, 1.f - uv_epsilon),
		Vector2(1.f - uv_epsilon, 1.f - uv_epsilon),
		Vector2(1.f - uv_epsilon, uv_epsilon),
		Vector2(uv_epsilon, uv_epsilon),
	};

	float atlas_size = (float)library->get_atlas_size();
	CRASH_COND(atlas_size <= 0);
	float s = 1.0 / atlas_size;

	for (unsigned int side = 0; side < Cube::SIDE_COUNT; ++side) {
		_model_side_uvs[side].resize(4);
		PoolVector<Vector2>::Write w = _model_side_uvs[side].write();
		for (unsigned int i = 0; i < 4; ++i) {
			w[i] = (_cube_tiles[side] + uv[i]) * s;
		}
	}
}

//Ref<Voxel> Voxel::set_xquad_geometry(Vector2 atlas_pos) {
//    // TODO
//    return Ref<Voxel>(this);
//}

void Voxel::_bind_methods() {

	ClassDB::bind_method(D_METHOD("set_voxel_name", "name"), &Voxel::set_voxel_name);
	ClassDB::bind_method(D_METHOD("get_voxel_name"), &Voxel::get_voxel_name);

	ClassDB::bind_method(D_METHOD("set_id", "id"), &Voxel::set_id);
	ClassDB::bind_method(D_METHOD("get_id"), &Voxel::get_id);

	ClassDB::bind_method(D_METHOD("set_color", "color"), &Voxel::set_color);
	ClassDB::bind_method(D_METHOD("get_color"), &Voxel::get_color);

	ClassDB::bind_method(D_METHOD("set_transparent", "transparent"), &Voxel::set_transparent, DEFVAL(true));
	ClassDB::bind_method(D_METHOD("is_transparent"), &Voxel::is_transparent);

	ClassDB::bind_method(D_METHOD("set_material_id", "id"), &Voxel::set_material_id);
	ClassDB::bind_method(D_METHOD("get_material_id"), &Voxel::get_material_id);

	ClassDB::bind_method(D_METHOD("set_geometry_type", "type"), &Voxel::set_geometry_type);
	ClassDB::bind_method(D_METHOD("get_geometry_type"), &Voxel::get_geometry_type);

	ADD_PROPERTY(PropertyInfo(Variant::STRING, "voxel_name"), "set_voxel_name", "get_voxel_name");
	ADD_PROPERTY(PropertyInfo(Variant::COLOR, "color"), "set_color", "get_color");
	ADD_PROPERTY(PropertyInfo(Variant::BOOL, "transparent"), "set_transparent", "is_transparent");
	ADD_PROPERTY(PropertyInfo(Variant::INT, "material_id"), "set_material_id", "get_material_id");
	ADD_PROPERTY(PropertyInfo(Variant::INT, "geometry_type", PROPERTY_HINT_ENUM, "None,Cube,Plant"), "set_geometry_type", "get_geometry_type");

	BIND_ENUM_CONSTANT(GEOMETRY_NONE);
	BIND_ENUM_CONSTANT(GEOMETRY_CUBE);
	BIND_ENUM_CONSTANT(GEOMETRY_PLANT);
	BIND_ENUM_CONSTANT(GEOMETRY_MAX);

	BIND_ENUM_CONSTANT(CHANNEL_TYPE)
	BIND_ENUM_CONSTANT(CHANNEL_ISOLEVEL)
	BIND_ENUM_CONSTANT(CHANNEL_DATA)
}
