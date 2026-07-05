// dump_snd.cpp — Load a .snd file and print all sound group/number pairs
#include <cstdio>
#include <iostream>
#include <string>
#include <vector>
#include <cstdint>

#include "ssz_native/sound_resource_service.hpp"

int main(int argc, char** argv) {
    if (argc < 2) {
        std::cerr << "Usage: dump_snd <path-to-snd>" << std::endl;
        return 1;
    }
    
    ikemen::ssz_native::sound_resource_init();
    std::string err = ikemen::ssz_native::sound_table_load_file(argv[1]);
    if (!err.empty()) {
        std::cerr << "Error: " << err << std::endl;
        return 1;
    }
    
    std::cout << "Loaded: " << argv[1] << std::endl;
    // Enumerate sounds by scanning group/number space
    int found = 0;
    for (int g = 0; g <= 100; g++) {
        for (int n = 0; n <= 100; n++) {
            auto* w = ikemen::ssz_native::sound_table_get_sound(g, n);
            if (w) {
                std::cout << "  Sound group=" << g << " number=" << n
                          << " ch=" << w->channels
                          << " bps=" << w->bytesPerSample
                          << " rate=" << w->samplesPerSec
                          << " size=" << w->wav.size() << std::endl;
                found++;
            }
        }
    }
    std::cout << "Total: " << found << " sounds" << std::endl;
    return 0;
}
