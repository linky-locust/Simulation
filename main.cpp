#include<bits/stdc++.h>

#include "node.cpp"

using namespace std;

const int claimingMethod;
const int numOfNode = 256; // 設定有幾個node
const double downlinkedProbability = 0.1;
const double uplinkedProbability = 0.3;

const int numOfDTIM = 4;
const int numOfTIMEachDTIM = 4;
const int numOfRAWEachTIM = 4;
const int numOfSlotEachRAW = 4;
const int numOfSTAEachSlot = numOfNode / numOfTIMEachDTIM / numOfRAWEachTIM / numOfSlotEachRAW;

const double DTIMDuration = 5.12; // 秒(s)
const double slotDuration = DTIMDuration / numOfTIMEachDTIM / numOfRAWEachTIM / numOfSlotEachRAW;

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
                if(nodes[k].isTransmitting()) {
                    if(mightCollision){ // 發生碰撞
                        collision = true;
                        break;
                    } else
                        mightCollision =true;
                }
            }

            if(!mightCollision) // 如果沒有STA要發起傳輸
                backoffCounterCountDown(startAID);
            else {
                if(collision) //如果有碰撞發生，則重骰發生碰撞的STAs的backoff counter
                    generateNewBackoffCounter(startAID);
                else // 沒碰撞發生，紀錄claim成功的STA的time stamp和claim的data frame數量
                    recordTimeStamp(startAID);
            }
        }

        startAID += numOfSTAEachSlot;
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
                // Claiming Phase
                claimingPhase(startAID);

                //Slot Adjustment Phase

                startAID += (numOfSTAEachSlot * numOfSlotEachRAW);
            }
        }
    }
}