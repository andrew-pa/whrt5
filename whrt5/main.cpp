
#include "cmmn.h"
#include "texture.h"
#include "camera.h"
#include "video.h"
#include "surface.h"

namespace whrt5 {

	struct material {
		shared_ptr<texture<vec3, vec2>> tex;
		float reflect;

		material(shared_ptr<texture<vec3, vec2>> t, float ref = 0.f) : tex(t), reflect(ref) {}
	};
	struct hit_record : public surfaces::hit_record {
		material* mat;
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
				hr->mat = mat.get();
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
			auto R = ray(vec4(r.e, 1.f)*t, vec4(r.d, 0.f)*t, r.time);
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
				vec3 p = r(hr.t);
				hit_record shr;
				float shadow = 1.f;
				auto sr = ray(p + hr.norm*0.01f, L, r.time);
				if (scene->hit(sr, &shr)) {
					shadow = 0.f;
				}
				vec3 col =  hr.mat->tex->texel(hr.texc)*glm::max(0.f, dot(hr.norm, L))*shadow;
				if (hr.mat->reflect > 0.f) {
					col += hr.mat->reflect * ray_color(ray(p + hr.norm*0.01f, reflect(r.d, hr.norm)), rc + 1);
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
//#define VIDEO
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
	fns << "r" << chrono::system_clock::now().time_since_epoch().count()
#ifdef VIDEO
		<< ".ogg";
#else
		<< ".bmp";
#endif
	uint32 fps = 30;
	auto res = uvec2(1920, 1080); //uvec2(640, 480);
	const int fc = fps * 10;
	const uint8 smp = 8;

	auto rndr = renderer(make_shared<pgroup>(
		pgroup {
			make_shared<surface_primitive>(make_shared<surfaces::sphere>([](float t) {
				return vec3(cosf(t*pi<float>()*2.f)*2.f,
							sinf(t*pi<float>()*2.f)*2.f + 3.f,
							cosf(t)*2.f);
				}, 1.f), 
				make_shared<material>(make_shared<const_texture<vec3, vec2>>(vec3(0.8f, 0.6f, 0.f)))
			),
			make_shared<surface_primitive>(make_shared<surfaces::sphere>([](float t) {
				return vec3(sinf(t*pi<float>()*3.f)*4.f,
							cosf(t*pi<float>()*.5f)*2.f + 3.5f,
							cosf(t+20.f)*2.f);
				}, 1.5f), 
				make_shared<material>(make_shared<grid_texture>(vec3(0.4f, 0.0f, .8f), vec3(0.6f, 0.6f, 0.6f), 8.f, 0.2f), .9f)
			),
			make_shared<surface_primitive>(make_shared<surfaces::sphere>([](float t) {
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
			make_shared<surface_primitive>(make_shared<surfaces::box>(vec3(0.f), vec3(5.f, 0.1f, 5.f)),
				make_shared<material>(make_shared<checkerboard_texture>(vec3(1.f, 1.f, 0.f), vec3(0.f, 1.f, 0.f), 2.f)))
		}),
		camera(vec3(0.f, 12.f, -12.f), vec3(0.f), 0.01f, 5.f, 1.f / (float)fps), smp
	);
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
	rndr.render(rt, 0.f);
	rt.write_bmp(fns.str());
#endif


	/*ostringstream fns;
	fns << "image_" << chrono::system_clock::now().time_since_epoch().count() << ".bmp";
	rt.write_bmp(fns.str());*/
	return 0;
}