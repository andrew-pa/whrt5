#pragma once
#include "cmmn.h"

namespace whrt5 {

	/*
		base class for all textures
		C:	resulting color type
		Tx:	texel index type
	*/
	template<typename C, typename Tx>
	class texture
	{
	public:
		// maps Tx -> C
		virtual C texel(Tx c) const = 0;
		virtual ~texture() {}
	};

	/*
		a 2D texture class backed by a pixel array

		!!! memory allocated (_pixels) ownership is murky when considering copying this class,
		!!! perhaps someone should fix that
	*/
	class texture2d : public texture<vec3, vec2>
	{
	protected:
		vector<vec3> _pixels;
	public:
		// size of texture in pixels
		uvec2 size;

		// create a new, uninitialized texture of size _s
		texture2d(uvec2 _s) : size(_s), _pixels(_s.x*_s.y, vec3(0.f)) {}
		// create a texture by loading data out of a BMP file
		texture2d(const string& bmp_filename);

		// gets a pixel value from the texture, must be in [0, size)
		inline const vec3& pixel(uvec2 c) const
		{
			return _pixels[c.x + c.y*size.x];
		}
		inline vec3& pixel(uvec2 c)
		{
			return _pixels[c.x + c.y*size.x];
		}

		// maps texel coords to pixel coords and reads the subsequent pixel
		// c is in [0, 1] range
		// currently repeats the texture if it is beyond that
		// additionaly does not perform any kind of filtering
		inline vec3 texel(vec2 c) const override
		{
			c = mod(c, vec2(1.f));
			uvec2 ic = floor(c*(vec2)size);
			if (ic.x == size.x) ic.x--;
			if (ic.y == size.y) ic.y--;
			return pixel(ic);
		}

		// write this texture to a BMP file
		// doesn't perform any gamma correction/tonemap, just dumps bits in a file
		void write_bmp(const string& bmp_filename) const;

		// draw some bitmap font text on the texture
		// not all possible characters are in the font
		void draw_text(const string& text, uvec2 pos, vec3 color);

		void tiled_multithreaded_raster(uvec2 tilesize, function<vec3(uvec2)> f);
	};

	template<typename C, typename T>
	struct const_texture : public texture<C, T> {
		C val;
		const_texture(C v) : val(v) {}

		C texel(T) const override { return val; }
	};

	struct checkerboard_texture : public texture<vec3, vec2> {
		vec3 colors[2];
		float scale;

		checkerboard_texture(vec3 cA, vec3 cB, float scl) : scale(scl) {
			colors[0] = cA; colors[1] = cB;
		}

		vec3 texel(vec2 uv) const override {
			uv = floor(uv*scale);
			return colors[(size_t)mod(uv.x + uv.y, 2.f)];
		}
	};

	struct grid_texture : public texture<vec3, vec2> {
		vec3 bg_color, fg_color;
		float scale, line_size;

		grid_texture(vec3 fg, vec3 bg, float s, float ls) : scale(s), line_size(ls), bg_color(bg), fg_color(fg) {}

		vec3 texel(vec2 uv) const override {
			uv = step(fract(uv*scale), vec2(line_size));
			return mix(bg_color, fg_color, glm::max(uv.x, uv.y));
		}
	};
}