#include<bits/stdc++.h>

#include "node.cpp"

using namespace std;

const int numOfNode = 576; // �]�w���X��node
const int maxNumOfDownlinkedDataFrame = 1;
const int maxNumOfUplinkedDataFrame = 3;
const int lambdaOfDownlinkedDataFrame = 0.5;
const int lambdaOfUplinkedDataFrame = 1;
const double downlinkedProbability = 30;
const double uplinkedProbability = 30;
const double dataSize = 128; // bytes
const double dataRate = 150000; // bps
const double countDownTimeSlice = 0.000052; // s (52us)
const double transTimePerDataFrame = ((dataSize * 8) / dataRate);

const int numOfDTIM = 10;
const int numOfTIMEachDTIM = 2;
const int numOfRAWEachTIM = 2;
const int numOfSlotEachRAW = 4;
const int numOfSTAEachSlot = numOfNode / numOfTIMEachDTIM / numOfRAWEachTIM / numOfSlotEachRAW;


double slotUsage = 0;
double totalSlotUsage = 0;

int numOfDataTrans[numOfDTIM][numOfNode] = {0};
int DTIMRound = 0;

int timesOfEyeOpen = 0;

const double DTIMDuration = 3.84; // ��
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
int numOfDownlinkedDataFrame[numOfNode];
int numOfUplinkedDataFrame[numOfNode];
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
        numOfUplinkedDataFrame[i] = 0;
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

void generateUplinkedData(double lambda) {
    unsigned seed = std::chrono::system_clock::now().time_since_epoch().count(); // ��l�ƥΩ�Poisson Distribution��seed
    default_random_engine generator(seed);
    poisson_distribution<int> distribution(lambda);
    int zero = 0, one = 0, two = 0, three = 0;
    for (int i = 0; i < numOfNode; i++) {
        int packets;
        do {
            packets = distribution(generator);
        } while (packets > maxNumOfUplinkedDataFrame);

        nodes[i].setNumOfUplinkedData(nodes[i].getNumOfUplinkedData() + packets);

        switch (packets)
        {
        case 0:
            zero++;
            break;
        case 1:
            one++;
            break;
        case 2:
            two++;
            break;
        case 3:
            three++;
            break;
        default:
            break;
        }
    }
}

void generateDownlinkedData(double lambda) {
    unsigned seed = std::chrono::system_clock::now().time_since_epoch().count(); // ��l�ƥΩ�Poisson Distribution��seed
    default_random_engine generator(seed);
    poisson_distribution<int> distribution(lambda);

    for (int i = 0; i < numOfNode; i++) {
        int packets;
        do {
            packets = distribution(generator);
        } while (packets > maxNumOfDownlinkedDataFrame);
        
        numOfDownlinkedDataFrame[i] += packets;
    }
}

void generateNewBackoffCounter(int start) {
    for(int i = start; i < start + numOfSTAEachSlot; i++) {
        if(nodes[i].isBackoffCounterZero() && (numOfUplinkedDataFrame[i] || numOfDownlinkedDataFrame[i])) {
            nodes[i].doubleCW();
            nodes[i].collide();
            nodes[i].generateBackoffCounter();
        }
    }
}

void backoffCounterCountDown(int start) {
    for(int i = start; i < start + numOfSTAEachSlot; i++)
        if(numOfUplinkedDataFrame[i] || numOfDownlinkedDataFrame[i])
            nodes[i].backoffCountDown();
}

void wakeSTAsUp(int startAID) {
    for(int i = startAID; i < startAID + numOfSTAEachSlot; i++) {
        if(nodes[i].getNumOfUplinkedData() || numOfDownlinkedDataFrame[i])
            nodes[i].wakingUp();
    }
}

void contendInRemainingSubSlot(int startAID, int i, double &time) {
    while(slots[i] > 0 && slots[i] >= countDownTimeSlice) {
        bool mightCollision = false, collision = false;
        int temp;
        //Check is there any STA whose backoff counter equals 0
        for(int j = startAID; j < startAID + numOfSTAEachSlot; j++) {
            if(nodes[j].isBackoffCounterZero() && (numOfUplinkedDataFrame[j] || numOfDownlinkedDataFrame[j])) {
                nodes[j].attemptToAccessChannel();
                if(mightCollision) { // �o�͸I��
                    collision = true;
                } else {
                    mightCollision = true;
                    temp = j;
                }
            }
        }

        if(!mightCollision) { // �p�G�S��STA�n�o�_�ǿ�
            backoffCounterCountDown(startAID);
            slots[i] -= countDownTimeSlice;
            time += countDownTimeSlice;
        } else {
            if(collision) { //�@�p�G���I���o�͡A�h����o�͸I����STAs��backoff counter
                generateNewBackoffCounter(startAID);
                if(slots[i] >= transTimePerDataFrame) {
                    slots[i] -= (2 * transTimePerDataFrame);
                    time += (2 * transTimePerDataFrame);
                } else {
                    time += slots[i];
                    slots[i] = 0;
                }
            } else { // �S�I���o�͡ASTA�o�euplinked data
                while(numOfUplinkedDataFrame[temp] || numOfDownlinkedDataFrame[temp]) {
                    if(slots[i] >= transTimePerDataFrame) {
                        timeOfUnscheduled++;
                        slots[i] -= transTimePerDataFrame;
                        time += transTimePerDataFrame;
                        numOfDataTrans[DTIMRound][temp]++;
                        if(numOfUplinkedDataFrame[temp]) {
                            numOfUplinkedDataFrame[temp]--;
                            nodes[temp].aDataGetTransed();
                        } else
                            numOfDownlinkedDataFrame[temp]--;
                        
                        if(!numOfUplinkedDataFrame[temp] && !numOfDownlinkedDataFrame[temp])
                            nodes[temp].fallAsleep(time);
                    } else {
                        time += slots[i];
                        slots[i] = 0;
                        break;
                    }
                }
            }
        }
    }
}

void resetDTIMInfo() {
    for(int i = 0; i < numOfNode; i++) {
        numOfClaimedUplinkedDataFrame[i] = 0;
        uplinkedDataTimeStamp[i] = 0;
    }

    claimed.clear();
}

// ��?�Τ_??�u�l�[�� CSV ���
void appendToCSV(const std::string& filename, double value) {
    std::ofstream file;
    // �H�l�[�Ҧ���?���
    file.open(filename, std::ios::app);
    if (!file.is_open()) {
        std::cerr << "Failed to open file." << std::endl;
        return;
    }

    // ??�u?�J���A�C��?�J�@?�s��
    file << value << "\n";

    file.close();
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
        generateDownlinkedData(lambdaOfDownlinkedDataFrame); // �ھ�Poisson Distribution����doenlinked data frame�Alambda��0.7
        generateUplinkedData(lambdaOfUplinkedDataFrame); // �ھ�Poisson Distribution����uplinked data frame�Alambda��0.5
        for(int j = 0; j < numOfNode; j++) {
            nodes[j].resetCW();
            nodes[j].generateBackoffCounter();
            numOfUplinkedDataFrame[j] = nodes[j].getNumOfUplinkedData();
        }
        int startAID = 0;
        for(int j = 0; j < numOfTIMEachDTIM; j++) {
            for(int k = 0; k < numOfRAWEachTIM; k++) {
                for(int l = 0; l < numOfSlotEachRAW; l++) {
                    double time = 0;
                    wakeSTAsUp(startAID);
                    contendInRemainingSubSlot(startAID, l, time);
                    for(int m = startAID; m < startAID + numOfSTAEachSlot; m++) {
                        nodes[m].isAwaking(slotDuration); // �P�_�O�_��������channel access��STA�A�p�G���A��slot duration��i�o��STA��transmission time
                        if(nodes[m].eyesOpen())
                            timesOfEyeOpen++;
                    }

                    double temp = 0;

                    for(int j = startAID; j < startAID + numOfSTAEachSlot; j++)
                        temp += numOfDataTrans[DTIMRound][j];

                    temp *= transTimePerDataFrame;

                    slotUsage += (temp / (slotDuration));

                    startAID += numOfSTAEachSlot;
                    slots[l] = slotDuration;
                }
            }
        }

        totalSlotUsage += (slotUsage / (numOfTIMEachDTIM * numOfRAWEachTIM * numOfSlotEachRAW));
        slotUsage = 0;

        // resetDTIMInfo();
        DTIMRound++;
    }

    double temp = 0;
    double th[numOfNode] = {0};
    int sum = 0;
    int totalTransDataFrame = 0;
    for(int i = 0; i < numOfNode; i++) {
        int num = 0;
        for(int j = 0; j < DTIMRound; j++)
            num += numOfDataTrans[j][i];
        
        totalTransDataFrame +=num;
        num *= dataSize * 8;
        if(nodes[i].getAwakingTime())
            th[i] += (num / nodes[i].getAwakingTime());
        sum += th[i];
    }

    cout << "Total Trans Frame: " << totalTransDataFrame << endl;   

    cout << "Channel Utilization: " << ((totalTransDataFrame * transTimePerDataFrame) / (DTIMDuration * numOfDTIM)) * 100 << endl;
    
    cout << "Channel Utilization2: " << totalSlotUsage / numOfDTIM << endl;

    appendToCSV("CU.csv", ((totalTransDataFrame * transTimePerDataFrame) / (DTIMDuration * numOfDTIM)) * 100);
    
    // cout << "Sum: " << sum << endl;

    // cout << "Throughput: " << sum / numOfNode << endl;

    cout << "Throughput: " << (totalTransDataFrame * dataSize * 8) / (DTIMDuration * numOfDTIM) << endl;

    appendToCSV("Throughput.csv", (totalTransDataFrame * dataSize * 8) / (DTIMDuration * numOfDTIM));

    double totalColRate = 0;
    for(int i = 0; i < numOfNode; i++) {
        if(nodes[i].getTimesOfAttemptAccessChannel())
            totalColRate += (nodes[i].getTimesOfCollision() / nodes[i].getTimesOfAttemptAccessChannel());
    }
    cout << "Collision Rate: " << (totalColRate / numOfNode) * 100 << endl;

    appendToCSV("Collision.csv", (totalColRate / numOfNode) * 100);

    cout << "Unscheduled: " << timeOfUnscheduled << endl;

    for(int i = 0; i < numOfNode; i++) {
        // cout << i << ": " << nodes[i].getAwakingTime() << endl;
        temp += nodes[i].getAwakingTime();
    }

    cout << "Avg: " << temp / numOfNode << endl;

    cout << "Total Time: " << temp << endl;

    appendToCSV("Transmission.csv", temp / numOfNode);

    // cout << "finish" << endl;

    cout << "Times of Eye Open: " << timesOfEyeOpen << endl;

    return 0;
}