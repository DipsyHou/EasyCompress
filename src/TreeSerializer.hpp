#pragma once

#include "HuffmanNode.hpp"
#include "BitStream.hpp"
#include <fstream>

class TreeSerializer {
public:
    // 序列化哈夫曼树到位流
    static void serialize(HuffmanNode* root, BitWriter& writer) {
        if (root == nullptr) {
            return;
        }

        if (root->isLeaf()) {
            writer.writeBit(0);  // 叶子节点标记
            writer.writeByte(static_cast<unsigned char>(root->character));
            return;
        }
        
        // 内部节点
        writer.writeBit(1);  // 内部节点标记
        serialize(root->left, writer);
        serialize(root->right, writer);
    }

    // 从位流反序列化哈夫曼树
    static HuffmanNode* deserialize(BitReader& reader) {
        int bit = reader.readBit();

        if (bit == 0) {
            // 叶子节点：读取字符
            int ch = reader.readByte();
            if (ch == -1) {
                std::cerr << "错误：读取字符失败" << std::endl;
                return nullptr;
            }
            return new HuffmanNode(static_cast<char>(ch), 0);
        }
        else if (bit == 1) {
            // 内部节点：递归读取左右子树
            HuffmanNode* node = new HuffmanNode('\0', 0);
            node->left = deserialize(reader);
            node->right = deserialize(reader);
            return node;
        }
        else {
            std::cerr << "错误：无效的位值 " << bit << std::endl;
            return nullptr;
        }
    }
};
