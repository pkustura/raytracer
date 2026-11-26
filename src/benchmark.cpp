#include "benchmark.h"
#include <algorithm>
#include <cmath>

BenchmarkRun::BenchmarkRun(const Scene& scene, int frames, int width, int height, int threads, int max_depth)
    : target_(scene.camera_at), vfov_(scene.vfov) {
    Vector3 offset = scene.camera_from - target_;
    radius_ = std::sqrt(offset.x * offset.x + offset.z * offset.z);
    height_ = offset.y;
    start_angle_ = std::atan2(offset.z, offset.x);

    result_.frames = std::max(1, frames);
    result_.width = width;
    result_.height = height;
    result_.threads = threads;
    result_.max_depth = max_depth;
    result_.min_frame = 1e30;
}

Camera BenchmarkRun::camera() const {
    float angle = start_angle_ + 2.0f * float(M_PI) * frame_ / result_.frames;
    Vector3 from = target_ + Vector3(radius_ * std::cos(angle), height_, radius_ * std::sin(angle));
    float aspect = float(result_.width) / float(result_.height);
    return Camera(from, target_, Vector3(0, 1, 0), vfov_, aspect, 0.1f);
}

void BenchmarkRun::record(double seconds, uint64_t rays) {
    if (done()) return;
    result_.total_seconds += seconds;
    result_.min_frame = std::min(result_.min_frame, seconds);
    result_.max_frame = std::max(result_.max_frame, seconds);
    result_.rays += rays;
    ++frame_;
}
