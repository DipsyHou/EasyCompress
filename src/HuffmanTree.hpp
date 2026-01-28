#pragma once

#include "HuffmanNode.hpp"
#include <queue>
#include <vector>
#include <unordered_map>
#include <string>
#include <iostream>

class HuffmanTree {
private:
    HuffmanNode* root;
    std::unordered_map<char, std::string> codeTable;

    // 递归生成编码表
    void generateCodesRecursive(HuffmanNode* node, const std::string& code) {
        if (node == nullptr) return;

        if (node->isLeaf()) {
            codeTable[node->character] = code.empty() ? "0" : code;
            return;
        }

        generateCodesRecursive(node->left, code + "0");
        generateCodesRecursive(node->right, code + "1");
    }

public:
    HuffmanTree() : root(nullptr) {}

    ~HuffmanTree() {
        delete root;
    }

    // 从字符频率构建哈夫曼树
    void buildFromFrequencies(const std::vector<std::pair<char, int>>& charFreqs) {
        if (charFreqs.empty()) {
            root = nullptr;
            return;
        }

        // 使用优先队列（最小堆）
        auto cmp = [](HuffmanNode* left, HuffmanNode* right) { 
            return left->frequency > right->frequency; 
        };
        std::priority_queue<HuffmanNode*, std::vector<HuffmanNode*>, decltype(cmp)> minHeap(cmp);

        // 为每个字符创建叶子节点
        for (const auto& pair : charFreqs) {
            minHeap.push(new HuffmanNode(pair.first, pair.second));
        }

        // 构建哈夫曼树
        while (minHeap.size() > 1) {
            HuffmanNode* left = minHeap.top(); 
            minHeap.pop();
            HuffmanNode* right = minHeap.top(); 
            minHeap.pop();

            HuffmanNode* merged = new HuffmanNode('\0', left->frequency + right->frequency);
            merged->left = left;
            merged->right = right;

            minHeap.push(merged);
        }

        root = minHeap.top();
    }

    // 生成编码表
    void generateCodeTable() {
        codeTable.clear();
        generateCodesRecursive(root, "");
    }

    // 获取编码表
    const std::unordered_map<char, std::string>& getCodeTable() const {
        return codeTable;
    }

    // 获取树根节点
    HuffmanNode* getRoot() const {
        return root;
    }

    // 设置树根节点（用于反序列化）
    void setRoot(HuffmanNode* newRoot) {
        delete root;
        root = newRoot;
    }

    // 释放根节点的所有权（用于序列化后转移所有权）
    HuffmanNode* releaseRoot() {
        HuffmanNode* temp = root;
        root = nullptr;
        return temp;
    }

    // 编码：将文本转换为哈夫曼编码位串
    std::string encode(const std::string& text) const {
        std::string encoded;
        for (char ch : text) {
            auto it = codeTable.find(ch);
            if (it != codeTable.end()) {
                encoded += it->second;
            } else {
                std::cerr << "警告：字符 '" << ch << "' 不在编码表中" << std::endl;
            }
        }
        return encoded;
    }

    // 解码：将哈夫曼编码位串还原为文本
    std::string decode(const std::string& encodedString) const {
        if (root == nullptr) {
            std::cerr << "错误：树根为空" << std::endl;
            return "";
        }

        std::string decodedString;
        HuffmanNode* current = root;

        for (char bit : encodedString) {
            // 根据当前位走左子树或右子树
            if (bit == '0') {
                current = current->left;
            } else if (bit == '1') {
                current = current->right;
            } else {
                std::cerr << "错误：编码字符串包含无效字符 '" << bit << "'" << std::endl;
                return "";
            }

            // 检查是否到达叶子节点
            if (current == nullptr) {
                std::cerr << "错误：无效的编码路径" << std::endl;
                return "";
            }

            if (current->isLeaf()) {
                decodedString += current->character;
                current = root; // 回到根节点继续解码
            }
        }

        // 检查是否正好结束在根节点（完整解码）
        if (current != root) {
            std::cerr << "警告：编码字符串不完整" << std::endl;
        }

        return decodedString;
    }

    // 静态方法：用于解压时直接用root解码（向后兼容）
    static std::string decodeWithRoot(HuffmanNode* root, const std::string& encodedString) {
        if (root == nullptr) {
            std::cerr << "错误：树根为空" << std::endl;
            return "";
        }

        std::string decodedString;
        HuffmanNode* current = root;

        for (char bit : encodedString) {
            if (bit == '0') {
                current = current->left;
            } else if (bit == '1') {
                current = current->right;
            } else {
                std::cerr << "错误：编码字符串包含无效字符 '" << bit << "'" << std::endl;
                return "";
            }

            if (current == nullptr) {
                std::cerr << "错误：无效的编码路径" << std::endl;
                return "";
            }

            if (current->isLeaf()) {
                decodedString += current->character;
                current = root;
            }
        }

        if (current != root) {
            std::cerr << "警告：编码字符串不完整" << std::endl;
        }

        return decodedString;
    }

    // 禁止拷贝
    HuffmanTree(const HuffmanTree&) = delete;
    HuffmanTree& operator=(const HuffmanTree&) = delete;
};
