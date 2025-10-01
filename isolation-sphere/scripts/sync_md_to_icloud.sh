#!/usr/bin/env zsh
# sync_md_to_icloud.sh
# Create symlinks for Markdown files under doc/ into a target iCloud/Obsidian vault path.
# Usage: ./scripts/sync_md_to_icloud.sh /path/to/Obsidian/Vault
# Example iCloud Obsidian path: ~/Library/Mobile Documents/com~apple~CloudDocs/Obsidian/MyVault

set -euo pipefail

if [ "$#" -ne 1 ]; then
  echo "Usage: $0 /absolute/path/to/ObsidianVault"
  exit 2
fi

TARGET_ROOT="$1"
# Expand ~ if present
TARGET_ROOT="${TARGET_ROOT/#~/$HOME}"

SCRIPT_DIR="$(cd "$(dirname "$0")" && pwd)"
REPO_ROOT="$(cd "$SCRIPT_DIR/.." && pwd)"
SRC_DIR="$REPO_ROOT/doc"

if [ ! -d "$SRC_DIR" ]; then
  echo "Source doc directory not found: $SRC_DIR"
  exit 3
fi

if [ ! -d "$TARGET_ROOT" ]; then
  echo "Target directory does not exist: $TARGET_ROOT"
  echo "Create it and re-run, or check your Vault path."
  exit 4
fi

echo "Source: $SRC_DIR"
echo "Target: $TARGET_ROOT"

echo "Creating directory tree in target..."
# Create subdirectories in target to mirror doc/
find "$SRC_DIR" -type d -print0 | while IFS= read -r -d $'\0' dir; do
  rel="${dir#$SRC_DIR/}"
  if [ "$rel" = "$dir" ]; then
    # top-level
    continue
  fi
  mkdir -p "$TARGET_ROOT/$rel"
done

# Create symlinks for .md files
echo "Linking markdown files..."
find "$SRC_DIR" -type f -name '*.md' -print0 | while IFS= read -r -d $'\0' file; do
  rel="${file#$SRC_DIR/}"
  dest="$TARGET_ROOT/$rel"
  destdir="$(dirname "$dest")"
  mkdir -p "$destdir"
  # If a file already exists and is not a symlink, back it up
  if [ -e "$dest" ] && [ ! -L "$dest" ]; then
    echo "Backing up existing file at $dest -> ${dest}.bak"
    mv "$dest" "${dest}.bak"
  fi
  ln -sf "$file" "$dest"
  echo "Linked: $dest -> $file"
done

echo "Done. Refresh Obsidian (or reopen vault) to pick up new files."

exit 0
