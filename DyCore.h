﻿#pragma once
#include <windows.h>
#include <zstd.h>

#include <map>
#include <mutex>
#include <sol/sol.hpp>
#include <string>
#include <vector>

#include "json.hpp"
#include "pugixml.hpp"

#if !defined(_MSC_VER)
#define EXPORTED_FN __attribute__((visibility("default")))
#else
#define EXPORTED_FN __declspec(dllexport)
#endif

#define DYCORE_API extern "C" EXPORTED_FN

using json = nlohmann::json;
using std::map;
using std::string;
using std::vector;

/// Notes Management

namespace dyn {

struct Note;
inline void from_json(const json &j, Note &n);
inline void to_json(json &j, const Note &n);

struct Note {
    double time;
    int side;
    double width;
    double position;
    double lastTime;
    int noteType;
    string inst;
    double beginTime;

    string dump() {
        return json(*this).dump();
    }
    string full_dump() {
        json ret = json(*this);
        ret["inst"] = inst;
        ret["beginTime"] = beginTime;
        return ret.dump();
    }
};

inline void from_json(const json &j, Note &n) {
    j.at("time").get_to(n.time);
    j.at("side").get_to(n.side);
    j.at("width").get_to(n.width);
    j.at("position").get_to(n.position);
    j.at("lastTime").get_to(n.lastTime);
    j.at("noteType").get_to(n.noteType);
    j.at("inst").get_to(n.inst);
    j.at("beginTime").get_to(n.beginTime);
}

inline void to_json(json &j, const Note &n) {
    j = json{{"time", n.time},         {"side", n.side},
             {"width", n.width},       {"position", n.position},
             {"lastTime", n.lastTime}, {"noteType", n.noteType}};
}
}  // namespace dyn

/// Async Event Management

enum ASYNC_EVENT_TYPE { PROJECT_SAVING, GENERAL_ERROR };

struct AsyncEvent {
    ASYNC_EVENT_TYPE type;
    int status;
    string message;
};
inline void to_json(json &j, const AsyncEvent &a) {
    j = json{{"type", a.type}, {"status", a.status}, {"message", a.message}};
}

extern std::vector<AsyncEvent> asyncEventStack;
extern std::mutex mtxSaveProject;

DYCORE_API double DyCore_has_async_event();

DYCORE_API const char *DyCore_get_async_event();

/// Project Management

namespace dyn {

struct SaveProjectParams {
    std::string projectProp;
    std::string filePath;
    int compressionLevel;
};

void __async_save_project(SaveProjectParams params);

void save_project(const char *projectProp, const char *filePath,
                  double compressionLevel);

DYCORE_API double DyCore_save_project(const char *projectProp,
                                      const char *filePath,
                                      double compressionLevel);
}  // namespace dyn

/// ZSTD Stuff

/*! CHECK
 * Check that the condition holds. If it doesn't print a message and die.
 */
#define CHECK(cond, ...)                                                    \
    do {                                                                    \
        if (!(cond)) {                                                      \
            fprintf(stderr, "%s:%d CHECK(%s) failed: ", __FILE__, __LINE__, \
                    #cond);                                                 \
            fprintf(stderr, "" __VA_ARGS__);                                \
            fprintf(stderr, "\n");                                          \
            throw;                                                          \
        }                                                                   \
    } while (0)

/*! CHECK_ZSTD
 * Check the zstd error code and die if an error occurred after printing a
 * message.
 */
#define CHECK_ZSTD(fn)                                           \
    do {                                                         \
        size_t const err = (fn);                                 \
        CHECK(!ZSTD_isError(err), "%s", ZSTD_getErrorName(err)); \
    } while (0)

/// Main Functions

void throw_error_event(std::string error_info);

DYCORE_API const char *DyCore_init();

DYCORE_API const char *DyCore_delaunator(char *in_struct);

string get_project_string(string projectProp);

double get_project_buffer(string projectString, char *targetBuffer,
                          double compressionLevel);

DYCORE_API double DyCore_get_project_buffer(const char *projectProp,
                                            char *targetBuffer,
                                            double compressionLevel);

DYCORE_API double DyCore_compress_string(const char *src, char *dst,
                                         double compressionLevel);

DYCORE_API double DyCore_is_compressed(const char *str, double _sSize);

string decompress_string(const char *str, double _sSize);
string decompress_string(string str);

DYCORE_API const char *DyCore_decompress_string(const char *str, double _sSize);
std::string get_file_modification_time(char *file_path);
DYCORE_API double DyCore_clear_notes();
DYCORE_API double DyCore_sync_notes_array(const char *notesArray);
DYCORE_API double DyCore_modify_note(const char *prop);
DYCORE_API double DyCore_delete_note(const char *prop);
DYCORE_API double DyCore_insert_note(const char *prop);
DYCORE_API double DyCore_buffer_copy(void *dst, void *src, double size);
DYCORE_API double DyCore_save_project(const char *projectProp,
                                      const char *filePath,
                                      double compressionLevel);