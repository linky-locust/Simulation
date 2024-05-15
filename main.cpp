#include<bits/stdc++.h>

#include "node.cpp"

using namespace std;

const int numOfNode = 512; // 設定有幾個node
const double downlinkedProbability = 10;
const double uplinkedProbability = 30;
const double dataSize = 64; // bytes
const double dataRate = 150000;
const double miniSlot = 0.0005; // 0.5ms
const double countDownTimeSlice = 0.000052; // 52us
const double transTimePerDataFrame = ((dataSize * 8) / dataRate);

const int numOfDTIM = 4;
const int numOfTIMEachDTIM = 4;
const int numOfRAWEachTIM = 4;
const int numOfSlotEachRAW = 4;
const int numOfSTAEachSlot = numOfNode / numOfTIMEachDTIM / numOfRAWEachTIM / numOfSlotEachRAW;

int numOfUplinkedDataTrans[numOfDTIM][numOfNode] = {0};
int numOfDownlinkedDataTrans[numOfDTIM][numOfNode] = {0};
int DTIMRound = 0;
int adjusted = 0;

const double DTIMDuration = 2.56; // 秒
const double slotDuration = DTIMDuration / numOfTIMEachDTIM / numOfRAWEachTIM / numOfSlotEachRAW;

double slots[numOfSlotEachRAW];
double tScheduled[numOfSlotEachRAW];
double tRemaining[numOfSlotEachRAW];
bool haveCollision[numOfSlotEachRAW];
vector<int> surplusSlots;
vector<int> deficitSlots;
set<int> claimed;
int numOfClaimedSTA[numOfSlotEachRAW];

int totalPayloadSize;
int uplinkedTimeStamp;
int downlinkedTimeStamp;
int numOfDownlinkedDataEachNode[numOfNode];
int numOfClaimedUplinkedDataEachNode[numOfNode];
int downlinkedDataTimeStamp[numOfNode];
int uplinkedDataTimeStamp[numOfNode];

vector<Node> nodes;

void init() {
    totalPayloadSize = 0;
    uplinkedTimeStamp = 1;
    downlinkedTimeStamp = 1;

    for(int i = 0; i < numOfNode; i++) {
        downlinkedDataTimeStamp[i] = 0;
        uplinkedDataTimeStamp[i] = 0;
        numOfDownlinkedDataEachNode[i] = 0;
        numOfClaimedUplinkedDataEachNode[i] = 0;
    }

    for(int i = 0; i < numOfSlotEachRAW; i++) {
        slots[i] = slotDuration;
        tScheduled[i] = 0;
        tRemaining[i] = 0;
        haveCollision[i] = false;
        numOfClaimedSTA[i] = 0;
    }
}

void recordTimeStamp(int AID) {
    uplinkedDataTimeStamp[AID] = uplinkedTimeStamp;
    // 這邊同時記錄成功claim的STA有幾個uplinked data frame
    numOfClaimedUplinkedDataEachNode[AID] = nodes[AID].getNumOfUplinkedData();
    claimed.insert(AID);
    uplinkedTimeStamp++;

    return;
}

void clearUplinkedDataTimeStamp(int aid) {
    uplinkedDataTimeStamp[aid] = 0;
}

void generateDownlinkedData() {
    // srand( time(NULL) );

    /* 指定亂數範圍 */
    int min = 1;
    int max = 100;

    int x;

    for(int i = 0; i < numOfNode; i++) {
        x = rand() % (max - min + 1) + min;
        if(x < downlinkedProbability) {
            if(downlinkedDataTimeStamp[i] == 0) {
                downlinkedDataTimeStamp[i] = downlinkedTimeStamp;
                downlinkedTimeStamp++;
            }
            numOfDownlinkedDataEachNode[i]++;
        }

        // for(int j = 0; j < 2; j++) {
        //     x = rand() % (max - min + 1) + min;
        //     if(x < downlinkedProbability)
        //         numOfDownlinkedDataEachNode[i]++;
        // }
    }
}

// double calculateThroughput() {
//     // TODO
// }

void generateNewBackoffCounter(int start) {
    for(int i = start; i < start + numOfSTAEachSlot; i++) {
        if(nodes[i].isBackoffCounterZero() && nodes[i].getNumOfUplinkedData())
            nodes[i].generateBackoffCounter();
    }
}

void backoffCounterCountDown(int start) {
    for(int i = start; i < start + numOfSTAEachSlot; i++)
        nodes[i].backoffCountDown();
}

void claimingPhase(int startAID) {
    for(int i = 0; i < numOfSlotEachRAW; i++) {
        for(int j = 0; j < 32; j++) {
            bool mightCollision = false, collision = false;   
            int temp;
            //Check is there any STA whose backoff counter equals 0
            for(int k = startAID; k < startAID + numOfSTAEachSlot; k++) {
                if(nodes[k].isBackoffCounterZero() && nodes[k].getNumOfUplinkedData() && !claimed.count(k)) {
                    if(mightCollision) { // 發生碰撞
                        collision = true;
                        break;
                    } else {
                        mightCollision = true;
                        temp = k;
                    }
                }
            }

            if(!mightCollision) // 如果沒有STA要發起傳輸
                backoffCounterCountDown(startAID);
            else {
                if(collision) { //　如果有碰撞發生，則重骰發生碰撞的STAs的backoff counter
                    generateNewBackoffCounter(startAID);
                    haveCollision[i] = true;
                } else { // 沒碰撞發生，紀錄claim成功的STA的time stamp和claim的data frame數量
                    recordTimeStamp(temp);
                    numOfClaimedSTA[i]++;
                    j--;
                }
            }
        }

        startAID += numOfSTAEachSlot;
    }
}

void calculateTScheduled(int startAID) {
    double numOfData;
    for(int i = 0; i < numOfSlotEachRAW; i++) {
        numOfData = 0;
        for(int j = startAID; j < startAID + numOfSTAEachSlot; j++) {
            if(numOfClaimedUplinkedDataEachNode[j])
                numOfData += numOfClaimedUplinkedDataEachNode[j];

            if(numOfDownlinkedDataEachNode[j])
                numOfData += numOfDownlinkedDataEachNode[j];
        }
        tScheduled[i] = (numOfData * dataSize * 8) / 150000;
        startAID += numOfSTAEachSlot;
    }
}

void calculateTRemaining() {
    for(int i = 0; i < numOfSlotEachRAW; i++) 
        tRemaining[i] = slotDuration - tScheduled[i];
}

void slotAdjustmentAlgo() {
    adjusted++;
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

    if(deficitSlots.empty() || surplusSlots.empty()) {
        deficitSlots.clear();
        surplusSlots.clear();
        return;
    }

    sort(deficitSlots.begin(), deficitSlots.end(), ascending);
    sort(surplusSlots.begin(), surplusSlots.end(), descending);

    slotAdjustmentAlgo();

    deficitSlots.clear();
    surplusSlots.clear();
}


int findMinSTSWithUplinkedData(int startAID) {
    int min = INT_MAX;
    int result = -1;
    for(int i = startAID; i < startAID + numOfSTAEachSlot; i++) {
        if(numOfClaimedUplinkedDataEachNode[i]) {
            if(numOfDownlinkedDataEachNode[i] && downlinkedDataTimeStamp[i] < min) {
                min = downlinkedDataTimeStamp[i];
                result = i;
            } else if(uplinkedDataTimeStamp[i] < min) {
                min = uplinkedDataTimeStamp[i];
                result = i;
            }
        }
    }

    return result;
} 

int findMinSTSWithOnlyDownlinkedData(int startAID) {
    int min = INT_MAX;
    int result = -1;
    for(int i = startAID; i < startAID + numOfSTAEachSlot; i++) {
        if(numOfDownlinkedDataEachNode[i] && downlinkedDataTimeStamp[i] < min) {
            min = downlinkedDataTimeStamp[i];
            result = i;
        }
    }

    return result;
} 

void transOfSTAWithClaimedUplinkedData(int startAID, int i) {
    while(slots[i] && (findMinSTSWithUplinkedData(startAID) != -1)) {
        int nextSTA = findMinSTSWithUplinkedData(startAID);

        while(numOfClaimedUplinkedDataEachNode[nextSTA]) {
            if(slots[i] >= transTimePerDataFrame) {
                slots[i] -= transTimePerDataFrame;
                if(!--numOfClaimedUplinkedDataEachNode[nextSTA]) // 如果這是該node的最後一個uplinked data frame，reset time stamp
                    uplinkedDataTimeStamp[nextSTA] = 0;
                nodes[nextSTA].aDataGetTransed();
                numOfUplinkedDataTrans[DTIMRound][nextSTA]++;
            } else {
                slots[i] = 0;
                break;
            }
        }

        while(numOfDownlinkedDataEachNode[nextSTA]) {
            if(slots[i] >= transTimePerDataFrame) {
                slots[i] -= transTimePerDataFrame;
                numOfDownlinkedDataEachNode[nextSTA]--;
                numOfDownlinkedDataTrans[DTIMRound][nextSTA]++;
            } else {
                slots[i] = 0;
                break;
            }
        }

        if(numOfDownlinkedDataEachNode[nextSTA] == 0)
            downlinkedDataTimeStamp[nextSTA] = 0;
    }
}

void transOfSTAWithOnlyDownlinkedData(int startAID, int i) {
    while(slots[i] && (findMinSTSWithOnlyDownlinkedData(startAID) != -1)) {
        int nextSTA = findMinSTSWithOnlyDownlinkedData(startAID);

        while(numOfDownlinkedDataEachNode[nextSTA]) {
            if(slots[i] >= transTimePerDataFrame) {
                slots[i] -= transTimePerDataFrame;
                numOfDownlinkedDataEachNode[nextSTA]--;
                numOfDownlinkedDataTrans[DTIMRound][nextSTA]++;
            } else {
                slots[i] = 0;
                break;
            }
        }

        if(numOfDownlinkedDataEachNode[nextSTA] == 0)
            downlinkedDataTimeStamp[nextSTA] = 0;
    }
}

void contendInRemainingSubSlot(int startAID, int i, unordered_map<int, int> claimedFail) {
    while(slots[i] > 0) {
        bool mightCollision = false, collision = false;
        int temp;
        //Check is there any STA whose backoff counter equals 0
        for(int j = startAID; j < startAID + numOfSTAEachSlot; j++) {
            if(nodes[j].isBackoffCounterZero() && claimedFail.count(j) && claimedFail[j]) {
                if(mightCollision){ // 發生碰撞
                    collision = true;
                    break;
                } else {
                    mightCollision = true;
                    temp = j;
                }
            }
        }

        if(!mightCollision) { // 如果沒有STA要發起傳輸
            backoffCounterCountDown(startAID);
            slots[i] -= miniSlot;
        } else {
            if(collision) { //　如果有碰撞發生，則重骰發生碰撞的STAs的backoff counter
                generateNewBackoffCounter(startAID);
                slots[i] -= transTimePerDataFrame;
            } else { // 沒碰撞發生，STA發送uplinked data
                while(claimedFail[temp]) {
                    if(slots[i] >= transTimePerDataFrame) {
                        slots[i] -= transTimePerDataFrame;
                        nodes[temp].aDataGetTransed();
                        numOfUplinkedDataTrans[DTIMRound][temp]++;
                        claimedFail[temp]--;
                    } else {
                        slots[i] = 0;
                        break;
                    }
                }
            }
        }
    }
}

void transOfUnscheduledSTA(int startAID, int i) {
    unordered_map<int, int> claimedFail;
    for(int i = startAID; i < startAID + numOfSTAEachSlot; i++) {
        if(nodes[i].getNumOfUplinkedData() > numOfClaimedUplinkedDataEachNode[i]) {
            nodes[i].generateBackoffCounter();  
            claimedFail[i] = nodes[i].getNumOfUplinkedData() - numOfClaimedUplinkedDataEachNode[i];
        }
    }
    contendInRemainingSubSlot(startAID, i, claimedFail);
}

void dataTransPhase(int startAID) {
    for(int i = 0; i < numOfSlotEachRAW; i++) {
        transOfSTAWithClaimedUplinkedData(startAID, i);
        transOfSTAWithOnlyDownlinkedData(startAID, i);
        transOfUnscheduledSTA(startAID, i);
        startAID += numOfSTAEachSlot;
    }
}

void resetSlotInfo() {
    for(int i = 0; i < numOfSlotEachRAW; i++) {
        slots[i] = slotDuration;
        tScheduled[i] = 0;
        tRemaining[i] = 0;
        haveCollision[i] = false;
        numOfClaimedSTA[i] = 0;
    }
}

void resetDTIMInfo() {
    for(int i = 0; i < numOfNode; i++) {
        numOfClaimedUplinkedDataEachNode[i] = 0;
        uplinkedDataTimeStamp[i] = 0;
    }

    claimed.clear();
}

int main() {
    srand(time(0));

    init();

    // Create nodes
    for(int i = 0; i < numOfNode; i++) {
        Node node(i, uplinkedProbability);
        nodes.push_back(node);
    }

    for(int i = 0; i < numOfDTIM; i++) {
        for(int j = 0; j < numOfNode; j++) {
            nodes[j].generateBackoffCounter();
            nodes[j].generateUplinkedData();
        }
        generateDownlinkedData();
        uplinkedTimeStamp = downlinkedTimeStamp + 1;
        int startAID = 0;
        for(int j = 0; j < numOfTIMEachDTIM; j++) {
            for(int k = 0; k < numOfRAWEachTIM; k++) {

                claimingPhase(startAID);

                slotAdjustmentPhase(startAID);

                dataTransPhase(startAID); // 包含scheduled sub-slot和remaining sub-slot

                startAID += (numOfSTAEachSlot * numOfSlotEachRAW);

                resetSlotInfo();
            }
        }
        resetDTIMInfo();
        DTIMRound++;
    }

    cout << "finish" << endl;

    return 0;
}