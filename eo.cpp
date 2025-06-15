#include <iostream>
#include <string>
#include <vector>
#include <sstream>
#include <algorithm>
#include <fstream>
#include <regex>
#include "httplib.h"
#include "nlohmann/json.hpp"
#include <unistd.h>
#include <cstdlib>
#include <iostream>

enum class Format { JSON, TABLE, PLAIN_TEXT };

std::string read_input() {
    std::stringstream buffer;
    buffer << std::cin.rdbuf();
    return buffer.str();
}

Format detect_format(const std::string& input) {
    if (input.empty()) return Format::PLAIN_TEXT;

    // Verificar si es JSON
    if (input[0] == '{' || input[0] == '[') {
        try {
            auto j = nlohmann::json::parse(input);
            return Format::JSON;
        } catch (...) {}
    }

    // Verificar si es salida de nvme smart-log
    std::istringstream iss(input);
    std::string line;
    std::getline(iss, line);
    if (line.find("Metric") != std::string::npos && line.find("Value") != std::string::npos) {
        return Format::TABLE;
    }

    // Lógica para otras tablas
    iss = std::istringstream(input); // Reiniciar el stream
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

std::string format_json(const std::string& input) {
    try {
        auto j = nlohmann::json::parse(input);
        return j.dump(4);
    } catch (const nlohmann::json::exception& e) {
        return "Error: Invalid JSON ─ " + std::string(e.what());
    }
}

std::string format_table(const std::string& input) {
    std::istringstream iss(input);
    std::string line;
    std::vector<std::vector<std::string>> rows;
    while (std::getline(iss, line)) {
        // Normalizar espacios para nvme smart-log
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

    std::vector<size_t> col_widths(rows[0].size(), 0);
    for (const auto& row : rows) {
        for (size_t i = 0; i < row.size(); ++i) {
            col_widths[i] = std::max(col_widths[i], row[i].size());
        }
    }

    std::stringstream ss;
    for (size_t i = 0; i < rows.size(); ++i) {
        for (size_t j = 0; j < rows[i].size(); ++j) {
            ss << std::left << std::setw(col_widths[j] + 2) << rows[i][j];
        }
        ss << '\n';
    }
    return ss.str();
}

std::string get_url(int argc, char* argv[]) {
    std::string url = "http://localhost:11434";
    std::ifstream config_file("config.txt");
    if (config_file.is_open()) {
        std::getline(config_file, url);
        config_file.close();
    }
    for (int i = 1; i < argc; ++i) {
        std::string arg = argv[i];
        if (arg.find("--url=") == 0) {
            url = arg.substr(6);
            std::ofstream config_file("config.txt");
            if (config_file.is_open()) {
                config_file << url;
                config_file.close();
            }
        }
    }
    return url;
}

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

std::string enhance_with_ai(const std::string& prompt, const std::string& url, const nlohmann::json& models, bool use_colors) {
    httplib::Client cli(url);
    std::string model_name = models["models"].size() > 0 ? models["models"][0]["name"].get<std::string>() : "llama3:8b-instruct-q4_0";
    nlohmann::json payload = {
        {"model", model_name},
        {"prompt", prompt},
        {"stream", false}
    };

    auto res = cli.Post("/api/generate", payload.dump(), "application/json");
    if (!res || res->status != 200) {
        std::cerr << "HTTP request failed: Status " << (res ? res->status : 0) << ", Body: " << (res ? res->body : "No response") << std::endl;
        return "Error: AI server issue";
    }

    try {
        auto json_res = nlohmann::json::parse(res->body);
        if (!json_res.contains("response")) {
            std::cerr << "AI response missing 'response' field: " << res->body << std::endl;
            return "Error: No 'response' in AI output";
        }

        std::string response = json_res["response"].get<std::string>();

        // Unescape the response
        response = unescape_string(response);

        std::regex note_regex("\n*Note:.*$", std::regex::multiline);
        response = std::regex_replace(response, note_regex, "");
        std::regex bold_regex("\\*\\*([^\\*]+)\\*\\*");
        response = std::regex_replace(response, bold_regex, use_colors ? "\033[1m$1\033[0m" : "$1");
        std::map<std::string, std::string> color_codes = {
            {"red", "\033[31m"},
            {"green", "\033[32m"},
            {"yellow", "\033[33m"},
            {"blue", "\033[34m"}
        };
        for (const auto& [color, code] : color_codes) {
            std::regex color_bold_regex(color + "\\[\\*\\*([^\\*]+)\\*\\*\\]");
            response = std::regex_replace(response, color_bold_regex, use_colors ? code + "\033[1m$1\033[0m" : "$1");
        }

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
        return response;
    } catch (const nlohmann::json::exception& e) {
        std::cerr << "JSON parsing error: " << e.what() << ", Body: " << res->body << std::endl;
        return "Error: Invalid AI response";
    }
}

int main(int argc, char* argv[]) {
    bool use_colors = isatty(STDOUT_FILENO) || getenv("FORCE_COLOR") != nullptr;
    std::string url = get_url(argc, argv);
    nlohmann::json models;
    if (!check_service(url, models)) {
        return 1;
    }

    std::string input = read_input();
    if (input.empty()) {
        std::cout << "No input provided." << std::endl;
        return 0;
    }

    Format format = detect_format(input);
    std::string formatted_output;
    std::string ai_prompt;

    if (format == Format::JSON) {
        formatted_output = format_json(input);
        ai_prompt = "Act as a data analyst do not talk to me. Analyze the provided JSON data and provide concise insights or conclusions. Highlight key points, patterns, trends, or notable observations. Do not repeat the data; focus on interpretation. Here's the data:\n\n" + input;
    } else if (format == Format::TABLE) {
        formatted_output = format_table(input);
        ai_prompt = "Act as a data analyst do not talk to me. Analyze the provided table data and provide concise, actionable insights or conclusions. identify potential issues, and suggest next steps if applicable. Do not repeat the data; focus on interpretation. Do not use markdown code blocks; output plain text only. Here's the data:\n\n" + input;
    } else {
        ai_prompt = "Act as a command-line output enhancer do not talk to me. Transform the raw output from a command into a highly readable and visually appealing format suitable for a terminal and removing unnecessary data (resume the information), using ANSI escape codes (e.g., \\033[31m for red, \\033[32m for green, \\033[33m for yellow, \\033[34m for blue, \\033[1m for bold, \\033[0m to reset). For text wrapped in ** (e.g., **something**), apply bold formatting. For text prefixed with a color name followed by [**text**] (e.g., yellow[**All Clear!**]), apply the specified color and bold formatting. Supported colors are red (\\033[31m), green (\\033[32m), yellow (\\033[33m), blue (\\033[34m). Use icons (e.g., ★, ►, ✔) or emojis for clarity. Do not use markdown code blocks (e.g., ```) or any markdown formatting; output plain text with ANSI codes only. Here's the output to enhance:\n\n" + input;
    }

    std::string ai_response = enhance_with_ai(ai_prompt, url, models, use_colors);

    if (format == Format::JSON || format == Format::TABLE) {
        std::cout << formatted_output << "\n\n" << ai_response << std::endl;
    } else {
        std::cout << ai_response << std::endl;
    }

    return 0;
}