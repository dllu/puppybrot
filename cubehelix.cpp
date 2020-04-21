#include <cmath>
#include <png++/png.hpp>
#include <sstream>

double sigmoid(double x, double amount) {
    return 1.0 / (1.0 + std::exp(-(x - 0.5) * amount));
}

double brighten(double x, double m = 2.0, double k = 15) {
    const double amount = 1.2;
    const double x0 = -std::log(std::expm1(k / m));
    x = 1 - (m / k) * std::log1p(std::exp(-k * x - x0));
    return x;
}

int main(int argc, char** argv) {
    if (argc < 2) {
        std::cout << "usage: ./cubehelix my_picture.png amount" << std::endl;
        return 1;
    }
    double amount = 3.0;
    if (argc == 3) {
        amount = std::atof(argv[2]);
    }

    png::image<png::gray_pixel_16> input(argv[1]);
    std::stringstream filename_ss;
    filename_ss << "cubehelix_" << argv[1];
    png::image<png::index_pixel> output(input.get_width(), input.get_height());

    png::palette pal(256);

    // generate CubeHelix palette from http://www.mrao.cam.ac.uk/~dag/CUBEHELIX/
    for (size_t i = 0; i < pal.size(); i++) {
        const double start = 0.5;
        const double rotations = -1.5;
        const double hue = 1.0;
        const double gamma = 1.0;

        const double lambda = i / 255.0;
        const double phi = 2 * M_PI * (start / 3 + rotations * lambda);
        const double lg = std::pow(lambda, gamma);
        const double alpha = hue * lg * (1 - lg) / 2;

        const double cphi = std::cos(phi);
        const double sphi = std::sin(phi);

        const double r = lg + alpha * (-0.14861 * cphi + 1.78277 * sphi);
        const double g = lg + alpha * (-0.29227 * cphi - 0.90649 * sphi);
        const double b = lg + alpha * (1.97294 * cphi + 0.0 * sphi);

        pal[i] = png::color(r * 255, g * 255, b * 255);
    }

    output.set_palette(pal);

    for (png::uint_32 u = 0; u < input.get_height(); u++) {
        for (png::uint_32 v = 0; v < input.get_width(); v++) {
            output[u][v] = png::index_pixel(std::min(
                255.0, 255.0 * std::sqrt(sigmoid(
                                   brighten(input[u][v] * (1.0 / (1 << 16))),
                                   amount))));
        }
    }

    output.write(filename_ss.str());
}
