#pragma once
#include <fstream>

// 位级写入器
class BitWriter {
private:
    std::ofstream& out;
    unsigned char buffer;
    int bitCount;  // buffer中已有的位数

public:
    BitWriter(std::ofstream& output) : out(output), buffer(0), bitCount(0) {}

    // 写入一个位
    void writeBit(int bit) {
        buffer = (buffer << 1) | (bit & 1);
        bitCount++;
        
        if (bitCount == 8) {
            out.put(buffer);
            buffer = 0;
            bitCount = 0;
        }
    }

    // 写入一个字节（8位）
    void writeByte(unsigned char byte) {
        for (int i = 7; i >= 0; i--) {
            writeBit((byte >> i) & 1);
        }
    }

    // 刷新缓冲区（写入文件结束时调用）
    void flush() {
        if (bitCount > 0) {
            // 将剩余位左移对齐到字节
            buffer <<= (8 - bitCount);
            out.put(buffer);
            buffer = 0;
            bitCount = 0;
        }
    }

    ~BitWriter() {
        flush();
    }
};

// 位级读取器
class BitReader {
private:
    std::ifstream& in;
    unsigned char buffer;
    int bitCount;  // buffer中剩余的位数

public:
    BitReader(std::ifstream& input) : in(input), buffer(0), bitCount(0) {}

    // 读取一个位
    int readBit() {
        if (bitCount == 0) {
            char byte;
            if (!in.get(byte)) {
                return -1;  // 文件结束
            }
            buffer = static_cast<unsigned char>(byte);
            bitCount = 8;
        }
        
        int bit = (buffer >> 7) & 1;
        buffer <<= 1;
        bitCount--;
        return bit;
    }

    // 读取一个字节（8位）
    int readByte() {
        unsigned char byte = 0;
        for (int i = 0; i < 8; i++) {
            int bit = readBit();
            if (bit == -1) return -1;
            byte = (byte << 1) | bit;
        }
        return byte;
    }
};
