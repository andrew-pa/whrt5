
#include "cmmn.h"
#include "texture.h"
#include "camera.h"
#include "video.h"

namespace whrt5 {
	struct hit_record {
		float t;
		vec3 norm;
		hit_record() : t(10000.f) {}
	};
	struct surface {
		virtual bool hit(const ray& r, hit_record* hr) const = 0;
	};

	template<typename T>
	struct animated {
		function<T(float)> F;
		T cv;
		bool const_val;
		animated(T t) : cv(t), const_val(true), F([t](float) {return t; }) {}
		template<typename Func>
		animated(Func f) : F(f), const_val(false) {}

		T operator()(float t) const { return const_val ? cv : F(t); }
	};

	struct group : public surface {
		vector<shared_ptr<surface>> surfaces;
		group(vector<shared_ptr<surface>> s) : surfaces(s) {}
		group(initializer_list<shared_ptr<surface>> s) : surfaces(s.begin(), s.end()) {}

		bool hit(const ray& r, hit_record* hr) const override {
			for (const auto s : surfaces)
				if (s->hit(r, hr)) return true;
			return false;
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
/*				float cos_phi = -dot(hr->norm, vec3(0, 1, 0));
				float phi = acosf(cos_phi);
				float sin_phi = sin(phi);
				hr->texture_coords.y = phi * one_over_pi<float>();
				float theta = acosf(dot(vec3(0, 0, -1), hr->norm) / sin_phi) * two_over_pi<float>();
				if (dot(vec3(1, 0, 0), hr->norm) >= 0) theta = 1.f - theta;
				hr->texture_coords.x = theta;

				auto p = r(i1);
				hr->dpdu = vec3(-2.f*pi<float>()*p.y, 2.f*pi<float>()*p.x, 0);
				hr->dpdv = vec3(p.z*cos_phi, p.z*sin_phi, -radius*sin(theta));

				hr->surf = this;*/
				return true;
			}
			return false;
		}
	};
}

using namespace whrt5;

int main(int argc, char* argv[]) {
	rnd::RNG = mt19937(random_device()());

	{
		animated<float> a(4.f);
		animated<float> b([](float t) {return sinf(t); });
		cout << a(0) << " " << a(1) << " " << b(0) << " " << b(1) << endl;
		assert(a(0) == 4.f);
		assert(a(1) == 4.f);
		assert(b(0) == 0.f);
	}

	ostringstream fns;
	fns << "anim_" << chrono::system_clock::now().time_since_epoch().count() << ".ogg";
	uint32 fps = 30;
	auto res = uvec2(640,480);
	const int fc = fps*5;
	const uint8 smp = 6;

	video v{ fns.str(), res, {fps,1} };

	auto objs = group{
		make_shared<sphere>([](float t) { return vec3(cosf(t*pi<float>()*4.f)*2.f,sinf(t*pi<float>()*4.f)*2.f,0.f); }, 1.f),
		make_shared<sphere>([](float t) { return vec3(cosf(t*pi<float>()*4.f + pi<float>())*2.f,sinf(t*pi<float>()*4.f + pi<float>())*2.f,sinf(t)*2.f); }, 1.f)
	};
	auto cam = camera(vec3(0.f, 0.f, -8.f), vec3(0.f), 0.01f, 5, 1.f/(float)fps);
	auto rt = texture2d(res);
	for (int i = 0; i < fc; ++i) {
		auto render_start = chrono::high_resolution_clock::now();
		rt.tiled_multithreaded_raster(uvec2(32), [&](uvec2 px) {
			vec3 col = vec3(0.f);
			for (uint8 sy = 0; sy < smp; ++sy)
				for (uint8 sx = 0; sx < smp; ++sx) {
					vec2 ss = (vec2(sx, sy) + rnd::randf2()) / (float)smp;
					vec2 uv = (((vec2)(px)+ss) / (vec2)rt.size)*2.f - 1.f;
					hit_record hr;
					auto r = cam.generate_ray(uv, (float)i / (float)fps);
					if (objs.hit(r, &hr)) {
						col += vec3(1.f);
					}
					else {
						col += vec3(0.05f, 0.05f, 0.5f);
					}
				}
			col /= (float)(smp*smp);
			col = pow(col, vec3(1.f / 2.2f));
			return col;
		});
		auto render_time = chrono::high_resolution_clock::now() - render_start;
		ostringstream watermark;
		watermark << "render took " << chrono::duration_cast<chrono::milliseconds>(render_time).count() << "ms" << endl
			<< i << "/" << fc << endl;
		rt.draw_text(watermark.str(), uvec2(2, 2), vec3(1.f, 1.f, 0.f));
		v.write_frame(rt, i == fc-1);
		cout << "frame " << i << " of " << fc << endl;
	}
	v.flush();

	/*ostringstream fns;
	fns << "image_" << chrono::system_clock::now().time_since_epoch().count() << ".bmp";
	rt.write_bmp(fns.str());*/
	return 0;
}