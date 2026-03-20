#!/bin/bash
# Скрипт для автоматического запуска LIT-тестов

VENV_DIR="$HOME/lit-env"

if [ ! -d "$VENV_DIR" ]; then
    echo "Creating virtual environment and installing lit..."
    python3 -m venv "$VENV_DIR"
    source "$VENV_DIR/bin/activate"
    pip install lit
else
    source "$VENV_DIR/bin/activate"
fi

lit test
