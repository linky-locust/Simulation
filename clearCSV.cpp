#include <iostream>
#include <fstream>
#include <string>

using namespace std;

void clearCSV(const std::string& filename) {
    std::ofstream file;
    // 打?文件并使用 std::ios::trunc 模式?清除文件?容
    file.open(filename, std::ios::out | std::ios::trunc);
    if (file.is_open()) {
        std::cout << "File " << filename << " has been cleared." << std::endl;
        file.close();
    } else {
        std::cerr << "Failed to open file " << filename << " for clearing." << std::endl;
    }
}

int main() {
    string filename[4] = {"Throughput.csv", "Collision.csv", "CU.csv", "Transmission.csv"};
    for(auto str : filename)
        clearCSV(str);
    return 0;
}