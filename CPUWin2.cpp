#include <windows.h>
#include <powrprof.h>
#include <intrin.h>
#include <iostream>
#include <vector>
#include <thread>
#include <chrono>
#include <iomanip>
#include <string>
#include <sstream>
#include <cmath>
#include <algorithm>
#include <random>
#include <atomic>
#include <mutex>
#include <map>

#pragma comment(lib, "powrprof.lib")

using namespace std;

struct CoreInfo {
    int id;
    int logicalCore;
    int physicalCore;
    string type;
    int efficiencyClass;
    
    // Benchmark scores
    double mathScore;
    double memoryScore;
    double branchScore;
    double cacheScore;
    double mixedScore;
    double overallScore;
    
    // Performance metrics
    double avgLatency;
    double peakThroughput;
    DWORD_PTR affinityMask;
    
    // Real-time metrics
    atomic<double> currentOps{0};
    atomic<bool> testComplete{false};
};

mutex consoleMutex;
atomic<int> completedCores{0};
atomic<bool> allTestsRunning{false};

void SetConsoleColor(int color) {
    SetConsoleTextAttribute(GetStdHandle(STD_OUTPUT_HANDLE), color);
}

void PrintProgressBar(int current, int total, const string& label) {
    lock_guard<mutex> lock(consoleMutex);
    int percentage = (current * 100) / total;
    int barWidth = 40;
    
    cout << "\r  " << label << " [";
    int pos = barWidth * current / total;
    for (int i = 0; i < barWidth; ++i) {
        if (i < pos) cout << "█";
        else if (i == pos) cout << "▓";
        else cout << "░";
    }
    cout << "] " << percentage << "% (" << current << "/" << total << ")   " << flush;
}

DWORD CountSetBits(ULONG_PTR bitMask) {
    DWORD count = 0;
    while (bitMask) {
        count += bitMask & 1;
        bitMask >>= 1;
    }
    return count;
}

vector<CoreInfo> DetectCPUCores() {
    vector<CoreInfo> cores;
    
    DWORD length = 0;
    GetLogicalProcessorInformationEx(RelationAll, nullptr, &length);
    
    vector<BYTE> buffer(length);
    PSYSTEM_LOGICAL_PROCESSOR_INFORMATION_EX info = 
        reinterpret_cast<PSYSTEM_LOGICAL_PROCESSOR_INFORMATION_EX>(buffer.data());
    
    if (!GetLogicalProcessorInformationEx(RelationAll, info, &length)) {
        return cores;
    }
    
    DWORD offset = 0;
    int coreId = 0;
    
    while (offset < length) {
        PSYSTEM_LOGICAL_PROCESSOR_INFORMATION_EX current = 
            reinterpret_cast<PSYSTEM_LOGICAL_PROCESSOR_INFORMATION_EX>(buffer.data() + offset);
        
        if (current->Relationship == RelationProcessorCore) {
            for (WORD i = 0; i < current->Processor.GroupCount; i++) {
                KAFFINITY mask = current->Processor.GroupMask[i].Mask;
                DWORD logicalCount = CountSetBits(mask);
                
                for (DWORD j = 0; j < logicalCount; j++) {
                    CoreInfo core;
                    core.id = coreId;
                    core.logicalCore = coreId;
                    core.physicalCore = coreId / 2;
                    core.efficiencyClass = current->Processor.EfficiencyClass;
                    
                    // Fix: Lower efficiency class = higher performance (P-Core)
                    core.type = (current->Processor.EfficiencyClass == 0) ? "P-Core" : "E-Core";
                    core.affinityMask = (DWORD_PTR)1 << coreId;
                    
                    core.mathScore = 0;
                    core.memoryScore = 0;
                    core.branchScore = 0;
                    core.cacheScore = 0;
                    core.mixedScore = 0;
                    core.overallScore = 0;
                    core.avgLatency = 0;
                    core.peakThroughput = 0;
                    
                    cores.push_back(core);
                    coreId++;
                }
            }
        }
        offset += current->Size;
    }
    
    return cores;
}

// Intensive Math Test - Heavy floating point operations
double MathIntensiveTest(DWORD_PTR affinityMask, int duration_ms) {
    SetThreadAffinityMask(GetCurrentThread(), affinityMask);
    SetThreadPriority(GetCurrentThread(), THREAD_PRIORITY_HIGHEST);
    
    auto start = chrono::high_resolution_clock::now();
    auto end = start + chrono::milliseconds(duration_ms);
    
    volatile double result = 1.0;
    unsigned long long operations = 0;
    
    random_device rd;
    mt19937 gen(rd());
    uniform_real_distribution<> dis(0.1, 10.0);
    
    while (chrono::high_resolution_clock::now() < end) {
        double a = dis(gen), b = dis(gen), c = dis(gen);
        
        // Complex mathematical operations
        result += sqrt(a * b + c);
        result *= sin(a) * cos(b);
        result /= (1.0 + tan(c));
        result = pow(abs(result), 0.5);
        result = log(abs(result) + 1.0) * exp(a * 0.01);
        result = atan2(b, c) * asin(a / 10.0);
        
        operations++;
    }
    
    auto actual_end = chrono::high_resolution_clock::now();
    double elapsed = chrono::duration<double>(actual_end - start).count();
    
    return operations / elapsed;
}

// Memory Intensive Test - Cache and memory bandwidth
double MemoryIntensiveTest(DWORD_PTR affinityMask, int duration_ms) {
    SetThreadAffinityMask(GetCurrentThread(), affinityMask);
    SetThreadPriority(GetCurrentThread(), THREAD_PRIORITY_HIGHEST);
    
    const size_t arraySize = 16 * 1024 * 1024; // 16MB array
    vector<int> data(arraySize);
    
    random_device rd;
    mt19937 gen(rd());
    
    for (size_t i = 0; i < arraySize; i++) {
        data[i] = gen();
    }
    
    auto start = chrono::high_resolution_clock::now();
    auto end = start + chrono::milliseconds(duration_ms);
    
    unsigned long long operations = 0;
    volatile int result = 0;
    
    while (chrono::high_resolution_clock::now() < end) {
        // Random memory access pattern (cache-unfriendly)
        for (int i = 0; i < 1000; i++) {
            size_t idx = gen() % arraySize;
            result += data[idx];
            data[idx] = result ^ (i * 31);
        }
        operations++;
    }
    
    auto actual_end = chrono::high_resolution_clock::now();
    double elapsed = chrono::duration<double>(actual_end - start).count();
    
    return operations / elapsed;
}

// Branch Prediction Test - Unpredictable branches
double BranchIntensiveTest(DWORD_PTR affinityMask, int duration_ms) {
    SetThreadAffinityMask(GetCurrentThread(), affinityMask);
    SetThreadPriority(GetCurrentThread(), THREAD_PRIORITY_HIGHEST);
    
    auto start = chrono::high_resolution_clock::now();
    auto end = start + chrono::milliseconds(duration_ms);
    
    random_device rd;
    mt19937 gen(rd());
    uniform_int_distribution<> dis(0, 100);
    
    volatile int result = 0;
    unsigned long long operations = 0;
    
    while (chrono::high_resolution_clock::now() < end) {
        for (int i = 0; i < 1000; i++) {
            int val = dis(gen);
            
            // Unpredictable branches
            if (val < 25) {
                result += val * 3;
            } else if (val < 50) {
                result -= val * 2;
            } else if (val < 75) {
                result ^= val;
            } else {
                result *= (val % 7 + 1);
            }
            
            // More complex branching
            if ((result & 1) && (val % 3 == 0)) {
                result = (result << 2) | (val & 0xF);
            } else if (result % 7 == 0) {
                result = result ^ (val * 13);
            }
        }
        operations++;
    }
    
    auto actual_end = chrono::high_resolution_clock::now();
    double elapsed = chrono::duration<double>(actual_end - start).count();
    
    return operations / elapsed;
}

// Cache Thrashing Test - L1/L2/L3 cache performance
double CacheIntensiveTest(DWORD_PTR affinityMask, int duration_ms) {
    SetThreadAffinityMask(GetCurrentThread(), affinityMask);
    SetThreadPriority(GetCurrentThread(), THREAD_PRIORITY_HIGHEST);
    
    // Create arrays larger than L3 cache (typically 24-36MB)
    const size_t smallSize = 32 * 1024;    // 32KB (L1)
    const size_t mediumSize = 512 * 1024;  // 512KB (L2)
    const size_t largeSize = 32 * 1024 * 1024; // 32MB (L3+)
    
    vector<int> small(smallSize), medium(mediumSize), large(largeSize);
    
    auto start = chrono::high_resolution_clock::now();
    auto end = start + chrono::milliseconds(duration_ms);
    
    unsigned long long operations = 0;
    volatile int result = 0;
    
    while (chrono::high_resolution_clock::now() < end) {
        // L1 cache test
        for (size_t i = 0; i < smallSize; i++) {
            result += small[i];
        }
        
        // L2 cache test
        for (size_t i = 0; i < mediumSize; i += 64) {
            result += medium[i];
        }
        
        // L3 cache thrashing
        for (size_t i = 0; i < largeSize; i += 4096) {
            result += large[i];
        }
        
        operations++;
    }
    
    auto actual_end = chrono::high_resolution_clock::now();
    double elapsed = chrono::duration<double>(actual_end - start).count();
    
    return operations / elapsed;
}

// Mixed Workload Test - Real-world simulation
double MixedWorkloadTest(DWORD_PTR affinityMask, int duration_ms) {
    SetThreadAffinityMask(GetCurrentThread(), affinityMask);
    SetThreadPriority(GetCurrentThread(), THREAD_PRIORITY_HIGHEST);
    
    auto start = chrono::high_resolution_clock::now();
    auto end = start + chrono::milliseconds(duration_ms);
    
    const size_t dataSize = 1024 * 1024;
    vector<double> data(dataSize);
    
    random_device rd;
    mt19937 gen(rd());
    uniform_real_distribution<> dis(0.0, 1000.0);
    
    unsigned long long operations = 0;
    
    while (chrono::high_resolution_clock::now() < end) {
        // Mix of operations
        for (int i = 0; i < 100; i++) {
            double val = dis(gen);
            size_t idx = gen() % dataSize;
            
            // Math
            data[idx] = sqrt(val) * sin(val);
            
            // Memory
            if (idx > 0) {
                data[idx] += data[idx - 1];
            }
            
            // Branches
            if (val > 500) {
                data[idx] *= 1.1;
            } else {
                data[idx] *= 0.9;
            }
        }
        operations++;
    }
    
    auto actual_end = chrono::high_resolution_clock::now();
    double elapsed = chrono::duration<double>(actual_end - start).count();
    
    return operations / elapsed;
}

void RunComprehensiveBenchmark(CoreInfo& core, int testDuration) {
    core.mathScore = MathIntensiveTest(core.affinityMask, testDuration);
    core.memoryScore = MemoryIntensiveTest(core.affinityMask, testDuration);
    core.branchScore = BranchIntensiveTest(core.affinityMask, testDuration);
    core.cacheScore = CacheIntensiveTest(core.affinityMask, testDuration);
    core.mixedScore = MixedWorkloadTest(core.affinityMask, testDuration);
    
    // Calculate weighted overall score
    core.overallScore = (core.mathScore * 0.25 + 
                         core.memoryScore * 0.2 + 
                         core.branchScore * 0.2 + 
                         core.cacheScore * 0.15 + 
                         core.mixedScore * 0.2);
    
    core.testComplete = true;
    completedCores++;
}

void PrintBanner() {
    SetConsoleColor(11);
    cout << "\n";
    cout << "  ╔════════════════════════════════════════════════════════════════╗\n";
    cout << "  ║     ADVANCED CPU CORE BENCHMARK & STRESS TEST UTILITY         ║\n";
    cout << "  ║              Multi-threaded Performance Analysis               ║\n";
    cout << "  ║                      Windows Edition v2.0                      ║\n";
    cout << "  ╚════════════════════════════════════════════════════════════════╝\n";
    SetConsoleColor(7);
    cout << "\n";
}

void PrintWarning() {
    SetConsoleColor(14);
    cout << "  ┌────────────────────────────────────────────────────────────────┐\n";
    cout << "  │                    ⚠️  IMPORTANT NOTICE                         │\n";
    cout << "  └────────────────────────────────────────────────────────────────┘\n\n";
    SetConsoleColor(7);
    
    cout << "  Before running this benchmark, please ensure:\n\n";
    SetConsoleColor(10);
    cout << "  ✓ Close all unnecessary applications\n";
    cout << "  ✓ Set Windows Power Plan to 'High Performance'\n";
    cout << "  ✓ Disable CPU frequency limits in BIOS/UEFI\n";
    cout << "  ✓ Remove any CPU core parking restrictions\n";
    cout << "  ✓ Disable CPU throttling in power settings\n";
    cout << "  ✓ Set processor state to 100% (min and max)\n";
    cout << "  ✓ Ensure proper cooling/ventilation\n";
    SetConsoleColor(7);
    
    cout << "\n  Location: Control Panel → Power Options → Edit Plan Settings\n";
    cout << "            → Change Advanced Power Settings → Processor Power\n";
    cout << "            Management → Maximum/Minimum Processor State → 100%\n\n";
    
    SetConsoleColor(12);
    cout << "  ⚠️  WARNING: This test will stress ALL CPU cores simultaneously!\n";
    SetConsoleColor(7);
    
    cout << "\n  ────────────────────────────────────────────────────────────────\n";
    cout << "\n  Type 'c' and press ENTER to continue: ";
    
    string input;
    while (true) {
        getline(cin, input);
        if (input == "c" || input == "C") {
            break;
        }
        cout << "  Invalid input. Please type 'c' to continue: ";
    }
    cout << "\n";
}

void PrintCoreDetection(const vector<CoreInfo>& cores) {
    SetConsoleColor(11);
    cout << "  ┌────────────────────────────────────────────────────────────────┐\n";
    cout << "  │              CPU CORE DETECTION RESULTS                        │\n";
    cout << "  └────────────────────────────────────────────────────────────────┘\n\n";
    SetConsoleColor(7);
    
    int pCoreCount = 0, eCoreCount = 0;
    for (const auto& core : cores) {
        if (core.type == "P-Core") pCoreCount++;
        else eCoreCount++;
    }
    
    cout << "  Total Logical Cores: " << cores.size() << "\n";
    cout << "  Performance Cores (P-Cores): " << pCoreCount << "\n";
    cout << "  Efficiency Cores (E-Cores): " << eCoreCount << "\n\n";
    
    cout << "  ┌──────┬───────────┬──────────┬────────────┬──────────────┐\n";
    cout << "  │  ID  │   Type    │ Physical │ Eff. Class │   Affinity   │\n";
    cout << "  ├──────┼───────────┼──────────┼────────────┼──────────────┤\n";
    
    for (const auto& core : cores) {
        cout << "  │ " << setw(4) << core.id << " │ ";
        
        if (core.type == "P-Core") {
            SetConsoleColor(10);
            cout << setw(9) << core.type;
            SetConsoleColor(7);
        } else {
            SetConsoleColor(14);
            cout << setw(9) << core.type;
            SetConsoleColor(7);
        }
        
        cout << " │ " << setw(8) << core.physicalCore 
             << " │ " << setw(10) << core.efficiencyClass
             << " │ 0x" << setw(10) << hex << core.affinityMask << dec << " │\n";
    }
    
    cout << "  └──────┴───────────┴──────────┴────────────┴──────────────┘\n\n";
}

void PrintDetailedResults(vector<CoreInfo>& cores) {
    sort(cores.begin(), cores.end(), 
         [](const CoreInfo& a, const CoreInfo& b) { return a.overallScore > b.overallScore; });
    
    SetConsoleColor(11);
    cout << "\n\n  ┌────────────────────────────────────────────────────────────────┐\n";
    cout << "  │           COMPREHENSIVE BENCHMARK RESULTS                      │\n";
    cout << "  └────────────────────────────────────────────────────────────────┘\n\n";
    SetConsoleColor(7);
    
    double maxScore = cores[0].overallScore;
    
    cout << "  ┌────┬─────────┬──────────┬────────┬────────┬───────┬────────┬──────┐\n";
    cout << "  │ ID │  Type   │  Math    │ Memory │ Branch │ Cache │ Mixed  │ Rank │\n";
    cout << "  ├────┼─────────┼──────────┼────────┼────────┼───────┼────────┼──────┤\n";
    
    for (size_t i = 0; i < cores.size(); i++) {
        cout << "  │" << setw(3) << cores[i].id << " │ ";
        
        if (cores[i].type == "P-Core") {
            SetConsoleColor(10);
            cout << "P-Core";
            SetConsoleColor(7);
        } else {
            SetConsoleColor(14);
            cout << "E-Core";
            SetConsoleColor(7);
        }
        
        cout << "  │ " << setw(8) << fixed << setprecision(2) << cores[i].mathScore / 1e3 << " │";
        cout << " " << setw(6) << fixed << setprecision(2) << cores[i].memoryScore / 1e3 << " │";
        cout << " " << setw(6) << fixed << setprecision(2) << cores[i].branchScore / 1e3 << " │";
        cout << " " << setw(5) << fixed << setprecision(2) << cores[i].cacheScore / 1e3 << " │";
        cout << " " << setw(6) << fixed << setprecision(2) << cores[i].mixedScore / 1e3 << " │";
        
        if (i < 3) SetConsoleColor(14);
        cout << " " << setw(4) << (i + 1);
        SetConsoleColor(7);
        cout << " │\n";
    }
    
    cout << "  └────┴─────────┴──────────┴────────┴────────┴───────┴────────┴──────┘\n";
    cout << "  Note: All scores in K-Ops/s (thousands of operations per second)\n\n";
    
    // Statistical Analysis
    SetConsoleColor(11);
    cout << "  ┌────────────────────────────────────────────────────────────────┐\n";
    cout << "  │                  STATISTICAL ANALYSIS                          │\n";
    cout << "  └────────────────────────────────────────────────────────────────┘\n\n";
    SetConsoleColor(7);
    
    map<string, vector<double>> typeScores;
    for (const auto& core : cores) {
        typeScores[core.type].push_back(core.overallScore);
    }
    
    for (const auto& [type, scores] : typeScores) {
        double avg = accumulate(scores.begin(), scores.end(), 0.0) / scores.size();
        double maxVal = *max_element(scores.begin(), scores.end());
        double minVal = *min_element(scores.begin(), scores.end());
        
        if (type == "P-Core") SetConsoleColor(10);
        else SetConsoleColor(14);
        
        cout << "  " << type << " Performance:\n";
        SetConsoleColor(7);
        cout << "    Average:  " << fixed << setprecision(2) << avg / 1e3 << " K-Ops/s\n";
        cout << "    Best:     " << fixed << setprecision(2) << maxVal / 1e3 << " K-Ops/s\n";
        cout << "    Worst:    " << fixed << setprecision(2) << minVal / 1e3 << " K-Ops/s\n";
        cout << "    Variance: " << fixed << setprecision(1) << ((maxVal - minVal) / avg * 100) << "%\n\n";
    }
    
    if (typeScores.size() > 1) {
        double pAvg = accumulate(typeScores["P-Core"].begin(), typeScores["P-Core"].end(), 0.0) / typeScores["P-Core"].size();
        double eAvg = accumulate(typeScores["E-Core"].begin(), typeScores["E-Core"].end(), 0.0) / typeScores["E-Core"].size();
        
        SetConsoleColor(13);
        cout << "  P-Core vs E-Core Performance Ratio: " 
             << fixed << setprecision(2) << (pAvg / eAvg) << ":1\n";
        SetConsoleColor(7);
        
        cout << "\n  📊 Performance Advantage: ";
        if (pAvg > eAvg) {
            SetConsoleColor(10);
            cout << "P-Cores are " << fixed << setprecision(1) 
                 << ((pAvg / eAvg - 1) * 100) << "% faster on average\n";
        } else {
            SetConsoleColor(14);
            cout << "E-Cores are " << fixed << setprecision(1) 
                 << ((eAvg / pAvg - 1) * 100) << "% faster on average\n";
        }
        SetConsoleColor(7);
    }
    
    cout << "\n  ────────────────────────────────────────────────────────────────\n";
}

void AnalyzeUserResults() {
    SetConsoleColor(13);
    cout << "\n  ┌────────────────────────────────────────────────────────────────┐\n";
    cout << "  │              ANALYSIS OF YOUR PREVIOUS RESULTS                 │\n";
    cout << "  └────────────────────────────────────────────────────────────────┘\n\n";
    SetConsoleColor(7);
    
    cout << "  🔍 Key Issues Identified:\n\n";
    
    SetConsoleColor(12);
    cout << "  ❌ CRITICAL: P-Cores showing MUCH lower performance than E-Cores\n";
    SetConsoleColor(7);
    cout << "     • E-Core average: ~48 MOps/s\n";
    cout << "     • P-Core average: ~24 MOps/s\n";
    cout << "     • Expected: P-Cores should be 50-100% FASTER, not slower!\n\n";
    
    cout << "  📉 Possible Root Causes:\n";
    cout << "     1. CPU Throttling: P-Cores may be thermally throttled\n";
    cout << "     2. Power Limits: TDP/PL1/PL2 limits restricting P-Core boost\n";
    cout << "     3. Windows Power Plan: Not set to High Performance\n";
    cout << "     4. Background Load: Something using P-Cores during test\n";
    cout << "     5. Thread Affinity Issue: Test may not be pinning correctly\n\n";
    
    cout << "  💡 Recommendations:\n";
    SetConsoleColor(10);
    cout << "     ✓ Check CPU temperature (should be < 90°C)\n";
    cout << "     ✓ Use Intel XTU or ThrottleStop to monitor throttling\n";
    cout << "     ✓ Verify power limits in BIOS (PL1/PL2 should be max)\n";
    cout << "     ✓ Close ALL applications including background services\n";
    cout << "     ✓ Run this new test which uses proper thread affinity\n";
    SetConsoleColor(7);
    
    cout << "\n  ────────────────────────────────────────────────────────────────\n\n";
}

int main() {
    system("chcp 65001 > nul");
    
    PrintBanner();
    AnalyzeUserResults();
    PrintWarning();
    
    SetConsoleColor(11);
    cout << "  ┌────────────────────────────────────────────────────────────────┐\n";
    cout << "  │              INITIALIZING BENCHMARK SUITE                      │\n";
    cout << "  └────────────────────────────────────────────────────────────────┘\n\n";
    SetConsoleColor(7);
    
    cout << "  Detecting CPU cores...";
    vector<CoreInfo> cores = DetectCPUCores();
    
    if (cores.empty()) {
        SetConsoleColor(12);
        cout << " FAILED!\n\n";
        cout << "  Error: Unable to detect CPU cores.\n";
        SetConsoleColor(7);
        system("pause");
        return 1;
    }
    
    SetConsoleColor(10);
    cout << " DONE!\n\n";
    SetConsoleColor(7);
    
    PrintCoreDetection(cores);
    
    SetConsoleColor(11);
    cout << "  ┌────────────────────────────────────────────────────────────────┐\n";
    cout << "  │         RUNNING COMPREHENSIVE STRESS TEST (ALL CORES)          │\n";
    cout << "  └────────────────────────────────────────────────────────────────┘\n\n";
    SetConsoleColor(7);
    
    cout << "  Test Duration: ~15 seconds per test type\n";
    cout << "  Tests: Math, Memory, Branch, Cache, Mixed Workload\n\n";
    
    int testDuration = 3000; // 3 seconds per test
    completedCores = 0;
    vector<thread> threads;
    
    SetConsoleColor(14);
    cout << "  🚀 Starting parallel benchmark on all " << cores.size() << " cores...\n\n";
    SetConsoleColor(7);
    
    allTestsRunning = true;
    
    // Launch all tests in parallel
    for (auto& core : cores) {
        threads.emplace_back([&core, testDuration]() {
            RunComprehensiveBenchmark(core, testDuration);
        });
    }
    
    // Monitor progress
    while (completedCores < (int)cores.size()) {
        PrintProgressBar(completedCores.load(), cores.size(), "Overall Progress");
        this_thread::sleep_for(chrono::milliseconds(100));
    }
    
    // Wait for all threads
    for (auto& t : threads) {
        t.join();
    }
    
    PrintProgressBar(cores.size(), cores.size(), "Overall Progress");
    cout << "\n\n";
    
    SetConsoleColor(10);
    cout << "  ✓ All benchmark tests completed successfully!\n";
    SetConsoleColor(7);
    
    PrintDetailedResults(cores);
    
    SetConsoleColor(10);
    cout << "\n  Benchmark completed successfully!\n";
    SetConsoleColor(7);
    cout << "\n  ";
    system("pause");
    
    return 0;
}
