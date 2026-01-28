#include "FileCompressor.hpp"
#include "FolderCompressor.hpp"
#include "HuffmanTree.hpp"
#include <iostream>
#include <filesystem>

namespace fs = std::filesystem;

void printTips(char* path)
{
    std::cout << "用法:" << std::endl;
    std::cout << "  压缩:   " << path << " -c <文件/文件夹>" << std::endl;
    std::cout << "  解压:   " << path << " -d <压缩文件>" << std::endl;
}

int main(int argc, char* argv[])
{
    // 检查命令行参数
    if (argc > 1) {
        std::string mode = argv[1];
        
        if (mode == "-c" || mode == "--compress") {
            // 压缩模式
            if (argc < 3) {
                std::cerr << "错误：请指定要压缩的文件或文件夹" << std::endl;
                std::cerr << "用法: " << argv[0] << " -c <文件/文件夹>" << std::endl;
                return 1;
            }
            std::string inputPath = argv[2];
            
            // 检查是文件还是文件夹
            if (fs::is_directory(inputPath)) {
                // 文件夹压缩：采用全局树和分离树两种方式压缩，保留较小的
                std::cout << "正在压缩文件夹: " << inputPath << std::endl;
                
                // 统一去掉尾部斜杠
                std::string folderPath = inputPath;
                if (folderPath.back() == '/' || folderPath.back() == '\\') {
                    folderPath.pop_back();
                }
                std::string defaultOutput = folderPath + ".huf";
                std::string globalTemp = folderPath + ".global.tmp.huf";
                std::string separateTemp = folderPath + ".separate.tmp.huf";
                
                // 1. 压缩为全局树格式
                std::cout << "正在生成全局树压缩" << std::endl;
                if (!FolderCompressor::compressWithGlobalTree(folderPath)) {
                    std::cerr << "全局树压缩失败" << std::endl;
                    return 1;
                }
                // 重命名为临时文件
                fs::rename(defaultOutput, globalTemp);
                
                // 2. 压缩为分离树格式
                std::cout << "正在生成分离树压缩" << std::endl;
                if (!FolderCompressor::compressWithSeparateTrees(folderPath)) {
                    std::cerr << "分离树压缩失败" << std::endl;
                    fs::remove(globalTemp);
                    return 1;
                }
                // 重命名为临时文件
                fs::rename(defaultOutput, separateTemp);
                
                // 3. 比较文件大小
                auto globalSize = fs::file_size(globalTemp);
                auto separateSize = fs::file_size(separateTemp);
                
                if (globalSize <= separateSize) {
                    std::cout << "全局树更优 (" << globalSize << " B vs " << separateSize << " B)" << std::endl;
                    fs::rename(globalTemp, defaultOutput);
                    fs::remove(separateTemp);
                } else {
                    std::cout << "单独树更优 (" << separateSize << " B vs " << globalSize << " B)" << std::endl;
                    fs::rename(separateTemp, defaultOutput);
                    fs::remove(globalTemp);
                }
                
                std::cout << "压缩完成: " << defaultOutput << std::endl;
                return 0;
            } else if (fs::is_regular_file(inputPath)) {
                // 单文件压缩
                return FileCompressor::compress(inputPath) ? 0 : 1;
            } else {
                std::cerr << "错误：" << inputPath << " 不是有效的文件或文件夹" << std::endl;
                return 1;
            }
        }
        else if (mode == "-d" || mode == "--decompress") {
            // 解压模式
            if (argc < 3) {
                std::cerr << "错误：请指定要解压的文件" << std::endl;
                std::cerr << "用法: " << argv[0] << " -d <压缩包.huf>" << std::endl;
                return 1;
            }
            std::string inputFile = argv[2];
            
            // 读取魔数判断格式
            std::ifstream file(inputFile, std::ios::binary);
            if (!file) {
                std::cerr << "错误：无法打开文件 " << inputFile << std::endl;
                return 1;
            }
            
            char magic;
            file.read(&magic, 1);
            file.close();
            
            if (magic == 'F') {
                // 单文件格式
                return FileCompressor::decompress(inputFile) ? 0 : 1;
            } else if (magic == 'G' || magic == 'S') {
                // 文件夹格式（全局树或单独树）
                return FolderCompressor::decompress(inputFile) ? 0 : 1;
            } else {
                std::cerr << "错误：未知的文件格式（魔数: 0x" << std::hex << (int)(unsigned char)magic << "）" << std::endl;
                return 1;
            }
        }
        else {
            std::cout << "未知参数: " << mode << std::endl;
            printTips(argv[0]);
            return 0;
        }
    }
    else {
        printTips(argv[0]);
        return 0;
    }
}
