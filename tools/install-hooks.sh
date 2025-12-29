#!/usr/bin/env bash
# Git Hooks 安裝腳本
#
# 用途：將 tools/hooks/ 中的 hook 模板複製到 .git/hooks/
#
# 使用方式：
#   ./tools/install-hooks.sh

set -euo pipefail
SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
REPO_ROOT="$(cd "${SCRIPT_DIR}/.." && pwd)"
HOOKS_SRC="${SCRIPT_DIR}/hooks"
HOOKS_DST="${REPO_ROOT}/.git/hooks"
echo "Git Hooks Installer for TX Trading Engine"
echo "=========================================="
echo ""
# 檢查是否在 Git repo 中
if [[ ! -d "${REPO_ROOT}/.git" ]]; then
  echo "ERROR: Not a git repository"
  echo "Please run this script from the repository root"
  exit 1
fi
# 檢查 hooks 來源目錄
if [[ ! -d "${HOOKS_SRC}" ]]; then
  echo "ERROR: Hooks source directory not found: ${HOOKS_SRC}"
  exit 1
fi
# 安裝 hooks
installed_count=0
for hook_file in "${HOOKS_SRC}"/*; do
  if [[ -f "${hook_file}" ]]; then
    hook_name=$(basename "${hook_file}")
    dst_file="${HOOKS_DST}/${hook_name}"

    # 檢查是否已存在
    if [[ -f "${dst_file}" ]]; then
      echo "WARNING: ${hook_name} already exists"
      read -p "Overwrite? (y/N): " -n 1 -r
      echo
      if [[ ! $REPLY =~ ^[Yy]$ ]]; then
        echo "Skipped: ${hook_name}"
        continue
      fi
    fi

    # 複製並設定執行權限
    cp "${hook_file}" "${dst_file}"
    chmod +x "${dst_file}"

    echo "Installed: ${hook_name}"
    ((installed_count++))
  fi
done
echo ""
echo "=========================================="
echo "Installed ${installed_count} hook(s) successfully!"
echo ""
echo "Available hooks:"
ls -1 "${HOOKS_DST}" | grep -v ".sample" || echo "  (none)"
echo ""
echo "To skip pre-commit checks:"
echo "  SKIP_PRECOMMIT=1 git commit -m \"message\""
echo ""
