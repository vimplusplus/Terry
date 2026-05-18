#pragma once
// skill_registry.h — compile-time registry of all curated AI skills for Luggage.

namespace terry {

struct SkillEntry {
    const char* id;           // e.g. "pdf"
    const char* label;        // e.g. "PDF Tools"
    const char* description;  // one-line human description
    const char* source_repo;  // e.g. "vimplusplus/BBP"
    const char* skill_path;   // path within workspace after install
    const char* content;      // full SKILL.md text, embedded at build time
    bool        recommended;  // true = pre-selected, shown in recommended tier
};

extern const SkillEntry kSkillRegistry[];
extern const int        kSkillRegistrySize;

} // namespace terry
