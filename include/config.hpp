#pragma once

#include <vector>
#include <string>

struct ServiceConfig {
    std::string name;
    std::string path;
    std::vector<std::string> args;
    bool restartEnabled = false;
    int maxRestarts = 0;
};

std::vector<ServiceConfig> loadConfig(const std::string& path);