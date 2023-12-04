#include "rtweekend.h"
#include "camera.h"
#include "hittable_list.h"
#include "sphere.h"

#include <chrono>

int main() {
  hittable_list world;

  world.add(make_shared<sphere>(point3(0.0f, 0.0f, -1.0f), 0.5f));
  world.add(make_shared<sphere>(point3(0.0f, -100.5f, -1.0f), 100.0f));

  camera cam;

  cam.aspect_ratio = 16.0f / 9.0f;
  cam.image_width = 400;
  cam.samples_per_pixel = 100;
  cam.max_depth = 50;

  auto start = std::chrono::high_resolution_clock::now();
  cam.render(world);
  auto end = std::chrono::high_resolution_clock::now();

  auto duration = std::chrono::duration_cast<std::chrono::milliseconds>(end - start);
  std::clog << "Time taken by function: " << duration.count() << " milliseconds" << std::endl;
}
