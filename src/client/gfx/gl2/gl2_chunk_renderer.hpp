#ifndef GL2_CHUNK_RENDERER_HPP_
#define GL2_CHUNK_RENDERER_HPP_

#include "client/gfx/chunk_renderer.hpp"

#include <GL/glew.h>

#include "shared/engine/macros.hpp"
#include "shared/engine/std_types.hpp"
#include "shared/game/chunk.hpp"

class Client;
class GL2Renderer;
struct GraphicsConf;

class GL2ChunkRenderer : public ChunkRenderer {

	// face buffer for chunk rendering
	struct FaceVertexData {
		vec3f vertex[4];
		vec3f color[4];
		vec2f tex[4];
		vec3f normal;
	};

	struct FaceIndexData {
		GLuint tex;
		int index;
	};

	// display lists
	GLuint dlFirstAddress = 0;
	vec3i64 *dlChunks = nullptr;
	uint8 *dlStatus = nullptr;

	// chunk construction state
	int numQuads = 0;
	FaceVertexData vb[(Chunk::WIDTH + 1) * Chunk::WIDTH * Chunk::WIDTH * 3];
	FaceIndexData faceIndexBuffer[(Chunk::WIDTH + 1) * Chunk::WIDTH * Chunk::WIDTH * 3];

public:
	GL2ChunkRenderer(Client *client, GL2Renderer *renderer);
	~GL2ChunkRenderer();

protected:
	void initRenderDistanceDependent(int renderDistance) override;
	void destroyRenderDistanceDependent() override;

	void beginRender() override {}
	void renderChunk(size_t index) override;
	void finishRender() override {}
	void beginChunkConstruction() override;
	void emitFace(vec3i64 bc, vec3i64 icc, uint blockType, uint faceDir, int shadowLevels[4]) override;
	void finishChunkConstruction(size_t index) override;

//	void renderTarget();
//	void renderPlayers();
};

#endif //GL2_CHUNK_RENDERER_HPP_