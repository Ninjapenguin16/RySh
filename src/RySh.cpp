#ifndef _GNU_SOURCE
#define _GNU_SOURCE
#endif

#include <iostream>
#include <unistd.h>
#include <errno.h>
#include <filesystem>
#include <fstream>
#include <unordered_map>
#include <vector>
#include <sstream>
#include <sys/wait.h>

namespace fs = std::filesystem;

std::unordered_map<std::string, std::string> EnvVars;
std::vector<std::string> PATH;

std::string prompt() {
    std::string Ret = "";
    do {
        std::cout << "RySh > ";
        std::getline(std::cin, Ret);
    } while(Ret.empty());
    return Ret;
}

bool fileExists(std::string FilePath) {
    return fs::exists(FilePath);
}

std::string findBin(std::string BinToFind) {
    for(const std::string& dir : PATH) {
        fs::path fullPath = fs::path(dir) / BinToFind;
        if(fs::exists(fullPath)) {
            return fullPath.string();
        }
    }
    return "";
}

void readEnvVars() {
    EnvVars["PATH"] = secure_getenv("PATH");
    std::stringstream ss(EnvVars["PATH"]);
    std::string token = "";
    while(std::getline(ss, token, ':')) {
        if(!token.empty()) {
            PATH.push_back(token);
        }
    }
}

std::vector<std::string> splitString(std::string Str, char Delimitor) {
    std::stringstream ss(Str);
    std::vector<std::string> Ret;
    std::string token = "";
    while(std::getline(ss, token, Delimitor)) {
        if(!token.empty()) {
            Ret.push_back(token);
        }
    }
    return Ret;
}

char** StrVecToCArr(std::vector<std::string> Vec) {
    size_t VecLen = Vec.size();
    size_t StrLen = 0;
    char** Ret = (char**)malloc(sizeof(char*) * (VecLen + 1));

    for(size_t i = 0; i < VecLen; i++) {
        StrLen = Vec[i].size();
        Ret[i] = (char*)malloc(sizeof(char) * (StrLen + 1));
        std::copy(Vec[i].begin(), Vec[i].end(), Ret[i]);
        Ret[i][StrLen] = '\0';
    }

    Ret[VecLen] = NULL;

    return Ret;
}

int main() {
    std::string Line;
    std::vector<std::string> LineSplit;
    char** Args = NULL;

    readEnvVars();

    while(1) {

        if(!std::cin.good()) {
            break;
        }

        Line = prompt();

        if(Line == "exit") {
            break;
        }

        LineSplit = splitString(Line, ' ');

        Args = StrVecToCArr(LineSplit);
        
        std::string BinPath = findBin(LineSplit[0]);

        if(BinPath.empty()) {
            std::cout << "Command \"" << LineSplit[0] << "\" not found" << '\n';
            continue;
        }

        //std::cout << BinPath << '\n';
        pid_t pid = fork();
        if (pid == 0) { // Child process
            //execl(BinPath.c_str(), Line.c_str(), NULL);
            execv(BinPath.c_str(), Args);
            perror("execv failed"); // If execv fails, print error
            exit(1);
        }
        else if (pid > 0) { // Parent process
            wait(NULL); // Wait for child process to finish
        }
        else {
            perror("fork failed");
        }

        for(size_t i = 0; Args[i] != NULL; i++) {
            free(Args[i]);
        }
        free(Args);

    }

    return 0;
}