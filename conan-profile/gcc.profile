[settings]
os=Linux
arch=x86_64
compiler=gcc
compiler.version=15
compiler.libcxx=libstdc++11
compiler.cppstd=23

[options]
# 禁止所有 shared 配置
*:shared=False

[conf]
# 因為 mysql-connector 忘記添加導致編譯錯誤
# tools.build:cxxflags=["-include", "cstdint"]
