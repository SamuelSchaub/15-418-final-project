#include "rtweekend.h"
#include "camera.h"
#include "hittable_list.h"
#include "sphere.h"
#include "bvh.h"

#include <chrono>

int main() {
  hittable_list world;

  auto ground_material = make_shared<lambertian>(color(0.5f, 0.5f, 0.5f));
  // world.add(make_shared<sphere>(point3(0.0f, 0.0f, -1.0f), 0.5f));
  world.add(make_shared<sphere>(point3(0.0f, -1000.0f, -1.0f), 1000.0f, ground_material));

  for (int a = -11; a < 11; a++) {
    for (int b = -11; b < 11; b++) {
      auto choose_mat = random_float();
      point3 center(a + 0.9f * random_float(), 0.2f, b + 0.9f * random_float());

      if ((center - point3(4.0f, 0.2f, 0.0f)).length() > 0.9f) {
        shared_ptr<material> sphere_material;

        if (choose_mat < 0.85f) {
            // diffuse
            auto albedo = color::random() * color::random();
            sphere_material = make_shared<lambertian>(albedo);
            world.add(make_shared<sphere>(center, 0.2f, sphere_material));
        } else {
            // mirror
            auto albedo = color::random(0.5f, 1.0f);
            sphere_material = make_shared<mirror>(albedo);
            world.add(make_shared<sphere>(center, 0.2f, sphere_material));
        } 
      }
    }
  }

  auto mat1 = make_shared<lambertian>(color(0.4, 0.2, 0.1));
  world.add(make_shared<sphere>(point3(-4, 1, 0), 1.0, mat1));

  auto mat2 = make_shared<mirror>(color(0.7, 0.6, 0.5));
  world.add(make_shared<sphere>(point3(4, 1, 0), 1.0, mat2));

  world = hittable_list(make_shared<bvh_node>(world));

  camera cam;

  cam.aspect_ratio = 16.0f / 9.0f;
  cam.image_width = 400;
  cam.samples_per_pixel = 16;
  cam.max_depth = 8;

  auto start = std::chrono::high_resolution_clock::now();
  cam.render(world);
  auto end = std::chrono::high_resolution_clock::now();

  auto duration = std::chrono::duration_cast<std::chrono::milliseconds>(end - start);
  std::clog << "Time taken by function: " << duration.count() << " milliseconds" << std::endl;
}
