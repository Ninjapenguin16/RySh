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

std::string ExpandVariables(const std::string& input) {
    std::string output;
    for(size_t i=0; i<input.length(); ++i) {
        if(input[i] == '$') {
            size_t start = i + 1;
            size_t j = start;

            while(j < input.length() && (std::isalnum(input[j]) || input[j] == '_')) {
                ++j;
            }

            std::string var = input.substr(start, j - start);
            const char* val = getenv(var.c_str());

            if(val) output += val;
            // else nothing (empty string)

            i = j - 1; // continue after the variable
        }
        else {
            output += input[i];
        }
    }
    return output;
}

std::vector<std::string> ExpandPrompt(std::string Input) {
    std::vector<std::pair<std::string, bool>> parsed;
    std::string token;
    bool in_quotes = false;
    bool escape = false;

    for(size_t i=0; i<Input.length(); ++i) {
        char c = Input[i];

        if(escape) {
            token += c;
            escape = false;
        }
        else if(c == '\\') {
            if(in_quotes) token += '\\';
            else escape = true;
        }
        else if(c == '"') {
            if(in_quotes && !escape) {
                in_quotes = false;
                parsed.emplace_back(token, true);
                token.clear();
            }
            else if(!in_quotes) {
                in_quotes = true;
            }
            else {
                token += c;
            }
        }
        else if(std::isspace(c) && !in_quotes) {
            if(!token.empty()) {
                parsed.emplace_back(token, false);
                token.clear();
            }
        }
        else {
            token += c;
        }
    }

    if(in_quotes) {
        throw std::runtime_error("Unclosed quote detected");
    }
    if(escape) {
        throw std::runtime_error("Trailing escape character");
    }
    if(!token.empty()) {
        parsed.emplace_back(token, false);
    }

    std::vector<std::string> result;
    for(auto& [arg, was_quoted] : parsed) {
        // Expand ~
        if(!arg.empty() && arg[0] == '~') {
            if(arg.size() == 1 || arg[1] == '/') {
                const char* home = getpwuid(getuid())->pw_dir;
                if(home) {
                    arg = std::string(home) + arg.substr(1);
                }
            }
        }

        // Expand $VAR
        arg = ExpandVariables(arg);

        // Escape if not quoted
        if(!was_quoted) {
            std::string escaped;
            for(char c : arg) {
                if(c == ' ' || c == '\\' || c == '"' || c == '\'' || c == '`' ||
                   c == '$' || c == '&' || c == '|' || c == ';') {
                    escaped += '\\';
                }
                escaped += c;
            }
            arg = escaped;
        }

        result.push_back(arg);
    }

    return result;
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

        //LineSplit = splitString(Line, ' ');

        LineSplit = ExpandPrompt(Line);

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