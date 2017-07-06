#pragma once
#include "cmmn.h"
#include "midi.h"
#include <glm/gtc/quaternion.hpp>

namespace motion {
	/*
		MIDI events ->
			Percussion -> Hit events -> Animatable paths -> Animation

*/

	struct hit_event {
		float time;
		float duration;
		uint8_t note;
		uint8_t velocity;
		hit_event(float t, float d, uint8_t n, uint8_t v) : time(t), duration(d), note(n), velocity(v) {}
	};

	/*
		keyframes for hitting a note:
		[starting from rest position]
		move down to strike position @ (shortlyBeforeNoteTime, noteTime)
		[hit note] @ noteTime
		move back up to rest position @ (noteTime, aShortTimeAfterNoteTime)
		move to next note @ (aShortTimeAfterNoteTime, shortlyBeforeNextNoteTime)

		last note										next note
		|-------------------------------------------------|
		^----|---------------------------------------|----^
		pickup					move					hit
	*/

	struct loc_rot {
		vec3 pos; quat rot;
		loc_rot(vec3 p = vec3(0.f), vec3 r = vec3(0.f)) : pos(p), rot(quat(r)) {}
		loc_rot(vec3 p, quat r) : pos(p), rot(r) {}

		operator mat4() {
			return translate(mat4(1), pos)*mat4(rot);
		}
	};

	loc_rot mix(loc_rot a, loc_rot b, float t) {
		return loc_rot(mix(a.pos, b.pos, t), slerp(a.rot, b.rot, t));
	}

	/*
		animate the path of a single mallet, origin at the base of the mallet
	*/
	struct single_mallet {
		vector<hit_event> evt; // these aren't necessarily all of the hit events but only the ones that _this mallet_ need to hit
		map<uint8_t, loc_rot> inst_pos; // position the mallet needs to be in to hit the target that makes note @ index
		map<uint8_t, loc_rot> rest_pos; // position the mallet needs to be in position to hit the target that makes note @ index

		mat4 operator()(float t) {
			for (int i = 0; i < evt.size() - 1; ++i) {
				auto e = evt[i];
				if (t >= e.time && t < e.time + e.duration) {
					//cout << t << " " << e.note << " " << e.duration << endl;
					float T = t - e.time;
					float x = T / e.duration;
					if (x < 0.1f) { // picking mallet up off the last note
						return mix(inst_pos.at(e.note), rest_pos.at(e.note), x*10.f);
					}
					else if (x < 0.9f) { // moving to the next note
						return mix(rest_pos.at(e.note), rest_pos.at(evt[i + 1].note), (x - 0.1f)*1.25f);
					}
					else { // strike the next note
						return mix(rest_pos.at(evt[i + 1].note), inst_pos.at(evt[i + 1].note), (x - 0.9f)*10.f);
					}
				}
			}
			return rest_pos[evt[evt.size() - 1].note];
		}
	};

}