# 本目录维护约定

用户要求：以后发生更改时同步更新 README_muon_stalactite_full.md。

- 修改 C++、CMake、build.sh、run.sh、默认 config.mac、材料定义或输出格式时，同步核对并更新本目录 README_muon_stalactite_full.md。
- 保持 WSL 操作命令、配置参数表、默认值、单位、统计口径与实际代码一致。
- 更新验证状态时区分当前版本与历史运行，不把 sampler 测试或旧版产物当作当前版本完整模拟验证。
- 保留用户模拟输出；不得为重跑自动删除现有输出目录。
