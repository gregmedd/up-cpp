#include "SafeMap.h"
#include <iostream>

int main() {
    uprotocol::utils::SafeMap<int, int> xsm;
    xsm[2] = 45;
    uprotocol::utils::SafeMap<int, int> ysm(xsm);
    uprotocol::utils::SafeMap<int, int> zsm({{1, 1}, {2, 2}, {3, 3}});

    uprotocol::utils::SafeUnorderedMap<int, int> xsum;
    xsum[-2] = -45;
    uprotocol::utils::SafeUnorderedMap<int, int> ysum(xsum);
    uprotocol::utils::SafeUnorderedMap<int, int> zsum({{1, 1}, {2, 2}, {3, 3}});

    std::cout << "ysm: " << ysm[2] << std::endl;
    std::cout << "ysum: " << ysum[-2] << std::endl;

    return 0;
}
