#include <iostream>
#include <fstream>
#include <vector>
#include <string>

using namespace std;

std::vector<double> readFromCSV(const std::string& filename) {
    std::ifstream file(filename);
    std::vector<double> data;
    std::string line;

    if (!file.is_open()) {
        std::cerr << "Failed to open file." << std::endl;
        return data;
    }

    // data.push_back(0000);

    while (std::getline(file, line)) {
        try {
            double value = std::stod(line);
            data.push_back(value);
        } catch (const std::invalid_argument& e) {
            std::cerr << "Invalid data format: " << line << std::endl;
        }
    }

    file.close();
    return data;
}

// ¨ç?¥Î¤_?ºâ¥­§¡­È
double calculateAverage(const std::vector<double>& data) {
    if (data.empty()) {
        std::cerr << "No data available to calculate average." << std::endl;
        return 0;
    }

    double sum = 0;
    for (double value : data) {
        sum += value;
    }

    return sum / data.size();
}

int main() {
    string filename[4] = {"Throughput.csv", "Collision.csv", "CU.csv", "Transmission.csv"};
    string met[4] = {"Throughput: ", "Collision: ", "Channel Utilization: ", "Transmission Delay: "};
    for(int i = 0; i < 4; i++) {
        std::vector<double> data = readFromCSV(filename[i]);

        if (!data.empty()) {
            double average = calculateAverage(data);
            std::cout << met[i] << average << std::endl;
        } else {
            std::cerr << "No data found in file " << filename << "." << std::endl;
        }
    }

    return 0;
}
