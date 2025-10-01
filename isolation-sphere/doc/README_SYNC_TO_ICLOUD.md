# Sync markdown docs to iCloud/Obsidian

This script helps create symlinks from the repository's `doc/` folder into your Obsidian vault on iCloud.

Usage (macOS zsh):

1. Ensure your Obsidian vault is in iCloud, e.g.:
   ~/Library/Mobile Documents/com~apple~CloudDocs/Obsidian/MyVault

   Example (your provided path):

   `/Users/katano/Library/Mobile Documents/iCloud~md~obsidian/Documents/obsidians/MFT2025`

2. Run the script from the repository root:

   ./scripts/sync_md_to_icloud.sh "~/Library/Mobile Documents/com~apple~CloudDocs/Obsidian/MyVault"

3. Open or refresh Obsidian; the linked files will appear in the vault structure.

Notes:

- The script will create subdirectories in the target to mirror `doc/` and create symlinks for every `.md` file.
- If a target file exists and is not a symlink, it will be backed up with a `.bak` suffix.
- Use with care; check the target path before running.
