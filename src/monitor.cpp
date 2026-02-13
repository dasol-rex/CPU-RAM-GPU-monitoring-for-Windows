#include "monitor.h"
#include <iostream>
#include <thread>
#include <chrono>
#include <sstream>
#include <iomanip>
#include <psapi.h> // 윈도우 프로세스 정보용

Monitor::Monitor(int pid, const std::string& logBaseName)
    : pid_(pid), logBaseName_(logBaseName), logger_(logBaseName) {
}

Monitor::~Monitor() {
    stop();
}

bool Monitor::init() {
    // 1. 프로세스 존재 확인 (윈도우 방식)
    HANDLE hProcess = OpenProcess(PROCESS_QUERY_INFORMATION, FALSE, pid_);
    if (hProcess == NULL) {
        std::cerr << "[Error] PID " << pid_ << " is not accessible or not running.\n";
        return false;
    }

    // 2. PID 재사용 확인용 시작 시간 가져오기
    FILETIME createTime, exitTime, kernelTime, userTime;
    if (GetProcessTimes(hProcess, &createTime, &exitTime, &kernelTime, &userTime)) {
        base_start_time_ = ((unsigned long long)createTime.dwHighDateTime << 32) | createTime.dwLowDateTime;
    }
    CloseHandle(hProcess);

    // PDH 시작
    if (!gpu_util_.start(pid_)) {
        std::cerr << "[Warning] Failed to start PDH GPU monitoring.\n";
    }
    return true;
}

void Monitor::run() {
    keep_running_ = true;
    std::cout << "Windows Monitoring started for PID [" << pid_ << "].\n";

    while (keep_running_) {
        std::this_thread::sleep_for(std::chrono::seconds(1));
        if (!keep_running_) break;

        if (!checkValidity()) {
            keep_running_ = false;
            break;
        }
        collectAndLog();
    }
}

bool Monitor::checkValidity() {
    HANDLE hProcess = OpenProcess(PROCESS_QUERY_INFORMATION, FALSE, pid_);
    if (hProcess == NULL) {
        if (!termination_logged_) {
            logger_.logLine("[PID:" + std::to_string(pid_) + "] Process terminated.");
            std::cout << "Process terminated.\n";
            termination_logged_ = true;
        }
        return false;
    }

    FILETIME ct, et, kt, ut;
    GetProcessTimes(hProcess, &ct, &et, &kt, &ut);
    unsigned long long current_start = ((unsigned long long)ct.dwHighDateTime << 32) | ct.dwLowDateTime;
    CloseHandle(hProcess);

    if (current_start != base_start_time_) {
        if (!reused_logged_) {
            logger_.logLine("[PID:" + std::to_string(pid_) + "] PID reused by another process.");
            reused_logged_ = true;
        }
        return false;
    }
    return true;
}

void Monitor::collectAndLog() {
    double cpu = logger_.getProcessCPUUsage(pid_);
    long procRam = logger_.getProcessMemoryUsage(pid_);
    
    // PDH를 통한 GPU 사용률 수집 (100ms 정도 짧게 샘플링)
    auto gpuRes = gpu_util_.sampleOnce(200);

    std::stringstream ss;
    ss << "[PID:" << pid_ << "] CPU: " << cpu << "%, RAM: " << procRam 
       << "MB, GPU: " << std::fixed << std::setprecision(1) << gpuRes.value_or(0.0) << "%";
    
    logger_.logLine(ss.str());
    std::cout << ss.str() << "\r" << std::flush;
}

void Monitor::stop() {
    keep_running_ = false;
}