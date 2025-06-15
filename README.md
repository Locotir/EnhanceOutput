# EnhanceOutput (`eo`) 🌟

EnhanceOutput (`eo`) is a powerful command-line tool designed to integrate with [Ollama](https://ollama.ai) to transform raw terminal outputs into visually appealing, readable, and insightful results. Whether you're dealing with JSON data, tabular outputs, or plain text, `eo` automatically detects the format, enhances it with clean formatting and ANSI colors, and leverages AI to provide concise analysis or summaries. Perfect for developers, data analysts, and terminal enthusiasts who want to make sense of command outputs quickly and beautifully.

## ✨ Features

- **Smart Format Detection**: Automatically identifies input as JSON, tables, or plain text based on structure.
- **Enhanced Formatting**: Converts raw outputs into neatly formatted JSON, aligned tables, or styled plain text with ANSI colors and icons (e.g., ✔, ►, ★).
- **AI-Powered Insights**: Uses Ollama to generate concise analysis reports for JSON and tables, or summarizes and enhances plain text outputs.
- **Customizable Output**: Supports colorized terminal output with bold and colored text for emphasis.
- **Flexible Configuration**: Allows custom Ollama service URLs, saved persistently in a config file.
- **Lightweight & Fast**: Written in C++ with optimized dependencies for performance.

## 🛠 Requirements

To use `eo`, ensure you have the following:

- **Ollama**: Installed and running on `http://localhost:11434` (or a custom URL). [Get Ollama](https://ollama.ai).
- **C++ Dependencies**:
  - `nlohmann/json`: For JSON parsing and formatting.
  - `cpp-httplib`: For HTTP requests to the Ollama service.
  - `libcurl`: For HTTP client functionality.
- **Compiler**: A C++17-compatible compiler (e.g., `g++`, `clang++`).
- **Operating System**: Linux (Arch, Ubuntu, etc.), macOS, or Windows with terminal support.
- **Optional**: `vcpkg` or manual installation for dependency management.

## 📦 Universal install commands

Follow these steps to set up `eo` on your system:

1. **Clone the Repository**:
   ```bash
   git clone https://github.com/Locotir/EnhanceOutput
   cd EnhanceOutput
   ```

2. **Install System Dependencies**:
   - On Arch Linux:
     ```bash
     sudo pacman -S gcc make curl
     ```
   - On Ubuntu/Debian:
     ```bash
     sudo apt-get install g++ make libcurl4-openssl-dev
     ```

3. **Install C++ Dependencies**:
   - **Option 1: Use `vcpkg`** (recommended for simplicity):
     ```bash
     vcpkg install nlohmann-json cpp-httplib curl
     ```
   - **Option 2: Manual Installation**:
     - Download `nlohmann/json`:
       ```bash
       mkdir -p include/nlohmann
       wget https://github.com/nlohmann/json/releases/download/v3.11.3/json.hpp -O include/nlohmann/json.hpp
       ```
     - Download `cpp-httplib`:
       ```bash
       wget https://github.com/yhirose/cpp-httplib/archive/refs/tags/v0.15.3.tar.gz
       tar -xzf v0.15.3.tar.gz
       mv cpp-httplib-0.15.3/httplib.h include/httplib.h
       ```

4. **Compile the Tool**:
   ```bash
   g++ -o eo eo.cpp -I/usr/include/nlohmann -std=c++17 -lcurl -flto=auto
   ```

5. **Install the Binary** (optional, for system-wide use):
   ```bash
   sudo mv eo /usr/local/bin/
   sudo mkdir -p /etc/eo
   sudo touch /etc/eo/config.txt
   sudo chmod 666 /etc/eo/config.txt
   ```

6. **Verify Installation**:
   ```bash
   eo --help
   ```

## 🚀 Usage

Pipe any command output to `eo` to enhance and analyze it:

```bash
command | eo
```

2
### Custom Ollama URL
To use a custom Ollama service URL (saved to `/etc/eo/config.txt`):
```bash
cat data.json | eo --url=http://custom-url:11434
```

## ⚙️ Configuration

- **Ollama URL**: Stored in `/etc/eo/config.txt` and defaults to `http://localhost:11434`. Updated via the `--url` flag.
- **Color Output**: Enabled by default for all environments, using ANSI escape codes for bold and colored text.
- **Default Model**: Uses `llama3:8b-instruct-q4_0` if no models are specified by Ollama.

## 📝 Notes

- **Ollama Service**: Ensure Ollama is running before using `eo`. Test with:
  ```bash
  curl http://localhost:11434/api/tags
  ```
- **Permissions**: The config file (`/etc/eo/config.txt`) requires write permissions for URL updates.
- **Performance**: For large inputs, ensure sufficient memory and processing power.
- **Error Handling**: The tool provides clear error messages for invalid JSON, unavailable Ollama services, or parsing issues.

## 🤝 Contributing

We welcome contributions! To contribute:

1. Fork the repository.
2. Create a feature branch (`git checkout -b feature/awesome-feature`).
3. Commit your changes (`git commit -m "Add awesome feature"`).
4. Push to the branch (`git push origin feature/awesome-feature`).
5. Open a Pull Request.

Please report issues or suggest improvements via the [GitHub Issues](https://github.com/Locotir/EnhanceOutput/issues) page.

## 📜 License

This project is licensed under the [MIT License](LICENSE). See the `LICENSE` file for details.

## 🙏 Acknowledgments

- [Ollama](https://ollama.ai) for providing a robust AI platform.
- [nlohmann/json](https://github.com/nlohmann/json) for seamless JSON handling.
- [cpp-httplib](https://github.com/yhirose/cpp-httplib) for lightweight HTTP client functionality.

---

**Enhance your terminal experience with `eo`!** Try it out and let us know your feedback. 🚀