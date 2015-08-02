#ifndef GL2_MENU_RENDERER_HPP_
#define GL2_MENU_RENDERER_HPP_

#include <FTGL/ftgl.h>

class Client;
class GL2Renderer;
struct GraphicsConf;

namespace gui {
	class Frame;
	class Label;
	class Widget;
	class Button;
}

class GL2MenuRenderer {
	Client *client = nullptr;
	GL2Renderer *renderer = nullptr;

	FTFont *font = nullptr;

public:
	GL2MenuRenderer(Client *client, GL2Renderer *renderer);
	~GL2MenuRenderer();

	void setConf(const GraphicsConf &, const GraphicsConf &);
	void render();

private:
	void renderWidget(const gui::Widget *);
	void renderFrame(const gui::Frame *);
	void renderLabel(const gui::Label *);
	void renderButton(const gui::Button *);
	void renderText(const char *text);
};

#endif //GL2_MENU_RENDERER_HPP_
