// skill_registry.cpp — populates kSkillRegistry[] with all curated skills.
// Content is embedded from src/generated/skill_content.h (built by embed_skills.cmake).

#include "skill_registry.h"
#include "generated/skill_content.h"

namespace terry {

// Registry order: RECOMMENDED tier first (always pre-checked), then OPTIONAL picks.
// Within each tier, skills are grouped by function.

const SkillEntry kSkillRegistry[] = {

    // ── RECOMMENDED: Token Saver (caveman bundle) ────────────────────────────
    {
        "caveman",
        "Token Saver",
        "AI responds in ~65% fewer words. Same quality, lower cost.",
        "JuliusBrussee/caveman",
        ".github/skills/caveman/SKILL.md",
        kCavemanContent,
        true
    },
    {
        "caveman-commit",
        "Terse Commits",
        "Conventional Commits messages, no filler.",
        "JuliusBrussee/caveman",
        ".github/skills/caveman-commit/SKILL.md",
        kCavemanCommitContent,
        true
    },
    {
        "caveman-compress",
        "Compress Memory",
        "Compress CLAUDE.md / memory files to caveman format.",
        "JuliusBrussee/caveman",
        ".github/skills/caveman-compress/SKILL.md",
        kCavemanCompressContent,
        true
    },
    {
        "caveman-help",
        "Caveman Help",
        "Quick reference for all /caveman commands.",
        "JuliusBrussee/caveman",
        ".github/skills/caveman-help/SKILL.md",
        kCavemanHelpContent,
        true
    },
    {
        "caveman-review",
        "Terse Reviews",
        "One-line code review comments: location, problem, fix.",
        "JuliusBrussee/caveman",
        ".github/skills/caveman-review/SKILL.md",
        kCavemanReviewContent,
        true
    },
    {
        "caveman-stats",
        "Token Stats",
        "Show real token usage and estimated savings for the session.",
        "JuliusBrussee/caveman",
        ".github/skills/caveman-stats/SKILL.md",
        kCavemanStatsContent,
        true
    },
    {
        "cavecrew",
        "Cavecrew Agents",
        "Delegate to investigator / builder / reviewer subagents.",
        "JuliusBrussee/caveman",
        ".github/skills/cavecrew/SKILL.md",
        kCavecrewContent,
        true
    },

    // ── RECOMMENDED: AI Project Planner (OpenSpec bundle) ───────────────────
    {
        "openspec-propose",
        "Propose Change",
        "Describe what to build — get a complete proposal with tasks.",
        "Fission-AI/OpenSpec",
        ".github/skills/openspec-propose/SKILL.md",
        kOpenspecProposeContent,
        true
    },
    {
        "openspec-apply-change",
        "Apply Change",
        "Implement tasks from an OpenSpec proposal.",
        "Fission-AI/OpenSpec",
        ".github/skills/openspec-apply-change/SKILL.md",
        kOpenspecApplyChangeContent,
        true
    },
    {
        "openspec-explore",
        "Explore Ideas",
        "Think through a problem or idea before building.",
        "Fission-AI/OpenSpec",
        ".github/skills/openspec-explore/SKILL.md",
        kOpenspecExploreContent,
        true
    },
    {
        "openspec-archive-change",
        "Archive Change",
        "Finalise and archive a completed change.",
        "Fission-AI/OpenSpec",
        ".github/skills/openspec-archive-change/SKILL.md",
        kOpenspecArchiveChangeContent,
        true
    },
    {
        "skill-creator",
        "Skill Creator",
        "Create, modify, and optimise agent skills.",
        "Fission-AI/OpenSpec",
        ".github/skills/skill-creator/SKILL.md",
        kSkillCreatorContent,
        true
    },

    // ── OPTIONAL: Work with documents ────────────────────────────────────────
    {
        "pdf",
        "PDF Tools",
        "Create, merge, split, OCR, and manipulate PDFs.",
        "Anthropic/official-skills",
        ".github/skills/pdf/SKILL.md",
        kPdfContent,
        false
    },
    {
        "pptx",
        "PowerPoint",
        "Create, read, and edit PowerPoint presentations.",
        "Anthropic/official-skills",
        ".github/skills/pptx/SKILL.md",
        kPptxContent,
        false
    },
    {
        "docx",
        "Word Documents",
        "Create, read, and edit Word documents.",
        "Anthropic/official-skills",
        ".github/skills/docx/SKILL.md",
        kDocxContent,
        false
    },
    {
        "xlsx",
        "Spreadsheets",
        "Create, read, and edit Excel spreadsheets.",
        "Anthropic/official-skills",
        ".github/skills/xlsx/SKILL.md",
        kXlsxContent,
        false
    },
    {
        "officecli",
        "OfficeCLI",
        "All Office document operations via CLI tool.",
        "iOfficeAI/OfficeCLI",
        ".github/skills/officecli/SKILL.md",
        kOfficecliContent,
        false
    },

    // ── OPTIONAL: Create visual content ─────────────────────────────────────
    {
        "canvas-design",
        "Canvas Design",
        "Create visual art and posters in PNG and PDF.",
        "Anthropic/official-skills",
        ".github/skills/canvas-design/SKILL.md",
        kCanvasDesignContent,
        false
    },
    {
        "frontend-design",
        "Frontend Design",
        "Build production-grade web interfaces and components.",
        "Anthropic/official-skills",
        ".github/skills/frontend-design/SKILL.md",
        kFrontendDesignContent,
        false
    },
};

const int kSkillRegistrySize = static_cast<int>(sizeof(kSkillRegistry) / sizeof(kSkillRegistry[0]));

} // namespace terry
