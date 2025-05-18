/*
 * linux.cpp
 *
 * Copyright (C) 2023-2024 Max Qian <lightapt.com>
 */

/*************************************************

Date: 2024-3-4

Description: System Information Module - CPU Linux Implementation

**************************************************/

#if defined(__linux__) || defined(__ANDROID__)

#include "common.hpp"

namespace atom::system {

// 添加Linux特定函数前向声明
auto getCurrentCpuUsage_Linux() -> float;
auto getPerCoreCpuUsage_Linux() -> std::vector<float>;
auto getCurrentCpuTemperature_Linux() -> float;
auto getPerCoreCpuTemperature_Linux() -> std::vector<float>;
auto getCPUModel_Linux() -> std::string;
// 这里应该添加所有函数的前向声明

auto getCurrentCpuUsage_Linux() -> float {
    LOG_F(INFO, "Starting getCurrentCpuUsage function on Linux");
    
    static std::mutex mutex;
    static unsigned long long lastTotalUser = 0, lastTotalUserLow = 0;
    static unsigned long long lastTotalSys = 0, lastTotalIdle = 0;
    
    float cpuUsage = 0.0;
    
    std::unique_lock<std::mutex> lock(mutex);
    
    std::ifstream statFile("/proc/stat");
    if (statFile.is_open()) {
        std::string line;
        if (std::getline(statFile, line)) {
            std::istringstream ss(line);
            std::string cpu;
            
            unsigned long long user, nice, system, idle, iowait, irq, softirq, steal;
            ss >> cpu >> user >> nice >> system >> idle >> iowait >> irq >> softirq >> steal;
            
            if (cpu == "cpu") {
                unsigned long long totalUser = user + nice;
                unsigned long long totalUserLow = user + nice;
                unsigned long long totalSys = system + irq + softirq;
                unsigned long long totalIdle = idle + iowait;
                
                // Calculate the total CPU time
                unsigned long long total = totalUser + totalSys + totalIdle + steal;
                
                // Calculate the delta between current and last measurement
                if (lastTotalUser > 0 || lastTotalUserLow > 0 || lastTotalSys > 0 || lastTotalIdle > 0) {
                    unsigned long long totalDelta = total - (lastTotalUser + lastTotalUserLow + lastTotalSys + lastTotalIdle);
                    
                    if (totalDelta > 0) {
                        unsigned long long idleDelta = totalIdle - lastTotalIdle;
                        cpuUsage = 100.0f * (1.0f - static_cast<float>(idleDelta) / static_cast<float>(totalDelta));
                    }
                }
                
                // Store the current values for the next calculation
                lastTotalUser = totalUser;
                lastTotalUserLow = totalUserLow;
                lastTotalSys = totalSys;
                lastTotalIdle = totalIdle;
            }
        }
    }
    
    // Clamp to 0-100 range
    cpuUsage = std::max(0.0f, std::min(100.0f, cpuUsage));
    
    LOG_F(INFO, "Linux CPU Usage: {}%", cpuUsage);
    return cpuUsage;
}

auto getPerCoreCpuUsage() -> std::vector<float> {
    LOG_F(INFO, "Starting getPerCoreCpuUsage function on Linux");
    
    static std::mutex mutex;
    static std::vector<unsigned long long> lastTotalUser;
    static std::vector<unsigned long long> lastTotalUserLow;
    static std::vector<unsigned long long> lastTotalSys;
    static std::vector<unsigned long long> lastTotalIdle;
    
    std::vector<float> coreUsages;
    
    std::unique_lock<std::mutex> lock(mutex);
    
    std::ifstream statFile("/proc/stat");
    if (statFile.is_open()) {
        std::string line;
        
        // Skip the first line (overall CPU usage)
        std::getline(statFile, line);
        
        int coreIndex = 0;
        while (std::getline(statFile, line)) {
            if (line.compare(0, 3, "cpu") != 0) {
                break;  // We've processed all CPU entries
            }
            
            std::istringstream ss(line);
            std::string cpu;
            unsigned long long user, nice, system, idle, iowait, irq, softirq, steal;
            
            ss >> cpu >> user >> nice >> system >> idle >> iowait >> irq >> softirq >> steal;
            
            // Resize vectors if needed
            if (coreIndex >= static_cast<int>(lastTotalUser.size())) {
                lastTotalUser.resize(coreIndex + 1, 0);
                lastTotalUserLow.resize(coreIndex + 1, 0);
                lastTotalSys.resize(coreIndex + 1, 0);
                lastTotalIdle.resize(coreIndex + 1, 0);
            }
            
            unsigned long long totalUser = user + nice;
            unsigned long long totalUserLow = user + nice;
            unsigned long long totalSys = system + irq + softirq;
            unsigned long long totalIdle = idle + iowait;
            
            // Calculate the total CPU time
            unsigned long long total = totalUser + totalSys + totalIdle + steal;
            
            float coreUsage = 0.0f;
            
            // Calculate the delta between current and last measurement
            if (lastTotalUser[coreIndex] > 0 || lastTotalUserLow[coreIndex] > 0 || 
                lastTotalSys[coreIndex] > 0 || lastTotalIdle[coreIndex] > 0) {
                unsigned long long totalDelta = total - (lastTotalUser[coreIndex] + lastTotalUserLow[coreIndex] + 
                                                       lastTotalSys[coreIndex] + lastTotalIdle[coreIndex]);
                
                if (totalDelta > 0) {
                    unsigned long long idleDelta = totalIdle - lastTotalIdle[coreIndex];
                    coreUsage = 100.0f * (1.0f - static_cast<float>(idleDelta) / static_cast<float>(totalDelta));
                }
            }
            
            // Store the current values for the next calculation
            lastTotalUser[coreIndex] = totalUser;
            lastTotalUserLow[coreIndex] = totalUserLow;
            lastTotalSys[coreIndex] = totalSys;
            lastTotalIdle[coreIndex] = totalIdle;
            
            // Clamp to 0-100 range
            coreUsage = std::max(0.0f, std::min(100.0f, coreUsage));
            coreUsages.push_back(coreUsage);
            
            coreIndex++;
        }
    }
    
    LOG_F(INFO, "Linux Per-Core CPU Usage collected for {} cores", coreUsages.size());
    return coreUsages;
}

auto getCurrentCpuTemperature() -> float {
    LOG_F(INFO, "Starting getCurrentCpuTemperature function on Linux");
    
    float temperature = 0.0f;
    bool found = false;
    
    // Check common thermal zone paths
    for (int i = 0; i < 10 && !found; i++) {
        std::string path = "/sys/class/thermal/thermal_zone" + std::to_string(i) + "/temp";
        std::ifstream tempFile(path);
        
        if (tempFile.is_open()) {
            std::string line;
            if (std::getline(tempFile, line)) {
                try {
                    // Temperature is often reported in millidegrees Celsius
                    temperature = static_cast<float>(std::stoi(line)) / 1000.0f;
                    found = true;
                    LOG_F(INFO, "Found CPU temperature from {}: {}°C", path, temperature);
                } catch (const std::exception& e) {
                    LOG_F(WARNING, "Error parsing temperature from {}: {}", path, e.what());
                }
            }
        }
    }
    
    // Check for CPU temperature in hwmon
    if (!found) {
        std::string hwmonDir = "/sys/class/hwmon/";
        
        for (int i = 0; i < 10 && !found; i++) {
            std::string hwmonPath = hwmonDir + "hwmon" + std::to_string(i) + "/";
            
            // Check if this is a CPU temperature sensor
            std::ifstream nameFile(hwmonPath + "name");
            if (nameFile.is_open()) {
                std::string name;
                if (std::getline(nameFile, name)) {
                    // Common CPU temperature sensor names
                    if (name.find("coretemp") != std::string::npos || 
                        name.find("k10temp") != std::string::npos ||
                        name.find("cpu_thermal") != std::string::npos) {
                        
                        // Try to read the temperature
                        for (int j = 1; j < 5 && !found; j++) {
                            std::string tempPath = hwmonPath + "temp" + std::to_string(j) + "_input";
                            std::ifstream tempFile(tempPath);
                            
                            if (tempFile.is_open()) {
                                std::string line;
                                if (std::getline(tempFile, line)) {
                                    try {
                                        // Temperature is often reported in millidegrees Celsius
                                        temperature = static_cast<float>(std::stoi(line)) / 1000.0f;
                                        found = true;
                                        LOG_F(INFO, "Found CPU temperature from {}: {}°C", tempPath, temperature);
                                    } catch (const std::exception& e) {
                                        LOG_F(WARNING, "Error parsing temperature from {}: {}", tempPath, e.what());
                                    }
                                }
                            }
                        }
                    }
                }
            }
        }
    }
    
    if (!found) {
        LOG_F(WARNING, "Could not find CPU temperature, returning 0");
    }
    
    return temperature;
}

auto getPerCoreCpuTemperature() -> std::vector<float> {
    LOG_F(INFO, "Starting getPerCoreCpuTemperature function on Linux");
    
    std::vector<float> temperatures;
    bool found = false;
    
    // Try to find per-core temperatures in hwmon
    std::string hwmonDir = "/sys/class/hwmon/";
    
    for (int i = 0; i < 10 && !found; i++) {
        std::string hwmonPath = hwmonDir + "hwmon" + std::to_string(i) + "/";
        
        // Check if this is a CPU temperature sensor
        std::ifstream nameFile(hwmonPath + "name");
        if (nameFile.is_open()) {
            std::string name;
            if (std::getline(nameFile, name)) {
                // Common CPU temperature sensor names
                if (name.find("coretemp") != std::string::npos || 
                    name.find("k10temp") != std::string::npos) {
                    
                    // Find all temperature inputs
                    std::vector<std::string> tempPaths;
                    for (int j = 1; j < 32; j++) {
                        std::string labelPath = hwmonPath + "temp" + std::to_string(j) + "_label";
                        std::ifstream labelFile(labelPath);
                        
                        if (labelFile.is_open()) {
                            std::string label;
                            if (std::getline(labelFile, label)) {
                                // Check if this is a core temperature
                                if (label.find("Core") != std::string::npos || 
                                    label.find("CPU") != std::string::npos) {
                                    tempPaths.push_back(hwmonPath + "temp" + std::to_string(j) + "_input");
                                }
                            }
                        }
                    }
                    
                    // Read each core temperature
                    if (!tempPaths.empty()) {
                        found = true;
                        
                        for (const auto& path : tempPaths) {
                            std::ifstream tempFile(path);
                            float temp = 0.0f;
                            
                            if (tempFile.is_open()) {
                                std::string line;
                                if (std::getline(tempFile, line)) {
                                    try {
                                        // Temperature is often reported in millidegrees Celsius
                                        temp = static_cast<float>(std::stoi(line)) / 1000.0f;
                                        LOG_F(INFO, "Found core temperature from {}: {}°C", path, temp);
                                    } catch (const std::exception& e) {
                                        LOG_F(WARNING, "Error parsing temperature from {}: {}", path, e.what());
                                    }
                                }
                            }
                            
                            temperatures.push_back(temp);
                        }
                    }
                }
            }
        }
    }
    
    // If we couldn't find per-core temperatures, fall back to single temperature
    if (!found) {
        float coreTemp = getCurrentCpuTemperature();
        int numCores = getNumberOfLogicalCores();
        
        temperatures.resize(numCores, coreTemp);
        LOG_F(INFO, "Could not find per-core temperatures, using single temperature for all cores: {}°C", coreTemp);
    }
    
    LOG_F(INFO, "Linux Per-Core CPU Temperature collected for {} cores", temperatures.size());
    return temperatures;
}

auto getCPUModel() -> std::string {
    LOG_F(INFO, "Starting getCPUModel function on Linux");
    
    if (!needsCacheRefresh() && !g_cpuInfoCache.model.empty()) {
        return g_cpuInfoCache.model;
    }
    
    std::string cpuModel = "Unknown";
    
    std::ifstream cpuinfo("/proc/cpuinfo");
    if (cpuinfo.is_open()) {
        std::string line;
        while (std::getline(cpuinfo, line)) {
            // Line format varies by architecture
            if (line.find("model name") != std::string::npos || 
                line.find("Processor") != std::string::npos ||
                line.find("cpu model") != std::string::npos) {
                
                size_t pos = line.find(':');
                if (pos != std::string::npos && pos + 2 < line.size()) {
                    cpuModel = line.substr(pos + 2);
                    break;
                }
            }
        }
    }
    
    LOG_F(INFO, "Linux CPU Model: {}", cpuModel);
    return cpuModel;
}

auto getProcessorIdentifier() -> std::string {
    LOG_F(INFO, "Starting getProcessorIdentifier function on Linux");
    
    if (!needsCacheRefresh() && !g_cpuInfoCache.identifier.empty()) {
        return g_cpuInfoCache.identifier;
    }
    
    std::string identifier;
    std::string vendor, family, model, stepping;
    
    std::ifstream cpuinfo("/proc/cpuinfo");
    if (cpuinfo.is_open()) {
        std::string line;
        while (std::getline(cpuinfo, line)) {
            if (line.find("vendor_id") != std::string::npos) {
                size_t pos = line.find(':');
                if (pos != std::string::npos && pos + 2 < line.size()) {
                    vendor = line.substr(pos + 2);
                }
            } else if (line.find("cpu family") != std::string::npos) {
                size_t pos = line.find(':');
                if (pos != std::string::npos && pos + 2 < line.size()) {
                    family = line.substr(pos + 2);
                }
            } else if (line.find("model") != std::string::npos && 
                      line.find("model name") == std::string::npos) {
                size_t pos = line.find(':');
                if (pos != std::string::npos && pos + 2 < line.size()) {
                    model = line.substr(pos + 2);
                }
            } else if (line.find("stepping") != std::string::npos) {
                size_t pos = line.find(':');
                if (pos != std::string::npos && pos + 2 < line.size()) {
                    stepping = line.substr(pos + 2);
                }
            }
        }
    }
    
    // Trim whitespace
    auto trim = [](std::string& s) {
        s.erase(0, s.find_first_not_of(" \t\n\r\f\v"));
        s.erase(s.find_last_not_of(" \t\n\r\f\v") + 1);
    };
    
    trim(vendor);
    trim(family);
    trim(model);
    trim(stepping);
    
    // Format the identifier
    if (!vendor.empty() && !family.empty() && !model.empty() && !stepping.empty()) {
        identifier = vendor + " Family " + family + " Model " + model + " Stepping " + stepping;
    } else {
        identifier = getCPUModel();
    }
    
    LOG_F(INFO, "Linux CPU Identifier: {}", identifier);
    return identifier;
}

auto getProcessorFrequency() -> double {
    LOG_F(INFO, "Starting getProcessorFrequency function on Linux");
    
    double frequency = 0.0;
    
    // Try to read from /proc/cpuinfo
    std::ifstream cpuinfo("/proc/cpuinfo");
    if (cpuinfo.is_open()) {
        std::string line;
        while (std::getline(cpuinfo, line)) {
            if (line.find("cpu MHz") != std::string::npos ||
                line.find("clock") != std::string::npos) {
                
                size_t pos = line.find(':');
                if (pos != std::string::npos && pos + 2 < line.size()) {
                    std::string freqStr = line.substr(pos + 2);
                    try {
                        // Convert MHz to GHz
                        frequency = std::stod(freqStr) / 1000.0;
                        break;
                    } catch (const std::exception& e) {
                        LOG_F(WARNING, "Error parsing CPU frequency: {}", e.what());
                    }
                }
            }
        }
    }
    
    // If we still don't have a frequency, try reading from /sys/devices
    if (frequency <= 0.0) {
        std::ifstream freqFile("/sys/devices/system/cpu/cpu0/cpufreq/scaling_cur_freq");
        if (freqFile.is_open()) {
            std::string line;
            if (std::getline(freqFile, line)) {
                try {
                    // Convert kHz to GHz
                    frequency = std::stod(line) / 1000000.0;
                } catch (const std::exception& e) {
                    LOG_F(WARNING, "Error parsing CPU frequency from scaling_cur_freq: {}", e.what());
                }
            }
        }
    }
    
    LOG_F(INFO, "Linux CPU Frequency: {} GHz", frequency);
    return frequency;
}

auto getMinProcessorFrequency() -> double {
    LOG_F(INFO, "Starting getMinProcessorFrequency function on Linux");
    
    double minFreq = 0.0;
    
    // Try to read from /sys/devices
    std::ifstream freqFile("/sys/devices/system/cpu/cpu0/cpufreq/scaling_min_freq");
    if (freqFile.is_open()) {
        std::string line;
        if (std::getline(freqFile, line)) {
            try {
                // Convert kHz to GHz
                minFreq = std::stod(line) / 1000000.0;
            } catch (const std::exception& e) {
                LOG_F(WARNING, "Error parsing CPU min frequency: {}", e.what());
            }
        }
    }
    
    // Ensure we have a reasonable minimum value
    if (minFreq <= 0.0) {
        // Try to get a reasonable estimate from cpuinfo_min_freq
        std::ifstream cpuinfoMinFreq("/sys/devices/system/cpu/cpu0/cpufreq/cpuinfo_min_freq");
        if (cpuinfoMinFreq.is_open()) {
            std::string line;
            if (std::getline(cpuinfoMinFreq, line)) {
                try {
                    // Convert kHz to GHz
                    minFreq = std::stod(line) / 1000000.0;
                } catch (const std::exception& e) {
                    LOG_F(WARNING, "Error parsing CPU min frequency from cpuinfo_min_freq: {}", e.what());
                }
            }
        }
    }
    
    // If still no valid minimum, use a fraction of the current frequency
    if (minFreq <= 0.0) {
        double currentFreq = getProcessorFrequency();
        if (currentFreq > 0.0) {
            minFreq = currentFreq * 0.5; // Assume minimum is half of current
        } else {
            minFreq = 1.0; // Default to 1 GHz if no other info available
        }
    }
    
    LOG_F(INFO, "Linux CPU Min Frequency: {} GHz", minFreq);
    return minFreq;
}

auto getMaxProcessorFrequency() -> double {
    LOG_F(INFO, "Starting getMaxProcessorFrequency function on Linux");
    
    double maxFreq = 0.0;
    
    // Try to read from /sys/devices
    std::ifstream freqFile("/sys/devices/system/cpu/cpu0/cpufreq/scaling_max_freq");
    if (freqFile.is_open()) {
        std::string line;
        if (std::getline(freqFile, line)) {
            try {
                // Convert kHz to GHz
                maxFreq = std::stod(line) / 1000000.0;
            } catch (const std::exception& e) {
                LOG_F(WARNING, "Error parsing CPU max frequency: {}", e.what());
            }
        }
    }
    
    // If no max frequency found, try cpuinfo_max_freq
    if (maxFreq <= 0.0) {
        std::ifstream cpuinfoMaxFreq("/sys/devices/system/cpu/cpu0/cpufreq/cpuinfo_max_freq");
        if (cpuinfoMaxFreq.is_open()) {
            std::string line;
            if (std::getline(cpuinfoMaxFreq, line)) {
                try {
                    // Convert kHz to GHz
                    maxFreq = std::stod(line) / 1000000.0;
                } catch (const std::exception& e) {
                    LOG_F(WARNING, "Error parsing CPU max frequency from cpuinfo_max_freq: {}", e.what());
                }
            }
        }
    }
    
    // If still no valid max frequency, use current as fallback
    if (maxFreq <= 0.0) {
        maxFreq = getProcessorFrequency();
        LOG_F(WARNING, "Could not determine max CPU frequency, using current frequency: {} GHz", maxFreq);
    }
    
    LOG_F(INFO, "Linux CPU Max Frequency: {} GHz", maxFreq);
    return maxFreq;
}

auto getPerCoreFrequencies() -> std::vector<double> {
    LOG_F(INFO, "Starting getPerCoreFrequencies function on Linux");
    
    int numCores = getNumberOfLogicalCores();
    std::vector<double> frequencies(numCores, 0.0);
    
    for (int i = 0; i < numCores; ++i) {
        std::string freqPath = "/sys/devices/system/cpu/cpu" + std::to_string(i) + "/cpufreq/scaling_cur_freq";
        std::ifstream freqFile(freqPath);
        
        if (freqFile.is_open()) {
            std::string line;
            if (std::getline(freqFile, line)) {
                try {
                    // Convert kHz to GHz
                    frequencies[i] = std::stod(line) / 1000000.0;
                } catch (const std::exception& e) {
                    LOG_F(WARNING, "Error parsing CPU frequency for core {}: {}", i, e.what());
                }
            }
        }
        
        // If we couldn't read the frequency, use the global frequency
        if (frequencies[i] <= 0.0) {
            if (i == 0) {
                frequencies[i] = getProcessorFrequency();
            } else {
                frequencies[i] = frequencies[0]; // Use the frequency of the first core as a fallback
            }
        }
    }
    
    LOG_F(INFO, "Linux Per-Core CPU Frequencies collected for {} cores", numCores);
    return frequencies;
}

auto getNumberOfPhysicalPackages() -> int {
    LOG_F(INFO, "Starting getNumberOfPhysicalPackages function on Linux");
    
    if (!needsCacheRefresh() && g_cpuInfoCache.numPhysicalPackages > 0) {
        return g_cpuInfoCache.numPhysicalPackages;
    }
    
    int numberOfPackages = 0;
    std::set<std::string> physicalIds;
    
    std::ifstream cpuinfo("/proc/cpuinfo");
    if (cpuinfo.is_open()) {
        std::string line;
        while (std::getline(cpuinfo, line)) {
            if (line.find("physical id") != std::string::npos) {
                size_t pos = line.find(':');
                if (pos != std::string::npos && pos + 2 < line.size()) {
                    physicalIds.insert(line.substr(pos + 2));
                }
            }
        }
    }
    
    numberOfPackages = static_cast<int>(physicalIds.size());
    
    // Ensure at least one package
    if (numberOfPackages <= 0) {
        numberOfPackages = 1;
        LOG_F(WARNING, "Could not determine number of physical CPU packages, assuming 1");
    }
    
    LOG_F(INFO, "Linux Physical CPU Packages: {}", numberOfPackages);
    return numberOfPackages;
}

auto getNumberOfPhysicalCores() -> int {
    LOG_F(INFO, "Starting getNumberOfPhysicalCores function on Linux");
    
    if (!needsCacheRefresh() && g_cpuInfoCache.numPhysicalCores > 0) {
        return g_cpuInfoCache.numPhysicalCores;
    }
    
    int numberOfCores = 0;
    
    // Try to get physical core count from /proc/cpuinfo
    std::ifstream cpuinfo("/proc/cpuinfo");
    if (cpuinfo.is_open()) {
        std::string line;
        std::map<std::string, std::set<std::string>> coresPerPackage;
        
        std::string currentPhysicalId;
        
        while (std::getline(cpuinfo, line)) {
            if (line.find("physical id") != std::string::npos) {
                size_t pos = line.find(':');
                if (pos != std::string::npos && pos + 2 < line.size()) {
                    currentPhysicalId = line.substr(pos + 2);
                }
            } else if (line.find("core id") != std::string::npos && !currentPhysicalId.empty()) {
                size_t pos = line.find(':');
                if (pos != std::string::npos && pos + 2 < line.size()) {
                    std::string coreId = line.substr(pos + 2);
                    coresPerPackage[currentPhysicalId].insert(coreId);
                }
            }
        }
        
        // Count unique cores across all packages
        for (const auto& package : coresPerPackage) {
            numberOfCores += static_cast<int>(package.second.size());
        }
    }
    
    // If we couldn't determine the number of physical cores from core_id
    if (numberOfCores <= 0) {
        // Try another approach by looking at cpu cores entries
        std::ifstream cpuinfo2("/proc/cpuinfo");
        if (cpuinfo2.is_open()) {
            std::string line;
            std::map<std::string, int> coresPerPackage;
            
            std::string currentPhysicalId;
            
            while (std::getline(cpuinfo2, line)) {
                if (line.find("physical id") != std::string::npos) {
                    size_t pos = line.find(':');
                    if (pos != std::string::npos && pos + 2 < line.size()) {
                        currentPhysicalId = line.substr(pos + 2);
                    }
                } else if (line.find("cpu cores") != std::string::npos && !currentPhysicalId.empty()) {
                    size_t pos = line.find(':');
                    if (pos != std::string::npos && pos + 2 < line.size()) {
                        try {
                            int cores = std::stoi(line.substr(pos + 2));
                            coresPerPackage[currentPhysicalId] = cores;
                        } catch (const std::exception& e) {
                            LOG_F(WARNING, "Error parsing CPU cores: {}", e.what());
                        }
                    }
                }
            }
            
            // Sum cores across all packages
            for (const auto& package : coresPerPackage) {
                numberOfCores += package.second;
            }
        }
    }
    
    // Ensure at least one core
    if (numberOfCores <= 0) {
        // Fall back to counting the number of directories in /sys/devices/system/cpu/
        DIR* dir = opendir("/sys/devices/system/cpu/");
        if (dir != nullptr) {
            struct dirent* entry;
            std::regex cpuRegex("cpu[0-9]+");
            
            while ((entry = readdir(dir)) != nullptr) {
                std::string name = entry->d_name;
                if (std::regex_match(name, cpuRegex)) {
                    numberOfCores++;
                }
            }
            
            closedir(dir);
            
            // Attempt to account for hyperthreading
            numberOfCores = std::max(1, numberOfCores / 2);
        } else {
            // Last resort: use logical core count
            numberOfCores = getNumberOfLogicalCores();
            LOG_F(WARNING, "Could not determine number of physical CPU cores, using logical core count: {}", numberOfCores);
        }
    }
    
    LOG_F(INFO, "Linux Physical CPU Cores: {}", numberOfCores);
    return numberOfCores;
}

auto getNumberOfLogicalCores() -> int {
    LOG_F(INFO, "Starting getNumberOfLogicalCores function on Linux");
    
    if (!needsCacheRefresh() && g_cpuInfoCache.numLogicalCores > 0) {
        return g_cpuInfoCache.numLogicalCores;
    }
    
    int numberOfCores = 0;
    
    // First try sysconf
    numberOfCores = sysconf(_SC_NPROCESSORS_ONLN);
    
    // If sysconf fails, count CPUs in /proc/cpuinfo
    if (numberOfCores <= 0) {
        std::ifstream cpuinfo("/proc/cpuinfo");
        if (cpuinfo.is_open()) {
            std::string line;
            while (std::getline(cpuinfo, line)) {
                if (line.find("processor") != std::string::npos) {
                    numberOfCores++;
                }
            }
        }
    }
    
    // If we still don't have a valid count, fall back to counting directories
    if (numberOfCores <= 0) {
        DIR* dir = opendir("/sys/devices/system/cpu/");
        if (dir != nullptr) {
            struct dirent* entry;
            std::regex cpuRegex("cpu[0-9]+");
            
            while ((entry = readdir(dir)) != nullptr) {
                std::string name = entry->d_name;
                if (std::regex_match(name, cpuRegex)) {
                    numberOfCores++;
                }
            }
            
            closedir(dir);
        }
    }
    
    // Ensure at least one core
    if (numberOfCores <= 0) {
        numberOfCores = 1;
        LOG_F(WARNING, "Could not determine number of logical CPU cores, assuming 1");
    }
    
    LOG_F(INFO, "Linux Logical CPU Cores: {}", numberOfCores);
    return numberOfCores;
}

auto getCacheSizes() -> CacheSizes {
    LOG_F(INFO, "Starting getCacheSizes function on Linux");
    
    if (!needsCacheRefresh() &&
        (g_cpuInfoCache.caches.l1d > 0 || g_cpuInfoCache.caches.l2 > 0 ||
         g_cpuInfoCache.caches.l3 > 0)) {
        return g_cpuInfoCache.caches;
    }
    
    CacheSizes cacheSizes{0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0};
    
    // Try to read from sysfs first
    auto readCacheInfo = [](const std::string& path, const std::string& file) -> size_t {
        std::ifstream cacheFile(path + file);
        if (cacheFile.is_open()) {
            std::string line;
            if (std::getline(cacheFile, line)) {
                try {
                    return static_cast<size_t>(std::stoul(line));
                } catch (const std::exception& e) {
                    LOG_F(WARNING, "Error parsing cache size from {}: {}", path + file, e.what());
                }
            }
        }
        return 0;
    };
    
    // Check /sys/devices/system/cpu/cpu0/cache/
    std::string cachePath = "/sys/devices/system/cpu/cpu0/cache/";
    DIR* dir = opendir(cachePath.c_str());
    
    if (dir != nullptr) {
        struct dirent* entry;
        while ((entry = readdir(dir)) != nullptr) {
            std::string name = entry->d_name;
            
            // Skip . and .. entries
            if (name == "." || name == "..") continue;
            
            // Only process indexN directories
            if (name.find("index") != 0) continue;
            
            std::string indexPath = cachePath + name + "/";
            
            // Read cache level and type
            std::ifstream levelFile(indexPath + "level");
            std::ifstream typeFile(indexPath + "type");
            
            if (levelFile.is_open() && typeFile.is_open()) {
                std::string levelStr, typeStr;
                if (std::getline(levelFile, levelStr) && std::getline(typeFile, typeStr)) {
                    int level = std::stoi(levelStr);
                    
                    // Read cache size
                    size_t size = readCacheInfo(indexPath, "size");
                    size_t lineSize = readCacheInfo(indexPath, "coherency_line_size");
                    size_t ways = readCacheInfo(indexPath, "ways_of_associativity");
                    
                    // If size is returned in a format like "32K", convert to bytes
                    if (size <= 0) {
                        std::ifstream sizeFile(indexPath + "size");
                        if (sizeFile.is_open()) {
                            std::string sizeStr;
                            if (std::getline(sizeFile, sizeStr)) {
                                size = stringToBytes(sizeStr);
                            }
                        }
                    }
                    
                    LOG_F(INFO, "Found cache: Level={}, Type={}, Size={}B", level, typeStr, size);
                    
                    // Assign to appropriate cache field
                    if (level == 1) {
                        if (typeStr == "Data") {
                            cacheSizes.l1d = size;
                            cacheSizes.l1d_line_size = lineSize;
                            cacheSizes.l1d_associativity = ways;
                        } else if (typeStr == "Instruction") {
                            cacheSizes.l1i = size;
                            cacheSizes.l1i_line_size = lineSize;
                            cacheSizes.l1i_associativity = ways;
                        }
                    } else if (level == 2) {
                        cacheSizes.l2 = size;
                        cacheSizes.l2_line_size = lineSize;
                        cacheSizes.l2_associativity = ways;
                    } else if (level == 3) {
                        cacheSizes.l3 = size;
                        cacheSizes.l3_line_size = lineSize;
                        cacheSizes.l3_associativity = ways;
                    }
                }
            }
        }
        
        closedir(dir);
    } else {
        // If sysfs entries not available, try /proc/cpuinfo
        LOG_F(WARNING, "Could not open {}, falling back to /proc/cpuinfo", cachePath);
        
        std::ifstream cpuinfo("/proc/cpuinfo");
        if (cpuinfo.is_open()) {
            std::string line;
            while (std::getline(cpuinfo, line)) {
                if (line.find("cache size") != std::string::npos) {
                    size_t pos = line.find(':');
                    if (pos != std::string::npos && pos + 2 < line.size()) {
                        std::string sizeStr = line.substr(pos + 2);
                        size_t size = stringToBytes(sizeStr);
                        
                        // Assume this is the largest cache (L3 or L2)
                        if (size > 0) {
                            if (size > 1024 * 1024) { // Larger than 1MB is likely L3
                                cacheSizes.l3 = size;
                            } else { // Smaller caches are likely L2
                                cacheSizes.l2 = size;
                            }
                        }
                    }
                }
            }
        }
    }
    
    LOG_F(INFO, "Linux Cache Sizes: L1d={}KB, L1i={}KB, L2={}KB, L3={}KB",
          cacheSizes.l1d / 1024, cacheSizes.l1i / 1024, cacheSizes.l2 / 1024, cacheSizes.l3 / 1024);
    
    return cacheSizes;
}

auto getCpuLoadAverage() -> LoadAverage {
    LOG_F(INFO, "Starting getCpuLoadAverage function on Linux");
    
    LoadAverage loadAvg{0.0, 0.0, 0.0};
    
    double avg[3];
    if (getloadavg(avg, 3) == 3) {
        loadAvg.oneMinute = avg[0];
        loadAvg.fiveMinutes = avg[1];
        loadAvg.fifteenMinutes = avg[2];
    }
    
    // Alternative approach if getloadavg fails
    if (loadAvg.oneMinute <= 0.0 && loadAvg.fiveMinutes <= 0.0 && loadAvg.fifteenMinutes <= 0.0) {
        std::ifstream loadFile("/proc/loadavg");
        if (loadFile.is_open()) {
            loadFile >> loadAvg.oneMinute >> loadAvg.fiveMinutes >> loadAvg.fifteenMinutes;
        }
    }
    
    LOG_F(INFO, "Linux Load Average: {}, {}, {}",
          loadAvg.oneMinute, loadAvg.fiveMinutes, loadAvg.fifteenMinutes);
    
    return loadAvg;
}

auto getCpuPowerInfo() -> CpuPowerInfo {
    LOG_F(INFO, "Starting getCpuPowerInfo function on Linux");
    
    CpuPowerInfo powerInfo{0.0, 0.0, 0.0};
    
    // Try to read from RAPL interface if available
    // Energy usage in microjoules
    std::ifstream energyFile("/sys/class/powercap/intel-rapl/intel-rapl:0/energy_uj");
    if (energyFile.is_open()) {
        static unsigned long long lastEnergy = 0;
        static std::chrono::steady_clock::time_point lastTime = std::chrono::steady_clock::now();
        
        unsigned long long energy;
        energyFile >> energy;
        
        auto now = std::chrono::steady_clock::now();
        auto elapsedMicroseconds = std::chrono::duration_cast<std::chrono::microseconds>(now - lastTime).count();
        
        if (lastEnergy > 0 && elapsedMicroseconds > 0) {
            // Calculate power in watts (energy in microjoules / time in microseconds)
            unsigned long long energyDelta = energy - lastEnergy;
            powerInfo.currentWatts = static_cast<double>(energyDelta) / elapsedMicroseconds;
        }
        
        lastEnergy = energy;
        lastTime = now;
    }
    
    // Try to read TDP from various possible locations
    std::ifstream tdpFile("/sys/class/powercap/intel-rapl/intel-rapl:0/constraint_0_power_limit_uw");
    if (tdpFile.is_open()) {
        unsigned long long tdpUw;
        tdpFile >> tdpUw;
        powerInfo.maxTDP = static_cast<double>(tdpUw) / 1000000.0; // Convert microWatts to Watts
    }
    
    LOG_F(INFO, "Linux CPU Power Info: currentWatts={}, maxTDP={}, energyImpact={}",
          powerInfo.currentWatts, powerInfo.maxTDP, powerInfo.energyImpact);
    
    return powerInfo;
}

auto getCpuFeatureFlags() -> std::vector<std::string> {
    LOG_F(INFO, "Starting getCpuFeatureFlags function on Linux");
    
    if (!needsCacheRefresh() && !g_cpuInfoCache.flags.empty()) {
        return g_cpuInfoCache.flags;
    }
    
    std::vector<std::string> flags;
    
    std::ifstream cpuinfo("/proc/cpuinfo");
    if (cpuinfo.is_open()) {
        std::string line;
        while (std::getline(cpuinfo, line)) {
            if (line.find("flags") != std::string::npos || 
                line.find("Features") != std::string::npos) { // "Features" on ARM
                
                size_t pos = line.find(':');
                if (pos != std::string::npos && pos + 2 < line.size()) {
                    std::string flagsStr = line.substr(pos + 2);
                    std::istringstream ss(flagsStr);
                    std::string flag;
                    
                    while (ss >> flag) {
                        flags.push_back(flag);
                    }
                    
                    break; // Only need one set of flags
                }
            }
        }
    }
    
    LOG_F(INFO, "Linux CPU Flags: {} features collected", flags.size());
    return flags;
}

auto getCpuArchitecture() -> CpuArchitecture {
    LOG_F(INFO, "Starting getCpuArchitecture function on Linux");
    
    if (!needsCacheRefresh()) {
        std::lock_guard<std::mutex> lock(g_cacheMutex);
        if (g_cacheInitialized && g_cpuInfoCache.architecture != CpuArchitecture::UNKNOWN) {
            return g_cpuInfoCache.architecture;
        }
    }
    
    CpuArchitecture arch = CpuArchitecture::UNKNOWN;
    
    // Get architecture using uname
    struct utsname sysInfo;
    if (uname(&sysInfo) == 0) {
        std::string machine = sysInfo.machine;
        
        if (machine == "x86_64") {
            arch = CpuArchitecture::X86_64;
        } else if (machine == "i386" || machine == "i686") {
            arch = CpuArchitecture::X86;
        } else if (machine == "aarch64") {
            arch = CpuArchitecture::ARM64;
        } else if (machine.find("arm") != std::string::npos) {
            arch = CpuArchitecture::ARM;
        } else if (machine.find("ppc") != std::string::npos || machine.find("powerpc") != std::string::npos) {
            arch = CpuArchitecture::POWERPC;
        } else if (machine.find("mips") != std::string::npos) {
            arch = CpuArchitecture::MIPS;
        } else if (machine.find("riscv") != std::string::npos) {
            arch = CpuArchitecture::RISC_V;
        }
    }
    
    LOG_F(INFO, "Linux CPU Architecture: {}", cpuArchitectureToString(arch));
    return arch;
}

auto getCpuVendor() -> CpuVendor {
    LOG_F(INFO, "Starting getCpuVendor function on Linux");
    
    if (!needsCacheRefresh()) {
        std::lock_guard<std::mutex> lock(g_cacheMutex);
        if (g_cacheInitialized && g_cpuInfoCache.vendor != CpuVendor::UNKNOWN) {
            return g_cpuInfoCache.vendor;
        }
    }
    
    CpuVendor vendor = CpuVendor::UNKNOWN;
    std::string vendorString;
    
    std::ifstream cpuinfo("/proc/cpuinfo");
    if (cpuinfo.is_open()) {
        std::string line;
        while (std::getline(cpuinfo, line)) {
            // Different CPU architectures use different fields
            if (line.find("vendor_id") != std::string::npos ||   // x86
                line.find("Hardware") != std::string::npos ||     // ARM
                line.find("vendor") != std::string::npos) {       // Others
                
                size_t pos = line.find(':');
                if (pos != std::string::npos && pos + 2 < line.size()) {
                    vendorString = line.substr(pos + 2);
                    // Trim whitespace
                    vendorString.erase(0, vendorString.find_first_not_of(" \t\n\r\f\v"));
                    vendorString.erase(vendorString.find_last_not_of(" \t\n\r\f\v") + 1);
                    break;
                }
            }
        }
    }
    
    // If vendor string is empty, try to get it from CPU model
    if (vendorString.empty()) {
        std::string model = getCPUModel();
        if (!model.empty()) {
            vendorString = model;
        }
    }
    
    vendor = getVendorFromString(vendorString);
    
    LOG_F(INFO, "Linux CPU Vendor: {} ({})", vendorString, cpuVendorToString(vendor));
    return vendor;
}

auto getCpuSocketType() -> std::string {
    LOG_F(INFO, "Starting getCpuSocketType function on Linux");
    
    if (!needsCacheRefresh() && !g_cpuInfoCache.socketType.empty()) {
        return g_cpuInfoCache.socketType;
    }
    
    std::string socketType = "Unknown";
    
    // Linux doesn't provide socket type directly
    // We would need to use external tools like dmidecode (requires root)
    // or parse hardware database files
    
    // This is a placeholder implementation
    LOG_F(INFO, "Linux CPU Socket Type: {} (placeholder)", socketType);
    return socketType;
}

auto getCpuScalingGovernor() -> std::string {
    LOG_F(INFO, "Starting getCpuScalingGovernor function on Linux");
    
    std::string governor = "Unknown";
    
    // Get the scaling governor for CPU 0
    std::ifstream govFile("/sys/devices/system/cpu/cpu0/cpufreq/scaling_governor");
    if (govFile.is_open()) {
        std::getline(govFile, governor);
    }
    
    LOG_F(INFO, "Linux CPU Scaling Governor: {}", governor);
    return governor;
}

auto getPerCoreScalingGovernors() -> std::vector<std::string> {
    LOG_F(INFO, "Starting getPerCoreScalingGovernors function on Linux");
    
    int numCores = getNumberOfLogicalCores();
    std::vector<std::string> governors(numCores, "Unknown");
    
    for (int i = 0; i < numCores; ++i) {
        std::string govPath = "/sys/devices/system/cpu/cpu" + std::to_string(i) + "/cpufreq/scaling_governor";
        std::ifstream govFile(govPath);
        
        if (govFile.is_open()) {
            std::getline(govFile, governors[i]);
        }
    }
    
    LOG_F(INFO, "Linux Per-Core CPU Scaling Governors collected for {} cores", numCores);
    return governors;
}

} // namespace atom::system

#endif /* __linux__ || __ANDROID__ */
