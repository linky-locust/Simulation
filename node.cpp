#include <iostream>
#include <random>
#include <vector>
#include <chrono>
#include <map>
#include <algorithm>
#include <cstdlib>
#include <ctime>

using namespace std;

class Node {
private:
    int aid;
    int backoffCounter;
    const int minContentionWindow = 32;
    int maxContentionWindow;
    int uplinkedProbability;
    int numOfUplinkedData = 0;
    int payloadSize = 128 * 8;  // 假設payload size為128bytes，轉換為位元

public:
    Node(int id, int probability) {
        aid = id;
        uplinkedProbability = probability;
    }

    void setNumOfUplinkedData(int num) {
        numOfUplinkedData = num;
    }

    void generateBackoffCounter() {
        backoffCounter = rand() % 32;
    }

    void generateUplinkedData() {
        // srand( time(NULL) );

        /* 指定亂數範圍 */
        int min = 1;
        int max = 100;
        
        int x;

        for(int j = 0; j < 3; j++) {
            x = rand() % (max - min + 1) + min;
            if(x < uplinkedProbability)
                numOfUplinkedData++;
        }
    }

    void backoffCountDown() {
        if(backoffCounter > 0)
            backoffCounter--;   
    }

    void setUplinkedProbability(double probability) {
        uplinkedProbability = probability;
    }

    int getNumOfUplinkedData() {
        return numOfUplinkedData;
    }

    void aDataGetTransed() {
        numOfUplinkedData--;
    }

    int getBackoffCounter() {
        return backoffCounter;
    }

    int getAID() {
        return aid;
    }

    bool isBackoffCounterZero() {
        if(backoffCounter == 0)
            return true;
        else
            return false;
    }
};