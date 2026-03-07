#ifndef ORBISTTY_H_
#define ORBISTTY_H_

#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <string.h>
#include <math.h>

#include <orbis/libkernel.h>
#include <orbis/VideoOut.h>
#include <orbis/Sysmodule.h>
#include <proto-include.h>

#include <string>
#include <vector>

struct OrbisTTY_Color {
	uint8_t red;
	uint8_t green;
	uint8_t blue;
};

struct OrbisTTY_Resolution {
	uint16_t x;
	uint16_t y;
};

static struct OrbisTTY_Color white = { 255, 255, 255 };
static struct OrbisTTY_Color black = {   0,   0,   0 };

class OrbisTTY_2D {
private:
	FT_Library freetype_lib;
	FT_Face font_face;
	
	int video;
	int depth;
	
	off_t direct_mem_off;
	size_t direct_mem_alloc_size;

	uintptr_t video_memory_sp;
	void* video_mem;

	char** frame_buffers;
	OrbisKernelEqueue flip_queue;
	OrbisVideoOutBufferAttribute attr;

	int frame_buffer_size;
	int frame_buffer_count;

	int active_frame_buffer_index;

	bool  init_flip_queue      (void);
	bool  alloc_frame_buffers  (int num);
	char* alloc_display_mem    (size_t size);
	bool  alloc_video_mem      (size_t size, int alignment);
	void  free_video_mem       (void);
	void  update_resolution    (void);

public:
	struct OrbisTTY_Resolution resolution = { 0, 0 }; // Screen resolution

	OrbisTTY_2D(int pixel_depth);
	~OrbisTTY_2D(void);

	bool init                    (size_t mem_size, int num_frame_buffers);
	
	void set_active_frame_buffer (int index);
	void submit_flip             (int frame_id);

	void frame_wait              (int frame_id);
	void frame_buffer_swap       (void);
	void frame_buffer_clear      (void);
	void frame_buffer_fill       (struct OrbisTTY_Color color);

	void draw_pixel              (int x, int y, struct OrbisTTY_Color color);
	void draw_rectangle          (int x, int y, int width, int height, struct OrbisTTY_Color color);

	bool use_font                (const char* font_path);
	void draw_text               (char* text, int start_x, int start_y, struct OrbisTTY_Color bg_color, struct OrbisTTY_Color fg_color);
	void draw_textf              (int start_x, int start_y, struct OrbisTTY_Color bgColor, struct OrbisTTY_Color fg_color, char* fmt, ...);
};

namespace OrbisTTY {
	static OrbisTTY_2D* scene;
	static std::vector<std::string> lines;
	static int line_index;
	static int frame_id;

	bool init                    (const char* font_path);
	bool set_font                (const char* font_path);
	void move_lines_up           (void);
	int  get_y_offset            (int line);
	void orbis_printf            (const char* fmt, ...);
}


#endif
