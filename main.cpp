#include<bits/stdc++.h>

#include "node.cpp"

using namespace std;

const int claimingMethod;
const int numOfNode = 256; // 設定有幾個node
const double downlinkedProbability;
const double uplinkedProbability;

const int numOfDTIM;
const int numOfTIMEachDTIM;
const int numOfRAWEachTIM;
const int numOfSlotEachRAW;

int totalPayloadSize = 0;
int uplinkedTimeStamp = 1;
int downlinkedTimeStamp = 1;
int numberOfDownlinkedDataEachNode[numOfNode];
int numberOfClaimedUplinkedDataEachNode[numOfNode];
int downlinkedDataTimeStamp[numOfNode];
int uplinkedDataTimeStamp[numOfNode];

void initialize() {
    for(int i = 0; i < numOfNode; i++) {
        downlinkedDataTimeStamp[i] = 0;
        uplinkedDataTimeStamp[i] = 0;
        numberOfDownlinkedDataEachNode[i] = 0;
        numberOfClaimedUplinkedDataEachNode[i] = 0;
    }
}

void recordTimeStamp(int aid) {
    uplinkedDataTimeStamp[aid] = uplinkedTimeStamp;
    uplinkedTimeStamp++;
}

void clearUplinkedDataTimeStamp(int aid) {
    uplinkedDataTimeStamp[aid] = 0;
}

void generateDownlinkedData() {
    srand( time(NULL) );

    /* 指定亂數範圍 */
    int min = 1;
    int max = 100;

    int x;

    for(int i = 0; i < numOfNode; i++) {
        x = rand() % (max - min + 1) + min;
        if(x < downlinkedProbability) {
            downlinkedDataTimeStamp[i] = downlinkedTimeStamp;
            numberOfDownlinkedDataEachNode[i]++;
            downlinkedTimeStamp++;
        }

        for(int j = 0; j < 2; j++) {
            x = rand() % (max - min + 1) + min;
            if(x < downlinkedProbability)
                numberOfDownlinkedDataEachNode[i]++;
        }
    }
}

double calculateThroughput() {
    // TODO
}


int main(){

}