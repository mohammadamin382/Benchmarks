#include <windows.h>
#include <iostream>
#include <vector>
#include <thread>
#include <atomic>
#include <chrono>
#include <iomanip>
#include <algorithm>
#include <numeric>
#include <cmath>
#include <mutex>

using namespace std;
using namespace std::chrono;

// Core information structure
struct CoreInfo {
    DWORD id;
    BYTE efficiencyClass; // 0 = E-Core, 1 = P-Core
    DWORD_PTR affinityMask;
    string type;
    atomic<uint64_t> workCompleted{0};
    double score = 0.0;
};

// Global variables
vector<CoreInfo> g_cores;
atomic<bool> g_stopTest{false};
mutex g_consoleMutex;

// Progress bar function
void DrawProgressBar(int percentage, int testDuration) {
    const int barWidth = 50;
    lock_guard<mutex> lock(g_consoleMutex);
    
    cout << "\r[";
    int pos = barWidth * percentage / 100;
    for (int i = 0; i < barWidth; ++i) {
        if (i < pos) cout << "=";
        else if (i == pos) cout << ">";
        else cout << " ";
    }
    cout << "] " << percentage << "% (" << (testDuration * percentage / 100) << "s / " << testDuration << "s)";
    cout.flush();
}

// CPU topology detection
bool DetectCPUTopology() {
    DWORD bufferSize = 0;
    GetLogicalProcessorInformationEx(RelationProcessorCore, nullptr, &bufferSize);
    
    if (GetLastError() != ERROR_INSUFFICIENT_BUFFER) {
        cerr << "Error: Failed to get processor information buffer size.\n";
        return false;
    }
    
    vector<BYTE> buffer(bufferSize);
    auto info = reinterpret_cast<SYSTEM_LOGICAL_PROCESSOR_INFORMATION_EX*>(buffer.data());
    
    if (!GetLogicalProcessorInformationEx(RelationProcessorCore, info, &bufferSize)) {
        cerr << "Error: Failed to get processor information.\n";
        return false;
    }
    
    DWORD coreId = 0;
    BYTE* ptr = buffer.data();
    BYTE* endPtr = buffer.data() + bufferSize;
    
    while (ptr < endPtr) {
        auto currentInfo = reinterpret_cast<SYSTEM_LOGICAL_PROCESSOR_INFORMATION_EX*>(ptr);
        
        if (currentInfo->Relationship == RelationProcessorCore) {
            CoreInfo core;
            core.id = coreId++;
            core.efficiencyClass = currentInfo->Processor.EfficiencyClass;
            core.affinityMask = currentInfo->Processor.GroupMask[0].Mask;
            core.type = (core.efficiencyClass > 0) ? "P-Core" : "E-Core";
            
            g_cores.push_back(core);
        }
        
        ptr += currentInfo->Size;
    }
    
    return !g_cores.empty();
}

// Computationally intensive workload (Prime counting with optimizations)
uint64_t ComputeWorkload() {
    uint64_t count = 0;
    const uint64_t iterations = 10000;
    
    for (uint64_t n = 2; n < iterations; ++n) {
        bool isPrime = true;
        uint64_t sqrtN = static_cast<uint64_t>(sqrt(n));
        
        for (uint64_t i = 2; i <= sqrtN; ++i) {
            if (n % i == 0) {
                isPrime = false;
                break;
            }
        }
        
        if (isPrime) {
            count++;
            // Additional computation to increase load
            volatile double x = sin(static_cast<double>(n)) * cos(static_cast<double>(n));
        }
    }
    
    return count;
}

// Worker thread function
void StressTestWorker(CoreInfo& core) {
    // Pin thread to specific core
    SetThreadAffinityMask(GetCurrentThread(), core.affinityMask);
    SetThreadPriority(GetCurrentThread(), THREAD_PRIORITY_HIGHEST);
    
    uint64_t workCount = 0;
    
    while (!g_stopTest.load()) {
        ComputeWorkload();
        workCount++;
    }
    
    core.workCompleted.store(workCount);
}

// Display system information
void DisplaySystemInfo() {
    SYSTEM_INFO sysInfo;
    GetSystemInfo(&sysInfo);
    
    cout << "\n========================================\n";
    cout << "    CPU STRESS TEST & BENCHMARK\n";
    cout << "========================================\n\n";
    cout << "System Information:\n";
    cout << "  Total Logical Processors: " << sysInfo.dwNumberOfProcessors << "\n";
    cout << "  Physical Cores Detected: " << g_cores.size() << "\n";
    
    int pCoreCount = count_if(g_cores.begin(), g_cores.end(), 
        [](const CoreInfo& c) { return c.efficiencyClass > 0; });
    int eCoreCount = g_cores.size() - pCoreCount;
    
    cout << "  P-Cores: " << pCoreCount << "\n";
    cout << "  E-Cores: " << eCoreCount << "\n\n";
}

// User preflight check
bool PreflightCheck() {
    cout << "========================================\n";
    cout << "         PRE-FLIGHT CHECK\n";
    cout << "========================================\n\n";
    cout << "WARNING: Please ensure the following:\n\n";
    cout << "  1. Remove all power limits in BIOS/OS\n";
    cout << "  2. Remove all frequency caps\n";
    cout << "  3. Disable core parking settings\n";
    cout << "  4. Set power plan to 'Ultimate Performance'\n";
    cout << "  5. Close all unnecessary applications\n";
    cout << "  6. Ensure adequate cooling is available\n\n";
    cout << "This test will stress all CPU cores to maximum capacity.\n\n";
    cout << "Type 'C' to continue: ";
    
    char input;
    cin >> input;
    
    return (input == 'C' || input == 'c');
}

// Generate comprehensive report
void GenerateReport(int testDuration) {
    cout << "\n\n========================================\n";
    cout << "         BENCHMARK RESULTS\n";
    cout << "========================================\n\n";
    
    // Calculate scores
    vector<double> pCoreScores, eCoreScores;
    
    cout << "Per-Core Performance:\n";
    cout << left << setw(10) << "Core ID" << setw(12) << "Type" 
         << setw(20) << "Work Completed" << "Score\n";
    cout << string(52, '-') << "\n";
    
    for (auto& core : g_cores) {
        core.score = static_cast<double>(core.workCompleted.load()) / testDuration;
        
        cout << left << setw(10) << core.id 
             << setw(12) << core.type
             << setw(20) << core.workCompleted.load()
             << fixed << setprecision(2) << core.score << "\n";
        
        if (core.efficiencyClass > 0) {
            pCoreScores.push_back(core.score);
        } else {
            eCoreScores.push_back(core.score);
        }
    }
    
    cout << "\n" << string(52, '-') << "\n\n";
    
    // Calculate averages
    double pCoreAvg = 0.0, eCoreAvg = 0.0;
    
    if (!pCoreScores.empty()) {
        pCoreAvg = accumulate(pCoreScores.begin(), pCoreScores.end(), 0.0) / pCoreScores.size();
        cout << "P-Core Average Throughput: " << fixed << setprecision(2) << pCoreAvg << " ops/sec\n";
    }
    
    if (!eCoreScores.empty()) {
        eCoreAvg = accumulate(eCoreScores.begin(), eCoreScores.end(), 0.0) / eCoreScores.size();
        cout << "E-Core Average Throughput: " << fixed << setprecision(2) << eCoreAvg << " ops/sec\n";
    }
    
    if (!pCoreScores.empty() && !eCoreScores.empty()) {
        double ratio = pCoreAvg / eCoreAvg;
        cout << "P-Core / E-Core Ratio: " << fixed << setprecision(2) << ratio << "x\n";
    }
    
    // Total system score
    double totalScore = 0.0;
    for (const auto& core : g_cores) {
        totalScore += core.score;
    }
    
    cout << "\n========================================\n";
    cout << "TOTAL SYSTEM SCORE: " << fixed << setprecision(2) << totalScore << " ops/sec\n";
    cout << "========================================\n\n";
}

int main() {
    // Set console to UTF-8
    SetConsoleOutputCP(CP_UTF8);
    
    // Detect CPU topology
    if (!DetectCPUTopology()) {
        cerr << "Failed to detect CPU topology. Exiting.\n";
        return 1;
    }
    
    // Display system info
    DisplaySystemInfo();
    
    // Preflight check
    if (!PreflightCheck()) {
        cout << "\nTest aborted by user.\n";
        return 0;
    }
    
    // Test configuration
    const int testDuration = 30; // seconds
    
    cout << "\n========================================\n";
    cout << "  Starting " << testDuration << "-second stress test...\n";
    cout << "========================================\n\n";
    
    // Launch worker threads
    vector<thread> workers;
    for (auto& core : g_cores) {
        workers.emplace_back(StressTestWorker, ref(core));
    }
    
    // Progress monitoring
    auto startTime = steady_clock::now();
    while (duration_cast<seconds>(steady_clock::now() - startTime).count() < testDuration) {
        int elapsed = duration_cast<seconds>(steady_clock::now() - startTime).count();
        int percentage = (elapsed * 100) / testDuration;
        DrawProgressBar(percentage, testDuration);
        this_thread::sleep_for(milliseconds(100));
    }
    
    DrawProgressBar(100, testDuration);
    
    // Stop all threads
    g_stopTest.store(true);
    for (auto& worker : workers) {
        worker.join();
    }
    
    // Generate report
    GenerateReport(testDuration);
    
    cout << "Press Enter to exit...";
    cin.ignore();
    cin.get();
    
    return 0;
}
