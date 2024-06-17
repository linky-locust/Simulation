#include<bits/stdc++.h>

#include "node.cpp"

using namespace std;

const int numOfNode = 1024; // 設定有幾個node
const double restriction = 0.047;
const double downlinkedProbability = 30;
const double uplinkedProbability = 30;
const double dataSize = 64; // bytes
const double dataRate = 150000; // bps
const double miniSlot = 0.0005; // 0.5ms
double tClaiming = 0.02; // s (20ms)
const double countDownTimeSlice = 0.000052; // s (52us)
const double transTimePerDataFrame = ((dataSize * 8) / dataRate);
const int numOfDTIM = 10;
const int numOfTIMEachDTIM = 2;
const int numOfRAWEachTIM = 2;
const int numOfSlotEachRAW = 4;
const int numOfSTAEachSlot = numOfNode / numOfTIMEachDTIM / numOfRAWEachTIM / numOfSlotEachRAW;


double delayRestriction[numOfTIMEachDTIM][numOfRAWEachTIM]; // s (500ms)

double awakingTimeOfRAW[numOfNode] = {0};

int numOfDataTrans[numOfDTIM][numOfNode] = {0};
int DTIMRound = 0;

int timesOfDP = 0;

double accumulatedTime = 0;

const double DTIMDuration = 2.56; // 秒
const double slotDuration = DTIMDuration / numOfTIMEachDTIM / numOfRAWEachTIM / numOfSlotEachRAW;
double RAWslotDuration[numOfTIMEachDTIM][numOfRAWEachTIM];

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
int numOfUplinkedDataFrame[numOfNode];
int numOfClaimedUplinkedDataFrame[numOfNode];
int downlinkedDataTimeStamp[numOfNode];
int uplinkedDataTimeStamp[numOfNode];

double DTIMLength[numOfDTIM] = {0};

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

    for(int i = 0; i < numOfTIMEachDTIM; i++) {
        for(int j = 0; j < numOfRAWEachTIM; j++) {
            RAWslotDuration[i][j] = slotDuration;
            delayRestriction[i][j] = restriction;
        }
    }
}

void generateUplinkedData(double lambda) {
    unsigned seed = std::chrono::system_clock::now().time_since_epoch().count(); // 初始化用於Poisson Distribution的seed
    default_random_engine generator(seed);
    poisson_distribution<int> distribution(lambda);
    int zero = 0, one = 0, two = 0, three = 0;
    for (int i = 0; i < numOfNode; i++) {
        int packets;
        do {
            packets = distribution(generator);
        } while (packets > 3); // 确保封包?量不超?3

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
    unsigned seed = std::chrono::system_clock::now().time_since_epoch().count(); // 初始化用於Poisson Distribution的seed
    default_random_engine generator(seed);
    poisson_distribution<int> distribution(lambda);

    for (int i = 0; i < numOfNode; i++) {
        int packets;
        do {
            packets = distribution(generator);
        } while (packets > 1);

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
    while(slots[i] > 0 && slots[i] >= miniSlot) {
        bool mightCollision = false, collision = false;
        int temp;
        //Check is there any STA whose backoff counter equals 0
        for(int j = startAID; j < startAID + numOfSTAEachSlot; j++) {
            if(nodes[j].isBackoffCounterZero() && (numOfUplinkedDataFrame[j] || numOfDownlinkedDataFrame[j])) {
                nodes[j].attemptToAccessChannel();
                if(mightCollision){ // 發生碰撞
                    collision = true;
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
                generateNewBackoffCounter(startAID);
                if(slots[i] >= transTimePerDataFrame) {
                    slots[i] -= transTimePerDataFrame;
                    time += transTimePerDataFrame;
                } else {
                    time += slots[i];
                    slots[i] = 0;
                }
            } else { // 沒碰撞發生，STA發送uplinked data
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
                        
                        if(!numOfUplinkedDataFrame[temp] && !numOfDownlinkedDataFrame[temp]) {
                            nodes[temp].fallAsleep(time);
                            awakingTimeOfRAW[temp] += time;
                        }
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

void dynamicPolicy(int j, int k) {
    double temp = 0, delay = 0;
    for(int i = 0; i < numOfNode; i++) 
        delay += awakingTimeOfRAW[i];

    // delay = temp - accumulatedTime;
    delay /= (numOfSlotEachRAW * numOfSTAEachSlot);
    if(delay > delayRestriction[j][k]) {
        delayRestriction[j][k] = (delay + delayRestriction[j][k]) / 2;
        timesOfDP++;
        RAWslotDuration[j][k] += RAWslotDuration[j][k];
    }

    accumulatedTime = temp;
}

void resetDTIMInfo() {
    for(int i = 0; i < numOfNode; i++) {
        numOfClaimedUplinkedDataFrame[i] = 0;
        uplinkedDataTimeStamp[i] = 0;
    }

    claimed.clear();
}

// 函?用于??据追加到 CSV 文件
void appendToCSV(const std::string& filename, double value) {
    std::ofstream file;
    // 以追加模式打?文件
    file.open(filename, std::ios::app);
    if (!file.is_open()) {
        std::cerr << "Failed to open file." << std::endl;
        return;
    }

    // ??据?入文件，每次?入一?新行
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
        for(int j = 0; j < numOfTIMEachDTIM; j++) {
            for(int k = 0; k < numOfRAWEachTIM; k++) {
                for(int l = 0; l < numOfSlotEachRAW; l++)
                    DTIMLength[i] += RAWslotDuration[j][k];
            }
        }
        generateDownlinkedData(0.7); // 根據Poisson Distribution產生doenlinked data frame，lambda為0.7
        generateUplinkedData(0.5); // 根據Poisson Distribution產生uplinked data frame，lambda為0.5
        for(int j = 0; j < numOfNode; j++) {
            nodes[j].resetCW();
            nodes[j].generateBackoffCounter();
            numOfUplinkedDataFrame[j] = nodes[j].getNumOfUplinkedData();
        }
        int startAID = 0;
        for(int j = 0; j < numOfTIMEachDTIM; j++) {
            for(int k = 0; k < numOfRAWEachTIM; k++) {
                for(int l = 0; l < numOfSlotEachRAW; l++) {
                    slots[l] = RAWslotDuration[j][k];
                    double time = 0;
                    wakeSTAsUp(startAID);
                    contendInRemainingSubSlot(startAID, l, time);
                    for(int m = startAID; m < startAID + numOfSTAEachSlot; m++) {
                        nodes[m].isAwaking(RAWslotDuration[j][k]); // 判斷是否有未完成channel access的STA，如果有，把slot duration算進這些STA的transmission time
                        if(nodes[m].eyesOpen())
                            awakingTimeOfRAW[m] += RAWslotDuration[j][k];
                    }

                    startAID += numOfSTAEachSlot;
                }
                dynamicPolicy(j, k);
                for(int l = 0; l < numOfNode; l++)
                    awakingTimeOfRAW[l] = 0;
            }
        }
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
        
        totalTransDataFrame += num;
        num *= dataSize * 8;
        if(nodes[i].getAwakingTime())
            th[i] += (num / nodes[i].getAwakingTime());
        sum += th[i];
    }

    cout << "Total Trans Frame: " << totalTransDataFrame << endl;   

    double du = 0;
    for(int i = 0; i < numOfDTIM; i++)
        du += DTIMLength[i];

    cout << "Channel Utilization: " << (totalTransDataFrame * transTimePerDataFrame) / du << endl;

    appendToCSV("CU.csv", (totalTransDataFrame * transTimePerDataFrame) / du);

    cout << "Throughput: " << sum / numOfNode << endl;

    appendToCSV("Throughput.csv", sum / numOfNode);

    double totalColRate = 0;
    for(int i = 0; i < numOfNode; i++) {
        if(nodes[i].getTimesOfAttemptAccessChannel())
            totalColRate += (nodes[i].getTimesOfCollision() / nodes[i].getTimesOfAttemptAccessChannel());
    }
    cout << "Collision Rate: " << totalColRate / numOfNode << endl;

    appendToCSV("Collision.csv", totalColRate / numOfNode);

    cout << "Unscheduled: " << timeOfUnscheduled << endl;

    for(int i = 0; i < numOfNode; i++) {
        // cout << i << ": " << nodes[i].getAwakingTime() << endl;
        temp += nodes[i].getAwakingTime();
    }

    cout << "Avg: " << temp / numOfDTIM / numOfNode << endl;

    appendToCSV("Transmission.csv", temp / numOfDTIM / numOfNode);

    // cout << "slotDuration: " << slotDuration << endl;

    cout << "TimesOfDP: " << timesOfDP << endl;

    cout << "finish" << endl;

    return 0;
}