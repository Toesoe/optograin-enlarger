#!/bin/bash
# Wrapper script to run protoc with venv environment

VENV_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)/.venv"
PYTHON_SITE_PACKAGES="$($VENV_DIR/bin/python3 -c 'import site; print(site.getsitepackages()[0])')"

export PYTHONPATH="$PYTHON_SITE_PACKAGES:$PYTHONPATH"
exec "$VENV_DIR/bin/python-grpc-tools-protoc" "$@"
