#pragma once

class HuffmanNode {
public:
    char character;      // 字符值（叶子节点有效）
    int frequency;       // 频率
    HuffmanNode* left;   // 左子节点
    HuffmanNode* right;  // 右子节点

    // 构造函数
    HuffmanNode(char ch, int freq) 
        : character(ch), frequency(freq), left(nullptr), right(nullptr) {}

    // 析构函数
    ~HuffmanNode() {
        delete left;
        delete right;
    }

    // 判断是否为叶子节点
    bool isLeaf() const {
        return left == nullptr && right == nullptr;
    }

    // 禁止拷贝
    HuffmanNode(const HuffmanNode&) = delete;
    HuffmanNode& operator=(const HuffmanNode&) = delete;
};
