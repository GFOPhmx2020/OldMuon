// 独立测试输出层，不需要启动 Geant4；Release 下也显式检查，不使用会被禁用的 assert。
#include "OutputFile.hh"
#include <fstream>
#include <iostream>
#include <iterator>
#include <string>

int main(int argc, char** argv) {
    try {
        if (argc != 2) throw std::runtime_error("Expected test output directory");
        const std::filesystem::path root(argv[1]);
        std::filesystem::create_directories(root);
        std::string expected;
        OutputFile plain, zipped;
        plain.open(root / "test.tsv", false);
        zipped.open(root / "test.tsv.gz", true);
        for (int i=0; i<40000; ++i) {
            const auto line=std::to_string(i)+"\t0.00010000000000000001\tNa23\n";
            expected += line; plain << line; zipped << line;
            if (i==20000) { plain.flush(); zipped.flush(); }
        }
        plain.close(); zipped.close();
        std::ifstream f(root / "test.tsv", std::ios::binary);
        const std::string actual((std::istreambuf_iterator<char>(f)), {});
        gzFile g=gzopen((root / "test.tsv.gz").string().c_str(),"rb");
        if (!g) throw std::runtime_error("Cannot read gzip test output");
        std::string decoded; char block[8192]; int n;
        while ((n=gzread(g,block,sizeof(block)))>0) decoded.append(block,n);
        const int closed=gzclose(g);
        if (n<0 || closed!=Z_OK || actual!=expected || decoded!=expected)
            throw std::runtime_error("Lossless output roundtrip failed");
        // Linux 特殊设备用于模拟磁盘满，验证不能误报写入成功。
        if (std::filesystem::exists("/dev/full")) {
            bool failed=false;
            try { OutputFile full; full.open("/dev/full",false); full << "test"; full.close(); }
            catch (const std::exception&) { failed=true; }
            if (!failed) throw std::runtime_error("Write failure was not reported");
        }
        std::cout << "Output roundtrip and write-error checks passed\n";
        return 0;
    } catch (const std::exception& e) { std::cerr << e.what() << '\n'; return 1; }
}
