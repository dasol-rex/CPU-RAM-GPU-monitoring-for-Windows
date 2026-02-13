#include "Logger.h"
#include <psapi.h>
#include <iostream>
#include <chrono>
#include <iomanip>
#include <sstream>
#include <ctime>

Logger::Logger(const std::string& baseName) : baseName_(baseName) {
    // numProcessors_ 초기화 확인 필요 (생성자나 헤더에서 -1로 초기화)
    SYSTEM_INFO sysInfo;
    GetSystemInfo(&sysInfo);
    numProcessors_ = sysInfo.dwNumberOfProcessors;
}

Logger::~Logger() {
    if (hProcess_) CloseHandle(hProcess_);
    if (logFile_.is_open()) logFile_.close();
}

// 매번 호출될 때 날짜를 확인해서 파일을 새로 열거나 유지하는 함수
void Logger::logLine(const std::string& text) {
    // 1. 현재 날짜 가져오기 (YYYY-MM-DD)
    auto now = std::chrono::system_clock::to_time_t(std::chrono::system_clock::now());
    struct tm tstruct;
    localtime_s(&tstruct, &now);
    char buf[80];
    strftime(buf, sizeof(buf), "%Y-%m-%d", &tstruct);
    std::string currentDate(buf);

    // 2. 날짜가 바뀌었으면 파일 새로 열기
    if (currentDate != lastLogDate_) {
        if (logFile_.is_open()) logFile_.close();
        lastLogDate_ = currentDate;
        std::string fileName = baseName_ + "_" + currentDate + ".log";
        logFile_.open(fileName, std::ios::app);
    }

    if (logFile_.is_open()) {
        logFile_ << text << std::endl;
    }
}

// CPU 사용률 계산 (오류 수정 버전)
double Logger::getProcessCPUUsage(int pid) {
    if (!hProcess_) {
        hProcess_ = OpenProcess(PROCESS_QUERY_INFORMATION | PROCESS_VM_READ, FALSE, pid);
        if (!hProcess_) return 0.0;
        
        // 첫 호출 시 베이스라인 설정
        GetSystemTimeAsFileTime((FILETIME*)&lastSysTime_);
        FILETIME ftCreate, ftExit, ftKernel, ftUser;
        GetProcessTimes(hProcess_, &ftCreate, &ftExit, &ftKernel, &ftUser);
        lastProcessTime_ = ((unsigned long long)ftKernel.dwHighDateTime << 32) | ftKernel.dwLowDateTime;
        lastProcessTime_ += ((unsigned long long)ftUser.dwHighDateTime << 32) | ftUser.dwLowDateTime;
        return 0.0; // 첫 호출은 비교 대상이 없어 0 반환
    }

    // 현재 시간 수집
    ULARGE_INTEGER nowSys;
    GetSystemTimeAsFileTime((FILETIME*)&nowSys);

    FILETIME ftCreate, ftExit, ftKernel, ftUser;
    if (!GetProcessTimes(hProcess_, &ftCreate, &ftExit, &ftKernel, &ftUser)) return 0.0;
    
    unsigned long long nowProcess = ((unsigned long long)ftKernel.dwHighDateTime << 32) | ftKernel.dwLowDateTime;
    nowProcess += ((unsigned long long)ftUser.dwHighDateTime << 32) | ftUser.dwLowDateTime;

    // 경과 시간 계산
    unsigned long long sysDiff = nowSys.QuadPart - lastSysTime_.QuadPart;
    unsigned long long procDiff = nowProcess - lastProcessTime_;

    double percent = 0.0;
    if (sysDiff > 0) {
        // (프로세스 사용 시간 / 시스템 경과 시간) / 코어 수 * 100
        percent = (double)(procDiff * 100) / sysDiff / numProcessors_;
    }

    lastSysTime_ = nowSys;
    lastProcessTime_ = nowProcess;

    return percent;
}

// 특정 프로세스 RAM 사용량 (MB) 
long Logger::getProcessMemoryUsage(int pid) {
    // 이미 hProcess_가 있다면 그것을 쓰고, 없다면 새로 연다.
    HANDLE h = hProcess_;
    bool closeNeeded = false;
    
    if (!h) {
        h = OpenProcess(PROCESS_QUERY_INFORMATION | PROCESS_VM_READ, FALSE, pid);
        if (!h) return 0;
        closeNeeded = true;
    }

    PROCESS_MEMORY_COUNTERS pmc;
    long memMB = 0;
    if (GetProcessMemoryInfo(h, &pmc, sizeof(pmc))) {
        // 단위를 Byte에서 MB로 변환
        memMB = (long)(pmc.WorkingSetSize / (1024 * 1024));
    }

    if (closeNeeded) CloseHandle(h);
    return memMB;
}

// 시스템 전체 RAM 사용량 (MB) 
long Logger::getMemoryUsage() {
    MEMORYSTATUSEX memInfo;
    memInfo.dwLength = sizeof(MEMORYSTATUSEX);
    if (GlobalMemoryStatusEx(&memInfo)) {
        return (long)((memInfo.ullTotalPhys - memInfo.ullAvailPhys) / (1024 * 1024));
    }
    return 0;
}