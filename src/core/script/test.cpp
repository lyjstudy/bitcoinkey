#include <cstdio>
#include <algorithm>
#include "machine.h"

void dumpVector(const std::vector<uint8_t> &data) {
    printf("[ ");
    std::for_each(data.begin(), data.end(), [] (uint8_t value) {
        printf("%02X ", value);
    });
    printf("]\n");
}

int main() {
    try {
        script::Machine machine;
        std::vector<uint8_t> data = {1, 2};
        dumpVector(data);
        std::vector<std::vector<uint8_t>> testData = {
            {1, 2, 3},
            {4, 5},
        };
        dumpVector(testData.back());
        auto backup = std::move(testData.back());
        dumpVector(testData.back());


    } catch (std::exception &ex) {
        printf("exception: %s\n", ex.what());
    }

    return 0;
}