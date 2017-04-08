#include "texture.h"

#define _MSVC_
namespace whrt5 {
	texture2d::texture2d(const string& bmp_filename)
	{
		/*
			I stole this code from somewhere, I don't remember where
			It's not the greatest but it works, so for now it can be kept
			In the future it may be aventagous to use the rest of stb_image to do this
		*/
		unsigned char header[54];
		unsigned int dataPos;
		unsigned int imageSize;
		unsigned int width, height;

		unsigned char * data;

		FILE * file;
#ifdef _MSVC_
		fopen_s(&file, bmp_filename.c_str(), "rb");
#else
		file = fopen(bmp_filename.c_str(), "rb");
#endif
		if (!file) throw runtime_error((string("couldn't open file ") + bmp_filename));
		if (fread(header, 1, 54, file) != 54)
			throw runtime_error("invalid BMP header");

		//// A BMP files always begins with "BM"

		if (header[0] != 'B' || header[1] != 'M')
			throw runtime_error("invalid BMP file");

		//// Make sure this is a 24bpp file

		if (*(int*)&(header[0x1E]) != 0)          throw runtime_error("Not a correct BMP file\n");
		if (*(int*)&(header[0x1C]) != 24)         throw runtime_error("Not a correct BMP file\n");

		dataPos = *(int*)&(header[0x0A]);
		imageSize = *(int*)&(header[0x22]);
		width = *(int*)&(header[0x12]);
		height = *(int*)&(header[0x16]);
		size = uvec2(width, height);


		if (imageSize == 0)    imageSize = width*height * 3;
		if (dataPos == 0)      dataPos = 54;

		data = new unsigned char[imageSize];
		fread(data, 1, imageSize, file);
		fclose(file);

		_pixels = vector<vec3>(imageSize / 3);

		int j = 0;
		for (int i = imageSize; i > 0; i -= 3)
		{
			vec3 d(0);
			d.b = (float)(data[i]) / 255.f;
			d.g = (float)(data[i + 1]) / 255.f;
			d.r = (float)(data[i + 2]) / 255.f;
			_pixels[j] = d;
			j++;
		}

	}

	//fantastic STB libary portion
#pragma region STB_IMAGE_WRITE
	namespace stb_image_write
	{
#include <stdlib.h>
#include <stdarg.h>
#include <assert.h>
		typedef unsigned int stbiw_uint32;
		typedef int stb_image_write_test[sizeof(stbiw_uint32) == 4 ? 1 : -1];

		static void writefv(FILE *f, const char *fmt, va_list v)
		{
			while (*fmt) {
				switch (*fmt++) {
				case ' ': break;
				case '1': { unsigned char x = (unsigned char)va_arg(v, int); fputc(x, f); break; }
				case '2': { int x = va_arg(v, int); unsigned char b[2];
					b[0] = (unsigned char)x; b[1] = (unsigned char)(x >> 8);
					fwrite(b, 2, 1, f); break; }
				case '4': { stbiw_uint32 x = va_arg(v, int); unsigned char b[4];
					b[0] = (unsigned char)x; b[1] = (unsigned char)(x >> 8);
					b[2] = (unsigned char)(x >> 16); b[3] = (unsigned char)(x >> 24);
					fwrite(b, 4, 1, f); break; }
				default:
					assert(0);
					return;
				}
			}
		}

		static void write3(FILE *f, unsigned char a, unsigned char b, unsigned char c)
		{
			unsigned char arr[3];
			arr[0] = a, arr[1] = b, arr[2] = c;
			fwrite(arr, 3, 1, f);
		}

		static void write_pixels(FILE *f, int rgb_dir, int vdir, int x, int y, int comp, void *data, int write_alpha, int scanline_pad)
		{
			unsigned char bg[3] = { 255, 0, 255 }, px[3];
			stbiw_uint32 zero = 0;
			int i, j, k, j_end;

			if (y <= 0)
				return;

			if (vdir < 0)
				j_end = -1, j = y - 1;
			else
				j_end = y, j = 0;

			for (; j != j_end; j += vdir) {
				for (i = 0; i < x; ++i) {
					unsigned char *d = (unsigned char *)data + (j*x + i)*comp;
					if (write_alpha < 0)
						fwrite(&d[comp - 1], 1, 1, f);
					switch (comp) {
					case 1:
					case 2: write3(f, d[0], d[0], d[0]);
						break;
					case 4:
						if (!write_alpha) {
							// composite against pink background
							for (k = 0; k < 3; ++k)
								px[k] = bg[k] + ((d[k] - bg[k]) * d[3]) / 255;
							write3(f, px[1 - rgb_dir], px[1], px[1 + rgb_dir]);
							break;
						}
						/* FALLTHROUGH */
					case 3:
						write3(f, d[1 - rgb_dir], d[1], d[1 + rgb_dir]);
						break;
					}
					if (write_alpha > 0)
						fwrite(&d[comp - 1], 1, 1, f);
				}
				fwrite(&zero, scanline_pad, 1, f);
			}
		}

		static int outfile(char const *filename, int rgb_dir, int vdir, int x, int y, int comp, void *data, int alpha, int pad, const char *fmt, ...)
		{
			FILE *f;
			if (y < 0 || x < 0) return 0;
#ifdef _MSVC_
			fopen_s(&f, filename, "wb");
#else
			f = fopen(filename, "wb");
#endif
			if (f) {
				va_list v;
				va_start(v, fmt);
				writefv(f, fmt, v);
				va_end(v);
				write_pixels(f, rgb_dir, vdir, x, y, comp, data, alpha, pad);
				fclose(f);
			}
			return f != NULL;
		}

		int stbi_write_bmp(char const *filename, int x, int y, int comp, const void *data)
		{
			int pad = (-x * 3) & 3;
			return outfile(filename, -1, -1, x, y, comp, (void *)data, 0, pad,
				"11 4 22 4" "4 44 22 444444",
				'B', 'M', 14 + 40 + (x * 3 + pad)*y, 0, 0, 14 + 40,  // file header
				40, x, y, 1, 24, 0, 0, 0, 0, 0, 0);             // bitmap header
		}
	}
#pragma endregion
	
	void texture2d::write_bmp(const string& filename) const
	{
		unsigned char* imgdata = new unsigned char[size.x*size.y * 4];
		for (int y = 0; y < size.y * 4; y += 4)
		{
			for (int x = 0; x < size.x * 4; x += 4)
			{
				vec3 d = clamp(_pixels[(y*size.x + x) / 4], vec3(0), vec3(1));
				imgdata[(y*size.x + x)] = (unsigned char)(d.x * 255.f);
				imgdata[(y*size.x + x) + 1] = (unsigned char)(d.y * 255.f);
				imgdata[(y*size.x + x) + 2] = (unsigned char)(d.z * 255.f);
				imgdata[(y*size.x + x) + 3] = 255;
			}
		}
		stb_image_write::stbi_write_bmp(filename.c_str(), size.x, size.y, 4, imgdata);
		delete imgdata;
	}

	//i'll admit this function has some silliness, but it works and i already typed all of those silly character bitmaps in long ago
	void texture2d::draw_text(const std::string& text, uvec2 pos, vec3 color)
	{
		const int char_width = 5;
		const int char_height = 7;
		static std::map<char, string>* chars = nullptr;
		if (chars == nullptr)
		{
			//bulid chars map
			chars = new std::map<char, string>;
#pragma region ABCs
			(*chars)['A'] =
				"xxxxx"
				"x...x"
				"x...x"
				"xxxxx"
				"x...x"
				"x...x"
				"x...x";
			(*chars)['B'] =
				"xxxx."
				"x...x"
				"x...x"
				"xxxx."
				"x...x"
				"x...x"
				"xxxx.";
			(*chars)['C'] =
				".xxxx"
				"x...."
				"x...."
				"x...."
				"x...."
				"x...."
				".xxxx";
			(*chars)['D'] =
				"xxxx."
				"x...x"
				"x...x"
				"x...x"
				"x...x"
				"x...x"
				"xxxx.";
			(*chars)['E'] =
				"xxxxx"
				"x...."
				"x...."
				"xxxxx"
				"x...."
				"x...."
				"xxxxx";
			(*chars)['F'] =
				"xxxxx"
				"x...."
				"x...."
				"xxxxx"
				"x...."
				"x...."
				"x....";
			(*chars)['G'] =
				"xxxxx"
				"x...."
				"x...."
				"x..xx"
				"x...x"
				"x...x"
				"xxxxx";
			(*chars)['H'] =
				"x...x"
				"x...x"
				"x...x"
				"xxxxx"
				"x...x"
				"x...x"
				"x...x";
			(*chars)['I'] =
				"xxxxx"
				"..x.."
				"..x.."
				"..x.."
				"..x.."
				"..x.."
				"xxxxx";
			(*chars)['J'] =
				"xxxxx"
				"...x."
				"...x."
				"...x."
				"...x."
				"x..x."
				".xx..";
			(*chars)['K'] =
				"x...x"
				"x..x."
				"x.x.."
				"xx..."
				"x.x.."
				"x..x."
				"x...x";
			(*chars)['L'] =
				"x...."
				"x...."
				"x...."
				"x...."
				"x...."
				"x...."
				"xxxxx";
			(*chars)['M'] =
				"xxxxx"
				"x.x.x"
				"x.x.x"
				"x.x.x"
				"x.x.x"
				"x...x"
				"x...x";
			(*chars)['N'] =
				"x...x"
				"xx..x"
				"xx..x"
				"x.x.x"
				"x..xx"
				"x..xx"
				"x...x";
			(*chars)['O'] =
				"xxxxx"
				"x...x"
				"x...x"
				"x...x"
				"x...x"
				"x...x"
				"xxxxx";
			(*chars)['P'] =
				"xxxxx"
				"x...x"
				"x...x"
				"xxxxx"
				"x...."
				"x...."
				"x....";
			(*chars)['Q'] =
				"xxxxx"
				"x...x"
				"x...x"
				"x...x"
				"x...x"
				"xxxx."
				"....x";
			(*chars)['R'] =
				"xxxxx"
				"x...x"
				"x...x"
				"xxxxx"
				"x.x.."
				"x..x."
				"x...x";
			(*chars)['S'] =
				"xxxxx"
				"x...."
				"x...."
				"xxxxx"
				"....x"
				"....x"
				"xxxxx";
			(*chars)['T'] =
				"xxxxx"
				"..x.."
				"..x.."
				"..x.."
				"..x.."
				"..x.."
				"..x..";
			(*chars)['U'] =
				"x...x"
				"x...x"
				"x...x"
				"x...x"
				"x...x"
				"x...x"
				"xxxxx";
			(*chars)['V'] =
				"x...x"
				"x...x"
				"x...x"
				"x...x"
				"x...x"
				".x.x."
				"..x..";
			(*chars)['W'] =
				"x...x"
				"x...x"
				"x...x"
				"x.x.x"
				"x.x.x"
				"x.x.x"
				"xxxxx";
			(*chars)['X'] =
				"x...x"
				".x.x."
				".x.x."
				"..x.."
				".x.x."
				".x.x."
				"x...x";
			(*chars)['Y'] =
				"x...x"
				"x...x"
				"x...x"
				"xxxxx"
				"..x.."
				"..x.."
				"..x..";
			(*chars)['Z'] =
				"xxxxx"
				"....x"
				"...x."
				"...x."
				"..x.."
				".x..."
				"xxxxx";

			(*chars)['0'] =
				"xxxxx"
				"xx..x"
				"x.x.x"
				"x.x.x"
				"x.x.x"
				"x..xx"
				"xxxxx";
			(*chars)['1'] =
				".xx.."
				"x.x.."
				"..x.."
				"..x.."
				"..x.."
				"..x.."
				"xxxxx";
			(*chars)['2'] =
				".xxx."
				"x...x"
				"....x"
				"...x."
				".xx.."
				"x...."
				"xxxxx";
			(*chars)['3'] =
				"xxxx."
				"....x"
				"....x"
				"..xx."
				"....x"
				"....x"
				"xxxx.";
			(*chars)['4'] =
				"...xx"
				".x..x"
				"x...x"
				"xxxxx"
				"....x"
				"....x"
				"....x";
			(*chars)['5'] =
				"xxxxx"
				"x...."
				"x...."
				"xxxx."
				"....x"
				"....x"
				"xxxx.";
			(*chars)['6'] =
				".xxxx"
				"x...."
				"x...."
				"xxxx."
				"x...x"
				"x...x"
				".xxx.";
			(*chars)['7'] =
				"xxxxx"
				"....x"
				"...x."
				"...x."
				"..x.."
				".x..."
				"x....";
			(*chars)['8'] =
				".xxx."
				"x...x"
				"x...x"
				".xxx."
				"x...x"
				"x...x"
				".xxx.";
			(*chars)['9'] =
				".xxx."
				"x...x"
				"x...x"
				".xxxx"
				"....x"
				"....x"
				"xxxx.";
			(*chars)[':'] =
				"....."
				"..x.."
				"..x.."
				"....."
				"..x.."
				"..x.."
				".....";
			(*chars)['.'] =
				"....."
				"....."
				"....."
				"....."
				"....."
				"xx..."
				"xx...";
			(*chars)['-'] =
				"....."
				"....."
				"....."
				"xxxxx"
				"....."
				"....."
				".....";
			(*chars)['['] =
				"xxxxx"
				"x...."
				"x...."
				"x...."
				"x...."
				"x...."
				"xxxxx";
			(*chars)[']'] =
				"xxxxx"
				"....x"
				"....x"
				"....x"
				"....x"
				"....x"
				"xxxxx";
			(*chars)['('] =
				"..xxx"
				".x..."
				"x...."
				"x...."
				"x...."
				".x..."
				"..xxx";
			(*chars)[')'] =
				"xxx.."
				"...x."
				"....x"
				"....x"
				"....x"
				"...x."
				"xxx..";
			(*chars)[','] =
				"....."
				"....."
				"....."
				"....."
				"....."
				".x..."
				"xx...";
			(*chars)['!'] =
				"..x.."
				"..x.."
				"..x.."
				"..x.."
				"..x.."
				"....."
				"..x..";
			(*chars)['_'] =
				"....."
				"....."
				"....."
				"....."
				"....."
				"....."
				"xxxxx";

			(*chars)['/'] =
				"....x"
				"....x"
				"...x."
				"..x.."
				".x..."
				"x...."
				"x....";

			(*chars)['\\'] =
				"x...."
				"x...."
				".x..."
				"..x.."
				"...x."
				"....x"
				"....x";
#pragma endregion
		}
		uvec2 texpos = pos;
		for (auto cv : text)
		{
			auto c = toupper(cv);
			if (c == ' ')
			{
				texpos.x += char_width + 2;
				continue;
			}
			if (c == '\n')
			{
				texpos.x = pos.x;
				texpos.y += char_height + 1;
				continue;
			}
			string chrm = (*chars)[c];
			uvec2 cpos = vec2(0, 0);
			for (auto c : chrm)
			{
				if (c == 'x')
				{
					auto c = cpos + texpos;
					_pixels[c.x + c.y*size.x] = color;
				}
				cpos.x++;
				if (cpos.x >= char_width)
				{
					cpos.x = 0;
					cpos.y++;
					if (cpos.y > char_height)
						break;
				}
			}
			texpos.x += char_width + 2;
		}

	}

	void texture2d::tiled_multithreaded_raster(uvec2 tilesize, function<vec3(uvec2)> f) {
		queue<uvec2> tiles;
		for (uint32 y = 0; y < size.y; y += tilesize.y) {
			for (uint32 x = 0; x < size.x; x += tilesize.x) {
				tiles.push(uvec2(x, y));
			}
		}
		mutex tile_qu_mutex;
		vector<thread> workers;
		for (int P = 0; P < thread::hardware_concurrency(); ++P) {
			workers.push_back(thread([&]() {
				while (true) {
					uvec2 tile;
					{
						unique_lock<mutex> lm(tile_qu_mutex);
						if (tiles.empty()) break;
						tile = tiles.front(); tiles.pop();
					}
					for (uint32 y = tile.y; y < tile.y+tilesize.y && y < size.y; ++y)
						for (uint32 x = tile.x; x < tile.x+tilesize.x && x < size.x; ++x)
							pixel(uvec2(x, y)) = f(uvec2(x, y));
					cout << "~";
				}
			}));
		}
		
		for (auto& t : workers) t.join();
	}
}