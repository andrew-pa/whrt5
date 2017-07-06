#include "..\whrt5\midi.h"
#include <iostream>
#include <thread>
#include <Windows.h>
#include <map>

uint32_t freq_from_midi(uint8_t midi) {
	return (uint32_t)(pow(2, (((float)midi) - 69.f) / 12.f)*440.f);
}

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