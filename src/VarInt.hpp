#pragma once

#include <cstdint>
#include <vector>
#include <fstream>

class VarInt {
public:
    // 编码：将uint32_t编码为变长字节序列
    static std::vector<uint8_t> encode(uint32_t value) {
        std::vector<uint8_t> bytes;
        
        while (value >= 0x80) {  // 128
            // 取低7位，MSB设为1
            bytes.push_back((value & 0x7F) | 0x80);
            value >>= 7;  // 右移7位
        }
        
        // 最后一个字节，MSB=0
        bytes.push_back(value & 0x7F);
        
        return bytes;
    }
    
    // 解码：从流中读取VarInt
    static uint32_t decode(std::istream& in) {
        uint32_t result = 0;
        int shift = 0;
        
        while (true) {
            uint8_t byte = in.get();
            
            if (in.eof()) {
                return result;
            }
            
            // 取低7位，拼到结果中
            result |= ((byte & 0x7F) << shift);
            
            // 如果MSB=0，结束
            if ((byte & 0x80) == 0) {
                break;
            }
            
            shift += 7;
        }
        
        return result;
    }
    
    // 写入VarInt到流
    static void write(std::ostream& out, uint32_t value) {
        auto bytes = encode(value);
        out.write(reinterpret_cast<const char*>(bytes.data()), bytes.size());
    }
    
    // 计算编码后的字节数（不实际编码）
    static size_t encodedSize(uint32_t value) {
        size_t size = 0;
        while (value >= 0x80) {
            size++;
            value >>= 7;
        }
        return size + 1;
    }
};
