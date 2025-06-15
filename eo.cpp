#include <iostream>      // Standard Input/Output Stream: Provides functionality for input (cin) and output (cout, cerr) operations
#include <string>       // String Library: Supports string manipulation, such as std::string for text handling
#include <vector>       // Vector Container: Provides a dynamic array (std::vector) for storing and manipulating sequences of elements
#include <sstream>      // String Stream: Enables string-based stream operations (e.g., std::stringstream) for parsing/formatting
#include <algorithm>    // Algorithms Library: Contains functions for operations like sorting, searching, and transforming containers
#include <fstream>      // File Stream: Supports file input/output operations (e.g., std::ifstream, std::ofstream)
#include <regex>        // Regular Expressions: Provides std::regex for pattern matching and text manipulation
#include "httplib.h"    // HTTP Library: External library for HTTP client/server operations (e.g., making HTTP requests)
#include "nlohmann/json.hpp" // JSON Library: External library for parsing and manipulating JSON data
#include <unistd.h>     // POSIX API: Provides access to POSIX system calls (e.g., sleep, getpid) for Unix-like systems
#include <cstdlib>      // C Standard Library: Includes functions for general utilities (e.g., rand, exit, atoi)
#include <sys/ioctl.h>  // System I/O Control: Provides access to terminal size information

// Define an enumeration for supported input formats
enum class Format { JSON, TABLE, PLAIN_TEXT };

/**
 * @brief Retrieves the current terminal width in characters.
 * @return The number of characters that fit in the terminal width, or a default value (80) if unable to retrieve.
 */
int get_terminal_width() {
    struct winsize w;
    if (ioctl(STDOUT_FILENO, TIOCGWINSZ, &w) == -1) {
        return 80; // Default width if retrieval fails
    }
    return w.ws_col;
}

/**
 * @brief Displays the help message for the program.
 */
void display_help() {
    std::cout << "Usage: <command> | eo [options]\n"
              << "\n"
              << "eo enhances command-line output by analyzing and formatting it using an AI model.\n"
              << "The program accepts output from another command via a pipe, detects its format\n"
              << "(JSON, table, or plain text), and provides a formatted and AI-enhanced version.\n"
              << "\n"
              << "Options:\n"
              << "  -h, --help        Display this help message and exit.\n"
              << "  --url=<URL>       Set the Ollama service URL (e.g., --url=http://localhost:11434).\n"
              << "                    The URL is saved to /etc/eo/config.txt for future use.\n"
              << "\n"
              << "Examples:\n"
              << "  echo '{\"key\": \"value\"}' | eo\n"
              << "  ls -l | eo\n"
              << "  eo --url=http://example.com:11434\n"
              << "\n"
              << "Notes:\n"
              << "  - The default URL is http://localhost:11434 if not specified or saved in /etc/eo/config.txt.\n"
              << "  - The program uses ANSI escape codes for colored and bold output in the terminal.\n"
              << "  - Supported input formats: JSON, table (space-separated), and plain text.\n";
}

/**
 * @brief Reads input from standard input (stdin) into a string.
 * @return The complete input as a string.
 */
std::string read_input() {
    std::stringstream buffer;
    buffer << std::cin.rdbuf();
    return buffer.str();
}

/**
 * @brief Detects the format of the input data (JSON, Table, or Plain Text).
 * @param input The input string to analyze.
 * @return The detected Format (JSON, TABLE, or PLAIN_TEXT).
 */
Format detect_format(const std::string& input) {
    if (input.empty()) return Format::PLAIN_TEXT;

    // Attempt to parse input as JSON
    try {
        auto j = nlohmann::json::parse(input);
        if (j.is_object() || j.is_array()) {
            return Format::JSON;
        }
    } catch (...) {}

    // Check for table structure by verifying consistent column counts
    std::istringstream iss(input);
    std::string line;
    std::vector<std::vector<std::string>> rows;
    bool is_table = true;
    while (std::getline(iss, line)) {
        std::istringstream line_stream(line);
        std::vector<std::string> fields;
        std::string field;
        while (line_stream >> field) fields.push_back(field);
        if (rows.empty()) {
            rows.push_back(fields);
        } else if (fields.size() != rows[0].size()) {
            is_table = false;
            break;
        } else {
            rows.push_back(fields);
        }
    }
    if (is_table && rows.size() > 1 && rows[0].size() > 1) {
        return Format::TABLE;
    }

    return Format::PLAIN_TEXT;
}

/**
 * @brief Formats JSON input with proper indentation, respecting terminal width.
 * @param input The raw JSON string.
 * @param terminal_width The width of the terminal in characters.
 * @return A formatted JSON string or an error message if invalid.
 */
std::string format_json(const std::string& input, int terminal_width) {
    try {
        auto j = nlohmann::json::parse(input);
        // Adjust indentation based on terminal width (e.g., use 2 spaces if terminal is narrow)
        int indent = terminal_width < 100 ? 2 : 4;
        return j.dump(indent);
    } catch (const nlohmann::json::exception& e) {
        return "Error: Invalid JSON ─ " + std::string(e.what());
    }
}

/**
 * @brief Formats table input into a neatly aligned table, respecting terminal width.
 * @param input The raw table string.
 * @param terminal_width The width of the terminal in characters.
 * @return A formatted table string with aligned columns.
 */
std::string format_table(const std::string& input, int terminal_width) {
    std::istringstream iss(input);
    std::string line;
    std::vector<std::vector<std::string>> rows;
    
    // Parse input into rows and fields
    while (std::getline(iss, line)) {
        std::regex space_regex("\\s+");
        line = std::regex_replace(line, space_regex, " ");
        std::istringstream line_stream(line);
        std::vector<std::string> fields;
        std::string field;
        while (std::getline(line_stream, field, ' ')) {
            if (!field.empty()) fields.push_back(field);
        }
        if (!fields.empty()) rows.push_back(fields);
    }
    if (rows.empty()) return "";

    // Calculate maximum width for each column, ensuring total width fits terminal
    std::vector<size_t> col_widths(rows[0].size(), 0);
    for (const auto& row : rows) {
        for (size_t i = 0; i < row.size(); ++i) {
            col_widths[i] = std::max(col_widths[i], row[i].size());
        }
    }

    // Adjust column widths to fit within terminal width
    size_t total_width = 0;
    for (size_t w : col_widths) total_width += w + 2; // +2 for padding
    if (total_width > static_cast<size_t>(terminal_width)) {
        size_t excess = total_width - terminal_width + col_widths.size(); // Account for padding
        size_t reduce_per_col = excess / col_widths.size() + 1;
        for (size_t& w : col_widths) {
            w = std::max<size_t>(w - reduce_per_col, 5); // Ensure minimum width of 5
        }
    }

    // Build formatted table output
    std::stringstream ss;
    for (size_t i = 0; i < rows.size(); ++i) {
        for (size_t j = 0; j < rows[i].size(); ++j) {
            std::string cell = rows[i][j].substr(0, col_widths[j]); // Truncate if too long
            ss << std::left << std::setw(col_widths[j] + 2) << cell;
        }
        ss << '\n';
    }
    return ss.str();
}

/**
 * @brief Retrieves the Ollama service URL from config or command-line arguments.
 * @param argc Number of command-line arguments.
 * @param argv Array of command-line arguments.
 * @return The URL to use for the Ollama service.
 */
std::string get_url(int argc, char* argv[]) {
    std::string url = "http://localhost:11434"; // Default URL
    std::ifstream config_file("/etc/eo/config.txt");
    if (config_file.is_open()) {
        std::getline(config_file, url);
        config_file.close();
    }
    for (int i = 1; i < argc; ++i) {
        std::string arg = argv[i];
        if (arg.find("--url=") == 0) {
            url = arg.substr(6);
            std::ofstream config_file("/etc/eo/config.txt");
            if (config_file.is_open()) {
                config_file << url;
                config_file.close();
            }
        }
    }
    return url;
}

/**
 * @brief Checks if the Ollama service is running and retrieves available models.
 * @param url The Ollama service URL.
 * @param models JSON object to store the retrieved models.
 * @return True if the service is running and models are retrieved, false otherwise.
 */
bool check_service(const std::string& url, nlohmann::json& models) {
    httplib::Client cli(url);
    auto res = cli.Get("/api/tags");
    if (res && res->status == 200) {
        try {
            models = nlohmann::json::parse(res->body);
            return true;
        } catch (const nlohmann::json::exception& e) {
            std::cerr << "\033[31mError parsing models data: " << e.what() << "\033[0m" << std::endl;
            return false;
        }
    } else {
        std::cerr << "\033[31mOllama service not started or invalid url\033[0m" << std::endl;
        return false;
    }
}

/**
 * @brief Unescapes special characters in a string (e.g., \n, \t).
 * @param input The input string with escape sequences.
 * @return The unescaped string.
 */
std::string unescape_string(const std::string& input) {
    std::string output;
    output.reserve(input.size());
    for (size_t i = 0; i < input.size(); ++i) {
        if (input[i] == '\\' && i + 1 < input.size()) {
            switch (input[i + 1]) {
                case '\\': output += '\\'; ++i; break;
                case 'n': output += '\n'; ++i; break;
                case 't': output += '\t'; ++i; break;
                case 'r': output += '\r'; ++i; break;
                case '0': // Handle \033 for ANSI escape
                    if (i + 3 < input.size() && input[i + 2] == '3' && input[i + 3] == '3') {
                        output += '\033'; i += 3; break;
                    }
                    output += input[i]; break;
                default: output += input[i]; break;
            }
        } else {
            output += input[i];
        }
    }
    return output;
}

/**
 * @brief Enhances input data using an AI model via the Ollama service.
 * @param prompt The prompt to send to the AI model.
 * @param url The Ollama service URL.
 * @param models JSON object containing available models.
 * @param terminal_width The width of the terminal in characters.
 * @return The AI-enhanced response or an error message.
 */
std::string enhance_with_ai(const std::string& prompt, const std::string& url, const nlohmann::json& models, int terminal_width) {
    httplib::Client cli(url);
    
    // Select the first available model from the models list
    std::string model_name;
    if (models.contains("models") && models["models"].is_array() && !models["models"].empty()) {
        model_name = models["models"][0]["name"].get<std::string>();
    } else {
        std::cerr << "\033[31mNo models available in the Ollama service\033[0m" << std::endl;
        return "Error: No models available";
    }

    // Prepare the payload for the AI request, including terminal width
    nlohmann::json payload = {
        {"model", model_name},
        {"prompt", prompt + "\n\nThe terminal width is " + std::to_string(terminal_width) + " characters. Ensure the output is formatted to fit within this width for symmetry and readability."},
        {"stream", false}
    };

    // Send the request to the Ollama service
    auto res = cli.Post("/api/generate", payload.dump(), "application/json");
    if (!res || res->status != 200) {
        std::cerr << "\033[31mHTTP request failed: Status " << (res ? res->status : 0) << ", Body: " << (res ? res->body : "No response") << "\033[0m" << std::endl;
        return "Error: AI server issue";
    }

    try {
        // Parse the AI response
        auto json_res = nlohmann::json::parse(res->body);
        if (!json_res.contains("response")) {
            std::cerr << "\033[31mAI response missing 'response' field: " << res->body << "\033[0m" << std::endl;
            return "Error: No 'response' in AI output";
        }

        // Process the AI response
        std::string response = json_res["response"].get<std::string>();
        // Remove <think> tags and their contents
        std::regex think_regex("<think>[^<]*</think>", std::regex::multiline);
        response = std::regex_replace(response, think_regex, "");
        response = unescape_string(response);

        // Remove trailing notes
        std::regex note_regex("\n*Note:.*$", std::regex::multiline);
        response = std::regex_replace(response, note_regex, "");

        // Remove triple backticks and their contents (including language specifiers and edge cases)
        std::regex backtick_regex("```[a-zA-Z0-9]*\\n?[^`]*?```", std::regex::multiline);
        response = std::regex_replace(response, backtick_regex, "");
        // Remove standalone, partial, or empty backticks
        std::regex standalone_backtick_regex("```[a-zA-Z0-9]*\\n?|\\n?```", std::regex::multiline);
        response = std::regex_replace(response, standalone_backtick_regex, "");

        // Apply ANSI formatting for bold text
        std::regex bold_regex("\\*\\*([^\\*]+)\\*\\*");
        response = std::regex_replace(response, bold_regex, "\033[1m$1\033[0m");

        // Apply ANSI color formatting for colored bold text
        std::map<std::string, std::string> color_codes = {
            {"red", "\033[31m"},
            {"green", "\033[32m"},
            {"yellow", "\033[33m"},
            {"blue", "\033[34m"}
        };
        for (const auto& [color, code] : color_codes) {
            // Corrected regex: matches color[**text**] (e.g., yellow[**All Clear!**])
            std::regex color_bold_regex(color + "\\$$   \\*\\*([^\\*]+)\\*\\*\\   $$", std::regex::multiline);
            response = std::regex_replace(response, color_bold_regex, code + "\033[1m$1\033[0m");
        }

        // Clean up table formatting
        std::regex table_border_regex("\\|_+\\|");
        response = std::regex_replace(response, table_border_regex, "");
        std::regex table_header_regex("\\|[- ]+\\|[- ]+\\|");
        response = std::regex_replace(response, table_header_regex, "");
        std::regex table_row_start("\\| ");
        response = std::regex_replace(response, table_row_start, "");
        std::regex table_row_end(" \\|");
        response = std::regex_replace(response, table_row_end, "");
        std::regex table_cell_divider(" \\| ");
        response = std::regex_replace(response, table_cell_divider, "  ");

        // Trim leading/trailing whitespace
        response.erase(0, response.find_first_not_of(" \n\r\t"));
        response.erase(response.find_last_not_of(" \n\r\t") + 1);

        return response;
    } catch (const nlohmann::json::exception& e) {
        std::cerr << "\033[31mJSON parsing error: " << e.what() << ", Body: " << res->body << "\033[0m" << std::endl;
        return "Error: Invalid AI response";
    }
}

/**
 * @brief Main program entry point.
 * @param argc Number of command-line arguments.
 * @param argv Array of command-line arguments.
 * @return Exit status (0 for success, 1 for failure).
 */
int main(int argc, char* argv[]) {
    bool use_colors = true; // Enable color output by default
    std::string url = "http://localhost:11434"; // Default URL
    int terminal_width = get_terminal_width(); // Get terminal width

    // Check for --help or -h
    for (int i = 1; i < argc; ++i) {
        std::string arg = argv[i];
        if (arg == "--help" || arg == "-h") {
            display_help();
            return 0;
        }
    }

    // Retrieve URL from config or arguments
    url = get_url(argc, argv);
    nlohmann::json models;

    // Verify Ollama service is running
    if (!check_service(url, models)) {
        return 1;
    }

    // Read and process input
    std::string input = read_input();
    if (input.empty()) {
        std::cout << "No input provided." << std::endl;
        return 0;
    }

    // Detect input format and prepare output
    Format format = detect_format(input);
    std::string formatted_output;
    std::string ai_prompt;

    // Handle input based on detected format
    if (format == Format::JSON) {
        formatted_output = format_json(input, terminal_width);
        ai_prompt = "Act as a data analyst !(do NOT talk to me; ej:'Here's your....). Transform the provided JSON data into a highly readable and visually appealing format suitable for a terminal, summarizing the information and removing unnecessary data. Use ANSI escape codes according to your criteria in the output for formatting (e.g., \\033[31m for red, \\033[32m for green, \\033[33m for yellow, \\033[34m for blue, \\033[1m for bold, \\033[0m to reset). For text wrapped in ** (e.g., **something**), apply bold formatting. For text prefixed with a color name followed by [**text**] (e.g., yellow[**All Clear!**]), apply the specified color and bold formatting. Supported colors are red (\\033[31m), green (\\033[32m), yellow (\\033[33m), blue (\\033[34m). Use icons (e.g., ★, ►, ✔) or emojis for clarity. Do not use markdown code blocks or any markdown formatting; output plain text with ANSI codes only !(Do NOT give icon legends). At the end, append a concise analysis of the data, including key points, patterns, trends, or notable observations, and provide the AI's opinions or interpretations of the data's implications. The terminal width is " + std::to_string(terminal_width) + " characters. Ensure the output is formatted to fit within this width for symmetry and readability. Here's the data:\n\n" + input;
    } else if (format == Format::TABLE) {
        formatted_output = format_table(input, terminal_width);
        ai_prompt = "Act as a data analyst !(do NOT talk to me; ej:'Here's your....). Transform the provided table data into a highly readable and visually appealing format suitable for a terminal, summarizing the information and removing unnecessary data. Use ANSI escape codes according to your criteria in the output for formatting (e.g., \\033[31m for red, \\033[32m for green, \\033[33m for yellow, \\033[34m for blue, \\033[1m for bold, \\033[0m to reset). For text wrapped in ** (e.g., **something**), apply bold formatting. For text prefixed with a color name followed by [**text**] (e.g., yellow[**All Clear!**]), apply the specified color and bold formatting. Supported colors are red (\\033[31m), green (\\033[32m), yellow (\\033[33m), blue (\\033[34m). Use icons (e.g., ★, ►, ✔) or emojis for clarity. Do not use markdown code blocks or any markdown formatting; output plain text with ANSI codes only !(Do NOT give icon legends). At the end, append a concise analysis of the data, including key points, patterns, trends, or notable observations, and provide the AI's opinions or interpretations of the data's implications. The terminal width is " + std::to_string(terminal_width) + " characters. Ensure the output is formatted to fit within this width for symmetry and readability. Here's the data:\n\n" + input;
    } else {
        ai_prompt = "Act as a command-line output enhancer !(do NOT talk to me; ej:'Here's your....). Transform the raw output from a command into a highly readable and visually appealing format suitable for a terminal, summarizing the information and removing unnecessary data. Use ANSI escape codes according to your criteria in the output for formatting (e.g., \\033[31m for red, \\033[32m for green, \\033[33m for yellow, \\033[34m for blue, \\033[1m for bold, \\033[0m to reset). For text wrapped in ** (e.g., **something**), apply bold formatting. For text prefixed with a color name followed by [**text**] (e.g., yellow[**All Clear!**]), apply the specified color and bold formatting. Supported colors are red (\\033[31m), green (\\033[32m), yellow (\\033[33m), blue (\\033[34m). Use icons (e.g., ★, ►, ✔) or emojis for clarity. Do not use markdown code blocks or any markdown formatting; output plain text with ANSI codes only !(Do NOT give icon legends). The terminal width is " + std::to_string(terminal_width) + " characters. Ensure the output is formatted to fit within this width for symmetry and readability. Here's the output to enhance:\n\n" + input;
    }

    // Get AI-enhanced response
    std::string ai_response = enhance_with_ai(ai_prompt, url, models, terminal_width);

    // Output results based on format
    if (format == Format::JSON || format == Format::TABLE) {
        std::cout << formatted_output << "\n\n" << ai_response << std::endl;
    } else {
        std::cout << ai_response << std::endl;
    }

    return 0;
}