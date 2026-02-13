// gpu_reader.cpp
#include "gpu_reader.h"
#include <thread>
#include <chrono>
#include <iostream>

std::wstring GpuPidUtilPDH::toW(const std::string& s) {
    if (s.empty()) return L"";
    int len = MultiByteToWideChar(CP_UTF8, 0, s.c_str(), (int)s.size(), nullptr, 0);
    std::wstring w(len, L'\0');
    MultiByteToWideChar(CP_UTF8, 0, s.c_str(), (int)s.size(), w.data(), len);
    return w;
}

bool GpuPidUtilPDH::start(DWORD pid) {
    stop();
    pid_ = pid;

    PDH_STATUS st = PdhOpenQueryW(nullptr, 0, &query_);
    if (st != ERROR_SUCCESS) return false;

    if (!buildCountersForPid()) {
        stop();
        return false;
    }

    // 첫 수집(베이스라인)
    PdhCollectQueryData(query_);
    return true;
}

void GpuPidUtilPDH::stop() {
    counters_.clear();
    if (query_) {
        PdhCloseQuery(query_);
        query_ = nullptr;
    }
    pid_ = 0;
}

bool GpuPidUtilPDH::buildCountersForPid() {
    // GPU Engine 오브젝트의 인스턴스 목록 가져오기
    DWORD instSize = 0, cntSize = 0;
    PDH_STATUS st = PdhEnumObjectItemsW(
        nullptr, nullptr,
        L"GPU Engine",
        nullptr, &cntSize,
        nullptr, &instSize,
        PERF_DETAIL_WIZARD,
        0
    );

    if (st != PDH_MORE_DATA) return false;

    std::vector<wchar_t> instBuf(instSize);
    std::vector<wchar_t> cntBuf(cntSize);

    st = PdhEnumObjectItemsW(
        nullptr, nullptr,
        L"GPU Engine",
        cntBuf.data(), &cntSize,
        instBuf.data(), &instSize,
        PERF_DETAIL_WIZARD,
        0
    );
    if (st != ERROR_SUCCESS) return false;

    // 인스턴스 문자열은 MULTI_SZ (널로 구분, 마지막은 더블널)
    std::wstring pidTag = L"pid_" + std::to_wstring(pid_);

    for (wchar_t* p = instBuf.data(); *p; p += wcslen(p) + 1) {
        std::wstring instName(p);

        // 인스턴스에 pid_#### 가 포함된 것만 선택
        if (instName.find(pidTag) == std::wstring::npos) continue;
        std::wcout << L"Found GPU instance: " << instName << std::endl;

        std::wstring path = L"\\GPU Engine(" + instName + L")\\Utilization Percentage";

        PDH_HCOUNTER c = nullptr;
        st = PdhAddCounterW(query_, path.c_str(), 0, &c);
        if (st == ERROR_SUCCESS && c) {
            counters_.push_back(c);
        }
    }
    

    return !counters_.empty();
}

std::optional<double> GpuPidUtilPDH::sampleOnce(int interval_ms) {
    if (!query_ || counters_.empty()) return std::nullopt;

    // 1. 첫 번째 수집 (Baseline)
    PdhCollectQueryData(query_);

    // 2. 대기 (정확한 샘플링을 위해 지정된 시간만큼 대기)
    std::this_thread::sleep_for(std::chrono::milliseconds(interval_ms));

    // 3. 두 번째 수집
    PDH_STATUS st = PdhCollectQueryData(query_);
    if (st != ERROR_SUCCESS) return std::nullopt;

    double sum = 0.0;
    for (auto c : counters_) {
        PDH_FMT_COUNTERVALUE v{};
        // PDH_FMT_NOSCALE을 쓰지 않고 PDH_FMT_DOUBLE을 사용하여 퍼센트 값을 직접 가져옵니다.
        st = PdhGetFormattedCounterValue(c, PDH_FMT_DOUBLE, nullptr, &v);
        if (st == ERROR_SUCCESS && (v.CStatus == PDH_CSTATUS_VALID_DATA || v.CStatus == ERROR_SUCCESS)) {
            sum += v.doubleValue;
        }
    }

    // GPU 엔진 합산이므로 여러 엔진(3D, Copy 등)이 동시에 돌면 100%를 넘을 수 있습니다.
    return sum;
}
