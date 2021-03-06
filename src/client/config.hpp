#ifndef CONFIG_HPP_
#define CONFIG_HPP_

#include <string>

#include "shared/engine/vmath.hpp"

enum class RenderBackend {
	OGL_2,
	OGL_3,
};

enum class AntiAliasing {
	NONE,
	MSAA_2,
	MSAA_4,
	MSAA_8,
	MSAA_16,
};

enum class Fog {
	NONE,
	FAST,
	FANCY,
};

enum class TexFiltering {
	NEAREST,
	LINEAR,
};

extern std::string   DEFAULT_LAST_WORLD_ID;
extern RenderBackend DEFAULT_RENDER_BACKEND;
extern bool          DEFAULT_FULLSCREEN;
extern vec2i         DEFAULT_WINDOWED_RES;
extern vec2i         DEFAULT_FULLSCREEN_RES;
extern AntiAliasing  DEFAULT_ANTI_ALIASING;
extern Fog           DEFAULT_FOG;
extern uint          DEFAULT_RENDER_DISTANCE;
extern float         DEFAULT_FOV;
extern uint          DEFAULT_TEX_MIPMAPPING;
extern TexFiltering  DEFAULT_TEX_FILTERING;
extern bool          DEFAULT_TEX_ATLAS;
extern std::string   DEFAUKT_TEXTURES_FILE;

struct GraphicsConf {
	std::string last_world_id;

	RenderBackend render_backend;
	bool fullscreen;
	vec2i windowed_res;
	vec2i fullscreen_res;
	AntiAliasing aa;
	Fog fog;
	uint render_distance;
	float fov;

	uint tex_mipmapping;
	TexFiltering tex_filtering;
	bool tex_atlas;

	std::string textures_file;
};

void store(const char *filename, const GraphicsConf &conf);
void load(const char *filename, GraphicsConf *conf);

#endif // CONFIG_HPP_
