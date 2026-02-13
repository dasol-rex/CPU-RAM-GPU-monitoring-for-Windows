#ifndef MONITOR_H
#define MONITOR_H

#include "Logger.h"
#include <string>
#include <atomic>
#include <windows.h> 
#include "gpu_reader.h" 

class Monitor {
public:
    Monitor(int pid, const std::string& logBaseName);
    ~Monitor();

    bool init();
    void run();
    void stop();

private:
    bool checkValidity();
    void collectAndLog();

    int pid_;
    unsigned long long base_start_time_; // 윈도우 시간 포맷에 맞게 변경
    std::string logBaseName_;
    
    Logger logger_;
    // GpuReader gpu_; // 윈도우에선 삭제하거나 NVML로 대체
    
    std::atomic<bool> keep_running_{false};
    bool termination_logged_ = false;
    bool reused_logged_ = false;

    GpuPidUtilPDH gpu_util_; // NVML 대신 PDH 유틸 사용
};

#endif