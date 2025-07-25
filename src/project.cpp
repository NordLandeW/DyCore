
#include "project.h"

#include <zstd.h>

#include <cstring>
#include <fstream>
#include <iostream>
#include <memory>

#include "api.h"
#include "async.h"
#include "compress.h"
#include "note.h"
#include "utils.h"

int verify_project(string projectStr) {
    print_debug_message("Verifying project property...");
    try {
        json j = json::parse(projectStr);
        if (!j.contains("charts") || !j["charts"].contains("notes") ||
            !j.contains("version") || !j["version"].is_string()) {
            print_debug_message("Invalid project property: " + projectStr);
            return -1;
        }
    } catch (json::exception& e) {
        print_debug_message("Parse failed:" + string(e.what()));
        return -1;
    }
    return 0;
}

int verify_project_buffer(const char* buffer, size_t size) {
    if (!check_compressed(buffer, size)) {
        return verify_project(buffer);
    }
    return verify_project(decompress_string(buffer, size));
}

void __async_save_project(SaveProjectParams params) {
    namespace fs = std::filesystem;

    bool err = false;
    string errInfo = "";
    string projectString = "";
    try {
        projectString = get_project_string(params.projectProp);
        if (projectString == "" || verify_project(projectString) != 0) {
            print_debug_message("Invalid saving project property.");
            push_async_event(
                {PROJECT_SAVING, -1,
                 "Invalid project format. projectString: " + projectString +
                     "\nprojectProp: " + params.projectProp});
            return;
        }
    } catch (const std::exception& e) {
        print_debug_message("Encounter unknown errors. Details:" +
                            string(e.what()));
        push_async_event({PROJECT_SAVING, -1});
        return;
    }

    auto chartBuffer =
        std::make_unique<char[]>(compress_bound(projectString.size()));
    fs::path finalPath, tempPath;

    try {
        size_t cSize = get_project_buffer(projectString, chartBuffer.get(),
                                          params.compressionLevel);

        print_debug_message("filePath in hex:");
        for (size_t i = 0; i < params.filePath.size(); i++) {
            std::cout << std::hex << (int)params.filePath[i] << " ";
        }
        std::cout << std::endl;

        // Write to file using std::ofstream
        print_debug_message("Open file at:" + params.filePath);
        finalPath = fs::path(std::u8string((char8_t*)params.filePath.c_str()));
        tempPath = finalPath.parent_path() /
                   (finalPath.filename().string() + random_string(8) + ".tmp");
        std::ofstream file(tempPath, std::ios::binary);
        if (!file.is_open()) {
            throw std::runtime_error("Error opening file: " +
                                     tempPath.string());
        } else {
            file.write(chartBuffer.get(), cSize);
            if (file.fail()) {
                throw std::runtime_error("Error writing to file: " +
                                         tempPath.string());
            }
            file.close();
        }

        print_debug_message("Verifying...");

        // Read the saved file.
        std::ifstream vfile(tempPath, std::ios::binary);
        if (!vfile.is_open()) {
            throw std::runtime_error(
                "Error opening saved file for verification: " +
                tempPath.string());
        } else {
            std::string buffer((std::istreambuf_iterator<char>(vfile)),
                               std::istreambuf_iterator<char>());
            vfile.close();

            if (verify_project_buffer(buffer.c_str(), buffer.size()) != 0) {
                throw std::exception("Saved file is corrupted.");
            }
        }

        fs::rename(tempPath, finalPath);
        print_debug_message("Project save completed.");

    } catch (const std::exception& e) {
        if (fs::exists(tempPath))
            fs::remove(tempPath);

        print_debug_message("Encounter errors. Details:" +
                            gb2312ToUtf8(e.what()));
        err = true;
        errInfo = gb2312ToUtf8(e.what());
    }

    push_async_event({PROJECT_SAVING, err ? -1 : 0, errInfo});
}

void save_project(const char* projectProp, const char* filePath,
                  double compressionLevel) {
    SaveProjectParams params;
    params.projectProp.assign(projectProp);
    params.filePath.assign(filePath);
    params.compressionLevel = (int)compressionLevel;
    std::thread t([=]() { __async_save_project(params); });
    t.detach();
    return;
}

DYCORE_API double DyCore_save_project(const char* projectProp,
                                      const char* filePath,
                                      double compressionLevel) {
    namespace fs = std::filesystem;

    if (!filePath || strlen(filePath) == 0) {
        throw_error_event("File path is empty.");
        return -1;
    }

    fs::path path = fs::path((char8_t*)filePath);
    fs::path parentDir = path.parent_path();
    if (!parentDir.empty() && !fs::exists(parentDir)) {
        throw_error_event("Parent directory does not exist: " +
                          parentDir.string());
        return -1;
    }

    save_project(projectProp, filePath, compressionLevel);
    return 0;
}

string get_project_string(string projectProp) {
    json js;
    try {
        js = json::parse(projectProp);
    } catch (json::exception& e) {
        print_debug_message("Parse failed:" + string(e.what()));
        print_debug_message("Project property: " + projectProp);
        return "";
    }

    // Get the final notes array.
    mtxSaveProject.lock();
    std::vector<Note> notes;
    for (auto note : currentNoteMap)
        if (note.second.noteType != 3)
            notes.push_back(note.second);
    mtxSaveProject.unlock();

    js["charts"]["notes"] = notes;
    return nlohmann::to_string(js);
}

double get_project_buffer(string projectString, char* targetBuffer,
                          double compressionLevel) {
    return DyCore_compress_string(projectString.c_str(), targetBuffer,
                                  compressionLevel);
}

// Insert the notes array into the project property.
// Might be called asynchronously.
DYCORE_API double DyCore_get_project_buffer(const char* projectProp,
                                            char* targetBuffer,
                                            double compressionLevel) {
    string project = get_project_string(projectProp);

    return get_project_buffer(project, targetBuffer, compressionLevel);
}
