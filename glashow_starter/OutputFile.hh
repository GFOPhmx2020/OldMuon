#pragma once
// 带缓冲的无损文本输出：格式和 17 位浮点精度不变，默认用 gzip level 1 减少磁盘写入。
// gzip 是存储层：解压后的内容与未压缩 TSV 完全相同，不抽样、不丢弃字段。
#include <zlib.h>
#include <cstdio>
#include <filesystem>
#include <ostream>
#include <stdexcept>
#include <streambuf>
#include <vector>

class OutputBuffer final: public std::streambuf {
    std::vector<char> buffer{std::vector<char>(256 * 1024)};
    gzFile zipped = nullptr;
    FILE* plain = nullptr;
    bool writePending() {
        const auto n = static_cast<unsigned>(pptr() - pbase());
        if (n && ((zipped && gzwrite(zipped, pbase(), n) != static_cast<int>(n)) ||
                  (plain && std::fwrite(pbase(), 1, n, plain) != n) || (!zipped && !plain)))
            return false;
        setp(buffer.data(), buffer.data() + buffer.size());
        return true;
    }
protected:
    int_type overflow(int_type ch) override {
        if (!writePending()) return traits_type::eof();
        if (!traits_type::eq_int_type(ch, traits_type::eof())) {
            *pptr() = traits_type::to_char_type(ch);
            pbump(1);
        }
        return traits_type::not_eof(ch);
    }
    int sync() override {
        if (!writePending()) return -1;
        // 进度刷新时把可恢复的压缩前缀送到系统；这不是完整事件的标志。
        if (zipped) return gzflush(zipped, Z_SYNC_FLUSH) == Z_OK ? 0 : -1;
        return plain ? std::fflush(plain) : -1;
    }
public:
    OutputBuffer() { setp(buffer.data(), buffer.data() + buffer.size()); }
    ~OutputBuffer() override { close(); }
    void open(const std::filesystem::path& path, bool compress) {
        if (compress) {
            zipped = gzopen(path.string().c_str(), "wb1");
            if (zipped) gzbuffer(zipped, 256 * 1024);
        } else {
            plain = std::fopen(path.string().c_str(), "wb");
        }
        if (!zipped && !plain) throw std::runtime_error("Cannot open output: " + path.string());
    }
    bool close() noexcept {
        if (!zipped && !plain) return true;
        bool ok = writePending();
        if (zipped) { ok = (gzclose(zipped) == Z_OK) && ok; zipped = nullptr; }
        if (plain) { ok = (std::fclose(plain) == 0) && ok; plain = nullptr; }
        return ok;
    }
};

class OutputFile final: public std::ostream {
    OutputBuffer buffer;
public:
    OutputFile(): std::ostream(nullptr) { rdbuf(&buffer); exceptions(badbit | failbit); }
    void open(const std::filesystem::path& path, bool compress) { buffer.open(path, compress); }
    // 必须检查关闭错误：磁盘满/压缩尾部写入失败时不能标记 complete=true。
    void close() { if (!buffer.close()) throw std::runtime_error("Output close failed (disk full or I/O error)"); }
};
