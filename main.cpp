#include<bits/stdc++.h>

#include "node.cpp"

using namespace std;

const int claimingMethod;
const int numOfNode = 256; // �]�w���X��node
const double downlinkedProbability=0.1;
const double uplinkedProbability=0.1;

const int numOfDTIM = 4;
const int numOfTIMEachDTIM = 4;
const int numOfRAWEachTIM = 4;
const int numOfSlotEachRAW = 4;
const int numOfSTAEachSlot = numOfNode / numOfTIMEachDTIM / numOfRAWEachTIM / numOfSlotEachRAW;

const double DTIMDuration = 5.12; // ��(s)
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
            // �o��P�ɰO�����\claim��STA���X��uplinked data frame
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

    /* ���w�üƽd�� */
    int min = 1;
    int max = 100;

    int x;

    for(int i = 0; i < numOfNode; i++) {
        x = rand() % (max - min + 1) + min;
        if(x < downlinkedProbability) {
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
    // CW equals 32
    for(int i = 0; i < 32; i++) {
        bool mightCollision = false, collision = false;
        //Check is there any STA whose backoff counter equals 0
        for(int j = startAID; j < startAID + numOfSTAEachSlot - 1; j++) {
            if(nodes[j].isTransmitting()) {
                if(mightCollision){ // �o�͸I��
                    collision = true;
                    break;
                } else
                    mightCollision =true;
            }
        }

        if(!mightCollision) // �p�G�S��STA�n�o�_�ǿ�
            backoffCounterCountDown(startAID);
        else {
            if(collision) //�p�G���I���o�͡A�h����o�͸I����STAs��backoff counter
                generateNewBackoffCounter(startAID);
            else // �S�I���o�͡A����claim���\��STA��time stamp�Mclaim��data frame�ƶq
                recordTimeStamp(startAID);
        }
    }
}


int main(){
    // Create nodes
    for(int i = 0; i < numOfNode; i++) {
        Node node(i, uplinkedProbability);
        nodes.push_back(node);
    }

    // Generate backoff counter
    for(int i = 0; i < numOfNode; i++) {
        nodes[i].generateBackoffCounter();
    }

    int startAID = 0;

    for(int i = 0; i < numOfDTIM; i++) {
        for(int j = 0; j < numOfTIMEachDTIM; j++) {
            for(int k = 0; k < numOfRAWEachTIM; k++) {
                for(int l = 0; l < numOfSlotEachRAW; l++) {
                    //Claiming Phase
                    claimingPhase(startAID);
                    
                }
            }
        }
    }
}