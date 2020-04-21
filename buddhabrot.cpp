#include <array>
#include <chrono>
#include <complex>
#include <future>
#include <iostream>
#include <png++/png.hpp>
#include <random>
#include <thread>

using idx = std::ptrdiff_t;
using pt = std::complex<double>;
using px = std::pair<idx, idx>;

class buddhabrot {
   public:
    static constexpr idx image_width = 16384;
    static constexpr idx image_height = 16384;

   private:
    static constexpr double escape_radius2 = 8.0;
    const idx iterations;
    const idx trials_count;
    std::vector<std::vector<double>> image;
    std::vector<pt> buf;
    std::mt19937 engine;

    struct bounds {
        double ulo, uhi, vlo, vhi;
    };

    pt random_pt(const bounds& bb) {
        std::uniform_real_distribution<double> uniform_dist_real(bb.ulo,
                                                                 bb.uhi);
        std::uniform_real_distribution<double> uniform_dist_imag(bb.vlo,
                                                                 bb.vhi);
        return pt(uniform_dist_real(engine), uniform_dist_imag(engine));
    }

    px to_px(pt z) {
        return std::make_pair(
            static_cast<idx>(std::lround(image_height * (z.real() + 2) / 4)),
            static_cast<idx>(image_width * (z.imag() + 2) / 4));
    }

    bool in_bounds(px y) {
        return y.first >= 0 && y.second >= 0 && y.first < image_height &&
               y.second < image_width;
    }

    bool render_region(const bounds& bb, const double weight, const idx depth,
                       const bool write = true) {
        bool escaped = false;
        bool noescape = false;
        idx max_escaped_time = 0;
        const idx trials = weight == 0 ? trials_count / 4.0 : trials_count;
        if (weight == 0 && bb.uhi - bb.ulo <= 1e-5) return false;
        for (idx trial = 0; trial < trials; trial++) {
            auto c = random_pt(bb);
            pt z(0, 0);
            idx escaped_time = 0;
            for (idx i = 0; i < iterations; i++) {
                z = z * z + c;
                buf[i] = z;
                if (z.imag() * z.imag() + z.real() * z.real() >
                    escape_radius2) {
                    escaped_time = i;
                    escaped = true;
                    break;
                }
            }
            if (escaped_time != 0) {
                if (write) {
                    for (idx i = 0; i < escaped_time; i++) {
                        px y = to_px(buf[i]);
                        if (in_bounds(y)) image[y.first][y.second] += trials;
                    }
                }
                max_escaped_time = std::max(max_escaped_time, escaped_time);
                escaped = true;
            } else {
                noescape = true;
            }
            if (weight == 0 &&
                ((bb.uhi - bb.ulo > 1e-5) &&
                 ((escaped && noescape) || (max_escaped_time > depth)))) {
                return true;
            }
        }
        return (bb.uhi - bb.ulo > 1e-5) &&
               ((escaped && noescape) || (max_escaped_time > depth));
    }

    void render_recurse(const bounds& bb = {-2.0, 2.0, -2.0, 2.0},
                        const double weight = image_width * image_height,
                        const idx depth = 1) {
        if (depth > 1024 && !render_region(bb, 0, depth, false)) {
            render_region(bb, weight, depth);
            return;
        }
        for (const bounds& qb : quadrants(bb)) {
            render_recurse(qb, weight / 4.0, depth * 2);
        }
    }

    std::array<bounds, 4> quadrants(const bounds& bb) {
        double umid = (bb.ulo + bb.uhi) / 2;
        double vmid = (bb.vlo + bb.vhi) / 2;
        return {bounds{umid, bb.uhi, vmid, bb.vhi},
                bounds{umid, bb.uhi, bb.vlo, vmid},
                bounds{bb.ulo, umid, vmid, bb.vhi},
                bounds{bb.ulo, umid, bb.vlo, vmid}};
    }

   public:
    buddhabrot(idx iterations_, idx trials_count_, idx seed)
        : iterations(iterations_),
          trials_count(trials_count_),
          image(image_height, std::vector<double>(image_width, 0)),
          buf(iterations),
          engine(seed) {}

    void render() { render_recurse(); }

    double operator()(idx u, idx v) const { return image[u][v]; }
};

void write(const std::string& filename,
           const std::vector<std::unique_ptr<buddhabrot>>& brots) {
    double max_val = 0;
    for (idx u = 0; u < buddhabrot::image_height; u++) {
        for (idx v = 0; v < buddhabrot::image_width; v++) {
            double x = 0;
            for (auto& b : brots) {
                x += (*b)(u, v);
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
            pimage[u][v] = png::gray_pixel_16(((1 << 16) - 1) *
                                              std::sqrt(x / max_val / 2));
        }
    }
    pimage.write(filename);
}

int main() {
    const idx n_threads = 24;
    const idx iters = 1000;
    std::vector<std::thread> threads;
    std::vector<std::unique_ptr<buddhabrot>> brots;
    for (idx i = 0; i < n_threads; i++) {
        std::random_device rd;
        auto seed =
            std::chrono::steady_clock::now().time_since_epoch().count() + i +
            rd();
        brots.emplace_back(
            std::make_unique<buddhabrot>(iters, 144.0 * 8 / n_threads, seed));
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

