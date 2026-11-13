# OldMuon 上传计划（当前方案）

按项目所有者最新决定：直接以当前 primus-paleo 目录为发布来源，不再使用精简文件清单，不再要求单独数据包，也不继续进行复现检查。历史输出已由所有者另行保存。

- 目标仓库：GFOPhmx2020/OldMuon，私有。
- 仓库根目录对应 primus-paleo 内部内容。
- 新仓库从当前文件快照开始，不带上游 Git 历史；保留原目录的 .git 不动。
- 保留许可证、上游署名和论文引用。
- 在独立副本中初始化新 Git 仓库，避免改动原克隆历史。
- 正式暂存仍会受 .gitignore 影响；“以整个目录为来源”不等于强制提交缓存、构建产物及本机环境配置。
- 尚未创建远程仓库或推送；上传命令在用户确认执行时提供。

此前 publish-manifest.tsv、publish-excluded.tsv、reproduction-data.tsv、REPRODUCING.md 是旧方案记录，不再作为上传依据。外层 output/OldMuon-release 下已生成的数据包无需用于新方案，暂保留且未删除。
