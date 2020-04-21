#include <array>
#include <chrono>
#include <complex>
#include <future>
#include <iostream>
#include <png++/png.hpp>
#include <random>
#include <thread>

using idx = std::ptrdiff_t;
using pt = std::complex<double>;  // a point in real life
using px = std::pair<idx, idx>;   // pixel in the image

class buddhabrot {
   public:
    static constexpr idx image_width = 16384;
    static constexpr idx image_height = 16384;

   private:
    static constexpr double escape_radius2 = 8.0;
    const idx iterations;
    const idx max_samples;
    const idx stride;
    const idx stride_offset;
    std::vector<std::vector<double>> image;
    std::vector<std::vector<pt>> buf;
    std::vector<idx> buflen;
    std::mt19937 engine;

    /**
     * struct to represent a bounding box
     */
    struct bounds {
        double ulo, uhi, vlo, vhi;
    };

    /**
     * sample a random point within bounding box
     */
    pt random_pt(const bounds& bb) {
        std::uniform_real_distribution<double> uniform_dist_real(bb.ulo,
                                                                 bb.uhi);
        std::uniform_real_distribution<double> uniform_dist_imag(bb.vlo,
                                                                 bb.vhi);
        return pt(uniform_dist_real(engine), uniform_dist_imag(engine));
    }

    /**
     * convert point to pixel
     */
    px to_px(const pt z) {
        return std::make_pair(
            static_cast<idx>(image_height * (z.real() + 2) / 4),
            static_cast<idx>(image_width * (z.imag() + 2) / 4));
    }

    /**
     * convert pixel to point
     */
    pt to_pt(const px x) {
        return pt(x.first * 4.0 / image_height - 2.0,
                  x.second * 4.0 / image_width - 2.0);
    }

    /**
     * check if a pixel is within bounds of the image
     */
    bool in_bounds(px y) {
        return y.first >= 0 && y.second >= 0 && y.first < image_height &&
               y.second < image_width;
    }

    /**
     * Render a region within bounding box
     *
     * This is an adaptive sampling algorithm that uses importance sampling.
     * The importance of the box is assumed to be 10 * the max path length
     * originating from a point in the region.
     *
     * As we continue to sample, we keep updating the importance of the region
     * as needed.
     *
     * For example, for points in the Mandelbrot set, after 5 samples, it will
     * immediately terminate. However, interesting points tend to be on the
     * edges of the set. So, cells that contain points both in and out of the
     * Mandelbrot set will be considered to have maximum importance.
     */
    void render_region(const bounds& bb) {
        idx samples = 5;
        idx max_path = -1;
        for (idx trial = 0; trial < samples; trial++) {
            auto c = random_pt(bb);
            pt z(0, 0);
            idx escaped_time = 0;
            for (idx i = 0; i < iterations; i++) {
                z = z * z + c;
                buf[trial][i] = z;
                if (z.imag() * z.imag() + z.real() * z.real() >
                    escape_radius2) {
                    escaped_time = i;
                    break;
                }
            }

            // the longer the path, the higher the importance.
            if (escaped_time > max_path) {
                max_path = escaped_time;
                samples = std::max(
                    samples,
                    std::min(max_samples, 5 + 2 * max_path * max_path));
            }

            // if we encounter the edge of the mandelbrot set, we treat this as
            // the maximum importance.
            if ((escaped_time == 0 && max_path > 0) ||
                (escaped_time != 0 && max_path == -1)) {
                samples = max_samples;
            }

            buflen[trial] = escaped_time;
        }

        const double weight = 1.0 / samples;
        for (idx trial = 0; trial < samples; trial++) {
            for (idx i = 0; i < buflen[trial]; i++) {
                px y = to_px(buf[trial][i]);
                if (in_bounds(y)) image[y.first][y.second] += weight;
            }
        }
    }

   public:
    buddhabrot(const idx iterations_, const idx max_samples_, const idx seed,
               const idx stride_ = 1, const idx stride_offset_ = 0)
        : iterations(iterations_),
          max_samples(max_samples_),
          stride(stride_),
          stride_offset(stride_offset_),
          image(image_height, std::vector<double>(image_width, 0)),
          buf(max_samples, std::vector<pt>(iterations)),
          buflen(max_samples),
          engine(seed) {}

    void render() {
        for (idx u = stride_offset; u < image_height; u += stride) {
            for (idx v = 0; v < image_width; v++) {
                pt a = to_pt(std::make_pair(u, v));
                pt b = to_pt(std::make_pair(u + 1, v + 1));
                render_region(bounds{a.real(), b.real(), a.imag(), b.imag()});
            }
        }
    }

    double operator()(idx u, idx v) const { return image[u][v]; }
};

void write(const std::string& filename,
           const std::vector<std::unique_ptr<buddhabrot>>& brots) {
    double max_val = 0;
    double min_val = std::numeric_limits<double>::infinity();
    for (idx u = 0; u < buddhabrot::image_height; u++) {
        for (idx v = 0; v < buddhabrot::image_width; v++) {
            double x = 0;
            for (auto& b : brots) {
                x += (*b)(u, v);
            }
            if (x < min_val) {
                min_val = x;
            }
            if (x > max_val) {
                max_val = x;
            }
        }
    }

    png::image<png::gray_pixel_16> pimage(buddhabrot::image_width,
                                          buddhabrot::image_height);
    for (idx u = 0; u < buddhabrot::image_height; u++) {
        for (idx v = 0; v < buddhabrot::image_width; v++) {
            double x = 0;
            for (auto& b : brots) {
                x += (*b)(u, v);
                x += (*b)(u, buddhabrot::image_height - 1 - v);
            }
            pimage[u][v] = png::gray_pixel_16(
                ((1 << 16) - 2) *
                std::sqrt((x - min_val) / (max_val - min_val)));
        }
    }
    pimage.write(filename);
}

int main() {
    const idx n_threads = 12;
    const idx iters = 20000;
    std::vector<std::thread> threads;
    std::vector<std::unique_ptr<buddhabrot>> brots;
    for (idx i = 0; i < n_threads; i++) {
        std::random_device rd;
        auto seed =
            std::chrono::steady_clock::now().time_since_epoch().count() + i +
            rd();
        brots.emplace_back(
            std::make_unique<buddhabrot>(iters, 64, seed, n_threads, i));
    }
    threads.reserve(n_threads);
    for (idx i = 0; i < n_threads; i++) {
        threads.emplace_back([=, &brots]() { brots[i]->render(); });
    }
    for (idx i = 0; i < n_threads; i++) {
        threads[i].join();
    }
    write("brot.png", brots);
}

