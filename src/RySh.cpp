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
#include <pwd.h>
#include <readline/readline.h>
#include <readline/history.h>
#include <cstring>

namespace fs = std::filesystem;

std::unordered_map<std::string, std::string> EnvVars;
std::vector<std::string> PATH;

std::string prompt() {
    char* input = nullptr;
    std::string Prompt = "RySh:";
    Prompt += getcwd(NULL, 0);
    Prompt += "> ";
    do {
        input = readline(Prompt.c_str()); // `readline()` waits for input
        if(!input) {
            return ""; // Handle EOF (Ctrl+D)
        }
    } while(input[0] == '\0');

    if(*input) {  // Non-empty input
        add_history(input);  // History makes a copy
    }

    std::string result = input;
    free(input); // `readline()` allocates memory, free it
    return result;
}


bool fileExists(std::string FilePath) {
    return fs::exists(FilePath);
}

std::string findBin(std::string BinToFind) {
    // Check if path specified
    if(BinToFind.find('/') != std::string::npos) {
        if(BinToFind[0] == '.') {
            char* CurPath = getcwd(NULL, 0);
            fs::path fullPath = fs::path(CurPath) / BinToFind.substr(2);
            if(fs::exists(fullPath)) {
                return fullPath.string();
            }
            return "";
        }
        if(fs::exists(fs::path(BinToFind))) {
            return BinToFind;
        }
        return "";
    }

    // Look in PATH variable
    for(const std::string& dir : PATH) {
        fs::path fullPath = fs::path(dir) / BinToFind;
        if(fs::exists(fullPath)) {
            return fullPath.string();
        }
    }
    return "";
}

void readEnvVars() {
    const char* pathEnv = secure_getenv("PATH");
    if(pathEnv) {
        EnvVars["PATH"] = pathEnv;
    }
    else {
        EnvVars["PATH"] = "/usr/bin:/bin"; // Set a safe fallback
    }
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

std::vector<std::string> ExpandPrompt(std::string Input) {
    // find '"' characters and merge into one element
    // split based on spaces
    // expand '~' into user home
    // escape special characters with '/'

    std::stringbuf CurEntry;

    for(size_t i = 0; i < Input.size(); i++) {

    }

    return std::vector<std::string>();
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

        if(Line.empty()) {
            break;
        }

        if(Line == "exit") {
            break;
        }

        LineSplit = splitString(Line, ' ');

        // Handle built-in `cd` command
        if(LineSplit[0] == "cd") {
            if(LineSplit.size() < 2) {
                if(chdir(getpwuid(getuid())->pw_dir) != 0) {
                    perror("cd failed");
                }
            }
            else {
                if(chdir(LineSplit[1].c_str()) != 0) {
                    perror("cd failed");
                }
            }
            continue;
        }
        
        std::string BinPath = findBin(LineSplit[0]);

        if(BinPath.empty()) {
            std::cout << "Command \"" << LineSplit[0] << "\" not found" << '\n';
            continue;
        }

        Args = StrVecToCArr(LineSplit);

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