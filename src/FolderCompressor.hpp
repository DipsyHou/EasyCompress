#pragma once

#include "HuffmanTree.hpp"
#include "TreeSerializer.hpp"
#include "BitStream.hpp"
#include "VarInt.hpp"
#include "FileCompressor.hpp"
#include <filesystem>
#include <vector>
#include <fstream>
#include <iostream>
#include <algorithm>

namespace fs = std::filesystem;

class FolderCompressor {
private:
    struct FileEntry {
        std::string relativePath;
        std::string absolutePath;
        uint64_t size;
    };
    
    // 收集文件夹下所有文件
    static std::vector<FileEntry> collectFiles(const fs::path& folderPath) {
        std::vector<FileEntry> files;
        fs::path baseFolder = folderPath;
        
        for (const auto& entry : fs::recursive_directory_iterator(folderPath)) {
            if (entry.is_regular_file()) {
                FileEntry file;
                file.relativePath = fs::relative(entry.path(), baseFolder).string();
                file.absolutePath = entry.path().string();
                file.size = entry.file_size();
                
                // 统一路径分隔符为 '/'
                std::replace(file.relativePath.begin(), file.relativePath.end(), '\\', '/');
                
                files.push_back(file);
            }
        }
        
        return files;
    }
    
    // 读取文件内容
    static std::string readFileContent(const std::string& filePath) {
        std::ifstream file(filePath, std::ios::binary);
        if (!file.is_open()) {
            std::cerr << "错误：无法打开文件 " << filePath << std::endl;
            return "";
        }
        
        return std::string((std::istreambuf_iterator<char>(file)),
                           std::istreambuf_iterator<char>());
    }
    
    // 获取字符频率
    static std::vector<std::pair<char, int>> getFrequencies(const std::string& content) {
        std::unordered_map<char, int> freqMap;
        for (char ch : content) {
            freqMap[ch]++;
        }
        
        std::vector<std::pair<char, int>> charFreqs;
        for (const auto& pair : freqMap) {
            charFreqs.push_back(pair);
        }
        
        return charFreqs;
    }

public:
    // 方案1：全局哈夫曼树（一棵树编码所有文件）
    static bool compressWithGlobalTree(const std::string& folderPath) {  
        std::string outputFile = folderPath + ".huf";
        std::cout << "正在压缩文件夹: " << folderPath << " -> " << outputFile << std::endl;
        
        // 1. 收集所有文件
        std::cout << "正在扫描文件..." << std::flush;
        auto files = collectFiles(folderPath);
        std::cout << "\r";
        if (files.empty()) {
            std::cerr << "错误：文件夹为空" << std::endl;
            return false;
        }
        
        std::cout << "发现 " << files.size() << " 个文件" << std::endl;
        
        // 2. 统计全局字符频率
        std::cout << "正在统计全局字符频率..." << std::endl;
        std::unordered_map<char, uint64_t> globalFreq;
        
        // 统计路径字符
        for (const auto& file : files) {
            for (char ch : file.relativePath) {
                globalFreq[ch]++;
            }
        }
        
        // 统计文件内容字符
        size_t processedFiles = 0;
        for (const auto& file : files) {
            std::string content = readFileContent(file.absolutePath);
            for (char ch : content) {
                globalFreq[ch]++;
            }
            processedFiles++;
            if (processedFiles % 100 == 0 || processedFiles == files.size()) {
                std::cout << "\r  已处理: " << processedFiles << "/" << files.size() << " 个文件" << std::flush;
            }
        }
        std::cout << std::endl;
        
        std::cout << "发现 " << globalFreq.size() << " 种不同字符" << std::endl;
        
        // 3. 构建全局哈夫曼树
        std::vector<std::pair<char, int>> charFreqs;
        for (const auto& pair : globalFreq) {
            charFreqs.push_back({pair.first, static_cast<int>(pair.second)});
        }
        
        HuffmanTree globalTree;
        globalTree.buildFromFrequencies(charFreqs);
        globalTree.generateCodeTable();
        
        // 4. 写入压缩文件
        std::ofstream out(outputFile, std::ios::binary);
        if (!out.is_open()) {
            std::cerr << "错误：无法创建输出文件" << std::endl;
            return false;
        }
        
        // 写入魔数标识（全局树模式）
        out.put('G');
        
        // 写入全局哈夫曼树
        BitWriter treeWriter(out);
        TreeSerializer::serialize(globalTree.getRoot(), treeWriter);
        treeWriter.flush();
        
        // 写入文件数量
        VarInt::write(out, files.size());
        
        // 5. 编码并写入每个文件
        uint64_t totalOriginalSize = 0;
        uint64_t totalEncodedBits = 0;
        
        for (const auto& file : files) {
            std::cout << "  压缩: " << file.relativePath << " (" << file.size << "字节)" << std::endl;
            
            // 编码路径
            std::string encodedPath = globalTree.encode(file.relativePath);
            
            // 读取并编码内容
            std::string content = readFileContent(file.absolutePath);
            std::string encodedContent = globalTree.encode(content);
            
            // 写入元数据
            VarInt::write(out, encodedPath.length());
            VarInt::write(out, encodedContent.length());
            
            // 写入编码数据
            FileCompressor::writeBits(encodedPath, out);
            FileCompressor::writeBits(encodedContent, out);
            
            totalOriginalSize += file.relativePath.length() + file.size;
            totalEncodedBits += encodedPath.length() + encodedContent.length();
        }
        
        out.close();
        
        // 6. 统计信息
        uint64_t compressedSize = fs::file_size(outputFile);
        std::cout << "\n压缩完成！" << std::endl;
        std::cout << "原始大小: " << totalOriginalSize << " 字节" << std::endl;
        std::cout << "压缩后大小: " << compressedSize << " 字节" << std::endl;
        std::cout << "压缩率: " << (1.0 - (double)compressedSize / totalOriginalSize) * 100 << "%" << std::endl;
        
        return true;
    }
    
    // 方案2：单独哈夫曼树（每个文件一棵树）
    static bool compressWithSeparateTrees(const std::string& folderPath) {
        std::string outputFile = folderPath + ".huf";
        std::cout << "正在压缩文件夹: " << folderPath << " -> " << outputFile << std::endl;
        
        // 1. 收集所有文件
        std::cout << "正在扫描文件..." << std::flush;
        auto files = collectFiles(folderPath);
        std::cout << "\r";
        if (files.empty()) {
            std::cerr << "错误：文件夹为空" << std::endl;
            return false;
        }
        
        std::cout << "发现 " << files.size() << " 个文件" << std::endl;
        
        // 2. 写入压缩文件
        std::ofstream out(outputFile, std::ios::binary);
        if (!out.is_open()) {
            std::cerr << "错误：无法创建输出文件" << std::endl;
            return false;
        }
        
        // 写入魔数标识（单独树模式）
        out.put('S');
        
        // 写入文件数量
        VarInt::write(out, files.size());
        
        // 3. 逐个文件压缩
        uint64_t totalOriginalSize = 0;
        uint64_t totalCompressedSize = 4 + VarInt::encodedSize(files.size());
        
        for (const auto& file : files) {
            std::cout << "  压缩: " << file.relativePath << " (" << file.size << "字节)" << std::endl;
            
            // 读取文件内容
            std::string content = readFileContent(file.absolutePath);
            
            // 为这个文件单独构建哈夫曼树（包含路径和内容的所有字符）
            auto charFreqs = getFrequencies(file.relativePath + content);
            
            HuffmanTree tree;
            tree.buildFromFrequencies(charFreqs);
            tree.generateCodeTable();
            
            // 写入这个文件的哈夫曼树
            size_t treeStartPos = out.tellp();
            BitWriter treeWriter(out);
            TreeSerializer::serialize(tree.getRoot(), treeWriter);
            treeWriter.flush();
            size_t treeEndPos = out.tellp();
            
            // 编码路径和内容
            std::string encodedPath = tree.encode(file.relativePath);
            std::string encodedContent = tree.encode(content);
            
            // 写入编码后的路径长度和内容长度（位数）
            VarInt::write(out, encodedPath.length());
            VarInt::write(out, encodedContent.length());
            
            // 写入编码后的路径数据
            for (size_t i = 0; i < encodedPath.length(); i += 8) {
                std::bitset<8> byte;
                for (int j = 0; j < 8 && i + j < encodedPath.length(); j++) {
                    if (encodedPath[i + j] == '1') {
                        byte[7 - j] = 1;
                    }
                }
                unsigned char byteValue = static_cast<unsigned char>(byte.to_ulong());
                out.put(byteValue);
            }
            
            // 写入编码后的内容数据
            for (size_t i = 0; i < encodedContent.length(); i += 8) {
                std::bitset<8> byte;
                for (int j = 0; j < 8 && i + j < encodedContent.length(); j++) {
                    if (encodedContent[i + j] == '1') {
                        byte[7 - j] = 1;
                    }
                }
                unsigned char byteValue = static_cast<unsigned char>(byte.to_ulong());
                out.put(byteValue);
            }
            
            totalOriginalSize += file.relativePath.length() + file.size;
            totalCompressedSize += (treeEndPos - treeStartPos)
                                 + VarInt::encodedSize(encodedPath.length())
                                 + VarInt::encodedSize(encodedContent.length())
                                 + (encodedPath.length() + 7) / 8
                                 + (encodedContent.length() + 7) / 8;
                                 + (encodedContent.length() + 7) / 8;
        }
        
        out.close();
        
        // 4. 统计信息
        uint64_t compressedSize = fs::file_size(outputFile);
        std::cout << "\n压缩完成！" << std::endl;
        std::cout << "原始大小: " << totalOriginalSize << " 字节" << std::endl;
        std::cout << "压缩后大小: " << compressedSize << " 字节" << std::endl;
        std::cout << "压缩率: " << (1.0 - (double)compressedSize / totalOriginalSize) * 100 << "%" << std::endl;
        
        return true;
    }
    
    // 解压全局树格式
    static bool decompressGlobal(const std::string& archivePath) {
        std::ifstream in(archivePath, std::ios::binary);
        if (!in.is_open()) {
            std::cerr << "错误：无法打开压缩文件" << std::endl;
            return false;
        }
        
        // 验证魔数
        char magic;
        in.get(magic);
        if (magic != 'G') {
            std::cerr << "错误：不是全局树格式" << std::endl;
            return false;
        }
        
        // 读取全局哈夫曼树
        BitReader treeReader(in);
        HuffmanNode* root = TreeSerializer::deserialize(treeReader);
        if (!root) {
            std::cerr << "错误：无法读取哈夫曼树" << std::endl;
            return false;
        }
        
        // 读取文件数量
        uint32_t fileCount = VarInt::decode(in);
        std::cout << "解压 " << fileCount << " 个文件" << std::endl;
        
        // 提取输出目录名
        std::string outputFolder = archivePath;
        size_t pos = outputFolder.find(".huf");
        if (pos != std::string::npos) {
            outputFolder = outputFolder.substr(0, pos);
        }
        
        // 解压每个文件
        for (uint32_t i = 0; i < fileCount; i++) {
            // 读取元数据（VarInt编码的位数，用于验证）
            uint32_t pathBits = VarInt::decode(in);
            uint32_t contentBits = VarInt::decode(in);
            
            // 读取并解码路径（readBits会自动读取4字节bitCount）
            std::string encodedPath = FileCompressor::readBits(in);
            std::string relativePath = HuffmanTree::decodeWithRoot(root, encodedPath);
            
            // 读取并解码内容（readBits会自动读取4字节bitCount）
            std::string encodedContent = FileCompressor::readBits(in);
            std::string content = HuffmanTree::decodeWithRoot(root, encodedContent);
            
            // 创建目录并写入文件
            fs::path targetPath = fs::path(outputFolder) / relativePath;
            fs::create_directories(targetPath.parent_path());
            
            std::ofstream outFile(targetPath, std::ios::binary);
            outFile << content;
            outFile.close();
            
            std::cout << "  解压: " << relativePath << " (" << content.size() << "字节)" << std::endl;
        }
        
        delete root;
        in.close();
        
        std::cout << "解压完成！输出目录: " << outputFolder << std::endl;
        return true;
    }
    
    // 解压单独树格式
    static bool decompressSeparate(const std::string& archivePath) {
        std::ifstream in(archivePath, std::ios::binary);
        if (!in.is_open()) {
            std::cerr << "错误：无法打开压缩文件" << std::endl;
            return false;
        }
        
        // 验证魔数
        char magic;
        in.get(magic);
        if (magic != 'S') {
            std::cerr << "错误：不是单独树格式" << std::endl;
            return false;
        }
        
        // 读取文件数量
        uint32_t fileCount = VarInt::decode(in);
        std::cout << "解压 " << fileCount << " 个文件" << std::endl;
        
        // 提取输出目录名
        std::string outputFolder = archivePath;
        size_t pos = outputFolder.find(".huf");
        if (pos != std::string::npos) {
            outputFolder = outputFolder.substr(0, pos);
        }
        
        // 解压每个文件
        for (uint32_t i = 0; i < fileCount; i++) {
            // 读取这个文件的哈夫曼树
            BitReader treeReader(in);
            HuffmanNode* root = TreeSerializer::deserialize(treeReader);
            
            // 读取编码后的路径长度和内容长度
            uint32_t pathBits = VarInt::decode(in);
            uint32_t contentBits = VarInt::decode(in);
            
            // 读取并解码路径
            std::string encodedPath = FileCompressor::readBits(in, pathBits);
            std::string relativePath = HuffmanTree::decodeWithRoot(root, encodedPath);
            
            // 读取并解码内容
            std::string encodedContent = FileCompressor::readBits(in, contentBits);
            std::string content = HuffmanTree::decodeWithRoot(root, encodedContent);
            
            // 创建目录并写入文件
            fs::path targetPath = fs::path(outputFolder) / relativePath;
            fs::create_directories(targetPath.parent_path());
            
            std::ofstream outFile(targetPath, std::ios::binary);
            outFile << content;
            outFile.close();
            
            std::cout << "  解压: " << relativePath << " (" << content.size() << "字节)" << std::endl;
            
            delete root;
        }
        
        in.close();
        
        std::cout << "解压完成！输出目录: " << outputFolder << std::endl;
        return true;
    }
    
    // 自动检测格式并解压
    static bool decompress(const std::string& archivePath) {
        std::ifstream in(archivePath, std::ios::binary);
        if (!in.is_open()) {
            std::cerr << "错误：无法打开压缩文件" << std::endl;
            return false;
        }
        
        char magic;
        in.get(magic);
        in.close();
        
        if (magic == 'G') {
            return decompressGlobal(archivePath);
        } else if (magic == 'S') {
            return decompressSeparate(archivePath);
        } else if (magic == 'F') {
            std::cerr << "错误：这是单文件压缩格式，请使用单文件解压命令" << std::endl;
            return false;
        } else {
            std::cerr << "错误：未知的压缩格式" << std::endl;
            return false;
        }
    }
    
    // FileCompressor的静态方法需要公开
    static void writeBits(const std::string& bits, std::ofstream& out) {
        int bitCount = bits.length();
        out.write(reinterpret_cast<const char*>(&bitCount), sizeof(bitCount));
        
        for (size_t i = 0; i < bits.length(); i += 8) {
            std::bitset<8> byte;
            for (int j = 0; j < 8 && i + j < bits.length(); j++) {
                if (bits[i + j] == '1') {
                    byte[7 - j] = 1;
                }
            }
            unsigned char byteValue = static_cast<unsigned char>(byte.to_ulong());
            out.put(byteValue);
        }
    }
    
    static std::string readBits(std::ifstream& in, uint32_t bitCount) {
        std::string bits;
        int bytesToRead = (bitCount + 7) / 8;
        
        for (int i = 0; i < bytesToRead; i++) {
            char byte;
            if (!in.get(byte)) {
                break;
            }
            
            unsigned char ubyte = static_cast<unsigned char>(byte);
            std::bitset<8> bitset(ubyte);
            
            for (int j = 7; j >= 0; j--) {
                if (bits.length() < bitCount) {
                    bits += (bitset[j] ? '1' : '0');
                }
            }
        }
        
        return bits;
    }
};
