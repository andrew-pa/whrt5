#include "video.h"

namespace whrt5 {
	inline unsigned char f2b(float f, float bias) {
		return clamp(f*255.f + bias, 0.f, 255.f);
	}
	video::video(const string& fn, uvec2 frame_size, pair<uint32, uint32> fps) {
		fopen_s(&of, fn.c_str(), "wb");
		ogg_stream_init(&ost, rand());

		th_info ti;
		th_info_init(&ti);
		ti.frame_width = ((frame_size.x + 15) >> 4) << 4; //round to multiples of 16
		ti.frame_height = ((frame_size.y + 15) >> 4) << 4;
		ti.pic_width = frame_size.x;
		ti.pic_height = frame_size.y;
		ti.pic_x = ti.pic_y = 0;
		ti.fps_numerator = fps.first;
		ti.fps_denominator = fps.second;
		ti.aspect_numerator = frame_size.y;
		ti.aspect_denominator = frame_size.x;
		ti.colorspace = TH_CS_UNSPECIFIED;
		ti.pixel_fmt = TH_PF_420;
		ti.target_bitrate = 0;
		ti.quality = 63;

		enc = th_encode_alloc(&ti);
		th_info_clear(&ti);

		ogg_packet op; ogg_page og;
		th_comment tc;
		th_comment_init(&tc);
		th_encode_flushheader(enc, &tc, &op);
		th_comment_clear(&tc);
		ogg_stream_packetin(&ost, &op);
		ogg_stream_pageout(&ost, &og);
		fwrite(og.header, 1, og.header_len, of);
		fwrite(og.body, 1, og.body_len, of);
		while (th_encode_flushheader(enc, &tc, &op)) {
			ogg_stream_packetin(&ost, &op);
		}
		ogg_stream_flush(&ost, &og);
		fwrite(og.header, 1, og.header_len, of);
		fwrite(og.body, 1, og.body_len, of);
	}
	void video::write_frame(const texture2d& tx, bool last) {
		auto yuvw = (tx.size.x + 15)&~15, yuvh = (tx.size.y + 15)&~15;
		th_ycbcr_buffer buf;
		buf[0].width = yuvw; buf[0].height = yuvh;
		buf[0].stride = yuvw;
		auto Y = buf[0].data = new unsigned char[buf[0].stride*buf[0].height];

		buf[1].width = yuvw >> 1; buf[1].height = yuvh >> 1;
		buf[1].stride = yuvw >> 1;
		auto U = buf[1].data = new unsigned char[buf[1].stride*buf[1].height];

		buf[2].width = yuvw >> 1; buf[2].height = yuvh >> 1;
		buf[2].stride = yuvw >> 1;
		auto V = buf[2].data = new unsigned char[buf[2].stride*buf[2].height];

		for (uint32 y = 0; y < tx.size.y; y += 2) {
			for (uint32 x = 0; x < tx.size.x; x += 2) {
				float u = 0.f, v = 0.f;
				for (uint8 dy = 0; dy < 2; ++dy) {
					if (y + dy > tx.size.y) dy = 0;
					for (uint8 dx = 0; dx < 2; ++dx) {
						if (x + dx > tx.size.x) dx = 0;
						vec3 pa = tx.pixel(uvec2(x + dx, y + dy));
						Y[(x + dx) + (y + dy)*yuvw] = f2b(0.299f*pa.r + 0.587f*pa.g + 0.114f*pa.b, 16);
						u += -0.168736f*pa.r - 0.331264f*pa.g + 0.5f*pa.b;
						v += 0.5f*pa.r - 0.418688f*pa.g - 0.081312*pa.b;
					}
				}
				U[(x >> 1) + (y >> 1) * (yuvw >> 1)] = f2b(u, 128);
				V[(x >> 1) + (y >> 1) * (yuvw >> 1)] = f2b(v, 128);
			}
		}

		ogg_packet op; ogg_page og;
		th_encode_ycbcr_in(enc, buf);
		th_encode_packetout(enc, last, &op);
		ogg_stream_packetin(&ost, &op);
		while (ogg_stream_pageout(&ost, &og)) {
			fwrite(og.header, 1, og.header_len, of);
			fwrite(og.body, 1, og.body_len, of);
		}

		delete[] Y, U, V;
	}
	void video::flush() {
		ogg_page og;
		if (ogg_stream_flush(&ost, &og)) {
			fwrite(og.header, 1, og.header_len, of);
			fwrite(og.body, 1, og.body_len, of);
		}
		fflush(of);
	}
	video::~video() {
		flush();
		th_encode_free(enc);
		ogg_stream_clear(&ost);
		fclose(of);
	}
}
