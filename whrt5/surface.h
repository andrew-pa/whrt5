#pragma once
#include "cmmn.h"
#include "texture.h"

namespace whrt5 {
	namespace surfaces {
		struct hit_record {
			float t;
			vec3 norm;
			vec2 texc;
			hit_record() : t(10000.f) {}
		};
		struct surface {
			virtual bool hit(const ray& r, hit_record* hr) const = 0;
		};

		struct group : public surface {
			vector<shared_ptr<surface>> surfaces;
			group(vector<shared_ptr<surface>> s) : surfaces(s) {}
			group(initializer_list<shared_ptr<surface>> s) : surfaces(s.begin(), s.end()) {}

			bool hit(const ray& r, hit_record* hr) const override {
				hit_record low; bool hit = false;
				for (const auto s : surfaces) {
					hit_record thr;
					if (s->hit(r, &thr)) {
						if (thr.t < low.t) low = thr;
						hit = true;
					}
				}
				if (hr != nullptr && low.t < hr->t) *hr = low;
				return hit;
			}
		};

		struct sphere : public surface {
			animated<vec3> center; float radius;

			sphere(animated<vec3> c, float r) : center(c), radius(r) {}

			/*inline aabb bounds() const override {
			return aabb(center - radius, center + radius);
			}*/

			bool hit(const ray& r, hit_record* hr) const override {
				vec3 centr = center(r.time);
				vec3 v = r.e - centr;
				float b = -dot(v, r.d);
				float det = (b*b) - dot(v, v) + radius*radius;
				if (det < 0) return false;
				det = sqrt(det);
				float i1 = b - det, i2 = b + det;
				if (i2 > 0 && i1 > 0) {
					if (hr == nullptr) return true;
					if (hr->t < i1) return false;
					hr->t = i1;
					hr->norm = normalize(r(i1) - centr);
					float cos_phi = -dot(hr->norm, vec3(0, 1, 0));
					float phi = acosf(cos_phi);
					float sin_phi = sin(phi);
					hr->texc.y = phi * one_over_pi<float>();
					float theta = acosf(dot(vec3(0, 0, -1), hr->norm) / sin_phi) * two_over_pi<float>();
					if (dot(vec3(1, 0, 0), hr->norm) >= 0) theta = 1.f - theta;
					hr->texc.x = theta;
					/*				auto p = r(i1);
					hr->dpdu = vec3(-2.f*pi<float>()*p.y, 2.f*pi<float>()*p.x, 0);
					hr->dpdv = vec3(p.z*cos_phi, p.z*sin_phi, -radius*sin(theta));

					hr->surf = this;*/
					return true;
				}
				return false;
			}
		};

		struct cylinder : public surface {
			float radius;
			float height;

			cylinder(float r, float h) : radius(r), height(h) {}
			
			bool hit(const ray& r, hit_record* hr) const override {
				// (ox+dx*t)^2 + (oz+dz*t)^2 = radius^2; 0 < y < height
				float I1 = 2.f * dot(r.e.xz(), r.d.xz());
				float denm = 2.f * dot(r.d.xz(), r.d.xz());
				float det = I1*I1 - 2.f*denm*(dot(r.e.xz(), r.e.xz()) - radius*radius);
				if (det < 0.f) return false;
				det = sqrt(det);
				float t1 = (det - 2.f*r.e.x*r.d.x - 2.f*r.e.z*r.d.z)/denm;
				float t2 = (-det - 2.f*r.e.x*r.d.x - 2.f*r.e.z*r.d.z)/denm;
				float t = glm::min(t1, t2);
				vec3 p = r(t);
				if (t < 0.f || p.y < 0.f || p.y > height) return false;
				if (hr != nullptr) {
					if (hr->t < t) return false;
					hr->t = t;
					hr->norm = normalize(vec3(p.x, 0.01f, p.z));
					hr->texc = vec2(atan(p.z/p.x), p.y);
					return true;
				}
				else {
					return true;
				}
			}
		};

		struct disk : public surface {
			float radius;
			vec3 norm;
			vec3 center;

			disk(vec3 center, float r, vec3 nm = vec3(0.f, 1.f, 0.f))
				: center(center), radius(r), norm(nm) {}

			bool hit(const ray& r, hit_record* hr) const override {
				float D = dot(norm, r.d);
				if (abs(D) > 0.000001f) {
					if (hr == nullptr) return true;
					float t = dot(center - r.e, norm) / D;
					if (t > hr->t) return false;
					vec3 p = r(t);
					if (dot(p, p) > radius*radius) return false;
					hr->t = t;
					hr->norm = norm;
					hr->texc = cross(p, norm).xz;
					return true;
				}
				return false;
			}
		};

		struct box : public surface {
			vec3 _min, _max;

			box(vec3 center, vec3 extent)
				: _min(center - extent), _max(center + extent) {}

			bool hit(const ray& r, hit_record* hr) const override {
				vec3 rrd = 1.f / r.d;

				vec3 t1 = (_min - r.e) * rrd;
				vec3 t2 = (_max - r.e) * rrd;

				vec3 m12 = glm::min(t1, t2);
				vec3 x12 = glm::max(t1, t2);

				float tmin = m12.x;
				tmin = glm::max(tmin, m12.y);
				tmin = glm::max(tmin, m12.z);

				float tmax = x12.x;
				tmax = glm::min(tmax, x12.y);
				tmax = glm::min(tmax, x12.z);

				if (tmax < tmin || tmin < 0) return false;
				if (hr == nullptr) return tmax >= tmin;
				if (hr->t < tmin) return false;
				hr->t = tmin;

				vec3 center = (_max + _min) * 0.5f;
				vec3 extents = _max - center;
				static const vec3 axises[] =
				{
				vec3(1,0,0),
				vec3(0,1,0),
				vec3(0,0,1),
				};
				vec3 n = vec3(0);
				float m = FLT_MAX;
				float dist;
				vec3 np = r(tmin) - center;
				for (int i = 0; i < 3; ++i)
				{
					dist = fabsf(extents[i] - fabsf(np[i]));
					if (dist < m)
					{
						m = dist;
						n = sign(np[i])*axises[i];
					}
				}
				hr->norm = n;
				hr->texc = cross(np, n).xz;
				return true;
			}
		};
	}
}
