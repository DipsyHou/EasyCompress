#pragma once

#include "HuffmanTree.hpp"
#include "TreeSerializer.hpp"
#include "BitStream.hpp"
#include <fstream>
#include <bitset>
#include <iostream>

class FileCompressor {
private:
    // 从文件读取内容并统计字符频率
    static std::vector<std::pair<char, int>> getFrequenciesFromFile(const std::string& filename) {
        std::ifstream file(filename, std::ios::binary);
        if (!file.is_open()) {
            std::cerr << "错误：无法打开文件 " << filename << std::endl;
            return {};
        }

        std::unordered_map<char, int> freqMap;
        char ch;
        while (file.get(ch)) {
            freqMap[ch]++;
        }
        file.close();

        std::vector<std::pair<char, int>> charFreqs;
        for (const auto& pair : freqMap) {
            charFreqs.push_back(pair);
        }

        return charFreqs;
    }

public:
    // 公开的工具方法（供文件夹压缩使用）
    static void writeBits(const std::string& bits, std::ofstream& out) {
        // 先写入原始位数
        int bitCount = bits.length();
        out.write(reinterpret_cast<const char*>(&bitCount), sizeof(bitCount));

        // 每8位打包成一个字节
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

    // 从文件读取位串
    static std::string readBits(std::ifstream& in, int bitCount = -1) {
        // 如果没有指定bitCount，从文件读取
        if (bitCount < 0) {
            in.read(reinterpret_cast<char*>(&bitCount), sizeof(bitCount));
            
            if (in.fail() || bitCount <= 0) {
                std::cerr << "错误：无法读取位数或位数无效" << std::endl;
                return "";
            }
        }

        std::string bits;
        int bytesToRead = (bitCount + 7) / 8;

        for (int i = 0; i < bytesToRead; i++) {
            char byte;
            if (!in.get(byte)) {
                std::cerr << "错误：读取字节失败" << std::endl;
                break;
            }
            
            unsigned char ubyte = static_cast<unsigned char>(byte);
            std::bitset<8> bitset(ubyte);
            
            // 转换为字符串
            for (int j = 7; j >= 0; j--) {
                if (bits.length() < static_cast<size_t>(bitCount)) {
                    bits += (bitset[j] ? '1' : '0');
                }
            }
        }

        return bits;
    }

    // 压缩文件
    static bool compress(const std::string& inputFile) {
        // 自动生成输出文件名：原文件名 + .huf
        std::string outputFile = inputFile + ".huf";
        std::cout << "正在压缩: " << inputFile << " -> " << outputFile << std::endl;

        // 1. 读取文件并统计字符频率
        auto charFreqs = getFrequenciesFromFile(inputFile);
        if (charFreqs.empty()) {
            std::cerr << "错误：文件为空或无法读取" << std::endl;
            return false;
        }

        std::cout << "发现 " << charFreqs.size() << " 种不同字符" << std::endl;

        // 2. 构建哈夫曼树
        HuffmanTree tree;
        tree.buildFromFrequencies(charFreqs);

        // 3. 生成编码表
        tree.generateCodeTable();

        // 4. 读取文件内容并编码
        std::ifstream inFile(inputFile, std::ios::binary);
        std::string fileContent;
        char ch;
        while (inFile.get(ch)) {
            fileContent += ch;
        }
        inFile.close();

        std::string encodedData = tree.encode(fileContent);

        // 5. 写入压缩文件
        std::ofstream outFile(outputFile, std::ios::binary);
        if (!outFile.is_open()) {
            std::cerr << "错误：无法创建输出文件" << std::endl;
            return false;
        }

        // 写入魔数标识（单文件模式）
        outFile.put('F');

        // 写入树结构
        BitWriter treeWriter(outFile);
        TreeSerializer::serialize(tree.getRoot(), treeWriter);
        treeWriter.flush();
        
        // 写入编码数据
        writeBits(encodedData, outFile);
        
        outFile.close();

        // 6. 统计信息
        std::ifstream origFile(inputFile, std::ios::binary | std::ios::ate);
        std::ifstream compFile(outputFile, std::ios::binary | std::ios::ate);
        
        long origSize = origFile.tellg();
        long compSize = compFile.tellg();
        
        origFile.close();
        compFile.close();

        std::cout << "压缩完成！" << std::endl;
        std::cout << "原始大小: " << origSize << " 字节" << std::endl;
        std::cout << "压缩后大小: " << compSize << " 字节" << std::endl;
        std::cout << "压缩率: " << (1.0 - (double)compSize / origSize) * 100 << "%" << std::endl;

        return true;
    }

    // 解压文件
    static bool decompress(const std::string& inputFile) {
        // 检查文件是否以.huf结尾
        if (inputFile.length() < 4 || inputFile.substr(inputFile.length() - 4) != ".huf") {
            std::cerr << "错误：文件格式错误，必须是.huf文件" << std::endl;
            return false;
        }
        
        // 自动生成输出文件名：删除.huf后缀
        std::string outputFile = inputFile.substr(0, inputFile.length() - 4);
        std::cout << "正在解压: " << inputFile << " -> " << outputFile << std::endl;

        std::ifstream inFile(inputFile, std::ios::binary);
        if (!inFile.is_open()) {
            std::cerr << "错误：无法打开压缩文件" << std::endl;
            return false;
        }
        // 验证魔数
        char magic;
        inFile.get(magic);
        if (magic != 'F') {
            std::cerr << "错误：不是单文件压缩格式" << std::endl;
            return false;
        }
        // 1. 反序列化哈夫曼树
        BitReader treeReader(inFile);
        HuffmanNode* root = TreeSerializer::deserialize(treeReader);
        
        if (root == nullptr) {
            std::cerr << "错误：无法读取哈夫曼树" << std::endl;
            return false;
        }

        // 2. 读取编码数据
        std::string encodedData = readBits(inFile);
        inFile.close();

        // 3. 解码
        std::string decodedData = HuffmanTree::decodeWithRoot(root, encodedData);

        // 4. 写入解压文件
        std::ofstream outFile(outputFile, std::ios::binary);
        if (!outFile.is_open()) {
            std::cerr << "错误：无法创建输出文件" << std::endl;
            delete root;
            return false;
        }

        outFile << decodedData;
        outFile.close();

        std::cout << "解压完成！" << std::endl;
        std::cout << "输出文件: " << outputFile << std::endl;

        delete root;
        return true;
    }
};
