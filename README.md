# EasyCompress

基于哈夫曼编码的文件压缩/解压工具。

## 编译

```bash
mkdir build && cd build
cmake ..
make
```

## 使用方法

### 压缩文件
```bash
./huffman_tree -c <文件/文件夹>
# 示例：
./huffman_tree -c ../tests/test.txt
./huffman_tree -c ../tests/
```

### 解压文件
```bash
./huffman_tree -d <压缩文件>
# 示例：
./huffman_tree -d ../tests/test.huf
```

