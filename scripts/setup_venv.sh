#!/usr/bin/env fish
# Setup portable Python virtual environment for nanopb development

set SCRIPT_DIR (cd (dirname (status --current-filename)) && pwd)
set PROJECT_ROOT (dirname $SCRIPT_DIR)
set VENV_DIR $PROJECT_ROOT/.venv

echo "Creating Python virtual environment at $VENV_DIR..."
python3 -m venv $VENV_DIR

echo "Activating venv..."
source $VENV_DIR/bin/activate.fish

echo "Installing nanopb dependencies..."
pip install --upgrade pip setuptools wheel
pip install protobuf grpcio-tools
pip install -r $PROJECT_ROOT/lib/nanopb/requirements.txt

echo "Creating protoc symlink..."
ln -sf python-grpc-tools-protoc $VENV_DIR/bin/protoc

echo ""
echo "✓ Virtual environment ready!"
echo ""
echo "To activate the venv, run:"
echo "  source .venv/bin/activate.fish"
echo ""
echo "To deactivate, run:"
echo "  deactivate"
