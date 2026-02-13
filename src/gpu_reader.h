// gpu_pdh_util.h
#pragma once
#ifndef UNICODE
#define UNICODE
#endif

#include <windows.h>
#include <pdh.h>
#include <pdhmsg.h> 
#include <vector>
#include <string>
#include <optional>

#pragma comment(lib, "pdh.lib")

class GpuPidUtilPDH {
public:
    bool start(DWORD pid);
    void stop();
    // interval_ms 동안 샘플링 후 현재 PID GPU util(%) 반환
    // (엔진 합산이므로 100을 넘을 수 있음)
    std::optional<double> sampleOnce(int interval_ms);

private:
    DWORD pid_ = 0;
    PDH_HQUERY query_ = nullptr;
    std::vector<PDH_HCOUNTER> counters_;

    bool buildCountersForPid();
    static std::wstring toW(const std::string& s);
};
