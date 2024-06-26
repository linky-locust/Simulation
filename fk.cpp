#include<bits/stdc++.h>

#include "node.cpp"

using namespace std;

const int numOfNode = 512; // 設定有幾個node
const int maxNumOfDownlinkedDataFrame = 3;
const int maxNumOfUplinkedDataFrame = 1;
const int lambdaOfDownlinkedDataFrame = 1;
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
const int numOfSlotEachRAW = 64;
const int numOfSTAEachSlot = numOfNode / numOfTIMEachDTIM / numOfRAWEachTIM / numOfSlotEachRAW;

int numOfDataTrans[numOfDTIM][numOfNode] = {0};
int DTIMRound = 0;

int timesOfEyeOpen = 0;

const double DTIMDuration = 3.84; // 秒
const double slotDuration = DTIMDuration / numOfTIMEachDTIM / numOfRAWEachTIM / numOfSlotEachRAW;

int numOfCollision[numOfTIMEachDTIM][numOfRAWEachTIM][numOfSlotEachRAW] = {0};
double slotDurationForRecord[numOfDTIM][numOfTIMEachDTIM][numOfRAWEachTIM][numOfSlotEachRAW];
bool shouldPlus[numOfTIMEachDTIM][numOfRAWEachTIM][numOfSlotEachRAW] = {false};
bool shouldMinus[numOfTIMEachDTIM][numOfRAWEachTIM][numOfSlotEachRAW] = {false};

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
    unsigned seed = std::chrono::system_clock::now().time_since_epoch().count(); // 初始化用於Poisson Distribution的seed
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
    unsigned seed = std::chrono::system_clock::now().time_since_epoch().count(); // 初始化用於Poisson Distribution的seed
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

void contendInRemainingSubSlot(int startAID, int i, double &time, int &col) {
    while(slots[i] > 0 && slots[i] >= countDownTimeSlice) {
        bool mightCollision = false, collision = false;
        int temp;
        //Check is there any STA whose backoff counter equals 0
        for(int j = startAID; j < startAID + numOfSTAEachSlot; j++) {
            if(nodes[j].isBackoffCounterZero() && (numOfUplinkedDataFrame[j] || numOfDownlinkedDataFrame[j])) {
                nodes[j].attemptToAccessChannel();
                if(mightCollision) { // 發生碰撞
                    collision = true;
                } else {
                    mightCollision = true;
                    temp = j;
                }
            }
        }

        if(!mightCollision) { // 如果沒有STA要發起傳輸
            backoffCounterCountDown(startAID);
            slots[i] -= countDownTimeSlice;
            time += countDownTimeSlice;
        } else {
            if(collision) { //　如果有碰撞發生，則重骰發生碰撞的STAs的backoff counter
                col++;
                generateNewBackoffCounter(startAID);
                if(slots[i] >= transTimePerDataFrame) {
                    slots[i] -= (1.5 * transTimePerDataFrame);
                    time += (1.5 * transTimePerDataFrame);
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

    for(int i = 0; i < numOfDTIM; i++)
        for(int j = 0; j < numOfTIMEachDTIM; j++)
            for(int k = 0; k < numOfRAWEachTIM; k++)
                for(int l = 0; l < numOfSlotEachRAW; l++)
                    slotDurationForRecord[i][j][k][l] = slotDuration;

    // Create nodes
    for(int i = 0; i < numOfNode; i++) {
        Node node(i, uplinkedProbability);
        nodes.push_back(node);
    }
    
    for(int i = 0; i < numOfDTIM; i++) {
        generateDownlinkedData(lambdaOfDownlinkedDataFrame); // 根據Poisson Distribution產生doenlinked data frame，lambda為0.7
        generateUplinkedData(lambdaOfUplinkedDataFrame); // 根據Poisson Distribution產生uplinked data frame，lambda為0.5
        for(int j = 0; j < numOfNode; j++) {
            nodes[j].resetCW();
            nodes[j].generateBackoffCounter();
            numOfUplinkedDataFrame[j] = nodes[j].getNumOfUplinkedData();
        }
        int startAID = 0;
        for(int j = 0; j < numOfTIMEachDTIM; j++) {
            for(int k = 0; k < numOfRAWEachTIM; k++) {
                for(int l = 0; l < numOfSlotEachRAW; l++) {
                    int col = 0;
                    double time = 0;
                    wakeSTAsUp(startAID);

                    if(shouldMinus[j][k][l] && !shouldPlus[j][k][l])
                        slotDurationForRecord[i][j][k][l] -= 0.1;
                    else if(!shouldMinus[j][k][l] && shouldPlus[j][k][l])
                        slotDurationForRecord[i][j][k][l] += 0.1;

                    slots[l] = slotDurationForRecord[i][j][k][l];

                    contendInRemainingSubSlot(startAID, l, time, col);
                    for(int m = startAID; m < startAID + numOfSTAEachSlot; m++) {
                        nodes[m].isAwaking(slotDuration); // 判斷是否有未完成channel access的STA，如果有，把slot duration算進這些STA的transmission time
                        if(nodes[m].eyesOpen())
                            timesOfEyeOpen++;
                    }

                    if(i > 0) {
                        if(col > numOfCollision[j][k][l]) {
                            shouldPlus[j][k][l] = true;
                            shouldMinus[j][k][l] = false;
                        } else if(col < numOfCollision[j][k][l]) {
                            shouldPlus[j][k][l] = false;
                            shouldMinus[j][k][l] = true;
                        } else {
                            shouldPlus[j][k][l] = false;
                            shouldMinus[j][k][l] = false;
                        }
                    } 

                    numOfCollision[j][k][l] = col;

                    startAID += numOfSTAEachSlot;
                    slots[l] = slotDuration;
                }
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
        
        totalTransDataFrame +=num;
        num *= dataSize * 8;
        if(nodes[i].getAwakingTime())
            th[i] += (num / nodes[i].getAwakingTime());
        sum += th[i];
    }

    double totalDTIMTime = 0;
    for(int i = 0; i < numOfDTIM; i++)
        for(int j = 0; j < numOfTIMEachDTIM; j++)
            for(int k = 0; k < numOfRAWEachTIM; k++)
                for(int l = 0; l < numOfSlotEachRAW; l++)
                    totalDTIMTime += slotDurationForRecord[i][j][k][l];

    cout << "Total Trans Frame: " << totalTransDataFrame << endl; 

    cout << "Total DTIM time: " << totalDTIMTime << endl;

    cout << "Channel Utilization: " << ((totalTransDataFrame * transTimePerDataFrame) / totalDTIMTime) * 100 << endl;

    appendToCSV("CU.csv", ((totalTransDataFrame * transTimePerDataFrame) / totalDTIMTime) * 100);
    
    // cout << "Sum: " << sum << endl;

    // cout << "Throughput: " << sum / numOfNode << endl;

    cout << "Throughput: " << (totalTransDataFrame * dataSize * 8) / totalDTIMTime << endl;

    appendToCSV("Throughput.csv", (totalTransDataFrame * dataSize * 8) / totalDTIMTime);

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