#ifndef LOGGER_H
#define LOGGER_H

#include <string>
#include <fstream>
#include <windows.h> // 윈도우 API용

class Logger {
public:
    Logger(const std::string& baseName);
    ~Logger();

    void logLine(const std::string& text);
    
    // 에러 해결을 위해 반드시 필요한 멤버 함수들
    double getProcessCPUUsage(int pid);
    long getProcessMemoryUsage(int pid); // MB 단위 반환
    long getMemoryUsage();               // 시스템 전체 RAM 사용량(MB)
    unsigned long long getProcessStartTime(int pid);

private:
    HANDLE hProcess_ = nullptr;
    int numProcessors_ = -1;
    ULARGE_INTEGER lastSysTime_;    // 시스템 전체 시간 저장
    unsigned long long lastProcessTime_; // 프로세스 사용 시간 저장
    std::string baseName_;          // 로그 파일 기본 이름
    std::string lastLogDate_;       // 마지막으로 기록한 날짜 확인용
    std::ofstream logFile_;
};

#endif