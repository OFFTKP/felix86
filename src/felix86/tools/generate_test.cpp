#include <cstdio>
#include <string>

int main() {
    std::string assembly;
    assembly += "bits 64\n";
    assembly += "global my_xmm_func\n";
    assembly += "my_xmm_func:";

    assembly += ".random_data:";
}