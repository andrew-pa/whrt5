
#include "cmmn.h"
#include "texture.h"
#include "camera.h"
#include "video.h"
#include "surface.h"
#include "motion.h"

namespace whrt5 {

	enum class interpolation {
		linear, exp, log
	};
	template<typename T>
	struct keyframes {
		struct key {
			float t;
			T value;
			interpolation interp;
			float k;
			key(float t, T v, interpolation i = interpolation::linear, float k = 1.f)
				: t(t), value(v), interp(i), k(k) {}
		};
		vector<key> keys;

		keyframes(const vector<key> keys) : keys(keys) {}
		keyframes(initializer_list<key> keys) : keys(keys.begin(), keys.end()) {}

		T operator()(float t) const {
			for (size_t i = 0; i < keys.size()-1; ++i) {
				float tt = keys[i].t, nt = keys[i + 1].t;
				if (tt < t && t < nt) {
					auto M = (t-tt)/(nt-tt);
					switch (keys[i].interp) {
					case interpolation::exp: M = exp(keys[i].k * M); break;
					case interpolation::log: M = log(keys[i].k * M); break;
					}
					return mix(keys[i].value, keys[i + 1].value, M);
				}
			}
			return keys[keys.size() - 1].value;
		}
	};

	struct material {
		shared_ptr<texture<vec3, vec2>> tex;
		float reflect;

		material(shared_ptr<texture<vec3, vec2>> t, float ref = 0.f) : tex(t), reflect(ref) {}
	};
	struct hit_record : public surfaces::hit_record {
		shared_ptr<material> mat;
	};
	struct primitive {
		virtual bool hit(const ray& r, hit_record* hr) const = 0;
	};
	struct surface_primitive : public primitive {
		shared_ptr<material> mat;
		shared_ptr<surfaces::surface> surf;

		surface_primitive(shared_ptr<surfaces::surface> surf, shared_ptr<material> m)
			: surf(surf), mat(m) {
		}

		bool hit(const ray& r, hit_record* hr) const {
			if (surf->hit(r, hr)) {
				hr->mat = mat;
				return true;
			}
			return false;
		}
	};

	struct transform_primitive : public primitive {
		shared_ptr<primitive> p;
		animated<mat4> transform;

		transform_primitive(shared_ptr<primitive> p, animated<mat4> t) : p(p), transform(t) {}

		bool hit(const ray& r, hit_record* hr) const {
			auto t = inverse(transform(r.time));
			auto R = ray(t*vec4(r.e, 1.f), t*vec4(r.d, 0.f), r.time);
			return p->hit(R, hr);
		}
	};

	struct pgroup : public primitive {
		vector<shared_ptr<primitive>> objs;
		pgroup(vector<shared_ptr<primitive>> s) : objs(s) {}
		pgroup(initializer_list<shared_ptr<primitive>> s) : objs(s.begin(), s.end()) {}

		bool hit(const ray& r, hit_record* hr) const override {
			hit_record low; bool hit = false;
			for (const auto s : objs) {
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

	struct renderer {
		shared_ptr<primitive> scene;
		camera cam;
		const uint8 smp;
		renderer(shared_ptr<primitive> scene, camera cam, uint8 smp)
			: scene(scene), cam(cam), smp(smp) {}

		vec3 background(const ray&) {
			return vec3(0.05f, 0.05f, 0.5f);
		}

		vec3 ray_color(const ray& r, uint32_t rc = 0) {
			if (rc > 6) return background(r);
			const vec3 L = vec3(0.f, 1.f, 0.f);
			hit_record hr;
			if (scene->hit(r, &hr)) {
				if (hr.mat == nullptr) return background(r);
				vec3 p = r(hr.t);
				hit_record shr;
				float shadow = 1.f;
				auto sr = ray(p + hr.norm*0.01f, L, r.time);
				if (scene->hit(sr, &shr)) {
					shadow = .0f;
				}
				vec3 col = hr.mat->tex->texel(hr.texc)*(glm::max(0.f, dot(hr.norm, L))*shadow);
				if (hr.mat->reflect > 0.f) {
					col += hr.mat->reflect * ray_color(ray(p + hr.norm*0.01f, reflect(r.d, hr.norm), r.time), rc + 1);
				}
				return col;
			}
			else
				return background(r);
		}

		void render(texture2d& rt, float t) {
			auto render_start = chrono::high_resolution_clock::now();
			rt.tiled_multithreaded_raster(uvec2(32), [&](uvec2 px) {
				vec3 col = vec3(0.f);
				for (uint8 sy = 0; sy < smp; ++sy)
					for (uint8 sx = 0; sx < smp; ++sx) {
						vec2 ss = (vec2(sx, sy) + rnd::randf2()) / (float)smp;
						vec2 uv = (((vec2)(px)+ss) / (vec2)rt.size)*2.f - 1.f;
						auto r = cam.generate_ray(uv, t);
						col += ray_color(r);
					}
				col /= (float)(smp*smp);
				col = pow(col, vec3(1.f / 2.2f));
				return col;
			});
			auto render_time = chrono::high_resolution_clock::now() - render_start;
			ostringstream watermark;
			watermark << "render took " << chrono::duration_cast<chrono::milliseconds>(render_time).count() << "ms" << endl;
			rt.draw_text(watermark.str(), uvec2(2, 2), vec3(1.f, 1.f, 0.f));
		}
	};
}

using namespace whrt5;
#define VIDEO
int main(int argc, char* argv[]) {
	//rnd::RNG = mt19937(random_device()());

	{
		animated<float> a(4.f);
		animated<float> b([](float t) {return sinf(t); });
		cout << a(0) << " " << a(1) << " " << b(0) << " " << b(1) << endl;
		assert(a(0) == 4.f);
		assert(a(1) == 4.f);
		assert(b(0) == 0.f);
	}

	ostringstream fns;
	fns << "r" << chrono::system_clock::now().time_since_epoch().count()
#ifdef VIDEO
		<< ".ogg";
#else
		<< ".bmp";
#endif
	uint32 fps = 30;
	auto res = //uvec2(320, 240);
				uvec2(640, 480);
	const int fc = fps * 15;
	const uint8 smp = 8; 
#ifdef TEST
	auto rndr = renderer(make_shared<pgroup>(
		pgroup {
			/*make_shared<surface_primitive>(make_shared<surfaces::sphere>([](float t) {
				return vec3(cosf(t*pi<float>()*4.f)*2.f,
							sinf(t*pi<float>()*4.f)*2.f + 3.f,
							cosf(t)*2.f);
				}, 1.f), 
				make_shared<material>(make_shared<const_texture<vec3, vec2>>(vec3(0.8f, 0.6f, 0.f)))
			),
			make_shared<surface_primitive>(make_shared<surfaces::sphere>(vec3(0.f,1.5f,0.f), 1.5f),
				make_shared<material>(/*make_shared<grid_texture>(vec3(0.4f, 0.0f, .8f), vec3(0.6f, 0.6f, 0.6f), 8.f, 0.2f)*/// make_shared<const_texture<vec3,vec2>>(vec3(0.f)), .99f)
			//),
			/*make_shared<surface_primitive>(make_shared<surfaces::sphere>([](float t) {
				return vec3(cosf(t*pi<float>()*2.f + pi<float>())*2.f,
				sinf(t*pi<float>()*2.f + pi<float>())*2.f + 3.f,
				sinf(t)*2.f);
				}, 1.f),
				make_shared<material>(make_shared<checkerboard_texture>(vec3(1.f), vec3(0.f), 8.f))
			),
			/*make_shared<transform_primitive>(
				make_shared<surface_primitive>(make_shared<surfaces::box>(vec3(0.f), vec3(.5f, .5f, .5f)),
					make_shared<checkerboard_texture>(vec3(0.f, 1.f, 1.f), vec3(1.f, 0.f, 0.f), 2.f)), 
				[](float t) {
					return translate(mat4(1), vec3(0.f, 3.f, 0.f));//, t, vec3(0.3f, 0.8f, 0.6f));
				}
			),*/
			/*make_shared<transform_primitive>(
				make_shared<surface_primitive>(make_shared<surfaces::cylinder>(1.0f, 5.f),
					make_shared<material>(make_shared<checkerboard_texture>(vec3(1.f), vec3(0.0f), 2.f))
				),
				[](float t) {
					return rotate(translate(mat4(1), vec3(0.f, 5.f, 0.f)), t*5.f, vec3(-0.4f, 0.2f, 1.f));
				}
			),*/
			make_shared<surface_primitive>(make_shared<surfaces::sphere>(keyframes<vec3> {
					{0.f, vec3(0.f, 1.f, 0.f)},
					{2.5f, vec3(5.f, 1.f, 0.f), interpolation::exp},
					{5.f, vec3(5.f, 1.f, 5.f)},
					{7.5f, vec3(0.f, 1.f, 5.f), interpolation::log},
					{10.f, vec3(0.f, 1.f, 0.f)},
				}, 1.f),
				make_shared<material>(make_shared<checkerboard_texture>(vec3(1.f), vec3(0.f), 8.f))
			),
			make_shared<surface_primitive>(make_shared<surfaces::box>(vec3(0.f), vec3(5.f, 0.1f, 5.f)),
				make_shared<material>(make_shared<checkerboard_texture>(vec3(1.f, 1.f, 0.f), vec3(0.f, 1.f, 0.f), 2.f)))
		}),
		camera(vec3(0.f, 12.f, -12.f), vec3(0.f), 0.01f, 5.f, 1.f / (float)fps), smp
	);
#else
	auto scene = make_shared<pgroup>(pgroup{
		make_shared<surface_primitive>(make_shared<surfaces::box>(vec3(0.f), vec3(5.f, 0.1f, 5.f)),
			make_shared<material>(make_shared<checkerboard_texture>(vec3(1.f, 1.f, 0.f), vec3(0.f, 1.f, 0.f), 2.f)))
	});

	auto mallet1 = motion::single_mallet();
	auto bar_mat = make_shared<material>(make_shared<const_texture<vec3, vec2>>(vec3(0.6f, 0.2f, 0.9f)));
	for (int i = 0; i < 5; ++i) {
		vec3 p = vec3((float)i / 2.f, .5f, 0.f);
		mallet1.inst_pos[i + 60] = motion::loc_rot(p+vec3(0.f, 0.2f, -.7f), vec3(-.3f + pi<float>()*0.5f, 0.f, 0.f));
		mallet1.rest_pos[i + 60] = motion::loc_rot(p+vec3(0.f, 0.3f, -.8f), vec3(.1f + pi<float>()*0.5f, 0.f, 0.f));
		scene->objs.push_back(make_shared<surface_primitive>(make_shared<surfaces::box>(
			p, vec3(.2f, 0.05f, .5f + (float)i / 4.f)), bar_mat));
	}
	for (int i = 0; i < 16; ++i) {
		mallet1.evt.push_back(motion::hit_event((float)i, 1.f, 60+rand()%5, 255));
	}

	scene->objs.push_back(make_shared<transform_primitive>(make_shared<surface_primitive>(make_shared<surfaces::cylinder>(0.15f, 1.f),
		make_shared<material>(make_shared<const_texture<vec3, vec2>>(vec3(0.4f)))), mallet1));

	auto rndr = renderer(scene, camera(vec3(3.f, 6.f, -4.f), vec3(0.f), 0.01f, 5.f, 1.f / (float)fps), smp);
#endif

	auto rt = texture2d(res);
	
#ifdef VIDEO
	video v{ fns.str(), res, {fps,1} };
	for (uint i = 0; i < fc; ++i) {
		rndr.render(rt, (float)i / (float)fps);
		v.write_frame(rt, i == fc - 1);
		cout << "frame " << i << " of " << fc << endl;
	}
	v.flush();
#else
	rndr.render(rt, 3.f);
	rt.write_bmp(fns.str());
#endif


	/*ostringstream fns;
	fns << "image_" << chrono::system_clock::now().time_since_epoch().count() << ".bmp";
	rt.write_bmp(fns.str());*/
	return 0;
}