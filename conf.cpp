#include<bits/stdc++.h>

#include "node.cpp"

using namespace std;

const int numOfNode = 64; // 設定有幾個node
const double downlinkedProbability = 30;
const double uplinkedProbability = 30;
const double dataSize = 256; // bytes
const double dataRate = 150000; // bps
const double miniSlot = 0.0005; // 0.5ms
double tClaiming = 0.02; // s (20ms)
const double countDownTimeSlice = 0.000052; // s (52us)
const double transTimePerDataFrame = ((dataSize * 8) / dataRate);

const int numOfDTIM = 4;
const int numOfTIMEachDTIM = 1;
const int numOfRAWEachTIM = 1;
const int numOfSlotEachRAW = 4;
const int numOfSTAEachSlot = numOfNode / numOfTIMEachDTIM / numOfRAWEachTIM / numOfSlotEachRAW;

int numOfUplinkedDataTrans[numOfDTIM][numOfNode] = {0};
int numOfDownlinkedDataTrans[numOfDTIM][numOfNode] = {0};
int DTIMRound = 0;

const double DTIMDuration = 0.6; // 秒
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
int TimeStamp = 1;
int numOfDownlinkedDataFrame[numOfNode];
int numOfClaimedUplinkedDataFrame[numOfNode];
int downlinkedDataTimeStamp[numOfNode];
int uplinkedDataTimeStamp[numOfNode];

int timeOfAlgo = 0;
int timeOfScheduled = 0;
int timeOfUnscheduled = 0;

vector<Node> nodes;

void init() {
    totalPayloadSize = 0;

    for(int i = 0; i < numOfNode; i++) {
        downlinkedDataTimeStamp[i] = 0;
        uplinkedDataTimeStamp[i] = 0;
        numOfDownlinkedDataFrame[i] = 0;
        numOfClaimedUplinkedDataFrame[i] = 0;
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
    uplinkedDataTimeStamp[AID] = TimeStamp;
    // 這邊同時記錄成功claim的STA有幾個uplinked data frame
    numOfClaimedUplinkedDataFrame[AID] = nodes[AID].getNumOfUplinkedData();
    claimed.insert(AID);
    TimeStamp++;

    return;
}

void clearUplinkedDataTimeStamp(int aid) {
    uplinkedDataTimeStamp[aid] = 0;
}

void generateDownlinkedData() {
    // srand( time(NULL) );

    // 指定亂數範圍
    int min = 1;
    int max = 100;

    int x;

    for(int i = 0; i < numOfNode; i++) {
        x = rand() % (max - min + 1) + min;
        if(x < downlinkedProbability) {
            if(downlinkedDataTimeStamp[i] == 0) {
                downlinkedDataTimeStamp[i] = TimeStamp;
                TimeStamp++;
            }
            numOfDownlinkedDataFrame[i]++;
        }

        // for(int j = 0; j < 2; j++) {
        //     x = rand() % (max - min + 1) + min;
        //     if(x < downlinkedProbability)
        //         numOfDownlinkedDataEachNode[i]++;
        // }
    }
}

void generateNewBackoffCounter(int start) {
    for(int i = start; i < start + numOfSTAEachSlot; i++) {
        if(nodes[i].isBackoffCounterZero() && nodes[i].getNumOfUplinkedData() && !claimed.count(i)) {
            nodes[i].collide();
            nodes[i].generateBackoffCounter();
        }
    }
}

void generateNewBackoffCounterInRemainingPhase(int start, unordered_map<int, int> claimedFail) {
    for(int i = start; i < start + numOfSTAEachSlot; i++) {
        if(nodes[i].isBackoffCounterZero() && claimedFail.count(i) && claimedFail[i]) {
            nodes[i].collide();
            nodes[i].generateBackoffCounter();
        }
    }
}

void backoffCounterCountDown(int start) {
    for(int i = start; i < start + numOfSTAEachSlot; i++)
        nodes[i].backoffCountDown();
}

void claimingPhase(int startAID) {
    for(int i = 0; i < numOfSlotEachRAW; i++) {
        double tClaim = tClaiming;
        while(tClaim > 0) {
            bool mightCollision = false, collision = false;   
            int temp;
            //Check is there any STA whose backoff counter equals 0
            for(int k = startAID; k < startAID + numOfSTAEachSlot; k++) {
                if(nodes[k].isBackoffCounterZero() && nodes[k].getNumOfUplinkedData() && !claimed.count(k)) {
                    nodes[k].attemptToAccessChannel();
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
                if(collision) //　如果有碰撞發生，則重骰發生碰撞的STAs的backoff counter
                    generateNewBackoffCounter(startAID);
                else // 沒碰撞發生，紀錄claim成功的STA的time stamp和claim的data frame數量
                    recordTimeStamp(temp);
            }

            tClaim -= miniSlot;
        }

        startAID += numOfSTAEachSlot;
    }
}

void calculateTScheduled(int startAID) {
    double numOfData;
    for(int i = 0; i < numOfSlotEachRAW; i++) {
        numOfData = 0;
        for(int j = startAID; j < startAID + numOfSTAEachSlot; j++) {
            if(numOfClaimedUplinkedDataFrame[j])
                numOfData += numOfClaimedUplinkedDataFrame[j];

            if(numOfDownlinkedDataFrame[j])
                numOfData += numOfDownlinkedDataFrame[j];
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
    timeOfAlgo++;
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
        else if(tRemaining[i] > 0)
            surplusSlots.push_back(i);
    }

    if(deficitSlots.empty() || surplusSlots.empty()) {
        deficitSlots.clear();
        surplusSlots.clear();
        return;
    }

    sort(deficitSlots.begin(), deficitSlots.end(), ascending);
    sort(surplusSlots.begin(), surplusSlots.end(), ascending);

    slotAdjustmentAlgo();

    deficitSlots.clear();
    surplusSlots.clear();
}


int findMinSTSWithUplinkedData(int startAID) {
    int min = INT_MAX;
    int result = -1;
    for(int i = startAID; i < startAID + numOfSTAEachSlot; i++) {
        if(numOfClaimedUplinkedDataFrame[i]) {
            if(numOfDownlinkedDataFrame[i] && downlinkedDataTimeStamp[i] < min) {
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
        if(!numOfClaimedUplinkedDataFrame[i] && numOfDownlinkedDataFrame[i] && downlinkedDataTimeStamp[i] < min) {
            min = downlinkedDataTimeStamp[i];
            result = i;
        }
    }

    return result;
} 

void transOfSTAWithClaimedUplinkedData(int startAID, int i, double &time) {
    while(slots[i] && (findMinSTSWithUplinkedData(startAID) != -1)) {
        int nextSTA = findMinSTSWithUplinkedData(startAID);
        nodes[nextSTA].attemptToAccessChannel();
        while(numOfClaimedUplinkedDataFrame[nextSTA]) {
            if(slots[i] >= transTimePerDataFrame) {
                timeOfScheduled++;
                slots[i] -= transTimePerDataFrame;
                time += transTimePerDataFrame;
                if(!--numOfClaimedUplinkedDataFrame[nextSTA]) { // 如果這是該node的最後一個uplinked data frame，reset time stamp
                    uplinkedDataTimeStamp[nextSTA] = 0;
                    claimed.erase(nextSTA);
                    numOfClaimedSTA[i]--;
                }
                nodes[nextSTA].aDataGetTransed();
                numOfUplinkedDataTrans[DTIMRound][nextSTA]++;
            } else {
                slots[i] = 0;
                break;
            }
        }

        while(numOfDownlinkedDataFrame[nextSTA]) {
            if(slots[i] >= transTimePerDataFrame) {
                timeOfScheduled++;
                slots[i] -= transTimePerDataFrame;
                time += transTimePerDataFrame;
                if(!--numOfDownlinkedDataFrame[nextSTA])
                    downlinkedDataTimeStamp[nextSTA] = 0;
                numOfDownlinkedDataTrans[DTIMRound][nextSTA]++;
            } else {
                slots[i] = 0;
                break;
            }
        }

        if(!numOfClaimedUplinkedDataFrame[nextSTA] && !numOfDownlinkedDataFrame[nextSTA])
            nodes[nextSTA].fallAsleep(time);
            
    }
}

void transOfSTAWithOnlyDownlinkedData(int startAID, int i, double &time) {
    while(slots[i] && (findMinSTSWithOnlyDownlinkedData(startAID) != -1)) {
        int nextSTA = findMinSTSWithOnlyDownlinkedData(startAID);
        nodes[nextSTA].attemptToAccessChannel();
        while(numOfDownlinkedDataFrame[nextSTA]) {
            if(slots[i] >= transTimePerDataFrame) {
                timeOfScheduled++;
                slots[i] -= transTimePerDataFrame;
                time += transTimePerDataFrame;
                if(!--numOfDownlinkedDataFrame[nextSTA]) {
                    downlinkedDataTimeStamp[nextSTA] = 0;
                    nodes[nextSTA].fallAsleep(time);
                }
                numOfDownlinkedDataTrans[DTIMRound][nextSTA]++;
            } else {
                slots[i] = 0;
                break;
            }
        }
    }
}

void contendInRemainingSubSlot(int startAID, int i, unordered_map<int, int> claimedFail, double &time) {
    while(slots[i] > 0) {
        bool mightCollision = false, collision = false;
        int temp;
        //Check is there any STA whose backoff counter equals 0
        for(int j = startAID; j < startAID + numOfSTAEachSlot; j++) {
            if(nodes[j].isBackoffCounterZero() && claimedFail.count(j) && claimedFail[j]) {
                nodes[j].attemptToAccessChannel();
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
            time += miniSlot;
        } else {
            if(collision) { //　如果有碰撞發生，則重骰發生碰撞的STAs的backoff counter
                generateNewBackoffCounterInRemainingPhase(startAID, claimedFail);
                slots[i] -= transTimePerDataFrame;
                time += transTimePerDataFrame;
            } else { // 沒碰撞發生，STA發送uplinked data
                while(claimedFail[temp]) {
                    if(slots[i] >= transTimePerDataFrame) {
                        timeOfUnscheduled++;
                        slots[i] -= transTimePerDataFrame;
                        time += transTimePerDataFrame;
                        nodes[temp].aDataGetTransed();
                        numOfUplinkedDataTrans[DTIMRound][temp]++;
                        if(!--claimedFail[temp])
                            nodes[temp].fallAsleep(time);
                    } else {
                        slots[i] = 0;
                        break;
                    }
                }
            }
        }
    }
}

void transOfUnscheduledSTA(int startAID, int i, double &time) {
    unordered_map<int, int> claimedFail;
    for(int i = startAID; i < startAID + numOfSTAEachSlot; i++) {
        if(nodes[i].getNumOfUplinkedData() > numOfClaimedUplinkedDataFrame[i]) {
            nodes[i].generateBackoffCounter();  
            claimedFail[i] = nodes[i].getNumOfUplinkedData() - numOfClaimedUplinkedDataFrame[i];
        }
    }
    contendInRemainingSubSlot(startAID, i, claimedFail, time);
}

void dataTransPhase(int startAID) {
    for(int i = 0; i < numOfSlotEachRAW; i++) {
        double time = 0;
        for(int j = startAID; j < startAID + numOfSTAEachSlot; j++) {
            if(claimed.count(j) || numOfDownlinkedDataFrame[j])
                nodes[j].wakingUp();
        }

        transOfSTAWithClaimedUplinkedData(startAID, i, time);
        transOfSTAWithOnlyDownlinkedData(startAID, i, time);
        transOfUnscheduledSTA(startAID, i, time);

        for(int j = startAID; j < startAID + numOfSTAEachSlot; j++) 
            nodes[j].isAwaking(slotDuration); // 判斷是否有未完成channel access的STA，如果有，把slot duration算進這些STA的transmission time

        startAID += numOfSTAEachSlot;
    }
}

void resetSlotInfo() {
    for(int i = 0; i < numOfSlotEachRAW; i++) {
        slots[i] = slotDuration;
        tScheduled[i] = 0;
        tRemaining[i] = 0;
        haveCollision[i] = false;
        // numOfClaimedSTA[i] = 0;
    }
}

void resetDTIMInfo() {
    for(int i = 0; i < numOfNode; i++) {
        numOfClaimedUplinkedDataFrame[i] = 0;
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
        generateDownlinkedData();
        for(int j = 0; j < numOfNode; j++) {
            nodes[j].generateBackoffCounter();
            nodes[j].generateUplinkedData();
        }
        int startAID = 0;
        for(int j = 0; j < numOfTIMEachDTIM; j++) {
            for(int k = 0; k < numOfRAWEachTIM; k++) {

                claimingPhase(startAID);

                slotAdjustmentPhase(startAID);

                dataTransPhase(startAID); // 包含scheduled sub-slot和remaining sub-slot

                // startAID += (numOfSTAEachSlot * numOfSlotEachRAW);

                resetSlotInfo();
            }
        }
        resetDTIMInfo();
        DTIMRound++;
    }

    double temp = 0;
    double th[numOfNode] = {0};
    int sum = 0;
    for(int i = 0; i < numOfNode; i++) {
        int num = 0;
        for(int j = 0; j < DTIMRound; j++) {
            num += numOfUplinkedDataTrans[j][i];
            num += numOfDownlinkedDataTrans[j][i];
        }
        num *= dataSize * 8;
        if(nodes[i].getAwakingTime())
            th[i] += (num / nodes[i].getAwakingTime());
        sum += th[i];
    }

    cout << "Throughput: " << sum / numOfNode << endl;

    double totalColRate = 0;
    for(int i = 0; i < numOfNode; i++) {
        if(nodes[i].getTimesOfAttemptAccessChannel())
            totalColRate += (nodes[i].getTimesOfCollision() / nodes[i].getTimesOfAttemptAccessChannel());
    }
    cout << "Collision Rate: " << totalColRate / numOfNode << endl;

    cout << "Algo: " << timeOfAlgo << endl;

    cout << "Scheduled: " << timeOfScheduled << endl;

    cout << "Unscheduled: " << timeOfUnscheduled << endl;

    for(int i = 0; i < numOfNode; i++) {
        // cout << i << ": " << nodes[i].getAwakingTime() << endl;
        temp += nodes[i].getAwakingTime();
    }

    cout << "Avg: " << temp / 4 / numOfNode << endl;

    cout << "finish" << endl;

    return 0;
}