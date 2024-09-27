#include <azure_ai_contentsafety_text.h>
#include <iostream>
#include <chrono>
#include <csignal>
#include "thread"
#include <filesystem>
#include <fstream>
#include <functional>
#include <cstdlib>  // For _dupenv_s
#include <string>   // For std::string
#include <stdexcept> // For std::invalid_argument, std::out_of_range
#include <streambuf>
#include <windows.h>

using namespace Azure::AI::ContentSafety;
// Define the signal handler function
void signalHandler(int signum) {
    std::cout << "Interrupt signal (" << signum << ") received.\n";

    // Cleanup and close up stuff here

    // Terminate program
    exit(signum);
}

int get_op_thread_num_from_env() {
    char* env_var = nullptr;
    size_t len = 0;
    if (_dupenv_s(&env_var, &len, "AACS_NUM_THREADS") == 0 && env_var != nullptr) {
        try {
            int num_threads = std::stoi(env_var);
            free(env_var); // Free the allocated memory
            return num_threads;
        }
        catch (const std::invalid_argument& e) {
            std::cerr << "Invalid value for AACS_NUM_THREADS: " << env_var << std::endl;
        }
        catch (const std::out_of_range& e) {
            std::cerr << "Value out of range for AACS_NUM_THREADS: " << env_var << std::endl;
        }
        free(env_var); // Free the allocated memory in case of exception
    }
    else {
        std::cerr << "Environment variable AACS_NUM_THREADS is not set." << std::endl;
    }
    return 4; // Default value
}

std::string getCategoryName(TextCategory category) {
    switch (category) {
    case TextCategory::Hate:
        return "Hate";
    case TextCategory::SelfHarm:
        return "Self Harm";
    case TextCategory::Sexual:
        return "Sexual";
    case TextCategory::Violence:
        return "Violence";
    default:
        return "Unknown";
    }
}

std::string GetCurrentDir() {
    char path[MAX_PATH];
    DWORD result = GetCurrentDirectoryA(sizeof(path), path);
    return path;
}

void Init(TextModelRuntime** aacs, const TextModelConfig& config, const std::string &license) {
    std::cout << "------license- " << license.c_str() << std::endl;
    (*aacs) = new TextModelRuntime(license.c_str(), config);
    (*aacs)->Reload();
}

void Uninit(TextModelRuntime* aacs) {
    if (aacs) {
        aacs->Unload();
        delete aacs;
        aacs = NULL;
    }
}

void TestFromCmd(const TextModelConfig& config, const std::string & license) {
    // Register signal and signal handler
    signal(SIGINT, signalHandler);

    // Ask for model path
    auto modelPath = std::filesystem::current_path().string() + "\\";
    // Get the starting timepoint
    auto start = std::chrono::high_resolution_clock::now();
    std::cout << "Loading model from " << modelPath << std::endl;
    TextModelRuntime* aacs = NULL;
    Init(&aacs, config, license);
    // Get the ending timepoint
    auto end = std::chrono::high_resolution_clock::now();
    auto modelLoadDuration = std::chrono::duration_cast<std::chrono::milliseconds>(end - start);
    std::cout << "Model loaded successfully, duration: " << modelLoadDuration.count() << " milliseconds" << std::endl;
    std::cout << "--------------------------------------------------------------------------------------" << std::endl;
    start = std::chrono::high_resolution_clock::now();
    aacs->Unload();
    end = std::chrono::high_resolution_clock::now();
    auto modelOffloadDuration = std::chrono::duration_cast<std::chrono::milliseconds>(end - start);
    std::cout << "Model offloaded successfully, duration: " << modelOffloadDuration.count() << " milliseconds" << std::endl;
    std::cout << "--------------------------------------------------------------------------------------" << std::endl;
    start = std::chrono::high_resolution_clock::now();
    aacs->Reload();
    end = std::chrono::high_resolution_clock::now();
    auto modelReloadDuration = std::chrono::duration_cast<std::chrono::milliseconds>(end - start);
    std::cout << "Model Reloaded successfully, duration: " << modelReloadDuration.count() << " milliseconds" << std::endl;
    std::cout << "--------------------------------------------------------------------------------------" << std::endl;


    // Loop to continuously ask for user input
    while (true) {
        AnalyzeTextOptions request;
        std::string inputText;

        std::cout << "Enter text to analyze (Ctrl+C to exit): ";
        std::getline(std::cin, inputText);
        std::cout << "  Your input: " << inputText << std::endl;

        request.text = inputText;
        std::cout << "  AnalyzeResult: " << std::endl;
        auto severityThreshold = 3;
        // Run inference
        auto analyzeStart = std::chrono::high_resolution_clock::now();
        AnalyzeTextResult result;
        aacs->AnalyzeText(request, result);
        // Print the result to the console
        for (const auto& categoryAnalysis : result.categoriesAnalysis) {
            if (categoryAnalysis.severity > 0 && categoryAnalysis.severity < severityThreshold) {
                std::cout << "\033[33m";  // Set the text color to yellow
            }
            else if (categoryAnalysis.severity >= severityThreshold) {
                std::cout << "\033[31m";  // Set the text color to red
            }
            else {
                std::cout << "\033[32m";  // Set the text color to green
            }
            std::cout << "    Category: " << getCategoryName(categoryAnalysis.category) << ", Severity: "
                << static_cast<int>(categoryAnalysis.severity) << std::endl;
            std::cout << "\033[0m";  // Reset the text color
        }
        auto analyzeEnd = std::chrono::high_resolution_clock::now();
        auto analyzeTextDuration = std::chrono::duration_cast<std::chrono::milliseconds>(analyzeEnd - analyzeStart);
        std::cout << "AnalyzeText duration: " << analyzeTextDuration.count() << " milliseconds" << std::endl;
        std::cout << "--------------------------------------------------------------------------------------" << std::endl;
    }
}


void AnalyzeText(TextModelRuntime* aacs, const std::string& inputText, AnalyzeTextResult& out)
{
    AnalyzeTextOptions request;
    request.text = inputText;
    aacs->AnalyzeText(request, out);
}

void AnalyzeTextFromFile(TextModelRuntime* aac, const std::string& file, int loop) {
    std::ifstream inputFile;

    inputFile.open(file);
    if (inputFile.is_open()) {
        std::string line;
        unsigned long long avg = 0;
        int count = 0;
        int triggered_count = 0;
        for (int i = 0; i < loop; i++) {
            while (std::getline(inputFile, line)) {
                AnalyzeTextResult result;
                auto analyzeStart = std::chrono::high_resolution_clock::now();

                AnalyzeText(aac, line, result);

                auto analyzeEnd = std::chrono::high_resolution_clock::now();
                auto analyzeTextDuration = std::chrono::duration_cast<std::chrono::milliseconds>(analyzeEnd - analyzeStart);
                avg += analyzeTextDuration.count();

                std::string buffer = "";
                buffer.append("*index:" + std::to_string(count));
                buffer.append(", duration:" + std::to_string(analyzeTextDuration.count()) + " ms ");
                std::cout << line << std::endl;
                bool levelGT0 = false;
                for (const auto& categoryAnalysis : result.categoriesAnalysis) {
                    buffer.append("[type:" + getCategoryName(categoryAnalysis.category));
                    buffer.append(", category:" + std::to_string(static_cast<int>(categoryAnalysis.severity)));
                    buffer.append("]");
                    if (static_cast<int>(categoryAnalysis.severity) > 0) {
                        levelGT0 = true;
                    }
                }
                buffer.append(", text:" + line);
                std::cout << buffer << std::endl;
                count++;
                if (levelGT0) {
                    triggered_count++;
                }
            }
            inputFile.clear();
            inputFile.seekg(0, std::ios::beg);
        }
        std::cout << "avg duration:" << avg / count << " ms" << std::endl;
        std::cout << "totlal count:" << count << " triggerd count:" << triggered_count << " rate((count-triggered_count)*100.0/count):" << (count - triggered_count) * 100.0 / count << "%" << std::endl;
        inputFile.close();
    }
    else {
        std::cout << "Failed to open the file." << std::endl;
    }
}

class NullStreamBuf : public std::streambuf {
protected:
    virtual int overflow(int c) { return c; }
};

bool FileExists(const std::string& filePath) {
    // 获取文件属性
    DWORD fileAttr = GetFileAttributes(filePath.c_str());

    // 检查是否返回无效属性
    return (fileAttr != INVALID_FILE_ATTRIBUTES && !(fileAttr & FILE_ATTRIBUTE_DIRECTORY));
}

std::string GetStringFromIni(const std::string& key, const std::string& filePath) {
    char buffer[2560] = { 0 };
    GetPrivateProfileString("AacsConfig", key.c_str(), "", buffer, sizeof(buffer), filePath.c_str());
    return buffer;
}

bool ReadConfigFromFile(const std::string& filePath, TextModelConfig& config) {
    if (!FileExists(filePath)) {
        std::cout << "can not open config file " << filePath << std::endl;
        return false;
    }
    
    config.gpuEnabled = GetPrivateProfileInt("AacsConfig", "gpuEnabled", config.gpuEnabled, filePath.c_str());
    config.gpuDeviceId = GetPrivateProfileInt("AacsConfig", "gpuDeviceId", config.gpuDeviceId, filePath.c_str());
    config.modelDirectory = GetCurrentDir();
    config.modelName = GetStringFromIni("modelName", filePath);
    config.spmModelName = GetStringFromIni("spmModelName", filePath);

    config.numThreads = GetPrivateProfileInt("AacsConfig", "numThreads", config.numThreads, filePath.c_str());
    std::cout << "config.gpuEnabled:" << config.gpuEnabled << std::endl
        << ",config.gpuDeviceId:" << config.gpuDeviceId << std::endl
        << ",config.modelDirectory:" << config.modelDirectory << std::endl
        << ",config.numThreads:" << config.numThreads << std::endl;
    return true;
}

//EmbeddedAACSTextDemo.exe 0 10
//EmbeddedAACSTextDemo.exe 1 10 input.txt 0
//EmbeddedAACSTextDemo.exe 2
int main(int argc, char* argv[])
{
#if 0
    if (argc < 2) {
        std::cout << "Test load and unload : EmbeddedAACSTextDemo.exe 0 10" << std::endl;
        std::cout << "Test analyze text from file : EmbeddedAACSTextDemo.exe 1 10 input.txt [1|0]" << std::endl;
        std::cout << "Test analyze text from cmd : EmbeddedAACSTextDemo.exe 2" << std::endl;
        return -1;
    }
#endif

    std::string configPath = GetCurrentDir() + "\\config.ini";
    TextModelConfig config;
    ReadConfigFromFile(configPath, config);
    std::string license = GetStringFromIni("licenseText", configPath);

    std::cout << "license:" << license << std::endl;

    int type = atoi(argv[1]);
    if (type == 0) {// test load and unload
        if (argc < 3) {
            std::cout << "Test load and unload : EmbeddedAACSTextDemo.exe 0 10" << std::endl;
            return -1;
        }

        long long initAvg = 0;
        long long uninitAvg = 0;
        int count = atoi(argv[2]);
        for (int i = 0; i < count; i++) {
            TextModelRuntime* aac = NULL;
            auto start = std::chrono::high_resolution_clock::now();
            Init(&aac, config, license);
            auto end = std::chrono::high_resolution_clock::now();
            auto duration = std::chrono::duration_cast<std::chrono::milliseconds>(end - start);
            std::cout << "*index:" << i << "[init]duration: " << duration.count() << " milliseconds" << std::endl;
            initAvg += duration.count();

            start = std::chrono::high_resolution_clock::now();
            aac->Unload();
            end = std::chrono::high_resolution_clock::now();
            duration = std::chrono::duration_cast<std::chrono::milliseconds>(end - start);
            std::cout << "*index:" << i << "[uninit] duration: " << duration.count() << " milliseconds" << std::endl;
            uninitAvg += duration.count();

            delete aac;
        }
        std::cout << "**summery average duration, init: " << initAvg / count << " milliseconds"
            << ",uinit:" << uninitAvg / count << " milliseconds" << std::endl;
    }
    else if (type == 1) { // test analyze text from file
        if (argc < 4) {
            std::cout << "Test analyze text from file : EmbeddedAACSTextDemo.exe 1 10 input.txt" << std::endl;
            return -1;
        }

        NullStreamBuf nullBuffer;
        std::streambuf* originalCoutStreamBuf = NULL;
        if (argc >= 5 && atoi(argv[4]) == 1) {
            originalCoutStreamBuf = std::cout.rdbuf(&nullBuffer);
            std::cout << "This will not be printed." << std::endl;
            //std::cout.rdbuf(originalCoutStreamBuf); //show log
        }

        int count = atoi(argv[2]);
        TextModelRuntime* aac = NULL;
        Init(&aac, config, license);

        std::string fileName = argv[3];
        AnalyzeTextFromFile(aac, fileName, count);
        Uninit(aac);
    }
    else {// test analyze text from cmd
        TestFromCmd(config, license);
    }
    std::cout << "end" << std::endl;
    return 0;
}

