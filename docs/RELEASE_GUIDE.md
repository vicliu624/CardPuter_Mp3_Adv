# 发布指南 (Release Guide)

本指南说明如何将编译好的固件发布到 GitHub Releases。

## 方式一：使用自动发布脚本（推荐）

### 步骤

1. **确保当前在 main 分支且工作区干净**
   ```powershell
   git checkout main
   git status
   ```

2. **运行发布脚本**
   ```powershell
   .\scripts\release.ps1
   ```
   
   脚本会自动：
   - 从 `VERSION` 文件读取版本号（当前：2.2.0）
   - 更新 `include/config.hpp` 中的版本信息
   - 编译固件（`pio run -e m5stack-cardputer`）
   - 创建 `releases/v2.2.0/` 目录
   - 复制固件到 `releases/v2.2.0/firmware_v2.2.0.bin`
   - 从 `CHANGELOG.md` 生成发布说明
   - 创建 git tag `v2.2.0`

3. **推送到 GitHub**
   ```powershell
   git push origin main
   git push origin v2.2.0
   ```

4. **创建 GitHub Release（两种方式）**

   **方式 A：使用 GitHub Actions（自动）**
   - 如果已配置 GitHub Actions（`.github/workflows/release.yml`），推送 tag 后会自动：
     - 在 GitHub Actions 中重新构建
     - 自动创建 Release
     - 上传固件文件
   - 查看 Actions：https://github.com/vicliu624/CardPuter_Mp3_Adv/actions

   **方式 B：手动创建 Release**
   - 访问：https://github.com/vicliu624/CardPuter_Mp3_Adv/releases/new
   - Tag: 选择 `v2.2.0`
   - Title: `Release v2.2.0`
   - Description: 复制 `releases/v2.2.0/RELEASE_NOTES.md` 的内容
   - 上传文件: 选择 `releases/v2.2.0/firmware_v2.2.0.bin`
   - 点击 "Publish release"

## 方式二：手动发布

### 步骤

1. **编译固件**
   ```powershell
   pio run -e m5stack-cardputer
   ```

2. **找到编译好的固件**
   - 路径：`.pio\build\m5stack-cardputer\firmware.bin`

3. **创建发布目录（可选）**
   ```powershell
   mkdir releases\v2.2.0
   copy .pio\build\m5stack-cardputer\firmware.bin releases\v2.2.0\firmware_v2.2.0.bin
   ```

4. **创建 git tag**
   ```powershell
   git tag -a v2.2.0 -m "Release v2.2.0"
   git push origin v2.2.0
   ```

5. **在 GitHub 上创建 Release**
   - 访问：https://github.com/vicliu624/CardPuter_Mp3_Adv/releases/new
   - 选择 tag `v2.2.0`
   - 填写标题和描述（可从 `CHANGELOG.md` 复制）
   - 上传 `firmware.bin` 文件
   - 发布

## 方式三：使用 GitHub Actions 自动发布（完全自动化）

如果你已经配置了 GitHub Actions（`.github/workflows/release.yml`），只需要：

1. **创建并推送 tag**
   ```powershell
   git tag -a v2.2.0 -m "Release v2.2.0"
   git push origin v2.2.0
   ```

2. **GitHub Actions 会自动：**
   - 检测到 tag 推送
   - 在云端构建固件
   - 创建 GitHub Release
   - 上传固件文件

3. **查看进度**
   - 访问：https://github.com/vicliu624/CardPuter_Mp3_Adv/actions

## 注意事项

- 确保 `VERSION` 文件中的版本号正确
- 确保 `CHANGELOG.md` 中有对应版本的更新日志
- 发布前建议先测试固件
- 如果使用 GitHub Actions，需要确保仓库已启用 Actions

## 当前版本

当前版本：**2.2.0**

查看版本历史：`CHANGELOG.md`

