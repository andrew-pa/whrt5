#pragma once

#include <exception>
#include <string>
#include <iostream>
#include <sstream>
#include <vector>
#include <map>
#include <functional>
#include <algorithm>
#include <chrono>
#include <random>
#include <thread>
#include <mutex>
#include <queue>
#include <memory>
using namespace std; //it's important to include as many symbols as possible

#define GLM_FORCE_RADIANS
#define GLM_FORCE_SWIZZLE
#define GLM_ENABLE_EXPERIMENTAL
#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtx/io.hpp>
#include <glm/gtc/noise.hpp>
#include <glm/gtc/random.hpp>
using namespace glm;

#if defined(__GNUC__) && __GNUC__ < 5
//we don't have C++14 so we're going to need some nice definitions here
template<typename T, typename... Args>
inline std::unique_ptr<T> make_unique(Args&& ...args) {
	return std::unique_ptr<T>(new T(std::forward<Args>(args)...));
}
#endif

namespace whrt5 {

	// a 3D ray, origin at e, direction d
	struct ray
	{
	public:
		vec3 e, d;
		float time;
		ray(vec3 _e = vec3(0.f), vec3 _d = vec3(0.f), float t = 0.f)
			: e(_e), d(_d), time(t) {}

		// calculate the position along the ray at t
		inline vec3 operator ()(float t) const
		{
			return e + d*t;
		}

		// transform this ray by a 4x4 matrix transform
		inline void transform(const mat4& m) {
			e = (m*vec4(e, 1.f)).xyz;
			d = (m*vec4(d, 0.f)).xyz;
		}
	};

	// an Axis Aligned Bounding Box
	struct aabb
	{
	public:
		// min point contained by the box
		vec3 _min;
		// max point contained by box
		vec3 _max;

		aabb()
			: _min(0), _max(0)
		{}

		aabb(vec3 m, vec3 x)
			: _min(m), _max(x)
		{}

		// create an AABB that is the union of AABBs a and b
		aabb(const aabb& a, const aabb& b)
			: _min(), _max()
		{
			add_point(a._min);
			add_point(a._max);
			add_point(b._min);
			add_point(b._max);
		}

		// extend the AABB to include point p
		inline void add_point(vec3 p)
		{
			if (p.x > _max.x) _max.x = p.x;
			if (p.y > _max.y) _max.y = p.y;
			if (p.z > _max.z) _max.z = p.z;

			if (p.x < _min.x) _min.x = p.x;
			if (p.y < _min.y) _min.y = p.y;
			if (p.z < _min.z) _min.z = p.z;
		}

		// check to see if the AABB contains p, including on the edges
		inline bool contains(vec3 p) const
		{
			if (p.x >= _min.x && p.x <= _max.x &&
				p.y >= _min.y && p.y <= _max.y &&
				p.z >= _min.z && p.z <= _max.z)
				return true;
			return false;
		}

		// check to see if the AABB contains any corner of AABB b
		// this may find intersections as well as containment
		inline bool inside_of(const aabb& b) const
		{
			return b.contains(vec3(_min.x, _min.y, _min.z)) ||

				b.contains(vec3(_max.x, _min.y, _min.z)) ||
				b.contains(vec3(_min.x, _max.y, _min.z)) ||
				b.contains(vec3(_min.x, _min.y, _max.z)) ||

				b.contains(vec3(_min.x, _max.y, _max.z)) ||
				b.contains(vec3(_max.x, _min.y, _max.z)) ||
				b.contains(vec3(_max.x, _max.y, _min.z)) ||

				b.contains(vec3(_max.x, _max.y, _max.z));
		}

		// transform AABB by matrix to obtain new AABB
		inline aabb transform(const mat4& m) const
		{
			vec3 min, max;
			min = vec3(m[3][0], m[3][1], m[3][2]);
			max = min;

			for (int i = 0; i < 3; ++i)
				for (int j = 0; j < 3; ++j)
				{
					if (m[i][j] > 0)
					{
						min[i] += m[i][j] * _min[j];
						max[i] += m[i][j] * _max[j];
					}
					else
					{
						min[i] += m[i][j] * _max[j];
						max[i] += m[i][j] * _min[j];
					}
				}
			return aabb(min, max);
		}

		// heavily optimized ray intersection function 
		inline bool hit(const ray& r) const
		{
			//if (contains(r.e)) return true;

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


			return tmax >= tmin;
		}

		// ray intersection function that returns the interval on the ray that is inside the AABB
		inline pair<float, float> hit_retint(const ray& r) const
		{
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


			return pair<float, float>(tmin, tmax);
		}

		inline vec3 center() const
		{
			return (_min + _max) * 1.f / 2.f;
		}
		inline vec3 extents() const {
			return _max - _min;
		}

		inline float surface_area() const {
			vec3 d = _max - _min;
			return 2.f * (d.x * d.y + d.x * d.z + d.y * d.z);
		}

		inline void add_aabb(const aabb& b)
		{
			add_point(b._min);
			add_point(b._max);
		}
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
	
	namespace rnd {
		static mt19937 RNG;
		//static minstd_rand RNG = minstd_rand(random_device()());

		inline int randi(int min, int max) {
			uniform_int_distribution<int> dist(min, max);
			return dist(RNG);
		}
		inline float randf() {
			uniform_real_distribution<float> dist;
			return dist(RNG);
		}

		inline vec2 randf2() {
			uniform_real_distribution<float> D;
			return vec2(D(RNG), D(RNG));
		}

		inline vec2 concentric_disk_sample(vec2 u) {
			vec2 rt;
			u = 2.f*u - 1.f;
			if (u.x == 0.f && u.y == 0.f) return vec2(0.f);
			if (u.x >= -u.y) {
				if (u.x > u.y)
					rt = vec2(u.x, u.y > 0.f ? u.y / u.x : 8.f + u.y / u.x);
				else
					rt = vec2(u.y, 2.f - u.x / u.y);
			}
			else {
				if (u.x <= u.y)
					rt = vec2(-u.x, 4.f - u.y / u.x);
				else
					rt = vec2(-u.y, 6.f - u.x / u.y);
			}
			rt.y *= pi<float>()*.25f;
			return vec2(cos(rt.y), sin(rt.y))*rt.x;
		}

		inline vec3 uniform_hemisphere_sample(vec2 u) {
			float r = sqrt(glm::max(0.f, 1.f - u.x*u.x));
			float phi = 2.f*pi<float>()*u.y;
			return vec3(r*cos(phi),r*sin(phi),u.x);	
		}
		inline float uniform_hemisphere_pdf() { return one_over_two_pi<float>(); }

		inline vec3 uniform_sphere_sample(vec2 u) {
			float z = 1.f - 2.f*u.x;
			float r = sqrt(glm::max(0.f, 1.f - z*z));
			float phi = 2.f*pi<float>()*u.y;
			return vec3(r*cos(phi), r*sin(phi), z);
		}
		inline float uniform_sphere_pdf() { return one_over_two_pi<float>()*.5f; }

		inline vec3 cosine_hemisphere_sample(vec2 u) {
			vec2 d = concentric_disk_sample(u);
			return vec3(d, sqrt(glm::max(0.f, 1.f - dot(d,d))));
		}
		inline float cosine_hemisphere_pdf(float costheta) {
			return costheta * one_over_pi<float>();
		}
	}
}
