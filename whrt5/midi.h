#pragma once
#include <vector>
#include <cassert>
#include <string>
#include <memory>

using namespace std;

namespace midi {

	struct midi_event {
		size_t delta_time;
		midi_event(size_t delta_time) : delta_time(delta_time) {}
		virtual ~midi_event() {}
	};

	uint32_t read_varlen(uint8_t*& data) {
		uint32_t num = 0;
		while (*data & 0x80) {
			num <<= 8;
			num |= *data & 0xef;
			data++;
		}
		num |= *data; data++;
		return num;
	}

	uint32_t readd(uint8_t* data) {
		return data[3] | data[2] << 8 | data[1] << 16 | data[0] << 24;
	}
	uint16_t readw(uint8_t* data) {
		return data[1] | data[0] << 8;
	}

	struct text_event : public midi_event {
		string text; uint8_t type;
		text_event(size_t dt, uint8_t*& data) : midi_event(dt) {
			assert(*data == 0xff);
			data++;
			type = *data; data++;
			size_t tlen = read_varlen(data);
			text = string(data, data + tlen);
			data += tlen;
		}
	};

	struct end_of_track : public midi_event {
		end_of_track(size_t dt, uint8_t*& data) : midi_event(dt) {
			data += 3;
		}
	};
	struct tempo_set : public midi_event {
		uint32_t tempo;
		tempo_set(size_t dt, uint8_t*& data) : midi_event(dt) {
			assert(*data == 0xff); data++;
			assert(*data == 0x51); data++;
			assert(*data == 0x03); data++;
			tempo = data[2] | data[1] << 8 | data[0] << 16;
		}
	};
	struct time_sig : public midi_event {
		uint8_t numer, denom, clcl, nqn;
		time_sig(size_t dt, uint8_t*& data) : midi_event(dt) {
			assert(*data == 0xff); data++;
			assert(*data == 0x58); data++;
			assert(*data == 0x04); data++;
			numer = *data; ++data;
			denom = *data; ++data;
			clcl = *data; ++data;
			nqn = *data; ++data;
		}
	};

	struct note_on : public midi_event {
		uint16_t channel;
		uint8_t note, velocity;
		note_on(size_t dt, uint8_t*& data) : midi_event(dt) {
			assert(*data >= 0x90 && *data <= 0x9f);
			channel = *data & 0xef; data++;
			note = *data; data++;
			velocity = *data; data++;
		}
	};
	struct note_off : public midi_event {
		uint16_t channel;
		uint8_t note, velocity;
		note_off(size_t dt, uint8_t*& data) : midi_event(dt) {
			assert(*data >= 0x80 && *data <= 0x8f);
			channel = *data & 0xef; data++;
			note = *data; data++;
			velocity = *data; data++;
		}
	};
	struct program_change : public midi_event {

	};

	struct midi_file {
		enum class track_format : uint16_t {
			single_multichannel = 0,
			simultaneous = 1,
			independent = 2
		} format;
		vector<vector<shared_ptr<midi_event>>> tracks;
		size_t ticks_per_quarter_note;

		midi_file(uint8_t* data, size_t len) {
			uint8_t* data_end = data+len;
			assert(strcmp((const char*)data, "MThd") == 0); data += 4; // check header chunk type
			assert(readd(data) == 6); data += sizeof(uint32_t); // check header length
			format = (track_format)readw(data); data += sizeof(uint16_t);
			auto num_tracks = readw(data); data += sizeof(uint16_t);
			uint16_t div = readw(data); data += sizeof(uint16_t);
			if ((div & 0x8000)) {
				ticks_per_quarter_note = -1; // wierd SMPTE format
			}
			else {
				ticks_per_quarter_note = div;
			}
			while (data < data_end) {
				assert(strcmp((const char*)data, "MTrk") == 0); data += 4; // check header chunk type
				uint32_t len = readd(data); data += sizeof(uint32_t);
				vector<shared_ptr<midi_event>> track;
				uint8_t* end = data + len;
				while (data < end) {
					uint32_t dt = read_varlen(data);
					uint8_t fb = *data;
					if (fb == 0xff) {
						uint8_t sb = *(data + 1);
						if (sb >= 1 && sb <= 0xf) track.push_back(make_shared<text_event>(dt, data));
						else if (sb == 0x2f) track.push_back(make_shared<end_of_track>(dt, data));
						else if (sb == 0x51) track.push_back(make_shared<tempo_set>(dt, data));
						else if (sb == 0x58) track.push_back(make_shared<time_sig>(dt, data));
						else data++;
					}
					else {
						uint8_t first_byte = fb >> 4;
						if (first_byte == 0b1001)
							track.push_back(make_shared<note_on>(dt, data));
						else if (first_byte == 0b1000)
							track.push_back(make_shared<note_off>(dt, data));
						else data++;
					}
				}
				tracks.push_back(track);
			}
		}
	};
}
