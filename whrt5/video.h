#pragma once
#include "cmmn.h"
#include "texture.h"

#include <ogg/ogg.h>
#include <theora/theoraenc.h>

namespace whrt5 {
	// video output via Ogg Theora encoding
	class video {
		FILE* of;
		ogg_stream_state ost;
		th_enc_ctx* enc;
	public:
		video(const string& fn, uvec2 frame_size, pair<uint32, uint32> fps);
		void write_frame(const texture2d& tx, bool last);
		void flush();

		~video();
	};
}
