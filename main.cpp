#include<bits/stdc++.h>

#include "node.cpp"

using namespace std;

const int claimingMethod;
const int numOfNode = 256; // 設定有幾個node
const double downlinkedProbability = 0.1;
const double uplinkedProbability = 0.3;
const int dataSize = 128; // bytes

const int numOfDTIM = 4;
const int numOfTIMEachDTIM = 4;
const int numOfRAWEachTIM = 4;
const int numOfSlotEachRAW = 4;
const int numOfSTAEachSlot = numOfNode / numOfTIMEachDTIM / numOfRAWEachTIM / numOfSlotEachRAW;

const double DTIMDuration = 5.12; // 秒
const double slotDuration = DTIMDuration / numOfTIMEachDTIM / numOfRAWEachTIM / numOfSlotEachRAW;

double slots[numOfSlotEachRAW] = {slotDuration};
double tScheduled[numOfSlotEachRAW] = {0};
double tRemaining[numOfSlotEachRAW] = {0};
bool haveCollision[numOfSlotEachRAW] = {false};
vector<int> surplusSlots;
vector<int> deficitSlots;
set<int> claimed;
int numOfClaimedSTA[numOfSlotEachRAW] = {0};

int totalPayloadSize = 0;
int uplinkedTimeStamp = 1;
int downlinkedTimeStamp = 1;
int numOfDownlinkedDataEachNode[numOfNode];
int numOfClaimedUplinkedDataEachNode[numOfNode];
int downlinkedDataTimeStamp[numOfNode];
int uplinkedDataTimeStamp[numOfNode];

vector<Node> nodes;

void initialize() {
    for(int i = 0; i < numOfNode; i++) {
        downlinkedDataTimeStamp[i] = 0;
        uplinkedDataTimeStamp[i] = 0;
        numOfDownlinkedDataEachNode[i] = 0;
        numOfClaimedUplinkedDataEachNode[i] = 0;
    }
}

void recordTimeStamp(int start) {
    for(int i = start; i < start + numOfSTAEachSlot - 1; i++) {
        if(nodes[i].isTransmitting()) {
            uplinkedDataTimeStamp[i] = uplinkedTimeStamp;
            // 這邊同時記錄成功claim的STA有幾個uplinked data frame
            numOfClaimedUplinkedDataEachNode[i] = nodes[i].getNumOfUplinkedData();
            claimed.insert(i);
            uplinkedTimeStamp++;

            return;
        }
    }
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
            if(downlinkedDataTimeStamp[i] == 0)
                downlinkedDataTimeStamp[i] = downlinkedTimeStamp;
            numOfDownlinkedDataEachNode[i]++;
            downlinkedTimeStamp++;
        }

        for(int j = 0; j < 2; j++) {
            x = rand() % (max - min + 1) + min;
            if(x < downlinkedProbability)
                numOfDownlinkedDataEachNode[i]++;
        }
    }
}

double calculateThroughput() {
    // TODO
}

void generateNewBackoffCounter(int start) {
    for(int i = start; i < start + numOfSTAEachSlot - 1; i++) {
        if(nodes[i].isTransmitting())
            nodes[i].generateBackoffCounter();
    }
}

void backoffCounterCountDown(int start) {
    for(int i = start; i < start + numOfSTAEachSlot - 1; i++)
        nodes[i].backoffCountDown();
}

void claimingPhase(int startAID) {
    for(int i = 0; i < numOfSlotEachRAW; i++) {
        // CW equals 32
        for(int j = 0; j < 32; j++) {
            bool mightCollision = false, collision = false;
            //Check is there any STA whose backoff counter equals 0
            for(int k = startAID; k < startAID + numOfSTAEachSlot - 1; k++) {
                if(nodes[k].isTransmitting() && !claimed.count(i)) {
                    if(mightCollision){ // 發生碰撞
                        collision = true;
                        break;
                    } else
                        mightCollision = true;
                }
            }

            if(!mightCollision) // 如果沒有STA要發起傳輸
                backoffCounterCountDown(startAID);
            else {
                if(collision) {//　如果有碰撞發生，則重骰發生碰撞的STAs的backoff counter
                    generateNewBackoffCounter(startAID);
                    haveCollision[i] = true;
                } else { // 沒碰撞發生，紀錄claim成功的STA的time stamp和claim的data frame數量
                    recordTimeStamp(startAID);
                    numOfClaimedSTA[i]++;
                }
            }
        }

        startAID += numOfSTAEachSlot;
    }
}

void calculateTScheduled(int startAID) {
    for(int i = 0; i < numOfSlotEachRAW; i++) {
        double numOfData = 0;
        for(int i = startAID; i < startAID + numOfSTAEachSlot - 1; i++) {
            if(numOfClaimedUplinkedDataEachNode[i])
                numOfData += numOfClaimedUplinkedDataEachNode[i];

            if(numOfDownlinkedDataEachNode[i])
                numOfData += numOfDownlinkedDataEachNode[i];
        }
        tScheduled[i] = (numOfData * dataSize) / 150000;
        startAID += numOfSTAEachSlot;
    }
}

void calculateTRemaining() {
    for(int i = 0; i < numOfSlotEachRAW; i++) 
        tRemaining[i] = slotDuration - tScheduled[i];
}

void slotAdjustmentAlgo() {
    // Algo 1
    int id = 0, is = 0;
    while(id < deficitSlots.size() && is < surplusSlots.size()) {
        if(abs(tRemaining[deficitSlots[id]]) <= tRemaining[surplusSlots[is]]) {
            slots[deficitSlots[id]] += abs(tRemaining[deficitSlots[id]]);
            slots[surplusSlots[is]] -= abs(tRemaining[deficitSlots[id]]);
            tRemaining[surplusSlots[is]] -= abs(tRemaining[deficitSlots[id]]);
            tRemaining[deficitSlots[id]] = 0;
            id++;
        } else {
            slots[deficitSlots[id]] += tRemaining[surplusSlots[is]];
            slots[surplusSlots[is]] -= tRemaining[surplusSlots[is]];
            tRemaining[deficitSlots[id]] += tRemaining[surplusSlots[is]];
            tRemaining[surplusSlots[is]] = 0;
            is++;
        }
    }

    // 如果Algo 1結束，surplus time還有剩，則平分給同時是D-slots且有unscheduled STAs的slot
    if(is < surplusSlots.size() - 1) {
        int counter = 0;
        double remainingSurplusTime = 0;
        vector<int> valid;
        for(int i = 0; i < deficitSlots.size(); i++) {
            if(haveCollision[deficitSlots[i]] && (numOfClaimedSTA[deficitSlots[i]] < numOfSTAEachSlot)) {
                valid.push_back(deficitSlots[i]);
                counter++;
            }
        }

        for(int i = is; i < surplusSlots.size(); i++)
            remainingSurplusTime += tRemaining[surplusSlots[i]];

        double temp = remainingSurplusTime / counter;

        double temp2 = temp;

        while(is < surplusSlots.size() && temp > 0) {
            if(tRemaining[surplusSlots[is]] >= temp2) {
                slots[surplusSlots[is]] -= temp2;
                tRemaining[surplusSlots[is]] -= temp2;
                temp2 = temp;
            } else {
                slots[surplusSlots[is]] -= tRemaining[surplusSlots[is]];
                temp2 -= tRemaining[surplusSlots[is]];
                tRemaining[surplusSlots[is]] = 0;
                is++;
            }
        }

        for(int i = 0; i < valid.size(); i++)
            slots[valid[i]] += temp;
    }
}

bool descending(int a, int b) {
    return tRemaining[a] > tRemaining[b];  // 降序排序
}

bool ascending(int a, int b) {
    return tRemaining[a] < tRemaining[b];  // 升序排序
}

void slotAdjustmentPhase(int startAID) {
    calculateTScheduled(startAID);
    calculateTRemaining();
    for(int i = 0; i < numOfSlotEachRAW; i++) {
        if(tRemaining[i] < 0)
            deficitSlots.push_back(i);
        else if(tRemaining[i] > 0 && (!haveCollision[i] || (numOfClaimedSTA[i] == numOfSTAEachSlot)))
            surplusSlots.push_back(i);
    }

    sort(deficitSlots.begin(), deficitSlots.end(), ascending);
    sort(surplusSlots.begin(), surplusSlots.end(), descending);

    slotAdjustmentAlgo();
}

void reset() {
    for(int i = 0; i < numOfSlotEachRAW; i++) {
        slots[i] = slotDuration;
        tScheduled[i] = 0;
        tRemaining[i] = 0;
        haveCollision[i] = false;
        numOfClaimedSTA[i] = 0;
    }
}


int main(){
    initialize();

    // Create nodes
    for(int i = 0; i < numOfNode; i++) {
        Node node(i, uplinkedProbability);
        node.generateBackoffCounter();
        node.generateUplinkedData();
        nodes.push_back(node);
    }

    int startAID = 0;

    for(int i = 0; i < numOfDTIM; i++) {
        generateDownlinkedData();
        for(int j = 0; j < numOfTIMEachDTIM; j++) {
            for(int k = 0; k < numOfRAWEachTIM; k++) {
                claimingPhase(startAID);
                slotAdjustmentPhase(startAID);

                startAID += (numOfSTAEachSlot * numOfSlotEachRAW);
            }
        }
    }
}