#include "..\whrt5\midi.h"
#include <iostream>
#include <thread>
#include <Windows.h>

uint32_t freq_from_midi(uint8_t midi) {
	return (uint32_t)(pow(2, (((float)midi) - 69.f) / 12.f)*440.f);
}

/* 
	MIDI events ->
		Percussion -> Hit events -> Animatable paths -> Animation

*/

struct hit_event {
	float time;
	float duration;
	uint8_t note;
	uint8_t velocity;
};

#ifdef PoC
vector<hit_event> evt; // in chronological order
int animate_single_mallet(float t) {
	for (int i = 0; i < evt.size(); ++i) {
		auto e = evt[i];
		if (t > e.time && t < e.time + e.duration) {
			// found current event
			float T = t - e.time; //time since start of note
			float pt = T / e.duration;
			if (pt < 0.5f) {
				// lift up mallet
				return lerp(INSTR_POS[e.note], REST_POS[e.note], pt*2.f);
			}
			else {
				return lerp(REST_POS[e.note], INSTR_POS[evt[i+1].note], (pt-.5f)*2.f);
			}
			return 0;
		}
	}
}
#endif

int main() {
	FILE* f; fopen_s(&f, R"(C:\Users\andre\Source\whrt5\whrt5\test.mid)", "rb");
	struct stat stat_buf;
	stat(R"(C:\Users\andre\Source\whrt5\whrt5\test.mid)", &stat_buf);
	auto len = stat_buf.st_size;
	uint8_t* data = new uint8_t[len];
	auto r = fread(data, 1, len, f);
	auto m = midi::midi_file(data, len);
	auto t = m.tracks[2];
	{
		for (auto x : t) {
			auto tn = dynamic_pointer_cast<midi::text_event>(x);
			if (tn) {
				cout << tn->type << " " << tn->text << endl;
				continue;
			}
			auto p = dynamic_pointer_cast<midi::note_on>(x);
			if (p) {
				Beep(freq_from_midi(p->note), 200);
				cout << "note on  " << p->delta_time << " c" << (int)p->channel << " n" << (int)p->note << " v" << (int)p->velocity << endl;
				continue;
			}
			auto s = dynamic_pointer_cast<midi::note_off>(x);
			if (s) {
				cout << "note off " << s->delta_time << " c" << (int)s->channel << " n" << (int)s->note << " v" << (int)s->velocity << endl;
				continue;
			}
		}
	}
	getchar();
}