mkdir -p ~/.local/bin
mkdir -p ~/.local/share/ls-interactive
touch ~/.local/share/ls-interactive/output

chmod +x bin/li-shell
cp bin/li-posix  ~/.local/bin
cp bin/li-shell  ~/.local/bin
cp bin/li-bashrc ~/.local/bin
