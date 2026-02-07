#include <windows.h>
#include <powrprof.h>
#include <iostream>
#include <vector>
#include <thread>
#include <chrono>
#include <iomanip>
#include <string>
#include <sstream>
#include <cmath>
#include <algorithm>

#pragma comment(lib, "powrprof.lib")

using namespace std;

struct CoreInfo {
    int id;
    int logicalCore;
    int physicalCore;
    string type; // "P-Core" or "E-Core"
    int efficiencyClass;
    double score;
    double frequency;
    double temperature;
    DWORD_PTR affinityMask;
};

void SetConsoleColor(int color) {
    SetConsoleTextAttribute(GetStdHandle(STD_OUTPUT_HANDLE), color);
}

void PrintProgressBar(int percentage, int barWidth = 50) {
    cout << "\r[";
    int pos = barWidth * percentage / 100;
    for (int i = 0; i < barWidth; ++i) {
        if (i < pos) cout << "=";
        else if (i == pos) cout << ">";
        else cout << " ";
    }
    cout << "] " << percentage << "% " << flush;
}

void ClearProgressBar() {
    cout << "\r" << string(70, ' ') << "\r";
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
                    core.type = (current->Processor.EfficiencyClass == 0) ? "P-Core" : "E-Core";
                    core.affinityMask = (DWORD_PTR)1 << coreId;
                    core.score = 0;
                    core.frequency = 0;
                    core.temperature = 0;
                    
                    cores.push_back(core);
                    coreId++;
                }
            }
        }
        offset += current->Size;
    }
    
    return cores;
}

double PerformStressTest(DWORD_PTR affinityMask, int duration_ms) {
    SetThreadAffinityMask(GetCurrentThread(), affinityMask);
    
    auto start = chrono::high_resolution_clock::now();
    auto end = start + chrono::milliseconds(duration_ms);
    
    volatile double result = 0;
    unsigned long long operations = 0;
    
    while (chrono::high_resolution_clock::now() < end) {
        for (int i = 0; i < 1000; i++) {
            result += sqrt(i * 3.14159265359);
            result *= 1.00001;
            result = sin(result) * cos(result);
            operations++;
        }
    }
    
    auto actual_end = chrono::high_resolution_clock::now();
    double elapsed = chrono::duration<double>(actual_end - start).count();
    
    return operations / elapsed;
}

void RunBenchmark(CoreInfo& core, int testDuration) {
    HANDLE hThread = GetCurrentThread();
    DWORD_PTR oldMask = SetThreadAffinityMask(hThread, core.affinityMask);
    
    core.score = PerformStressTest(core.affinityMask, testDuration);
    
    SetThreadAffinityMask(hThread, oldMask);
}

void PrintBanner() {
    SetConsoleColor(11);
    cout << "\n";
    cout << "  ╔════════════════════════════════════════════════════════════╗\n";
    cout << "  ║         CPU CORE BENCHMARK & STRESS TEST UTILITY          ║\n";
    cout << "  ║                    Windows Edition                         ║\n";
    cout << "  ╚════════════════════════════════════════════════════════════╝\n";
    SetConsoleColor(7);
    cout << "\n";
}

void PrintWarning() {
    SetConsoleColor(14);
    cout << "  ┌────────────────────────────────────────────────────────────┐\n";
    cout << "  │                    ⚠️  IMPORTANT NOTICE                     │\n";
    cout << "  └────────────────────────────────────────────────────────────┘\n\n";
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
    cout << "  ⚠️  WARNING: This test will stress your CPU to maximum capacity!\n";
    SetConsoleColor(7);
    
    cout << "\n  ──────────────────────────────────────────────────────────────\n";
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
    cout << "  ┌────────────────────────────────────────────────────────────┐\n";
    cout << "  │              CPU CORE DETECTION RESULTS                    │\n";
    cout << "  └────────────────────────────────────────────────────────────┘\n\n";
    SetConsoleColor(7);
    
    int pCoreCount = 0, eCoreCount = 0;
    for (const auto& core : cores) {
        if (core.type == "P-Core") pCoreCount++;
        else eCoreCount++;
    }
    
    cout << "  Total Logical Cores: " << cores.size() << "\n";
    cout << "  Performance Cores (P-Cores): " << pCoreCount << "\n";
    cout << "  Efficiency Cores (E-Cores): " << eCoreCount << "\n\n";
    
    cout << "  ┌─────┬───────────┬──────────┬───────────────┬─────────────┐\n";
    cout << "  │ ID  │   Type    │ Physical │ Efficiency Cl │  Affinity   │\n";
    cout << "  ├─────┼───────────┼──────────┼───────────────┼─────────────┤\n";
    
    for (const auto& core : cores) {
        cout << "  │ " << setw(3) << core.id << " │ ";
        
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
             << " │ " << setw(13) << core.efficiencyClass
             << " │ 0x" << setw(8) << hex << core.affinityMask << dec << " │\n";
    }
    
    cout << "  └─────┴───────────┴──────────┴───────────────┴─────────────┘\n\n";
}

void PrintBenchmarkResults(vector<CoreInfo>& cores) {
    SetConsoleColor(11);
    cout << "\n  ┌────────────────────────────────────────────────────────────┐\n";
    cout << "  │              BENCHMARK RESULTS SUMMARY                     │\n";
    cout << "  └────────────────────────────────────────────────────────────┘\n\n";
    SetConsoleColor(7);
    
    sort(cores.begin(), cores.end(), 
         [](const CoreInfo& a, const CoreInfo& b) { return a.score > b.score; });
    
    double maxScore = cores[0].score;
    
    cout << "  ┌─────┬───────────┬──────────────┬──────────────┬──────────┐\n";
    cout << "  │ ID  │   Type    │  Operations  │ Rel. Perf.   │   Rank   │\n";
    cout << "  ├─────┼───────────┼──────────────┼──────────────┼──────────┤\n";
    
    for (size_t i = 0; i < cores.size(); i++) {
        cout << "  │ " << setw(3) << cores[i].id << " │ ";
        
        if (cores[i].type == "P-Core") {
            SetConsoleColor(10);
            cout << setw(9) << cores[i].type;
            SetConsoleColor(7);
        } else {
            SetConsoleColor(14);
            cout << setw(9) << cores[i].type;
            SetConsoleColor(7);
        }
        
        cout << " │ " << setw(12) << fixed << setprecision(2) << cores[i].score / 1e6 << " │ ";
        
        double relPerf = (cores[i].score / maxScore) * 100.0;
        cout << setw(11) << fixed << setprecision(1) << relPerf << "% │ ";
        
        if (i < 3) {
            SetConsoleColor(14);
        }
        cout << setw(8) << (i + 1);
        SetConsoleColor(7);
        cout << " │\n";
    }
    
    cout << "  └─────┴───────────┴──────────────┴──────────────┴──────────┘\n\n";
    
    // Statistics
    double pCoreAvg = 0, eCoreAvg = 0;
    int pCount = 0, eCount = 0;
    
    for (const auto& core : cores) {
        if (core.type == "P-Core") {
            pCoreAvg += core.score;
            pCount++;
        } else {
            eCoreAvg += core.score;
            eCount++;
        }
    }
    
    if (pCount > 0) pCoreAvg /= pCount;
    if (eCount > 0) eCoreAvg /= eCount;
    
    SetConsoleColor(11);
    cout << "  ┌────────────────────────────────────────────────────────────┐\n";
    cout << "  │                  STATISTICAL ANALYSIS                      │\n";
    cout << "  └────────────────────────────────────────────────────────────┘\n\n";
    SetConsoleColor(7);
    
    cout << "  Best Performing Core:   Core #" << cores[0].id 
         << " (" << cores[0].type << ") - " 
         << fixed << setprecision(2) << cores[0].score / 1e6 << " MOps/s\n";
    
    cout << "  Worst Performing Core:  Core #" << cores.back().id 
         << " (" << cores.back().type << ") - " 
         << fixed << setprecision(2) << cores.back().score / 1e6 << " MOps/s\n\n";
    
    if (pCount > 0) {
        SetConsoleColor(10);
        cout << "  P-Core Average:         " 
             << fixed << setprecision(2) << pCoreAvg / 1e6 << " MOps/s\n";
        SetConsoleColor(7);
    }
    
    if (eCount > 0) {
        SetConsoleColor(14);
        cout << "  E-Core Average:         " 
             << fixed << setprecision(2) << eCoreAvg / 1e6 << " MOps/s\n";
        SetConsoleColor(7);
    }
    
    if (pCount > 0 && eCount > 0) {
        double ratio = pCoreAvg / eCoreAvg;
        cout << "\n  P-Core vs E-Core Ratio: " 
             << fixed << setprecision(2) << ratio << ":1\n";
    }
    
    cout << "\n  ──────────────────────────────────────────────────────────────\n";
}

int main() {
    system("chcp 65001 > nul");
    
    PrintBanner();
    PrintWarning();
    
    SetConsoleColor(11);
    cout << "  ┌────────────────────────────────────────────────────────────┐\n";
    cout << "  │              INITIALIZING BENCHMARK SUITE                  │\n";
    cout << "  └────────────────────────────────────────────────────────────┘\n\n";
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
    cout << "  ┌────────────────────────────────────────────────────────────┐\n";
    cout << "  │              STARTING STRESS TEST & BENCHMARK              │\n";
    cout << "  └────────────────────────────────────────────────────────────┘\n\n";
    SetConsoleColor(7);
    
    int testDuration = 3000; // 3 seconds per core
    
    for (size_t i = 0; i < cores.size(); i++) {
        cout << "  Testing Core #" << cores[i].id 
             << " (" << cores[i].type << ")...\n  ";
        
        for (int progress = 0; progress <= 100; progress += 5) {
            PrintProgressBar(progress);
            if (progress < 100) {
                RunBenchmark(cores[i], testDuration / 20);
            }
        }
        
        RunBenchmark(cores[i], testDuration);
        
        ClearProgressBar();
        SetConsoleColor(10);
        cout << "  ✓ Core #" << cores[i].id << " completed - " 
             << fixed << setprecision(2) << cores[i].score / 1e6 << " MOps/s\n";
        SetConsoleColor(7);
    }
    
    PrintBenchmarkResults(cores);
    
    SetConsoleColor(10);
    cout << "\n  Benchmark completed successfully!\n";
    SetConsoleColor(7);
    cout << "\n  ";
    system("pause");
    
    return 0;
}
