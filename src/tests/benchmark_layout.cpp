// Layout generation benchmark: loads a YAML map from resources/edgar_gui (or a path given via
// argv) and measures generation time over N seeds. Prints a single summary line:
//   benchmark map=<file> iterations=<n> min_ms=<..> median_ms=<..> max_ms=<..> rooms=<..>
// Exit code 1 when --threshold-ms <ms> is set and the median exceeds it (regression gate).
//
// Replaces the Windows-only tools/benchmark_layout_generation.ps1 for CI/local checks.

#include "preset_loader.hpp"

#include "edgar/generator/grid2d/graph_based_generator_configuration.hpp"
#include "edgar/generator/grid2d/graph_based_generator_grid2d.hpp"
#include "edgar/generator/grid2d/level_description_grid2d.hpp"

#include <algorithm>
#include <chrono>
#include <cstdio>
#include <cstdlib>
#include <filesystem>
#include <random>
#include <string>
#include <vector>

namespace {

namespace fs = std::filesystem;
namespace grid2d = edgar::generator::grid2d;

fs::path repo_root_from_this_file() {
    return fs::path(__FILE__).parent_path().parent_path().parent_path();
}

void usage() {
    std::fprintf(stderr,
                 "usage: benchmark_layout [--map <name.yml>] [--iterations N] [--threshold-ms MS]\n"
                 "  --map           file inside resources/edgar_gui/Maps (default: 9vertices.yml)\n"
                 "  --iterations    number of generations, seeds 1..N (default: 20)\n"
                 "  --threshold-ms  fail when median generation time exceeds MS\n");
}

} // namespace

int main(int argc, char** argv) {
    std::string map_filename = "9vertices.yml";
    int iterations = 20;
    double threshold_ms = -1.0;

    for (int i = 1; i < argc; ++i) {
        const std::string arg = argv[i];
        if (arg == "--map" && i + 1 < argc) {
            map_filename = argv[++i];
        } else if (arg == "--iterations" && i + 1 < argc) {
            iterations = std::max(1, std::atoi(argv[++i]));
        } else if (arg == "--threshold-ms" && i + 1 < argc) {
            threshold_ms = std::atof(argv[++i]);
        } else if (arg == "--help") {
            usage();
            return 0;
        } else {
            usage();
            return 2;
        }
    }

    const fs::path resources = repo_root_from_this_file() / "resources" / "edgar_gui";
    if (!fs::exists(resources)) {
        std::fprintf(stderr, "missing resources root: %s\n", resources.string().c_str());
        return 2;
    }

    auto loaded = grid2d::load_preset_catalog_with_status(resources.string());
    if (!loaded.error.empty() || loaded.catalog.maps.empty()) {
        std::fprintf(stderr, "catalog load error: %s\n", loaded.error.c_str());
        return 2;
    }

    const grid2d::PresetMap* map = nullptr;
    for (const auto& m : loaded.catalog.maps) {
        if (m.filename == map_filename) {
            map = &m;
            break;
        }
    }
    if (map == nullptr) {
        std::fprintf(stderr, "map not found in catalog: %s\n", map_filename.c_str());
        return 2;
    }

    std::vector<double> times_ms;
    times_ms.reserve(static_cast<std::size_t>(iterations));
    std::size_t rooms = 0;
    for (int i = 0; i < iterations; ++i) {
        grid2d::LevelDescriptionGrid2D<int> level = grid2d::build_level_from_preset(*map, loaded.catalog);
        grid2d::GraphBasedGeneratorConfiguration config{};
        grid2d::GraphBasedGeneratorGrid2D<int> generator(level, config);
        std::mt19937 rng(static_cast<unsigned>(i + 1));

        const auto t0 = std::chrono::steady_clock::now();
        generator.inject_random_generator(std::move(rng));
        const auto layout = generator.generate_layout();
        const auto t1 = std::chrono::steady_clock::now();

        times_ms.push_back(std::chrono::duration<double, std::milli>(t1 - t0).count());
        rooms = layout.rooms.size();
    }

    std::sort(times_ms.begin(), times_ms.end());
    const double median = times_ms[times_ms.size() / 2];
    std::printf("benchmark map=%s iterations=%d min_ms=%.3f median_ms=%.3f max_ms=%.3f rooms=%zu\n",
                map_filename.c_str(), iterations, times_ms.front(), median, times_ms.back(), rooms);

    if (threshold_ms >= 0.0 && median > threshold_ms) {
        std::fprintf(stderr, "REGRESSION: median %.3f ms exceeds threshold %.3f ms\n", median, threshold_ms);
        return 1;
    }
    return 0;
}
