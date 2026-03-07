#include "OrbisTTY.h"

OrbisTTY_2D::OrbisTTY_2D(int pixel_depth) {
	this->depth = pixel_depth;
}

void OrbisTTY_2D::update_resolution(void) {
	OrbisVideoOutResolutionStatus resolution_status;
	sceVideoOutGetResolutionStatus(this->video, &resolution_status);

	this->resolution.x = resolution_status.width;
	this->resolution.y = resolution_status.height;

	this->frame_buffer_size = this->resolution.x * this->resolution.y * this->depth;
}

bool OrbisTTY_2D::init(size_t mem_size, int num_frame_buffers) {
	this->video = sceVideoOutOpen(ORBIS_VIDEO_USER_MAIN, ORBIS_VIDEO_OUT_BUS_MAIN, 0, 0);
	this->video_mem = NULL;

	if (this->video < 0)
		return false;

	if (sceSysmoduleLoadModule(ORBIS_SYSMODULE_FREETYPE_OL) < 0)
		return false;

	if (FT_Init_FreeType(&this->freetype_lib) != 0)
		return false;

	if (!this->init_flip_queue())
		return false;

	this->update_resolution();

	if (!this->alloc_video_mem(mem_size, 0x200000))
		return false;

	if (!this->alloc_frame_buffers(num_frame_buffers))
		return false;

	sceVideoOutSetFlipRate(this->video, 0);
	return true;
}

bool OrbisTTY_2D::init_flip_queue(void) {
	if (sceKernelCreateEqueue(&flip_queue, "orbistty flip queue") < 0)
		return false;

	sceVideoOutAddFlipEvent(flip_queue, this->video, 0);
	return true;
}

bool OrbisTTY_2D::alloc_frame_buffers(int num) {
	this->frame_buffers = new char* [num];

	for (int index = 0; index < num; index++)
		this->frame_buffers[index] = this->alloc_display_mem(frame_buffer_size);

	sceVideoOutSetBufferAttribute(&this->attr, 0x80000000, 1, 0, this->resolution.x, this->resolution.y, this->resolution.x);
	
	return (sceVideoOutRegisterBuffers(this->video, 0, reinterpret_cast<void**>(this->frame_buffers), num, &this->attr) == 0);
}

char* OrbisTTY_2D::alloc_display_mem(size_t size) {
	char* alloc_ptr = reinterpret_cast<char*>(video_memory_sp);
	video_memory_sp += size;

	return alloc_ptr;
}

bool OrbisTTY_2D::alloc_video_mem(size_t size, int alignment) {
	this->direct_mem_alloc_size = (size + alignment - 1) / alignment * alignment;

	if (sceKernelAllocateDirectMemory(0, sceKernelGetDirectMemorySize(), this->direct_mem_alloc_size, alignment, 3, &this->direct_mem_off) < 0) {
		this->direct_mem_alloc_size = 0;
		return false;
	}

	if (sceKernelMapDirectMemory(&this->video_mem, this->direct_mem_alloc_size, 0x33, 0, this->direct_mem_off, alignment) < 0) {
		sceKernelReleaseDirectMemory(this->direct_mem_off, this->direct_mem_alloc_size);

		this->direct_mem_off = 0;
		this->direct_mem_alloc_size = 0;

		return false;
	}

	this->video_memory_sp = reinterpret_cast<uintptr_t>(this->video_mem);
	return true;
}

void OrbisTTY_2D::free_video_mem(void) {
	sceKernelReleaseDirectMemory(this->direct_mem_off, this->direct_mem_alloc_size);

	this->video_mem = 0;
	this->video_memory_sp = 0;
	this->direct_mem_off = 0;
	this->direct_mem_alloc_size = 0;

	delete this->frame_buffers;
	this->frame_buffers = 0;
}

void OrbisTTY_2D::set_active_frame_buffer(int index) {
	this->active_frame_buffer_index = index;
}

void OrbisTTY_2D::submit_flip(int frame_id) {
	sceVideoOutSubmitFlip(this->video, this->active_frame_buffer_index, ORBIS_VIDEO_OUT_FLIP_VSYNC, frame_id);
}

void OrbisTTY_2D::frame_wait(int frame_id) {
	OrbisKernelEvent event;
	int count;

	if (this->video == 0)
		return;

	for (;;) {
		OrbisVideoOutFlipStatus flip_status;
		sceVideoOutGetFlipStatus(video, &flip_status);
		if (flip_status.flipArg == frame_id)
			break;

		if (sceKernelWaitEqueue(this->flip_queue, &event, 1, &count, 0) != 0)
			break;
	}
}

void OrbisTTY_2D::frame_buffer_swap(void) {
	this->active_frame_buffer_index = (this->active_frame_buffer_index + 1) % 2;
}

void OrbisTTY_2D::frame_buffer_clear(void) {
	this->frame_buffer_fill(white);
}

bool OrbisTTY_2D::use_font(const char* font_path) {
	if (FT_New_Face(this->freetype_lib, font_path, 0, &this->font_face) < 0)
		return false;

	if (FT_Set_Pixel_Sizes(this->font_face, 0, 16) < 0)
		return false;

	return true;
}

void OrbisTTY_2D::frame_buffer_fill(struct OrbisTTY_Color color) {
	this->draw_rectangle(0, 0, this->resolution.x, this->resolution.y, color);
}

void OrbisTTY_2D::draw_pixel(int x, int y, struct OrbisTTY_Color color) {
	int pixel = (y * this->resolution.x) + x;

	uint32_t encoded_color = 0x80000000 + (color.red << 16) + (color.green << 8) + color.blue;

	(reinterpret_cast<uint32_t*>(this->frame_buffers[this->active_frame_buffer_index]))[pixel] = encoded_color;
}

void OrbisTTY_2D::draw_rectangle(int x, int y, int width, int height, struct OrbisTTY_Color color) {
	int x_position;
	int y_position;

	for (y_position = y; y_position < y + height; y_position++) {
		for (x_position = x; x_position < x + width; x_position++)
			this->draw_pixel(x_position, y_position, color);
	}
}

void OrbisTTY_2D::draw_text(char* text, int start_x, int start_y, struct OrbisTTY_Color bg_color, struct OrbisTTY_Color fg_color) {
	int x_offset = 0;
	int y_offset = 0;

	FT_GlyphSlot slot = this->font_face->glyph;

	for (int n = 0; n < strlen(text); n++) {
		FT_UInt glyph_index = FT_Get_Char_Index(this->font_face, text[n]);

		if (FT_Load_Glyph(this->font_face, glyph_index, FT_LOAD_DEFAULT))
			continue;

		if (FT_Render_Glyph(slot, ft_render_mode_normal))
			continue;

		if (text[n] == '\n') {
			x_offset = 0;
			y_offset += slot->bitmap.width * 2;

			continue;
		}

		for (int y_position = 0; y_position < slot->bitmap.rows; y_position++) {
			for (int x_position = 0; x_position < slot->bitmap.width; x_position++) {
				char pixel = slot->bitmap.buffer[(y_position * slot->bitmap.width) + x_position];

				int x = start_x + x_position + x_offset + slot->bitmap_left;
				int y = start_y + y_position + y_offset - slot->bitmap_top;

				uint8_t red = (pixel * fg_color.red) / 255;
				uint8_t green = (pixel * fg_color.green) / 255;
				uint8_t blue = (pixel * fg_color.blue) / 255;

				struct OrbisTTY_Color final_color = { red, green, blue };

				if (x < 0 || y < 0 || x >= this->resolution.x || y >= this->resolution.y)
					continue;

				if (pixel != 0x00)
					this->draw_pixel(x, y, final_color);
			}
		}

		x_offset += slot->advance.x >> 6;
	}
}

void OrbisTTY_2D::draw_textf(int start_x, int start_y, struct OrbisTTY_Color bg_color, struct OrbisTTY_Color fg_color, char* fmt, ...) {
	char buffer[512];

	va_list args;
	va_start(args, fmt);
	vsnprintf(buffer, sizeof(buffer), fmt, args);
	va_end(args);

	this->draw_text(buffer, start_x, start_y, bg_color, fg_color);
}


// OrbisTTY Namespace

bool OrbisTTY::init(const char* font_path) {
	scene = new OrbisTTY_2D(4);
	if (!scene->init(0xC000000, 2))
		return false;

	if (!set_font(font_path))
		return false;

	for (int index = 0; index <= (floor(scene->resolution.y / 17) - 1); index++)
		lines.push_back("");

	line_index = 0;

	return true;
}

bool OrbisTTY::set_font(const char* font_path) {
	if (!scene->use_font(font_path))
		return false;

	return true;
}

void OrbisTTY::move_lines_up(void) {
	int total_lines = floor(scene->resolution.y / 17) - 1;

	for (int index = 0; index <= total_lines - 1; index++)
		lines[index] = lines[index + 1];

	lines[total_lines] = "";
}

int OrbisTTY::get_y_offset(int line) {
	return (line * 17) + 17;
}

void OrbisTTY::orbis_printf(const char* fmt, ...) {
	char buffer[512];

	va_list args;
	va_start(args, fmt);
	vsnprintf(buffer, sizeof(buffer), fmt, args);
	va_end(args);

	int total_lines = floor(scene->resolution.y / 17) - 1;

	if (line_index <= total_lines - 1)
		lines[line_index++] = buffer;
	else {
		move_lines_up();
		lines[total_lines - 1] = buffer;
	}

	scene->set_active_frame_buffer((frame_id + 1) % 2);
	scene->frame_buffer_fill(black);

	for (int index = 0; index <= total_lines; index++)
		scene->draw_text((char*)lines[index].c_str(), 5, get_y_offset(index), black, white);

	scene->submit_flip(frame_id);
	scene->frame_wait(frame_id);
	scene->frame_buffer_swap();

	frame_id++;
}