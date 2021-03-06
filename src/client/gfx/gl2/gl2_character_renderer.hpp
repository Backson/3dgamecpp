#ifndef GL2_CHARACTER_RENDERER_HPP_
#define GL2_CHARACTER_RENDERER_HPP_

#include "shared/engine/macros.hpp"
#include "shared/engine/vmath.hpp"
#include "client/gfx/component_renderer.hpp"

class Client;
class GL2Renderer;

class GL2CharacterRenderer : public ComponentRenderer {
public:
	GL2CharacterRenderer(Client *client, GL2Renderer *renderer);

	void render() override;

private:
	Client *client = nullptr;
	GL2Renderer *renderer = nullptr;

	vec3f characterColor{ 0.6f, 0.0f, 0.0f };
};

#endif //GL2_CHARACTER_RENDERER_HPP_
